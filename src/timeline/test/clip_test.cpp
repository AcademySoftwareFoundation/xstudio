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

    Clip c2(c.serialise());
    EXPECT_EQ(c2.item().uuid(), c.uuid());
}
