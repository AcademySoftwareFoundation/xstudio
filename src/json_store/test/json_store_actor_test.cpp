// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_actor.hpp"


using namespace xstudio::json_store;
using namespace xstudio::utility;

using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


TEST(JsonStoreActorTest, Test) {
    fixture f;
    auto tmp = f.self->spawn<JsonStoreActor>();
    auto c   = make_function_view(tmp);

    // const Uuid u = c(Uuid, get_uuid_atom_v);

    f.self->mail(uuid_atom_v)
        .request(tmp, std::chrono::seconds(10))
        .receive(
            [&](const Uuid &uuid) {
                EXPECT_FALSE(uuid.is_null()) << "Should return valid uuid";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });


    JsonStore json_data(
        nlohmann::json::parse(R"({ "happy": true, "pi": 3.141, "arr": [0, 2, 4] })"));

    c(set_json_atom_v, json_data);

    f.self->mail(get_json_atom_v, "")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &_json) { EXPECT_EQ(_json, json_data) << "Should be equal"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid json"; });

    f.self->mail(get_json_atom_v, "/happy")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &_json) { EXPECT_TRUE(_json) << "Should be true"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

    f.self->mail(get_json_atom_v, "/arr/2")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &_json) { EXPECT_EQ(_json, 4) << "Should be 4"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return 4"; });

    f.self->mail(get_json_atom_v, "/arr/6")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &) { EXPECT_TRUE(false) << "Should be Exception"; },
            [&](const caf::error &) { EXPECT_TRUE(true) << "Should return exception"; });

    c(patch_atom_v,
      JsonStore(
          nlohmann::json::parse(
              R"([{"op": "remove", "path": "/arr"}, {"op": "add", "path": "/hello", "value": "goodbye"}])")));

    f.self->mail(get_json_atom_v, "/arr")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &) { EXPECT_TRUE(false) << "Should be Exception"; },
            [&](const caf::error &) { EXPECT_TRUE(true) << "Should return exception"; });

    f.self->mail(get_json_atom_v, "/hello")
        .request(tmp, caf::infinite)
        .receive(
            [&](const JsonStore &_json) { EXPECT_EQ(_json, "goodbye") << "Should be goodbye"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return goodbye"; });
    f.self->send_exit(tmp, caf::exit_reason::user_shutdown);

    // EXPECT_TRUE(c(get_uuid_atom_v)->is_null()) << "Structure should be null after clear";
}


TEST(JsonStoreActorTest, TestSubscribe) {
    fixture f;
    auto act1  = f.self->spawn<JsonStoreActor>();
    auto act1f = make_function_view(act1);
    auto act2  = f.self->spawn<JsonStoreActor>();
    auto act2f = make_function_view(act2);
    auto act3  = f.self->spawn<JsonStoreActor>();
    auto act3f = make_function_view(act3);

    JsonStore json_data1(
        nlohmann::json::parse(
            R"({ "sub1" : {}, "happy": true, "pi": 3.141, "arr": [0, 2, 4] })"));
    JsonStore json_data2(
        nlohmann::json::parse(
            R"({ "happy": false, "sub2" : {}, "pi": 3.141, "arr": [0, 2, 4] })"));
    JsonStore json_data3(
        nlohmann::json::parse(R"({ "sad": false, "pi": 3.141, "arr": [0, 2, 4] })"));

    act1f(set_json_atom_v, json_data1);
    act2f(set_json_atom_v, json_data2);
    act3f(set_json_atom_v, json_data3);
    auto a = act2f(xstudio::json_store::subscribe_atom_v, "/sub2", act3);
    auto b = act1f(xstudio::json_store::subscribe_atom_v, "/sub1", act2);

    // need small delay to allow sync to happen..
    std::this_thread::sleep_for(100ms);

    f.self->mail(get_json_atom_v, "/sub1/happy")
        .request(act1, caf::infinite)
        .receive(
            [&](const JsonStore &_json) {
                EXPECT_FALSE(_json) << "Should be equal to actor 2";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return json"; });

    act2f(set_json_atom_v, JsonStore(nlohmann::json::parse("true")), "/happy");
    act3f(set_json_atom_v, JsonStore(nlohmann::json::parse("true")), "/sad");
    // need small delay to allow sync to happen..
    std::this_thread::sleep_for(100ms);

    f.self->mail(get_json_atom_v, "/sub1/happy")
        .request(act1, caf::infinite)
        .receive(
            [&](const JsonStore &_json) { EXPECT_TRUE(_json) << "Should be equal to actor 2"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return json"; });

    f.self->mail(get_json_atom_v, "/sub1/sub2/sad")
        .request(act1, caf::infinite)
        .receive(
            [&](const JsonStore &_json) {
                EXPECT_TRUE(_json) << "Should be equal to actor true";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return json"; });

    act1f(xstudio::json_store::unsubscribe_atom_v, act2);
    act2f(xstudio::json_store::unsubscribe_atom_v, act3);
    f.self->send_exit(act1, caf::exit_reason::user_shutdown);
    f.self->send_exit(act2, caf::exit_reason::user_shutdown);
    f.self->send_exit(act3, caf::exit_reason::user_shutdown);
}

#pragma GCC diagnostic pop
