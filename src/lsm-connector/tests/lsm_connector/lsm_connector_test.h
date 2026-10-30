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

#include <gtest/gtest.h>
#include <iostream>
#include <utility>
#include <vector>
#include "lsm_connector.h"
#include "api_call_checker.h"
#include "wayland-client.h"

class LSMConnectorTest1 : public ::testing::Test
{
protected:
    LSM::Connector connector;
};

TEST_F(LSMConnectorTest1, RegisterAndUnregister)
{
    //Arrange
    bool expected          = true;
    const char *windowID   = "TEST_WINDOW_ID";
    const char *pipelineID = "TEST_PIPELINE_ID";

    //Act
    bool actual = connector.registerID(windowID, pipelineID);

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = connector.unregisterID();

    //Assert
    EXPECT_EQ(expected, actual);
}

class LSMConnectorTest2 : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        const char *windowID   = "TEST_WINDOW_ID";
        const char *pipelineID = "TEST_PIPELINE_ID";
        connector.registerID(windowID, pipelineID);
    }

    void TearDown(void)
    {
        connector.unregisterID();
    }

    LSM::Connector connector;
};

TEST_F(LSMConnectorTest2, Punchthrough)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    connector.attachPunchThrough();
    bool actual = isAPICalled("Wayland::Importer::attachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    connector.detachPunchThrough();
    actual = isAPICalled("Wayland::Importer::detachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMConnectorTest2, Surface)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    connector.attachSurface();
    bool actual = isAPICalled("Wayland::Importer::attachSurface");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    connector.detachSurface();
    actual = isAPICalled("Wayland::Importer::detachSurface");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMConnectorTest2, GetDisplay)
{
    //Arrange

    //Act
    struct wl_display *actual = connector.getDisplay();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(LSMConnectorTest2, GetSurface)
{
    //Arrange

    //Act
    struct wl_surface *actual = connector.getSurface();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}
// The size the compositor shows the video at, as the sink is told about it.
class LSMConnectorDestinationTest : public LSMConnectorTest2
{
protected:
    void SetUp(void) override
    {
        LSMConnectorTest2::SetUp();
        connector.attachSurface();
    }

    void TearDown(void) override
    {
        connector.detachSurface();
        LSMConnectorTest2::TearDown();
    }

    void watch(void)
    {
        connector.setDestinationHandler([this](gint w, gint h) { sizes.emplace_back(w, h); });
    }

    std::vector<std::pair<gint, gint>> sizes;
};

TEST_F(LSMConnectorDestinationTest, EachChangeReachesTheHandler)
{
    watch();
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));
    ASSERT_TRUE(fake_send_destination_region_changed(268, 151));

    ASSERT_EQ(2u, sizes.size());
    EXPECT_EQ(std::make_pair(448, 252), sizes[0]);
    EXPECT_EQ(std::make_pair(268, 151), sizes[1]);

    gint w = 0, h = 0;
    EXPECT_TRUE(connector.getDestinationSize(w, h));
    EXPECT_EQ(268, w);
    EXPECT_EQ(151, h);
}

// The size usually arrives before the sink asks for its window: a handler set
// later gets it at once.
TEST_F(LSMConnectorDestinationTest, ALateHandlerGetsTheLastSize)
{
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));
    watch();

    ASSERT_EQ(1u, sizes.size());
    EXPECT_EQ(std::make_pair(448, 252), sizes[0]);
}

// 0x0 is what the compositor sends before the exported window has a size.
TEST_F(LSMConnectorDestinationTest, AnEmptySizeIsIgnored)
{
    watch();
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));
    ASSERT_TRUE(fake_send_destination_region_changed(0, 0));
    ASSERT_TRUE(fake_send_destination_region_changed(448, 0));

    ASSERT_EQ(1u, sizes.size());
    gint w = 0, h = 0;
    EXPECT_TRUE(connector.getDestinationSize(w, h));
    EXPECT_EQ(448, w);
    EXPECT_EQ(252, h);
}

TEST_F(LSMConnectorDestinationTest, TheSameSizeIsNotSentTwice)
{
    watch();
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));

    EXPECT_EQ(1u, sizes.size());
}

// Detaching drops the handler and the size, so nothing reaches a sink that
// is going away, and the next attach starts from what the compositor sends.
TEST_F(LSMConnectorDestinationTest, DetachForgetsHandlerAndSize)
{
    watch();
    ASSERT_TRUE(fake_send_destination_region_changed(448, 252));
    connector.detachSurface();

    gint w = 0, h = 0;
    EXPECT_FALSE(connector.getDestinationSize(w, h));

    fake_send_destination_region_changed(268, 151);
    EXPECT_EQ(1u, sizes.size());
}

// Reading events: the read is prepared and, with nothing to read, cancelled,
// and what was queued is dispatched.
TEST_F(LSMConnectorDestinationTest, DispatchEventsSharesTheRead)
{
    clearCallLog();
    EXPECT_TRUE(connector.dispatchEvents());
    EXPECT_TRUE(isAPICalled("wl_display_prepare_read"));
    EXPECT_TRUE(isAPICalled("wl_display_cancel_read"));
    EXPECT_FALSE(isAPICalled("wl_display_read_events"));
    EXPECT_TRUE(isAPICalled("wl_display_dispatch_pending"));
}

TEST_F(LSMConnectorTest1, DispatchEventsNeedsARegisteredConnection)
{
    EXPECT_FALSE(connector.dispatchEvents());
}

// Attaching starts reading the compositor's events on the default main
// context, detaching stops it.
TEST_F(LSMConnectorDestinationTest, EventsAreReadFromTheMainContext)
{
    clearCallLog();
    GMainContext *context = g_main_context_default();
    gint64 until = g_get_monotonic_time() + 2 * G_USEC_PER_SEC;
    // Never block: with no source attached a blocking iteration would wait
    // forever instead of failing.
    while (!isAPICalled("wl_display_dispatch_pending") && g_get_monotonic_time() < until) {
        if (!g_main_context_iteration(context, FALSE))
            g_usleep(1000);
    }
    EXPECT_TRUE(isAPICalled("wl_display_dispatch_pending"));

    connector.detachSurface();
    clearCallLog();
    until = g_get_monotonic_time() + 200 * 1000;
    while (g_get_monotonic_time() < until) {
        if (!g_main_context_iteration(context, FALSE))
            g_usleep(1000);
    }
    EXPECT_FALSE(isAPICalled("wl_display_dispatch_pending"));
}
