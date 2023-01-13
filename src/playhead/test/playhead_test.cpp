// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/playhead/playhead.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::playhead;

TEST(PlayheadTest, Test) {
    PlayheadBase p("test");

    EXPECT_FALSE(p.playing());
    EXPECT_TRUE(p.loop());
    EXPECT_TRUE(p.forward());

    p.set_loop();
    EXPECT_EQ(p.loop(), LM_LOOP);
    p.set_loop(LM_PLAY_ONCE);
    EXPECT_EQ(p.loop(), LM_PLAY_ONCE);

    p.set_forward(false);
    EXPECT_FALSE(p.forward());
    p.set_forward();
    EXPECT_TRUE(p.forward());

    p.set_playing();
    EXPECT_TRUE(p.playing());
    p.set_playing(false);
    EXPECT_FALSE(p.playing());
}
