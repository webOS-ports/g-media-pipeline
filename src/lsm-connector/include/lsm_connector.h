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

/** @file lsm_connector.h
 *
 *  @author     Dongheon Kim (dongheon.kim@lge.com)
 *  @version    1.0
 *  @date       2018.09.07
 *  @note
 *  @see
 */

#ifndef _LSM_CONNECTOR_H_
#define _LSM_CONNECTOR_H_

#include <glib.h>
#include <functional>
#include <memory>
#include <mutex>

struct wl_display;
struct wl_surface;

namespace Wayland
{
class Foreign;
class Importer;
class Surface;
}

namespace LSM
{

class Connector
{
public:
    Connector(void);
    ~Connector(void);

    bool registerID(const char *windowID, const char *pipelineID);
    bool unregisterID(void);

    bool attachPunchThrough(void);
    bool detachPunchThrough(void);

    bool attachSurface(void);
    bool detachSurface(void);

    struct wl_display *getDisplay(void);
    struct wl_surface *getSurface(void);

    void getVideoSize(gint &width, gint &height);
    void setVideoSize(gint width, gint height);

    // The size the compositor shows the imported surface at. The handler is
    // called right away with the last size known, then on every change, from
    // the thread that runs the default GLib main context; nullptr clears it.
    // It runs under the connector's lock, so it must not call back into the
    // connector.
    // While a surface is attached the compositor's events are read on a
    // timeout there.
    using DestinationHandler = std::function<void(gint width, gint height)>;
    void setDestinationHandler(DestinationHandler handler);
    bool getDestinationSize(gint &width, gint &height);

    // Reads and dispatches the compositor's events for this connection
    // without blocking. Safe next to a sink's own event queue on the same
    // display.
    bool dispatchEvents(void);

private:
    //Disallow copy and assign
    Connector(const Connector &);
    void operator=(const Connector &);

    void onDestinationRegionChanged(gint width, gint height);
    void stopDispatching(void);
    static gboolean dispatchTimeout(gpointer data);

    gint video_width = 0;
    gint video_height = 0;

    std::mutex destinationMutex;
    DestinationHandler destinationHandler;
    gint destination_width = 0;
    gint destination_height = 0;
    guint dispatchSourceId = 0;

    std::shared_ptr<Wayland::Foreign> foreign;
    std::shared_ptr<Wayland::Surface> surface;
    std::shared_ptr<Wayland::Importer> importer;

    bool isRegistered;
};

} //namespace LSM

#endif //_LSM_CONNECTOR_H_
