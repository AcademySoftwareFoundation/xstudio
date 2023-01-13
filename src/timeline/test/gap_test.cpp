// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/timeline/gap.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

TEST(GapTest, Test) {
    auto duration = utility::FrameRateDuration(10, timebase::k_flicks_24fps);
    auto g        = Gap("Gap", duration);

    EXPECT_EQ(g.item().uuid(), g.uuid());
    EXPECT_EQ(g.item().item_type(), ItemType::IT_GAP);

    // check enable history
    {
        auto ur = g.item().set_enabled(false);
        EXPECT_FALSE(g.item().enabled());
        g.item().undo(ur);
        EXPECT_TRUE(g.item().enabled());
        g.item().redo(ur);
        EXPECT_FALSE(g.item().enabled());
    }

    {
        auto range = FrameRange(FrameRateDuration(0, 24.0), FrameRateDuration(5, 24.0));

        auto ur = g.item().set_active_range(range);
        EXPECT_EQ(*(g.item().active_range()), range);

        g.item().undo(ur);
        EXPECT_EQ((*(g.item().active_range())).frame_duration(), duration);

        g.item().redo(ur);
        EXPECT_EQ(*(g.item().active_range()), range);
    }

    {
        auto range = FrameRange(FrameRateDuration(0, 24.0), FrameRateDuration(6, 24.0));

        auto ur = g.item().set_available_range(range);
        EXPECT_EQ(*(g.item().available_range()), range);

        g.item().undo(ur);
        EXPECT_EQ((*(g.item().available_range())).frame_duration(), duration);

        g.item().redo(ur);
        EXPECT_EQ(*(g.item().available_range()), range);
    }


    // serialise test
    Gap g2(g.serialise());
    EXPECT_EQ(g2.item().uuid(), g.uuid());
}
