// SPDX-License-Identifier: Apache-2.0

// NotifySourceInfo(): the source info, and the video size as videoInfo.

#include <gtest/gtest.h>

#include <vector>

#include "player/SourceInfoNotify.h"

namespace {

struct Sent {
  gint type;
  void *payload;
};

std::vector<Sent> Notify(gmp::base::source_info_t &info) {
  std::vector<Sent> sent;
  gmp::player::NotifySourceInfo(
      [&sent](const gint type, const gint64, const gchar *, void *payload) {
        sent.push_back({type, payload});
      },
      info);
  return sent;
}

gmp::base::source_info_t WithVideo(uint32_t width, uint32_t height) {
  gmp::base::source_info_t info;
  gmp::base::video_info_t video;
  video.width = width;
  video.height = height;
  info.programs.push_back({1, 1});
  info.video_streams.push_back(video);
  info.audio_streams.push_back({});
  return info;
}

// What UriPlayer::GetSourceInfo() makes of a file with video: the size goes
// out in a videoInfo after the sourceInfo.
TEST(SourceInfoNotifyTest, VideoSizeIsSentAsVideoInfo) {
  auto info = WithVideo(640, 360);
  auto sent = Notify(info);

  ASSERT_EQ(sent.size(), 2u);
  EXPECT_EQ(sent[0].type, NOTIFY_SOURCE_INFO);
  EXPECT_EQ(sent[0].payload, &info);
  EXPECT_EQ(sent[1].type, NOTIFY_VIDEO_INFO);
  auto *video = static_cast<gmp::base::video_info_t *>(sent[1].payload);
  ASSERT_NE(video, nullptr);
  EXPECT_EQ(video->width, 640u);
  EXPECT_EQ(video->height, 360u);
}

// GetSourceInfo() leaves a zero-size video stream for an audio-only file.
TEST(SourceInfoNotifyTest, NoVideoInfoWithoutASize) {
  auto info = WithVideo(0, 0);
  auto sent = Notify(info);

  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].type, NOTIFY_SOURCE_INFO);

  auto half = WithVideo(640, 0);
  EXPECT_EQ(Notify(half).size(), 1u);
}

TEST(SourceInfoNotifyTest, NoVideoStreamsSendsTheSourceInfoOnly) {
  gmp::base::source_info_t info;
  auto sent = Notify(info);

  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].type, NOTIFY_SOURCE_INFO);
}

TEST(SourceInfoNotifyTest, NothingWithoutACallback) {
  auto info = WithVideo(640, 360);
  gmp::player::NotifySourceInfo(CALLBACK_T(), info);
  SUCCEED();
}

}  // namespace
