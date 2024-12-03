// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/json_store_sync.hpp"
#include "xstudio/utility/logging.hpp"

#include <gtest/gtest.h>

using namespace xstudio::utility;
using namespace nlohmann;

TEST(JsonStoreSyncTest, Test) {

    JsonStoreSync a(R"({"children":[]})"_json);
    JsonStoreSync b(R"({"children":[]})"_json);
    JsonStoreSync c(R"({"children":[]})"_json);

    a.bind_send_event_func(
        [&](auto &&PH1, auto &&PH2) { b.process_event(std::forward<decltype(PH1)>(PH1)); });

    b.bind_send_event_func(
        [&](auto &&PH1, auto &&PH2) { a.process_event(std::forward<decltype(PH1)>(PH1)); });

    a.insert_rows(0, 1, R"([{"hello":true}])"_json);
    EXPECT_EQ(a.dump(), b.dump());

    b.insert_rows(1, 1, R"([{"goodbye":true}])"_json);
    EXPECT_EQ(a.dump(), b.dump());

    a.set(0, "hello", R"(false)"_json);
    EXPECT_EQ(a.dump(), b.dump());

    b.set(1, "goodbye", R"(false)"_json);
    EXPECT_EQ(a.dump(), b.dump());

    a.remove_rows(1, 1);
    EXPECT_EQ(a.dump(), b.dump());

    b.remove_rows(0, 1);
    EXPECT_EQ(a.dump(), b.dump());


    a.insert("test1", R"({"hellogoodbye":true})"_json);
    EXPECT_EQ(a.dump(), b.dump());

    b.insert("test2", R"({"goodbyehello":true})"_json);
    EXPECT_EQ(a.dump(), b.dump());

    a.remove("test1");
    EXPECT_EQ(a.dump(), b.dump());

    b.remove("test2");
    EXPECT_EQ(a.dump(), b.dump());


    EXPECT_EQ(a.dump(), c.dump());
}

TEST(JsonStoreSyncTestMove, Test) {
    JsonStoreSync a(R"({"children":[]})"_json);
    JsonStoreSync b(R"({"children":[]})"_json);
    a.bind_send_event_func(
        [&](auto &&PH1, auto &&PH2) { b.process_event(std::forward<decltype(PH1)>(PH1)); });

    b.bind_send_event_func(
        [&](auto &&PH1, auto &&PH2) { a.process_event(std::forward<decltype(PH1)>(PH1)); });

    a.insert_rows(0, 1, R"([{"hello":true, "children":[1,2,3]}])"_json);
    EXPECT_EQ(a.dump(), b.dump());

    b.insert_rows(1, 1, R"([{"goodbye":true, "children":[4,5,6]}])"_json);
    EXPECT_EQ(a.dump(), b.dump());

    a.move_rows("/children/0", 0, 1, "/children/0", 1);
    EXPECT_EQ(a.dump(), b.dump());

    b.move_rows("/children/0", 0, 1, "/children/1", 1);
    EXPECT_EQ(a.dump(), b.dump());
}

TEST(JsonStoreSyncTestUndo, Test) {
    JsonStoreSync a(R"({"children":[]})"_json);
    JsonStoreSync b(R"({"children":[]})"_json);

    // insert data at row
    a.insert_rows(0, 1, R"([{"hello":true}])"_json);
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    // set existing key
    a.insert_rows(0, 1, R"([{"hello":true}])"_json);
    b.process_event(a.last_event());

    a.insert("hello", R"([{"hello":true}])"_json, "/children/0");
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    // set new key
    a.insert("hellome", R"([{"hello":true}])"_json, "/children/0");
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    // remove key
    a.remove("hello", "/children/0");
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    // set row keys
    a.set(0, R"({"hello":false})"_json, "");
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    // remove rows
    a.remove_rows(0, 1, "");
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.reset_data(
        R"({"children":[{"hello":true, "children":[1,2,3]}, {"goodbye":true, "children":[4,5,6]}]})"_json);
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());


    // move rows
    a.reset_data(R"({"children":[
		{"hello":true, "children":[1,2,3]},
		{"goodbye":true, "children":[4,5,6]}
	]})"_json);
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.move_rows("/children/0", 0, 1, "/children/0", 1);
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.move_rows("/children/0", 0, 1, "/children/1", 1);
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());

    a.unapply_event(a.last_event());
    b.process_event(a.last_event());
    EXPECT_EQ(a.dump(), b.dump());
}

TEST(JsonStoreSyncTestFind, Test) {
    JsonStoreSync a(R"(
        {
            "children":[
                {"a": 1},
                {"b": 2},
                {
                    "c": 3,
                    "children":[
                        {"a": 1},
                        {"b": 2},
                        {"c": 3}
                    ]
                }
            ]
        }
    )"_json);

    // start_logger(spdlog::level::debug);

    auto found = a.find("a");
    EXPECT_EQ(found.size(), 2);
    EXPECT_EQ(found.at(0), "/children/0");
    EXPECT_EQ(found.at(1), "/children/2/children/0");

    // test find count limit
    found = a.find("a", {}, 1);
    EXPECT_EQ(found.size(), 1);
    EXPECT_EQ(found.at(0), "/children/0");

    // test value
    found = a.find("a", 1, 1);
    EXPECT_EQ(found.size(), 1);
    EXPECT_EQ(found.at(0), "/children/0");

    // test value not match
    found = a.find("a", 2, 1);
    EXPECT_EQ(found.size(), 0);

    // test key not match
    found = a.find("d");
    EXPECT_EQ(found.size(), 0);

    // test depth prune
    found = a.find("a", {}, -1, 1);
    EXPECT_EQ(found.size(), 1);

    // test depth prune
    found = a.find("a", {}, -1, 2);
    EXPECT_EQ(found.size(), 2);
}