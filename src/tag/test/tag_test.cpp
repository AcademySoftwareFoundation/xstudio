// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/tag/tag.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::tag;

ACTOR_TEST_MINIMAL()

TEST(TagTest, Test) {
    // Bookmarks b1;

    // EXPECT_TRUE(b1.empty());
    // EXPECT_EQ(b1.size(), 0);
    // auto u1 = b1.add_bookmark();
    // EXPECT_FALSE(b1.empty());
    // EXPECT_EQ(b1.size(), 1);

    //    auto jsn = b1.serialise();

    //    Bookmarks b2(jsn);
    // EXPECT_FALSE(b2.empty());
    // EXPECT_EQ(b2.size(), 1);

    //    EXPECT_EQ(u1, b2.get_bookmark(u1)->uuid());
    //    auto bm = b2.erase_bookmark(u1);
    // EXPECT_TRUE(b2.empty());
    // EXPECT_EQ(b2.size(), 0);
}
