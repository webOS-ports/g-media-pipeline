// Copyright (c) 2018-2021 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <chrono>
#include <exception>
#include <string>
#include <unistd.h>

#include "LunaServiceClient.h"
#include "log.h"

namespace gmp {

struct AutoLSError : LSError {
  AutoLSError() { LSErrorInit(this); }
  ~AutoLSError() { LSErrorFree(this); }
};

std::string kServiceName = "com.webos.pipeline";

// LunaServiceClient implematation
LunaServiceClient::LunaServiceClient()
    : handle(nullptr)
    , context(nullptr) {
  GMP_INFO_PRINT("LunaServiceClient IN");
  AutoLSError error;
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

  // The time alone is not unique: two pipeline processes, or two players
  // in one process, can start in the same millisecond.
  static std::atomic<unsigned> instance_count{0};
  std::string service_name = kServiceName + std::to_string(ms.count()) +
                             "." + std::to_string(getpid()) +
                             "." + std::to_string(instance_count++);

  GMP_INFO_PRINT("LunaServiceClient service_name : %s",service_name.c_str());

  if (!LSRegister(service_name.c_str(), &handle, &error)) {
    GMP_INFO_PRINT("LSRegister %s failed: %s", service_name.c_str(),
                   error.message ? error.message : "");
    handle = nullptr;
  } else if (!LSGmainContextAttach(handle, g_main_context_default(), &error)) {
    GMP_INFO_PRINT("LSGmainContextAttach failed: %s",
                   error.message ? error.message : "");
    AutoLSError unregister_error;
    LSUnregister(handle, &unregister_error);
    handle = nullptr;
  } else {
    context = g_main_context_ref(g_main_context_default());
  }
  GMP_INFO_PRINT("LunaServiceClient OUT");
}

LunaServiceClient::~LunaServiceClient() {
  GMP_INFO_PRINT("~LunaServiceClient IN");
  AutoLSError error;
  // No reply is delivered after this, so the pending handlers can go.
  if (handle)
    LSUnregister(handle, &error);
  if (context)
    g_main_context_unref(context);
  pending.clear();
  handlers.clear();
  GMP_INFO_PRINT("~LunaServiceClient OUT");
}

bool LunaServiceClient::subscribe(const char *uri,
                                  const char *param,
                                  unsigned long *subscribeKey,
                                  ResponseHandler handler) {
  GMP_INFO_PRINT("LunaServiceClient subscribe IN");
  AutoLSError error;
  if (!handle)
    return false;
  ResponseHandlerWrapper *wrapper = new ResponseHandlerWrapper(std::move(handler));
  if (!wrapper) {
    GMP_INFO_PRINT("LunaServiceClient subscribe wrapper NULL");
    return false;
  }

  if (!LSCall(handle, uri, param, handleSubscribe,
               wrapper, subscribeKey, &error)) {
     GMP_INFO_PRINT("LunaServiceClient subscribe LSCall failed");
     delete wrapper;
     return false;
  }

  handlers[*subscribeKey] = std::unique_ptr<ResponseHandlerWrapper>(wrapper);
  GMP_INFO_PRINT("LunaServiceClient subscribe OUT");
  return true;
}

bool LunaServiceClient::unsubscribe(unsigned long subscribeKey) {
  GMP_INFO_PRINT("LunaServiceClient unsubscribe IN");
  AutoLSError error;
  if (!LSCallCancel(handle, subscribeKey, &error)) {
    GMP_INFO_PRINT("LunaServiceClient unsubscribe LSCallCancel failed");
    handlers.erase(subscribeKey);
    return false;
  }

  handlers.erase(subscribeKey);
  GMP_INFO_PRINT("LunaServiceClient unsubscribe OUT");
  return true;
}

bool handleSubscribe(LSHandle * /*sh*/, LSMessage *reply, void *ctx) {
  GMP_INFO_PRINT("LunaServiceClient handleSubscribe IN");
  ResponseHandlerWrapper *wrapper = static_cast<ResponseHandlerWrapper *>(ctx);
  if (!wrapper) {
    GMP_INFO_PRINT("LunaServiceClient handleSubscribe wrapper NULL");
    return false;
  }

  LunaServiceClient::RunHandler(wrapper->handler, reply);

  GMP_INFO_PRINT("LunaServiceClient handleSubscribe OUT");

  return true;
}

bool LunaServiceClient::CallAsync(const char *uri,
                                  const char *param,
                                  ResponseHandler handler) {
  GMP_INFO_PRINT("LunaServiceClient CallAsync IN");
  AutoLSError error;
  if (!handle)
    return false;

  // The client owns the wrapper until the one reply comes, or until it is
  // destroyed (LSUnregister drops the replies still to come).
  auto wrapper = std::make_unique<ResponseHandlerWrapper>(std::move(handler));
  wrapper->owner = this;

  if (!LSCallOneReply(handle, uri, param, handleAsync, wrapper.get(),
                      NULL, &error)) {
    GMP_INFO_PRINT("LunaServiceClient CallAsync LSCallOneReply failed: %s",
                   error.message ? error.message : "");
    return false;
  }
  ResponseHandlerWrapper *key = wrapper.get();
  pending.emplace(key, std::move(wrapper));
  GMP_INFO_PRINT("LunaServiceClient CallAsync OUT");
  return true;
}

void LunaServiceClient::RunHandler(const ResponseHandler &handler,
                                   LSMessage *reply) {
  if (!handler)
    return;
  const char *payload = LSMessageGetPayload(reply);
  // An exception must not unwind into luna-service2's C code.
  try {
    handler(payload ? payload : "{}");
  } catch (const std::exception &e) {
    GMP_INFO_PRINT("LunaServiceClient reply handler threw: %s", e.what());
  } catch (...) {
    GMP_INFO_PRINT("LunaServiceClient reply handler threw");
  }
}

bool handleAsync(LSHandle * /*sh*/, LSMessage *reply, void *ctx) {
  GMP_INFO_PRINT("LunaServiceClient handleAsync IN");
  ResponseHandlerWrapper *wrapper = static_cast<ResponseHandlerWrapper *>(ctx);
  if (!wrapper || !wrapper->owner) {
    return false;
  }

  LunaServiceClient *client = wrapper->owner;
  auto it = client->pending.find(wrapper);
  if (it == client->pending.end())
    return false;
  // Take it out first: the handler may call CallAsync() again.
  std::unique_ptr<ResponseHandlerWrapper> owned = std::move(it->second);
  client->pending.erase(it);
  LunaServiceClient::RunHandler(owned->handler, reply);

  GMP_INFO_PRINT("LunaServiceClient handleAsync OUT");

  return true;
}

} //gmp
