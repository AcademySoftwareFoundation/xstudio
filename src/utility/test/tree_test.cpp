// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/tree.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(TreeTest, Test) {
    // empty
    auto t = Tree<int>();

    // with default data
    t = Tree<int>(1);
    EXPECT_EQ(t.data(), 1);

    // set data
    t.set_data(2);
    EXPECT_EQ(t.data(), 2);

    // check size of "empty" tree
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.size(), 0);

    // insert child
    t.insert(t.begin(), 3);

    // check size
    EXPECT_FALSE(t.empty());
    EXPECT_EQ(t.size(), 1);

    // insert deeper child
    t.begin()->insert(t.begin()->begin(), 4);

    // check size
    EXPECT_FALSE(t.empty());
    EXPECT_EQ(t.size(), 1);

    // check total size
    EXPECT_EQ(t.total_size(), 2);

    // check parent set.
    EXPECT_EQ(t.begin()->begin()->parent(), &(*(t.begin())));
}
