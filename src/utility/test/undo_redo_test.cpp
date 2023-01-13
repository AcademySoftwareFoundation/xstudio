// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/undo_redo.hpp"

using namespace xstudio::utility;

TEST(UndoRedoTest, Test) {

    auto h = UndoRedo<int>();

    EXPECT_EQ(h.count(), 0);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.undo());
    EXPECT_FALSE(h.redo());

    h.push(1);

    EXPECT_EQ(h.count(), 1);
    EXPECT_FALSE(h.empty());
    h.clear();
    EXPECT_EQ(h.count(), 0);
    EXPECT_TRUE(h.empty());

    EXPECT_TRUE(h.empty());
    EXPECT_TRUE(h.empty());

    EXPECT_FALSE(h.undo());
    EXPECT_FALSE(h.redo());

    h.push(1);

    EXPECT_EQ(*(h.undo()), 1);
    EXPECT_FALSE(h.undo());

    EXPECT_EQ(*(h.redo()), 1);
    EXPECT_FALSE(h.redo());

    EXPECT_EQ(*(h.undo()), 1);

    h.clear();
    h.push(1);
    h.push(2);
    h.push(3);
    h.push(4);

    EXPECT_EQ(h.count(), 4);
    h.set_max_count(3);
    EXPECT_EQ(h.count(), 3);

    EXPECT_EQ(*(h.undo()), 4);
    EXPECT_EQ(*(h.undo()), 3);
    EXPECT_EQ(*(h.undo()), 2);
    EXPECT_FALSE(h.undo());

    EXPECT_EQ(*(h.redo()), 2);
    EXPECT_EQ(*(h.redo()), 3);
    EXPECT_EQ(*(h.redo()), 4);

    EXPECT_FALSE(h.redo());

    h.clear();
    h.push(1);
    h.push(2);
    h.push(3);
    h.push(4);
    EXPECT_TRUE(h.undo());
    EXPECT_TRUE(h.undo());
    EXPECT_TRUE(h.redo());
    h.push(5);
    EXPECT_FALSE(h.redo());
}

TEST(UndoRedoMapTest, Test) {

    auto h = UndoRedoMap<int, int>();

    EXPECT_EQ(h.count(), 0);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.undo());
    EXPECT_FALSE(h.redo());

    h.push(1, 1);

    EXPECT_EQ(h.count(), 1);
    EXPECT_FALSE(h.empty());
    h.clear();
    EXPECT_EQ(h.count(), 0);
    EXPECT_TRUE(h.empty());

    EXPECT_TRUE(h.empty());
    EXPECT_TRUE(h.empty());

    EXPECT_FALSE(h.undo());
    EXPECT_FALSE(h.redo());

    h.push(2, 1);

    EXPECT_EQ(*(h.undo()), 1);
    EXPECT_FALSE(h.undo());

    EXPECT_EQ(*(h.redo()), 1);
    EXPECT_FALSE(h.redo());

    EXPECT_EQ(*(h.undo()), 1);

    h.clear();
    h.push(1, 1);
    h.push(2, 2);
    h.push(3, 3);
    h.push(4, 4);

    EXPECT_EQ(h.count(), 4);
    h.set_max_count(3);
    EXPECT_EQ(h.count(), 3);

    EXPECT_EQ(*(h.undo()), 4);
    EXPECT_EQ(*(h.undo()), 3);
    EXPECT_EQ(*(h.undo()), 2);
    EXPECT_FALSE(h.undo());

    EXPECT_EQ(*(h.redo()), 2);
    EXPECT_EQ(*(h.redo()), 3);
    EXPECT_EQ(*(h.redo()), 4);

    EXPECT_FALSE(h.redo());

    h.clear();
    h.push(1, 1);
    h.push(2, 2);
    h.push(3, 3);
    h.push(4, 4);
    EXPECT_TRUE(h.undo());
    EXPECT_TRUE(h.undo());
    EXPECT_TRUE(h.redo());
    h.push(5, 5);
    EXPECT_FALSE(h.redo());

    h.clear();
    h.push(1, 1);
    h.push(2, 2);
    h.push(3, 3);
    h.push(3, 4);
    h.push(4, 5);

    EXPECT_FALSE(h.undo(5));
    EXPECT_FALSE(h.undo(4));

    EXPECT_EQ(*(h.undo(2)), 5);
    EXPECT_EQ(*(h.undo(2)), 4);
    EXPECT_EQ(*(h.undo(2)), 3);

    EXPECT_FALSE(h.undo(2));

    EXPECT_EQ(*(h.redo(2)), 3);
    EXPECT_EQ(*(h.redo(2)), 4);
    EXPECT_EQ(*(h.redo(2)), 5);
    EXPECT_FALSE(h.redo(2));
}
