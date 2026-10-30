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
#include "wayland_surface.h"
#include "wayland_importer.h"
#include "lsm_connector.h"

#include <poll.h>
#include <utility>

namespace LSM
{

// How often the compositor's events are read while a surface is attached.
static const guint kDispatchIntervalMs = 50;

Connector::Connector(void)
    : isRegistered(false)
{
    foreign  = std::make_shared<Wayland::Foreign>();
    surface  = std::make_shared<Wayland::Surface>();
    importer = std::make_shared<Wayland::Importer>();
}

Connector::~Connector(void)
{
    // A load that fails after the surface was attached never reaches
    // detachSurface()/unregisterID(), so do it here: otherwise the
    // compositor keeps the import and the display connection stays open.
    if (isRegistered) {
        detachSurface();
        unregisterID();
    }
    stopDispatching();
}

bool Connector::registerID(const char *windowID, const char * /*pipelineID*/)
{
    if (isRegistered)
        return false;

    //This value doesn't have meaning now.
    //It is reserved variable of wayland.
    uint32_t exportedType = WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT;

    // Each step needs the one before it: without a display there is no
    // compositor, and the importer needs wl_webos_foreign, which a
    // compositor may not offer. Calling on regardless crashes in libwayland.
    if (!foreign->initialize() ||
        !surface->initialize(foreign->getCompositor()) ||
        !importer->initialize(foreign->getWebosForeign(), windowID, exportedType)) {
        importer->finalize();
        surface->finalize();
        foreign->finalize();
        return false;
    }
    importer->setDestinationHandler([this](uint32_t width, uint32_t height) {
        onDestinationRegionChanged(static_cast<gint>(width), static_cast<gint>(height));
    });
    foreign->flush();

    isRegistered = true;

    return true;
}

bool Connector::unregisterID(void)
{
    if (!isRegistered)
        return false;

    stopDispatching();
    foreign->flush();

    importer->setDestinationHandler(nullptr);
    importer->finalize();
    surface->finalize();
    foreign->finalize();

    isRegistered = false;

    return true;
}

bool Connector::attachPunchThrough(void)
{
    if (!isRegistered)
        return false;

    importer->attachPunchThrough();
    foreign->flush();

    return true;
}

bool Connector::detachPunchThrough(void)
{
    if (!isRegistered)
        return false;

    importer->detachPunchThrough();
    foreign->flush();

    return true;
}

bool Connector::attachSurface(void)
{
    if (!isRegistered)
        return false;

    importer->attachSurface(surface->getSurface());
    foreign->flush();

    if (!dispatchSourceId)
        dispatchSourceId = g_timeout_add(kDispatchIntervalMs, dispatchTimeout, this);

    return true;
}

bool Connector::detachSurface(void)
{
    if (!isRegistered)
        return false;

    stopDispatching();
    importer->detachSurface(surface->getSurface());
    foreign->flush();

    return true;
}

struct wl_display *Connector::getDisplay(void)
{
    if (!isRegistered)
        return nullptr;

    return foreign->getDisplay();
}

struct wl_surface *Connector::getSurface(void)
{
    if (!isRegistered)
        return nullptr;

    return surface->getSurface();
}

void Connector::getVideoSize(gint &width, gint &height) {
  width = video_width;
  height = video_height;
}

void Connector::setVideoSize(gint width, gint height) {
  video_width = width;
  video_height = height;
}

void Connector::setDestinationHandler(DestinationHandler handler)
{
    // The handler runs with the lock held, here (the sink's streaming thread)
    // and on a change (the main context): otherwise the size read here could
    // reach the sink after a newer one sent from the other thread.
    std::lock_guard<std::mutex> lock(destinationMutex);
    destinationHandler = std::move(handler);
    if (destinationHandler && destination_width > 0 && destination_height > 0)
        destinationHandler(destination_width, destination_height);
}

bool Connector::getDestinationSize(gint &width, gint &height)
{
    std::lock_guard<std::mutex> lock(destinationMutex);
    width = destination_width;
    height = destination_height;
    return width > 0 && height > 0;
}

void Connector::onDestinationRegionChanged(gint width, gint height)
{
    // The compositor sends 0x0 while the exported window has no size yet:
    // there is nothing to show the video in, so keep the last real size.
    if (width <= 0 || height <= 0)
        return;

    std::lock_guard<std::mutex> lock(destinationMutex);
    if (width == destination_width && height == destination_height)
        return;
    destination_width = width;
    destination_height = height;
    if (destinationHandler)
        destinationHandler(width, height);
}

bool Connector::dispatchEvents(void)
{
    struct wl_display *display = isRegistered ? foreign->getDisplay() : nullptr;
    if (!display)
        return false;

    // A sink may be reading this display on its own queue: take part in the
    // read the way libwayland asks threads to share one connection.
    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) < 0)
            return false;
    }
    wl_display_flush(display);

    struct pollfd pfd = {wl_display_get_fd(display), POLLIN, 0};
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        if (wl_display_read_events(display) < 0)
            return false;
    } else {
        wl_display_cancel_read(display);
    }

    return wl_display_dispatch_pending(display) >= 0;
}

void Connector::stopDispatching(void)
{
    if (dispatchSourceId) {
        g_source_remove(dispatchSourceId);
        dispatchSourceId = 0;
    }

    std::lock_guard<std::mutex> lock(destinationMutex);
    destinationHandler = nullptr;
    destination_width = 0;
    destination_height = 0;
}

gboolean Connector::dispatchTimeout(gpointer data)
{
    Connector *connector = static_cast<Connector *>(data);
    if (connector->dispatchEvents())
        return G_SOURCE_CONTINUE;

    // The connection is gone; nothing more will arrive on it.
    connector->dispatchSourceId = 0;
    return G_SOURCE_REMOVE;
}

} // namespace LSM
