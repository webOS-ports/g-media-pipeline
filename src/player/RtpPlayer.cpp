// Copyright (c) 2022-2024 LG Electronics, Inc.
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

#include "RtpPlayer.h"
#include "ElementFactory.h"

namespace gmp { namespace player {

bool UriRtpPlayer::mIsObjRegistered = UriRtpPlayer::RegisterObject();


UriRtpPlayer::UriRtpPlayer() {}

UriRtpPlayer::~UriRtpPlayer() {}

namespace {

// Links pad to the "sink" pad of element unless that is linked already.
// Returns true when element now has its input.
bool LinkToSink(GstPad *pad, GstElement *element) {
  if (!element)
    return false;
  GstPad *sinkpad = gst_element_get_static_pad(element, "sink");
  if (!sinkpad) {
    GMP_INFO_PRINT("ERROR : Can't find sink pad of %s for linking !!",
                   GST_ELEMENT_NAME(element));
    return false;
  }
  bool linked = GST_PAD_IS_LINKED(sinkpad);
  if (!linked) {
    linked = gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK;
    if (!linked)
      GMP_INFO_PRINT("ERROR : Linking failed for the pads !!");
    else
      gst_element_sync_state_with_parent(element);
  }
  gst_object_unref(sinkpad);
  return linked;
}

}  // namespace

bool UriRtpPlayer::LoadPipeline() {

  pipeline_ = gst_pipeline_new("rtp-player");
  if (!pipeline_) {
    GMP_INFO_PRINT("ERROR : Cannot create rtp pipeline!");
    return false;
  }

  pSrcElement_ = gst_element_factory_make("rtpsrc", "streamingrtpsrc");
  pDecElement_ = gst_element_factory_make("decodebin3","decodebin3");
  pVConvertElement_ = gst_element_factory_make("videoconvert", "videoconvert");
  pVSinkElement_ = gst_element_factory_make("waylandsink", "waylandsink");
  aSink = pf::ElementFactory::Create("custom", "audio-sink", display_path_);
  pAConvertElement_ = gst_element_factory_make("audioconvert", "audioconvert");

  // videoconvert is optional; every other element is needed.
  if (!pSrcElement_ || !pDecElement_ || !pVSinkElement_ || !aSink ||
      !pAConvertElement_) {
    GMP_INFO_PRINT("ERROR : missing element: rtpsrc %p decodebin3 %p "
                   "waylandsink %p audio-sink %p audioconvert %p",
                   pSrcElement_, pDecElement_, pVSinkElement_, aSink,
                   pAConvertElement_);
    // Not in a bin yet, so these are still ours to drop.
    for (GstElement **e : {&pSrcElement_, &pDecElement_, &pVConvertElement_,
                           &pVSinkElement_, &aSink, &pAConvertElement_})
      g_clear_pointer(e, gst_object_unref);
    return false;
  }

  // The source's pad feeds decodebin.
  auto addDecoderCB = +[] (GstElement *, GstPad *pad, UriRtpPlayer *player) -> void {
    LinkToSink(pad, player->pDecElement_);
  };

  // decodebin's pads feed the video and audio branches, once each.
  auto addSinkCB = +[] (GstElement *, GstPad *pad, UriRtpPlayer *player) -> void {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
      caps = gst_pad_query_caps(pad, NULL);
    if (!caps || gst_caps_is_empty(caps)) {
      if (caps)
        gst_caps_unref(caps);
      return;
    }
    // The structure belongs to caps: use it before the unref.
    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    if (g_str_has_prefix(name, "video")) {
      LinkToSink(pad, player->pVConvertElement_ ? player->pVConvertElement_
                                                : player->pVSinkElement_);
    } else if (g_str_has_prefix(name, "audio")) {
      LinkToSink(pad, player->pAConvertElement_);
    }
    gst_caps_unref(caps);
  };

  //set stream-properties for pulsesink
  if (!RegisterTrack()) {
    GMP_INFO_PRINT("RegisterTrack failed ");
  }

  // As in UriPlayer: no video stream, or no size yet, would divide by zero.
  if (!source_info_.video_streams.empty() &&
      source_info_.video_streams[0].width > 0) {
    gint scale_width = VIDEO_SCALE_WIDTH;
    gint scale_height = (source_info_.video_streams[0].height * scale_width) /
                       source_info_.video_streams[0].width;
    lsm_connector_.setVideoSize(scale_width, scale_height);
  }

  // From here the pipeline owns the elements.
  gst_bin_add_many(GST_BIN(pipeline_), pSrcElement_, pDecElement_,
                   pAConvertElement_, aSink, pVSinkElement_, NULL);
  if (pVConvertElement_) {
    gst_bin_add(GST_BIN(pipeline_), pVConvertElement_);
    if (!gst_element_link(pVConvertElement_, pVSinkElement_)) {
      GMP_INFO_PRINT("ERROR : Linking failed pVConvertElement_ -> pVSinkElement_ !" );
      return false;
    }
  }

  if (!gst_element_link(pAConvertElement_, aSink)) {
    GMP_INFO_PRINT("ERROR : Linking failed pAConvertElement_ -> aSink !" );
    return false;
  }

  g_signal_connect(pSrcElement_,"pad-added", G_CALLBACK(addDecoderCB),this);
  g_signal_connect(pDecElement_,"pad-added", G_CALLBACK(addSinkCB),this);

  g_object_set(G_OBJECT(pSrcElement_), "uri", uri_.c_str(), NULL);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  busWatchId_ = gst_bus_add_watch(bus, UriPlayer::HandleBusMessage, this);
  gst_bus_set_sync_handler(bus, UriPlayer::HandleSyncBusMessage,
                           &lsm_connector_, NULL);

  gst_object_unref(bus);
  GMP_INFO_PRINT("LoadPipeline and Play");
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  return true;
}

}  // namespace player
}  // namespace gmp
