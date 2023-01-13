// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/container.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;
using namespace nlohmann;

TEST(UuidTreeTest, Test) {
    UuidTree<int> u(0);

    EXPECT_FALSE(u.uuid().is_null());
    EXPECT_EQ(u.value(), 0);
    EXPECT_EQ(u.size(), size_t(0));
    EXPECT_TRUE(u.empty());
    EXPECT_FALSE(u.count(Uuid()));
    EXPECT_TRUE(u.count(u.uuid()));

    auto new_uid = u.insert(2);
    EXPECT_EQ(u.size(), size_t(1));
    EXPECT_FALSE(u.empty());
    EXPECT_TRUE(u.count(*new_uid));

    UuidTree<int> uu = *u.copy(*new_uid);
    EXPECT_EQ(uu.uuid(), *new_uid);


    EXPECT_TRUE(u.remove(*new_uid));
    EXPECT_EQ(u.size(), size_t(0));
    EXPECT_TRUE(u.empty());
    EXPECT_FALSE(u.count(*new_uid));

    // UuidTree<int> uu(u.serialise());
    // EXPECT_EQ(u.value(), uu.value());

    EXPECT_TRUE(u.find(u.uuid()));

    std::cout << u << std::endl;
}

TEST(ContainerTest, Test) {
    Container c{"test", "unknown"};

    EXPECT_EQ(c.name(), "test");

    c.set_name("test2");
    EXPECT_EQ(c.name(), "test2");

    EXPECT_FALSE(c.uuid().is_null());

    Container c2{c.serialise()};
    EXPECT_EQ(c2.name(), "test2");
    EXPECT_EQ(c.uuid(), c.uuid());
}

TEST(PlaylistTreeTest, Test) {
    Uuid u1 = Uuid::generate();
    PlaylistTree r("root", "", u1);

    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.name(), "root");

    auto u2 = r.insert(PlaylistItem("test1"));
    EXPECT_TRUE(u2);

    EXPECT_FALSE(r.insert(PlaylistItem("test1"), Uuid::generate()));

    EXPECT_FALSE(r.empty());
    EXPECT_TRUE(r.count(r.uuid()));
    EXPECT_TRUE(r.count(*u2));

    EXPECT_TRUE(r.insert(PlaylistItem("test2"), *u2));
    EXPECT_TRUE(r.rename("hello", *u2));
    EXPECT_EQ((*r.find(*u2))->value().name(), "hello");


    PlaylistTree r2(r.serialise());
    EXPECT_EQ(r.name(), r2.name());

    static_cast<void>(r2.children_uuid(true));
    static_cast<void>(r.children_uuid(true));
    r2.reset_uuid(true);

    PlaylistTree r4(**r.find(*u2));

    // EXPECT_TRUE(r.insert(PlaylistTree("test3", "", tu3), tu2, true));
    // EXPECT_TRUE(r.insert(PlaylistTree("test4", "", tu4), tu2, true));

    // EXPECT_TRUE(r.move(tu4));
    // EXPECT_TRUE(r.move(tu4, tu2));

    // EXPECT_TRUE(r.move(tu4, tu2, true));
    // EXPECT_FALSE(r.move(tu2, tu4, true));

    // EXPECT_TRUE(r.remove(tu4, true));
    // EXPECT_TRUE(r.remove(tu1));
    // EXPECT_TRUE(r.remove(tu3, true));

    // EXPECT_FALSE(r.rename("test",tu3));
    // EXPECT_TRUE(r.rename("test",tu2));

    // PlaylistTree r2{r.serialise()};
    // EXPECT_FALSE(r2.empty());
}

TEST(PlaylistTreeTest2, Test) {
    Uuid u1 = Uuid::generate();
    PlaylistTree r("root", "", u1);

    auto u2 = r.insert(PlaylistItem("test1"));
    EXPECT_TRUE(u2);

    auto u3 = r.insert(PlaylistItem("test2"));
    EXPECT_TRUE(u3);

    EXPECT_TRUE(r.count(r.uuid()));
    EXPECT_TRUE(r.count(*u2));
    EXPECT_TRUE(r.count(*u3));

    auto t2 = *(r.intersect({*u2}));

    EXPECT_TRUE(t2.count(r.uuid()));
    EXPECT_TRUE(t2.count(*u2));
    EXPECT_FALSE(t2.count(*u3));

    // EXPECT_TRUE(r.insert(PlaylistTree("test3", "", tu3), tu2, true));
    // EXPECT_TRUE(r.insert(PlaylistTree("test4", "", tu4), tu2, true));

    // EXPECT_TRUE(r.move(tu4));
    // EXPECT_TRUE(r.move(tu4, tu2));

    // EXPECT_TRUE(r.move(tu4, tu2, true));
    // EXPECT_FALSE(r.move(tu2, tu4, true));

    // EXPECT_TRUE(r.remove(tu4, true));
    // EXPECT_TRUE(r.remove(tu1));
    // EXPECT_TRUE(r.remove(tu3, true));

    // EXPECT_FALSE(r.rename("test",tu3));
    // EXPECT_TRUE(r.rename("test",tu2));

    // PlaylistTree r2{r.serialise()};
    // EXPECT_FALSE(r2.empty());
}
