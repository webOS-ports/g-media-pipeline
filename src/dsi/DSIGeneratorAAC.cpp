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

#include "DSIGeneratorAAC.h"
#include "log/log.h"

namespace gmp { namespace dsi {

DSIGeneratorAAC::DSIGeneratorAAC(const MEDIA_LOAD_DATA_T* loadData)
    : channels_(0),
      audioObjectType_(0),
      sampleRate_(0)
{
    if (loadData) {
        channels_ = loadData->channels;
        audioObjectType_ = loadData->audioObjectType;
        sampleRate_ = loadData->sampleRate;
        if (loadData->codecData && loadData->codecDataSize > 0)
            audioCodecData_.assign(loadData->codecData,
                                   loadData->codecData + loadData->codecDataSize);
    }
}

DSIGeneratorAAC::~DSIGeneratorAAC() = default;

GstCaps* DSIGeneratorAAC::GenerateSpecificInfo()
{
    GMP_DEBUG_PRINT("");
    GstCaps *caps = gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, NULL);
    if (caps == NULL) {
        GMP_INFO_PRINT("caps is NULL! DSI generation fails!");
        return NULL;
    }

    // in case of received decoding specific information from application.
    // if audioCodecData doesn't initialized from application, playback is not working.
    if (audioCodecData_.empty()) {
        // AudioSpecificConfig (ISO/IEC 14496-3 1.6.2.1), MSB first:
        // audioObjectType 5 bits (31: 6 more bits of extension),
        // samplingFrequencyIndex 4 bits (15: 24 more bits of frequency),
        // channelConfiguration 4 bits.
        const guint8 audioObjectTypeExt = 0;
        guint8 codec_data[6] = {0, };
        guint8 size = 0;
        guint8 frequencyIdx = GetFrequencyIndex(sampleRate_);

        codec_data[0] = (audioObjectType_ & 0x1F) << 3;
        if (audioObjectType_ == 31) {
            // bits 5-10 extension, 11-14 frequency index
            codec_data[0] |= (audioObjectTypeExt & 0x38) >> 3;
            codec_data[1] = (audioObjectTypeExt & 0x07) << 5;
            codec_data[1] |= (frequencyIdx & 0x0F) << 1;

            if (frequencyIdx == 15) {
                // bits 15-38 frequency, 39-42 channels
                codec_data[1] |= (sampleRate_ & 0x800000) >> 23;
                codec_data[2] = (sampleRate_ & 0x7F8000) >> 15;
                codec_data[3] = (sampleRate_ & 0x007F80) >> 7;
                codec_data[4] = (sampleRate_ & 0x00007F) << 1;
                codec_data[4] |= (channels_ & 0x08) >> 3;
                codec_data[5] = (channels_ & 0x07) << 5;
                size = 6;
            } else {
                // bits 15-18 channels
                codec_data[1] |= (channels_ & 0x08) >> 3;
                codec_data[2] = (channels_ & 0x07) << 5;
                size = 3;
            }
        } else {
            // bits 5-8 frequency index
            codec_data[0] |= (frequencyIdx & 0x0E) >> 1;
            codec_data[1] = (frequencyIdx & 0x01) << 7;

            if (frequencyIdx == 15) {
                // bits 9-32 frequency, 33-36 channels
                codec_data[1] |= (sampleRate_ & 0xFE0000) >> 17;
                codec_data[2] = (sampleRate_ & 0x01FE00) >> 9;
                codec_data[3] = (sampleRate_ & 0x0001FE) >> 1;
                codec_data[4] = (sampleRate_ & 0x000001) << 7;
                codec_data[4] |= (channels_ & 0x0F) << 3;
                size = 5;
            } else {
                // bits 9-12 channels
                codec_data[1] |= (channels_ & 0x0F) << 3;
                size = 2;
            }
        }

        if ((codec_data[0] == 0x00) && (codec_data[1] == 0x00)) {
            GMP_INFO_PRINT("DSI generation success!");
            return caps;
        }
        audioCodecData_.assign(codec_data, codec_data + size);
    }

    if (!SetGstCodecData(caps)) {
        GMP_INFO_PRINT("SetGstCodecData fails!");
        gst_caps_unref(caps);
        return NULL;
    }

    GMP_INFO_PRINT("DSI generation success!");
    return caps;
}

bool DSIGeneratorAAC::SetGstCodecData(GstCaps *caps) const
{
    GstBuffer *codec_buf = gst_buffer_new_memdup(audioCodecData_.data(),
                                                 audioCodecData_.size());
    if (codec_buf == NULL) {
        GMP_INFO_PRINT("gst_buffer_new_memdup fails!");
        return false;
    }
    gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, codec_buf, NULL);
    gst_buffer_unref(codec_buf);

    return true;
}

/* Gets the Sampling Frequency Index value, as defined by table 1.18 in ISO/IEC 14496-3 */
guint8 DSIGeneratorAAC::GetFrequencyIndex(const guint32 &sampleRate) {
  GMP_DEBUG_PRINT("");
  guint8 ret = 0;
  switch (sampleRate) {
    case 96000: {
      ret = 0x0U;
      break;
    }
    case 88200: {
      ret = 0x1U;
      break;
    }
    case 64000: {
      ret = 0x2U;
      break;
    }
    case 48000: {
      ret = 0x3U;
      break;
    }
    case 44100: {
      ret = 0x4U;
      break;
    }
    case 32000: {
      ret = 0x5U;
      break;
    }
    case 24000: {
      ret = 0x6U;
      break;
    }
    case 22050: {
      ret = 0x7U;
      break;
    }
    case 16000: {
      ret = 0x8U;
      break;
    }
    case 12000: {
      ret = 0x9U;
      break;
    }
    case 11025: {
      ret = 0xAU;
      break;
    }
    case 8000: {
      ret = 0xBU;
      break;
    }
    case 7350: {
      ret = 0xCU;
      break;
    }
    default: {
      ret = 0xFU;
      break;
    }
  }

  return ret;
}

}  // namespace dsi
}  // namespace gmp


