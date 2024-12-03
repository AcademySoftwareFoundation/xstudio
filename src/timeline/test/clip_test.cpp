// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::timeline;

TEST(ClipTest, Test) {
    Clip c;

    // check uuid copied
    EXPECT_EQ(c.item().uuid(), c.uuid());
    EXPECT_FALSE(c.item().uuid().is_null());

    // check type
    EXPECT_EQ(c.item().item_type(), ItemType::IT_CLIP);

    // check initial value
    EXPECT_EQ(c.item().trimmed_range().duration(), timebase::k_flicks_zero_seconds);

    EXPECT_EQ(c.item().top_left().first, 0);
    EXPECT_EQ(c.item().top_left().second, 0);

    EXPECT_EQ(c.item().bottom_right().first, 0);
    EXPECT_EQ(c.item().bottom_right().second, 1);

    c.item().set_available_range(FrameRange(
        FrameRateDuration(1, timebase::k_flicks_24fps),
        FrameRateDuration(10, timebase::k_flicks_24fps)));

    EXPECT_EQ(c.item().top_left().first, 0);
    EXPECT_EQ(c.item().top_left().second, 0);

    EXPECT_EQ(c.item().bottom_right().first, 10);
    EXPECT_EQ(c.item().bottom_right().second, 1);


    Clip c2(c.serialise());
    EXPECT_EQ(c2.item().uuid(), c.uuid());
}
