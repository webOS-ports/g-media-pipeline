// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_PLAYER_SOURCEINFONOTIFY_H_
#define SRC_PLAYER_SOURCEINFONOTIFY_H_

#include "PlayerTypes.h"

namespace gmp { namespace player {

// Sends the source info, then the first video stream's size as videoInfo.
//
// A client takes the video size from videoInfo: Chromium reads it from
// sourceInfo only as video_streams[programs[0].video_stream], and with one
// stream and video_stream = 1 that is past the end. During playback the
// videoInfo would come from the sink's "video-info" message, which
// waylandsink does not post. Without a size the page's video element stays
// 0x0, so no video window geometry ever reaches the compositor.
inline void NotifySourceInfo(const CALLBACK_T &notify,
                             base::source_info_t &info) {
  if (!notify)
    return;

  notify(NOTIFY_SOURCE_INFO, 0, nullptr, &info);

  if (info.video_streams.empty())
    return;

  base::video_info_t &video = info.video_streams.front();
  if (video.width > 0 && video.height > 0)
    notify(NOTIFY_VIDEO_INFO, 0, nullptr, &video);
}

}  // namespace player
}  // namespace gmp

#endif  // SRC_PLAYER_SOURCEINFONOTIFY_H_
