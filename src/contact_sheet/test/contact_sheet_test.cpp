// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/contact_sheet/contact_sheet.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::contact_sheet;
using namespace xstudio::media;

TEST(SubsetTest, Test) {
    ContactSheet s;

    Uuid u1(Uuid::generate());
    Uuid u2(Uuid::generate());
    Uuid u3(Uuid::generate());

    s.insert_media(u1);
    EXPECT_FALSE(s.media().empty());
    EXPECT_TRUE(s.remove_media(u1));
    EXPECT_FALSE(s.remove_media(u1));
    EXPECT_TRUE(s.media().empty());

    s.insert_media(u1);
    s.insert_media(u2);

    EXPECT_EQ(*(s.media().begin()), u1);

    EXPECT_TRUE(s.move_media(u2));
    EXPECT_EQ(*(s.media().begin()), u1);

    EXPECT_TRUE(s.move_media(u1));
    EXPECT_EQ(*(s.media().begin()), u2);

    EXPECT_TRUE(s.move_media(u1, u2));
    EXPECT_EQ(*(s.media().begin()), u1);

    EXPECT_FALSE(s.move_media(u3, u2));
    EXPECT_FALSE(s.move_media(u1, u3));

    ContactSheet s2(s.serialise());
    EXPECT_FALSE(s2.empty());
}
