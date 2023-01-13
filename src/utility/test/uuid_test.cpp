// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;

TEST(UUIDTest, Test) {
    Uuid u;
    EXPECT_TRUE(u.is_null()) << "Structure should be null on init";
    u.generate_in_place();
    EXPECT_FALSE(u.is_null()) << "Structure should not be be null after generate";
    u.clear();
    EXPECT_TRUE(u.is_null()) << "Structure should be null after clear";

    u.generate_in_place();
    Uuid uu(u);
    EXPECT_EQ(u, uu) << "Copy should be equal";

    EXPECT_EQ(u, Uuid(u.uuid_)) << "Copy should be equal";

    EXPECT_EQ(to_string(u), to_string(uu)) << "Strings should be equal";

    EXPECT_EQ(u, Uuid(to_string(uu))) << "Creation from string should be equal";

    u.from_string(to_string(uu));
    EXPECT_EQ(u, uu) << "Inplace creation from string should be equal";

    EXPECT_NE(Uuid::generate(), Uuid::generate())
        << "Inplace creation from on init should not be equal";
}


TEST(UUIDListContainerTest, Test) {
    Uuid u1(Uuid::generate());
    Uuid u2(Uuid::generate());
    Uuid u3(Uuid::generate());

    UuidListContainer c1;

    EXPECT_TRUE(c1.empty());
    EXPECT_EQ(c1.size(), unsigned(0));

    std::vector<Uuid> v{u1};
    UuidListContainer c2(v);

    EXPECT_FALSE(c2.contains(u2));
    EXPECT_TRUE(c2.contains(u1));

    EXPECT_EQ(c2.count(u1), unsigned(1));
    EXPECT_EQ(c2.count(u2), unsigned(0));

    EXPECT_FALSE(c2.empty());
    EXPECT_EQ(c2.size(), unsigned(1));

    c1.insert(u1);
    EXPECT_FALSE(c1.empty());
    EXPECT_TRUE(c1.remove(u1));
    EXPECT_FALSE(c1.remove(u1));
    EXPECT_TRUE(c1.empty());

    c1.insert(u1);
    c1.insert(u2);

    EXPECT_EQ(*(c1.uuids().begin()), u1);

    EXPECT_TRUE(c1.move(u2));
    EXPECT_EQ(*(c1.uuids().begin()), u1);

    EXPECT_TRUE(c1.move(u1));
    EXPECT_EQ(*(c1.uuids().begin()), u2);

    EXPECT_TRUE(c1.move(u1, u2));
    EXPECT_EQ(*(c1.uuids().begin()), u1);

    EXPECT_FALSE(c1.move(u3, u2));
    EXPECT_FALSE(c1.move(u1, u3));

    UuidListContainer c3(c1.serialise());
    EXPECT_FALSE(c3.empty());
}
