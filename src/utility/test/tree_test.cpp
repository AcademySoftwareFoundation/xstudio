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
    EXPECT_EQ(t.parent(), nullptr);
    EXPECT_EQ(t.front().parent(), &t);
    EXPECT_EQ(t.begin()->begin()->parent(), &(*(t.begin())));
}

TEST(TreeTest, Test2) {
    auto j = R"({"hello":"world"})"_json;
    auto t = json_to_tree(j, "children");

    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.data().at("hello"), "world");
    EXPECT_EQ(tree_to_json(t, "children").dump(), j.dump());


    j = R"({"hello":"world", "children": null})"_json;
    t = json_to_tree(j, "children");
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.data().at("hello"), "world");
    EXPECT_EQ(tree_to_json(t, "children").dump(), j.dump());

    j = R"({"hello":"world", "children": [{"goodbye": "cruel world"}]})"_json;
    auto j2 =
        R"({"hello":"world", "children": [{"bunny":"ears","goodbye": "cruel world"}]})"_json;
    t = json_to_tree(j, "children");
    EXPECT_FALSE(t.empty());
    EXPECT_EQ(t.data().at("hello"), "world");
    EXPECT_EQ(t.begin()->data().at("goodbye"), "cruel world");
    EXPECT_EQ(tree_to_json(t, "children").dump(), j.dump());

    t.begin()->data()["bunny"] = "ears";
    EXPECT_EQ(tree_to_json(t, "children").dump(), j2.dump());

    auto tt = t;

    EXPECT_EQ(tree_to_json(tt, "children").dump(), j2.dump());


    EXPECT_EQ(tree_to_pointer(tt, "children").to_string(), "");
    EXPECT_EQ(tree_to_pointer(tt.front(), "children").to_string(), "/children/0");

    EXPECT_EQ(pointer_to_tree(tt, "children", tree_to_pointer(tt, "children")), &tt);
    EXPECT_EQ(
        pointer_to_tree(tt, "children", tree_to_pointer(tt.front(), "children")),
        &(tt.front()));
}
