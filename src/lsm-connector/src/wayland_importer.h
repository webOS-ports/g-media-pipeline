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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#ifndef _WAYLAND_IMPORTER_H_
#define _WAYLAND_IMPORTER_H_

#include "common.h"
#include <functional>
#include <wayland-webos-foreign-client-protocol.h>

namespace Wayland
{
class Importer
{
public:
    Importer(void);
    ~Importer(void);

    bool initialize(struct wl_webos_foreign *foreign, const char *windowID, uint32_t exportedType);
    void finalize(void);

    struct wl_webos_imported *getWebosImported(void);

    void attachPunchThrough(void);
    void detachPunchThrough(void);

    void attachSurface(struct wl_surface *surface);
    void detachSurface(struct wl_surface *surface);

    // The compositor's destination_region_changed: the size it shows the
    // imported surface at, sent on attach and whenever it changes.
    using DestinationHandler = std::function<void(uint32_t width, uint32_t height)>;
    void setDestinationHandler(DestinationHandler handler);

private:
    DISALLOW_COPY_AND_ASSIGN(Importer);

    static void handleDestinationRegionChanged(void *data, struct wl_webos_imported *imported,
                                               uint32_t width, uint32_t height);
    static const struct wl_webos_imported_listener importedListener;

    struct wl_webos_imported *webosImported;
    DestinationHandler destinationHandler;
};

} //namespace Wayland

#endif //_WAYLAND_IMPORTER_H_
