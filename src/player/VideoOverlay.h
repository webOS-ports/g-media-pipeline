// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_PLAYER_VIDEOOVERLAY_H_
#define SRC_PLAYER_VIDEOOVERLAY_H_

#include <memory>

#include <gst/video/videooverlay.h>
#include <lsm_connector.h>
#include <log/log.h>

namespace gmp { namespace player {

// Gives the video sink the connector's surface, and keeps the sink's render
// rectangle on the area the compositor shows that surface at.
//
// The compositor sizes the imported surface to the exported window's
// destination and sends that size in destination_region_changed. It does
// not scale an imported surface's subsurfaces, and waylandsink draws the
// video in subsurfaces placed at the render rectangle, so the rectangle has
// to be that size at the origin. The video used to be centred on a 1920x1080
// screen instead: on a phone that put it at (420,236) on a 448x252 surface,
// where nothing of it showed.
inline void AttachVideoOverlay(LSM::Connector *connector, GstVideoOverlay *overlay)
{
  gst_video_overlay_set_window_handle(overlay, (guintptr)(connector->getSurface()));

  std::shared_ptr<GstVideoOverlay> sink(
      GST_VIDEO_OVERLAY(gst_object_ref(overlay)),
      [](GstVideoOverlay *o) { gst_object_unref(o); });
  connector->setDestinationHandler([sink](gint width, gint height) {
    GMP_DEBUG_PRINT("Set render rectangle :(0, 0, %d, %d)", width, height);
    gst_video_overlay_set_render_rectangle(sink.get(), 0, 0, width, height);
    gst_video_overlay_expose(sink.get());
  });
}

}  // namespace player
}  // namespace gmp

#endif  // SRC_PLAYER_VIDEOOVERLAY_H_
