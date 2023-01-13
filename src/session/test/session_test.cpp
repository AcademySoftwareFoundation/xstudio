// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/session/session.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/caf_helpers.hpp"


using namespace xstudio::utility;
using namespace xstudio::session;

ACTOR_TEST_MINIMAL()

TEST(SessionTest, Test) {
    Session s{"test"};

    EXPECT_EQ(s.name(), "test");

    EXPECT_TRUE(s.empty());

    s.set_media_rate(FrameRate(1.0 / 30.0));
    EXPECT_EQ(s.media_rate().to_seconds(), FrameRate(1.0 / 30.0).to_seconds());

    Session s2{s.serialise()};
    EXPECT_EQ(s2.name(), "test");
    EXPECT_TRUE(s2.empty());
    EXPECT_EQ(s2.media_rate().to_seconds(), FrameRate(1.0 / 30.0).to_seconds());
}
