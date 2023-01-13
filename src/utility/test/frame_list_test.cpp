// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/frame_list.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(FrameGroupTest, Test) {
    EXPECT_EQ(to_string(FrameGroup(1, 1)), "1");
    EXPECT_EQ(to_string(FrameGroup(1, 2)), "1-2");
    EXPECT_EQ(to_string(FrameGroup(1, 2, 1)), "1-2");
    EXPECT_EQ(to_string(FrameGroup(1, 2, 2)), "1-2x2");

    EXPECT_EQ(to_string(FrameGroup("1")), "1");
    EXPECT_EQ(to_string(FrameGroup("1-1")), "1");
    EXPECT_EQ(to_string(FrameGroup("1-2")), "1-2");
    EXPECT_EQ(to_string(FrameGroup("1-2x2")), "1-2x2");

    EXPECT_EQ(FrameGroup("1").start(), 1);
    EXPECT_EQ(FrameGroup("1").end(), 1);
    EXPECT_EQ(FrameGroup("1").count(), unsigned(1));
    EXPECT_EQ(FrameGroup("1").frame(0), 1);
    EXPECT_THROW(static_cast<void>(FrameGroup("1").frame(1)), std::runtime_error);

    EXPECT_EQ(FrameGroup("1-1").start(), 1);
    EXPECT_EQ(FrameGroup("1-1").end(), 1);
    EXPECT_EQ(FrameGroup("1-1").count(), unsigned(1));
    EXPECT_EQ(FrameGroup("1-1").frame(0), 1);
    EXPECT_THROW(static_cast<void>(FrameGroup("1-1").frame(1)), std::runtime_error);

    EXPECT_EQ(FrameGroup("1-2").start(), 1);
    EXPECT_EQ(FrameGroup("1-2").end(), 2);
    EXPECT_EQ(FrameGroup("1-2").count(), unsigned(2));
    EXPECT_EQ(FrameGroup("1-2").count(true), unsigned(2));
    EXPECT_EQ(FrameGroup("1-2").frame(0), 1);
    EXPECT_EQ(FrameGroup("1-2").frame(1), 2);
    EXPECT_THROW(static_cast<void>(FrameGroup("1-2").frame(3)), std::runtime_error);

    EXPECT_EQ(FrameGroup("1-2x2").start(), 1);
    EXPECT_EQ(FrameGroup("1-2x2").end(), 1);
    EXPECT_EQ(FrameGroup("1-2x2").end(true), 2);
    EXPECT_EQ(FrameGroup("1-2x2").count(), unsigned(1));
    EXPECT_EQ(FrameGroup("1-2x2").count(true), unsigned(2));

    EXPECT_EQ(FrameGroup("1-2x2").frame(0), 1);
    EXPECT_THROW(static_cast<void>(FrameGroup("1-2x2").frame(1)), std::runtime_error);
    EXPECT_EQ(FrameGroup("1-2x2").frame(1, true), 2);


    EXPECT_EQ(FrameGroup("1-4x2").frame(0, true, true), 1);
    EXPECT_EQ(FrameGroup("1-4x2").frame(1, true, true), 1);
    EXPECT_EQ(FrameGroup("1-4x2").frame(2, true, true), 3);
    EXPECT_EQ(FrameGroup("1-4x2").frame(3, true, true), 3);

    EXPECT_EQ(FrameGroup("1-5x3").frame(0, true, true), 1);
    EXPECT_EQ(FrameGroup("1-5x3").frame(1, true, true), 1);
    EXPECT_EQ(FrameGroup("1-5x3").frame(2, true, true), 1);
    EXPECT_EQ(FrameGroup("1-5x3").frame(3, true, true), 4);
    EXPECT_EQ(FrameGroup("1-5x3").frame(4, true, true), 4);

    EXPECT_EQ(FrameGroup("1-3x2").start(), 1);
    EXPECT_EQ(FrameGroup("1-3x2").end(), 3);
    EXPECT_EQ(FrameGroup("1-3x2").end(true), 3);
    EXPECT_EQ(FrameGroup("1-3x2").count(), unsigned(2));
    EXPECT_EQ(FrameGroup("1-3x2").count(true), unsigned(3));

    EXPECT_EQ(FrameGroup("2-10x2").count(), unsigned(5));
    EXPECT_EQ(FrameGroup("2-10x2").frame(0), 2);

    EXPECT_EQ(FrameGroup("1-5").frames(), std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(FrameListTest, Test) {
    EXPECT_EQ(to_string(FrameList("1-3x2")), "1-3x2");
    EXPECT_EQ(to_string(FrameList("1,2")), "1,2");
    EXPECT_EQ(to_string(FrameList("1-19,20-30")), "1-19,20-30");

    EXPECT_EQ(FrameList("1001-1300").count(), unsigned(300));

    EXPECT_EQ(FrameList("1-2,3-4").count(), unsigned(4));
    EXPECT_EQ(FrameList("1-2x2,3-4x2").count(), unsigned(2));
    EXPECT_EQ(FrameList("1-2x2,3-4x2").count(true), unsigned(4));

    EXPECT_EQ(FrameList("3-4,1-2").start(), 1);
    EXPECT_EQ(FrameList("3-4,1-2").end(), 4);

    EXPECT_EQ(FrameList("1-2x2,3-4x2").end(), 3);
    EXPECT_EQ(FrameList("1-2x2,3-4x2").end(true), 4);

    EXPECT_EQ(FrameList("1-2x2,3-4x2").frame(0), 1);
    EXPECT_EQ(FrameList("1-2x2,3-4x2").frame(1), 3);
    EXPECT_EQ(FrameList("1-2x2,3-4x2").frame(1, true), 2);

    EXPECT_EQ(FrameList("2-10x2").frame(0), 2);

    EXPECT_EQ(FrameList("1-2x2,5-8").count(true), unsigned(8));
    EXPECT_EQ(FrameList("1-2x2,5-8").frame(3), 7);
    EXPECT_EQ(FrameList("1-2x2,5-8").frame(3, true), 4);

    EXPECT_EQ(FrameList("1-2x2,5-8").frame(0, true, true), 1);
    EXPECT_EQ(FrameList("1-2x2,5-8").frame(1, true, true), 1);
    EXPECT_EQ(FrameList("1-2x2,5-8").frame(2, true, true), 1);
    EXPECT_EQ(FrameList("1-2x2,5-8").frame(3, true, true), 1);

    EXPECT_EQ(FrameList("1-2x2,5-8").frame(4, true, true), 5);

    EXPECT_EQ(FrameList("1-2x2,5-8").frame(5, true, true), 6);

    EXPECT_EQ(FrameList("1-5").frames(), std::vector<int>({1, 2, 3, 4, 5}));
    EXPECT_EQ(FrameList("1-5,6-10x2").frames(), std::vector<int>({1, 2, 3, 4, 5, 6, 8, 10}));
}
