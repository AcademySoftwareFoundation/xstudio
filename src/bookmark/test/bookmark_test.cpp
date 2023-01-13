// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::bookmark;

ACTOR_TEST_MINIMAL()

TEST(BookmarksTest, Test) {
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

TEST(BookmarkTest, Test) {
    Bookmark b1;


    auto jsn = b1.serialise();

    Bookmark b2(jsn);

    EXPECT_EQ(b1.enabled(), b2.enabled());
    EXPECT_EQ(b1.has_focus(), b2.has_focus());
    EXPECT_EQ(b1.start(), b2.start());
    EXPECT_EQ(b1.duration(), b2.duration());
}

TEST(NoteTest, Test) {
    Note n1;

    EXPECT_EQ(n1.note(), "");
    EXPECT_EQ(n1.author(), "Anonymous");

    auto jsn = n1.serialise();

    Note n2(jsn);
    EXPECT_EQ(n1.note(), n2.note());
    EXPECT_EQ(n1.author(), n2.author());
    EXPECT_EQ(to_string(n1.created()), to_string(n2.created()));
}
