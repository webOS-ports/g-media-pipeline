// SPDX-License-Identifier: Apache-2.0

// The AudioSpecificConfig that DSIGeneratorAAC builds for raw AAC, checked
// against an independent bit writer and against GStreamer's own parser.

#include <gtest/gtest.h>
#include <gst/gst.h>
#include <gst/pbutils/codec-utils.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "dsi/DSIGeneratorAAC.h"

namespace {

// MSB-first bit writer, as ISO/IEC 14496-3 lays the fields out.
struct Bits {
  std::vector<guint8> bytes;
  size_t count = 0;
  Bits &put(guint32 value, int width) {
    for (int i = width - 1; i >= 0; --i) {
      if (count % 8 == 0)
        bytes.push_back(0);
      if ((value >> i) & 1)
        bytes.back() |= 0x80 >> (count % 8);
      ++count;
    }
    return *this;
  }
};

MEDIA_LOAD_DATA_T Aac(guint32 objectType, guint32 sampleRate, guint32 channels) {
  MEDIA_LOAD_DATA_T data;
  data.audioCodec = GMP_AUDIO_CODEC_AAC;
  data.audioObjectType = objectType;
  data.sampleRate = sampleRate;
  data.channels = channels;
  return data;
}

std::vector<guint8> CodecData(const MEDIA_LOAD_DATA_T &data) {
  gmp::dsi::DSIGeneratorAAC generator(&data);
  GstCaps *caps = generator.GenerateSpecificInfo();
  EXPECT_NE(caps, nullptr);
  if (!caps)
    return {};
  std::vector<guint8> out;
  const GValue *value =
      gst_structure_get_value(gst_caps_get_structure(caps, 0), "codec_data");
  if (value) {
    GstBuffer *buffer = gst_value_get_buffer(value);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      out.assign(map.data, map.data + map.size);
      gst_buffer_unmap(buffer, &map);
    }
  }
  gst_caps_unref(caps);
  return out;
}

class DSIGeneratorAACTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { gst_init(nullptr, nullptr); }
};

TEST_F(DSIGeneratorAACTest, TableRate) {
  auto got = CodecData(Aac(2, 44100, 2));
  EXPECT_EQ(got, Bits().put(2, 5).put(4, 4).put(2, 4).bytes);
  ASSERT_EQ(got.size(), 2u);
  EXPECT_EQ(gst_codec_utils_aac_get_sample_rate(got.data(), got.size()), 44100u);
  EXPECT_EQ(gst_codec_utils_aac_get_channels(got.data(), got.size()), 2u);
}

// Channel configuration 8 sets the top channel bit, which landed in the
// frequency index after an escaped object type.
TEST_F(DSIGeneratorAACTest, TableRateTopChannelBit) {
  EXPECT_EQ(CodecData(Aac(2, 48000, 8)),
            Bits().put(2, 5).put(3, 4).put(8, 4).bytes);
}

// A rate outside the table is sent explicitly (index 15, 24 bits).
TEST_F(DSIGeneratorAACTest, ExplicitRate) {
  auto got = CodecData(Aac(2, 44001, 1));
  EXPECT_EQ(got, Bits().put(2, 5).put(15, 4).put(44001, 24).put(1, 4).bytes);
  ASSERT_EQ(got.size(), 5u);
  EXPECT_EQ(gst_codec_utils_aac_get_sample_rate(got.data(), got.size()), 44001u);
  // (gst_codec_utils_aac_get_channels() only reads the two-byte layout.)
}

TEST_F(DSIGeneratorAACTest, EscapedObjectType) {
  EXPECT_EQ(CodecData(Aac(31, 44100, 8)),
            Bits().put(31, 5).put(0, 6).put(4, 4).put(8, 4).bytes);
}

TEST_F(DSIGeneratorAACTest, EscapedObjectTypeExplicitRate) {
  EXPECT_EQ(CodecData(Aac(31, 44001, 8)),
            Bits().put(31, 5).put(0, 6).put(15, 4).put(44001, 24).put(8, 4).bytes);
}

// Codec data from the application is passed on as it is, and stays the
// caller's: the generator used to delete[] it.
TEST_F(DSIGeneratorAACTest, CallerCodecDataIsCopied) {
  auto owned = std::make_unique<guint8[]>(2);
  owned[0] = 0x12;
  owned[1] = 0x10;
  MEDIA_LOAD_DATA_T data = Aac(2, 44100, 2);
  data.codecData = owned.get();
  data.codecDataSize = 2;
  EXPECT_EQ(CodecData(data), (std::vector<guint8>{0x12, 0x10}));
  // owned is freed here, once; ASan reports a double free otherwise.
  EXPECT_EQ(owned[0], 0x12);
}

}  // namespace
