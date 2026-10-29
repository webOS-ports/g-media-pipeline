// Copyright (c) 2018-2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <log/log.h>
#include <service/service.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <glib-unix.h>

#define MAX_SERVICE_STRING 120

int main(int argc, char * argv[]) {
  int c;
  char service_name[MAX_SERVICE_STRING+1] = {'\0',};
  bool service_name_specified = false;

  while ((c = getopt(argc, argv, "s:")) != -1) {
    switch (c) {
      case 's':
        snprintf(service_name, sizeof(service_name), "%s", optarg);
        service_name_specified = true;
        break;

      case '?':
        GMP_DEBUG_PRINT("unknown option or missing service name");
        break;

      default:
        break;
    }
  }

  if (!service_name_specified)
    return 1;

  gmp::service::Service* service
    = gmp::service::Service::GetInstance(service_name);

  // umediaserver ends a pipeline with SIGTERM: unload (audiod track, media
  // resources) before the main loop ends.
  auto shutdown = +[](gpointer data) -> gboolean {
    static_cast<gmp::service::Service *>(data)->Shutdown();
    return G_SOURCE_REMOVE;
  };
  g_unix_signal_add(SIGTERM, shutdown, service);
  g_unix_signal_add(SIGINT, shutdown, service);

  service->Wait();

  delete service;

  return 0;
}
