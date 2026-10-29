// SPDX-License-Identifier: Apache-2.0

// UriPlayerFactory hands each player to the caller alone.

#include <gtest/gtest.h>

#include <memory>

#include "playerfactory/UriPlayerFactory.h"
#include "player/Player.h"

namespace {

class FakePlayer : public gmp::player::Player {
 public:
  bool Load(const std::string &) override { return true; }
  bool Load(const MEDIA_LOAD_DATA_T *) override { return true; }
  bool Unload() override { return true; }
  bool Play() override { return true; }
  bool Pause() override { return true; }
  bool SetPlayRate(const double) override { return true; }
  bool Seek(const int64_t) override { return true; }
  bool SetVolume(int) override { return true; }
  bool SetPlane(int) override { return true; }
  bool SetDisplayPath(const uint32_t) override { return true; }
  void notifyFunctionUMSPolicyAction() override {}
  bool UpdateVideoResData(const gmp::base::source_info_t &) override { return true; }
  MEDIA_STATUS_T Feed(const guint8 *, guint32, guint64, MEDIA_DATA_CHANNEL_T) override {
    return MEDIA_OK;
  }
  bool Flush() override { return true; }
  void RegisterCbFunction(CALLBACK_T) override {}
  bool PushEndOfStream() override { return true; }
  GstElement *GetPipeline() override { return nullptr; }
};

std::shared_ptr<gmp::player::Player> MakeFake() {
  return std::make_shared<FakePlayer>();
}

TEST(UriPlayerFactoryTest, PlayersAreOwnedByTheCaller) {
  auto *factory = gmp::pf::UriPlayerFactory::getInstance();
  ASSERT_TRUE(factory->Register("faketest", MakeFake));
  EXPECT_FALSE(factory->Register("faketest", MakeFake));

  std::weak_ptr<gmp::player::Player> first;
  {
    auto player = factory->CreateUriTypePlayer("faketest");
    ASSERT_TRUE(player);
    first = player;
  }
  // The factory used to keep the first player of a protocol for good.
  EXPECT_TRUE(first.expired());

  auto a = factory->CreateUriTypePlayer("faketest");
  auto b = factory->CreateUriTypePlayer("faketest");
  EXPECT_NE(a, b);
  EXPECT_EQ(a.use_count(), 1);

  EXPECT_FALSE(factory->CreateUriTypePlayer("nosuchprotocol"));

  EXPECT_TRUE(factory->UnRegister("faketest"));
  EXPECT_FALSE(factory->UnRegister("faketest"));
  EXPECT_FALSE(factory->CreateUriTypePlayer("faketest"));
}

}  // namespace
