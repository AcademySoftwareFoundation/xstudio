// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::playlist;
using namespace xstudio::media;

ACTOR_TEST_MINIMAL()

TEST(PlaylistTest, Test) {
    Playlist pl{"test"};

    EXPECT_EQ(pl.name(), "test");

    EXPECT_TRUE(pl.empty());

    Uuid u1(Uuid::generate());
    Uuid u2(Uuid::generate());
    Uuid u3(Uuid::generate());

    pl.insert_media(u1);

    EXPECT_FALSE(pl.empty());
    EXPECT_FALSE(pl.media().empty());
    EXPECT_TRUE(pl.remove_media(u1));
    EXPECT_FALSE(pl.remove_media(u1));
    EXPECT_TRUE(pl.media().empty());

    pl.insert_media(u1);
    pl.insert_media(u2);

    EXPECT_EQ(*(pl.media().begin()), u1);

    EXPECT_TRUE(pl.move_media(u2));
    EXPECT_EQ(*(pl.media().begin()), u1);

    EXPECT_TRUE(pl.move_media(u1));
    EXPECT_EQ(*(pl.media().begin()), u2);

    EXPECT_TRUE(pl.move_media(u1, u2));
    EXPECT_EQ(*(pl.media().begin()), u1);

    EXPECT_FALSE(pl.move_media(u3, u2));
    EXPECT_FALSE(pl.move_media(u1, u3));

    Playlist pl2{pl.serialise()};
    EXPECT_EQ(pl2.name(), "test");
    EXPECT_FALSE(pl2.empty());
}

TEST(PlaylistContainerTest, Test) {
    Playlist pl{"test"};

    EXPECT_TRUE(pl.empty());
    pl.insert_group("test group");

    EXPECT_FALSE(pl.empty());

    Playlist pl2{pl.serialise()};
    EXPECT_EQ(pl2.name(), "test");
    EXPECT_FALSE(pl2.empty());
}
