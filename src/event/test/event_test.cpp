// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/event/event.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::utility;
using namespace xstudio::event;


TEST(EventTest, Test) {
    auto e = Event("Test");

    EXPECT_EQ(e.progress_text(), "Test");
    EXPECT_EQ(e.progress_text_percentage(), "Test");
    EXPECT_EQ(e.progress_text_range(), "Test");

    e = Event("Test {}");

    EXPECT_EQ(e.progress_text(), "Test {}");
    EXPECT_EQ(e.progress_text_percentage(), "Test   0.0%");
    EXPECT_EQ(e.progress_text_range(), "Test 0/100");

    EXPECT_FALSE(e.contains(Uuid()));

    e.set_progress_maximum(100);
    EXPECT_EQ(e.progress_text_percentage(), "Test   0.0%");
    e.set_progress(1);
    EXPECT_EQ(e.progress_text_percentage(), "Test   1.0%");
    e.set_progress(100);
    EXPECT_EQ(e.progress_text_percentage(), "Test 100.0%");
    EXPECT_TRUE(e.complete());

    e.set_progress_minimum(0);
    e.set_progress_maximum(20);
    e.set_complete();
    EXPECT_EQ(e.progress_percentage(), 100.0);
}
