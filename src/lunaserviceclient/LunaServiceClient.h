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

#ifndef SRC_LUNA_CLIENT_H_
#define SRC_LUNA_CLIENT_H_

#include <luna-service2/lunaservice.h>
#include <glib.h>

#include <map>
#include <memory>
#include <thread>
#include <functional>

namespace gmp {
typedef std::function<void (const char *)> ResponseHandler;
class LunaServiceClient;
    struct ResponseHandlerWrapper {
          ResponseHandler handler;
          LunaServiceClient *owner = nullptr;
          explicit ResponseHandlerWrapper(ResponseHandler rh) : handler(std::move(rh)) { }
    };

bool handleAsync(LSHandle *sh, LSMessage *reply, void *ctx);
bool handleSubscribe(LSHandle *sh, LSMessage *reply, void *ctx);

class LunaServiceClient {
public:
   LunaServiceClient();
   ~LunaServiceClient();

   bool CallAsync(const char *uri,
                  const char *param,
                  ResponseHandler handler);
   bool subscribe(const char *uri,
                  const char *param,
                  unsigned long *subscribeKey,
                  ResponseHandler handler);
   bool unsubscribe(unsigned long subscribeKey);

   LunaServiceClient(const LunaServiceClient &) = delete;
   LunaServiceClient &operator=(const LunaServiceClient &) = delete;

private:
  friend bool handleAsync(LSHandle *sh, LSMessage *reply, void *ctx);
  friend bool handleSubscribe(LSHandle *sh, LSMessage *reply, void *ctx);
  static void RunHandler(const ResponseHandler &handler, LSMessage *reply);

  LSHandle* handle;
  GMainContext* context;
  std::map<unsigned long, std::unique_ptr<ResponseHandlerWrapper>> handlers;
  // CallAsync() calls still waiting for their reply.
  std::map<ResponseHandlerWrapper *, std::unique_ptr<ResponseHandlerWrapper>> pending;
};//LunaServiceClient

} //gmp

#endif  // SRC_LUNA_CLIENT_H_
