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

#include "wayland_foreign.h"
#include <cstdio>
#include <cstring>

static void display_handle_global(void *waylandData, struct wl_registry *registry, uint32_t id, const char *interface,
                                  uint32_t version)
{
    auto foreign = (Wayland::Foreign *)waylandData;

    if (strcmp(interface, "wl_compositor") == 0) {
        // Version 4 at most, and never more than the compositor offers:
        // binding a higher version than advertised is a protocol error.
        uint32_t bindVersion = version < 4 ? version : 4;
        foreign->setCompositor(
            (struct wl_compositor *)wl_registry_bind(registry, id, &wl_compositor_interface, bindVersion));
    } else if (strcmp(interface, "wl_shell") == 0) {
        foreign->setShell((struct wl_shell *)wl_registry_bind(registry, id, &wl_shell_interface, 1));
    } else if (strcmp(interface, "wl_webos_shell") == 0) {
        foreign->setWebosShell((struct wl_webos_shell *)wl_registry_bind(registry, id, &wl_webos_shell_interface, 1));
    } else if (strcmp(interface, "wl_webos_foreign") == 0) {
        foreign->setWebosForeign(
            (struct wl_webos_foreign *)wl_registry_bind(registry, id, &wl_webos_foreign_interface, 1));
    }
}

static void display_handle_global_remove(void * /*waylandData*/, struct wl_registry * /*registry*/,
                                         uint32_t /*name*/)
{
}

static const struct wl_registry_listener registryListener = {display_handle_global, display_handle_global_remove};

namespace Wayland
{

Foreign::Foreign(void)
    : display(nullptr), registry(nullptr), compositor(nullptr), shell(nullptr), webosShell(nullptr),
      webosForeign(nullptr)
{
}

Foreign::~Foreign(void) {}

bool Foreign::initialize(void)
{
    display = wl_display_connect(nullptr);
    if (!display) {
        return false;
    }

    registry = wl_display_get_registry(display);
    if (!registry) {
        return false;
    }

    wl_registry_add_listener(registry, &registryListener, this);

    // A roundtrip, not one dispatch: the globals need not all arrive in the
    // first read from the socket.
    if (wl_display_roundtrip(display) < 0)
        return false;

    return compositor != nullptr && webosForeign != nullptr;
}

void Foreign::finalize(void)
{
    // Cleared as they go, so that a second initialize() that does not get
    // every global back cannot leave a destroyed proxy to be destroyed again.
    if (webosForeign)
        wl_webos_foreign_destroy(webosForeign);
    webosForeign = nullptr;
    if (webosShell)
        wl_webos_shell_destroy(webosShell);
    webosShell = nullptr;
    if (shell)
        wl_shell_destroy(shell);
    shell = nullptr;
    if (compositor)
        wl_compositor_destroy(compositor);
    compositor = nullptr;
    if (registry)
        wl_registry_destroy(registry);
    registry = nullptr;

    if (display) {
        wl_display_flush(display);
        wl_display_disconnect(display);
    }
    display = nullptr;
}

void Foreign::setCompositor(struct wl_compositor *compositor) { this->compositor = compositor; }

void Foreign::setShell(struct wl_shell *shell) { this->shell = shell; }

void Foreign::setWebosShell(struct wl_webos_shell *webosShell) { this->webosShell = webosShell; }

void Foreign::setWebosForeign(struct wl_webos_foreign *webosForeign) { this->webosForeign = webosForeign; }

struct wl_display *Foreign::getDisplay(void) { return display; }

struct wl_compositor *Foreign::getCompositor(void) { return compositor; }

struct wl_shell *Foreign::getShell(void) { return shell; }

struct wl_webos_shell *Foreign::getWebosShell(void) { return webosShell; }

struct wl_webos_foreign *Foreign::getWebosForeign(void) { return webosForeign; }

struct wl_region *Foreign::createRegion(int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (!compositor)
        return nullptr;
    struct wl_region *region = wl_compositor_create_region(compositor);
    wl_region_add(region, x, y, width, height);

    return region;
}

void Foreign::destroyRegion(struct wl_region *region) { wl_region_destroy(region); }

// wl_display_flush() returns the bytes sent, or -1 on error.
bool Foreign::flush(void) { return display && wl_display_flush(display) >= 0; }

} // namespace Wayland
