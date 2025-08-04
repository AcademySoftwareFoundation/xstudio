// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"


using namespace xstudio::json_store;
using namespace xstudio::global_store;
using namespace xstudio::utility;

using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(GlobalStoreActorTest, Test) {
    fixture f;

    // TODO: re-do these tests. GlobalStoreDef not used anywhere in the application
    // so it's getting removed.

    /*GlobalStoreDef gsd_hello{"/hello", "goodbye", "string", "Says goodbye"};
    GlobalStoreDef gsd_beast{"/beast", 666, "int", "Number of the beast"};
    GlobalStoreDef gsd_happy{"/happy", true, "bool", "Am I happy"};
    GlobalStoreDef gsd_nested_happy{"/nested/happy", true, "bool", "Am I happy"};
    GlobalStoreDef gsd_nested_sad{"/nested/sad", false, "bool", "Am I sad"};

    std::vector<GlobalStoreDef> test_gsds{
        gsd_hello, gsd_beast, gsd_happy, gsd_nested_happy, gsd_nested_sad};

    auto tmp = f.self->spawn<GlobalStoreActor>("test", test_gsds);
    auto c   = make_function_view(tmp);

    // const Uuid u = c(Uuid, get_uuid_atom_v);

    f.self->request(tmp, std::chrono::seconds(10), uuid_atom_v)
        .receive(
            [&](const Uuid &uuid) {
                EXPECT_FALSE(uuid.is_null()) << "Should return valid uuid";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });

    f.self->request(tmp, caf::infinite, get_json_atom_v, "/hello/value")
        .receive(
            [&](const JsonStore &_json) { EXPECT_EQ(_json, "goodbye") << "Should be goodbye"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return goodbye"; });

    try {
        GlobalStoreHelper gs(f.system);
        EXPECT_EQ(gs.value<std::string>(gsd_hello), "goodbye") << "Should be goodbye";
        gs.set_value("hello", gsd_hello, false);
        EXPECT_EQ(gs.value<std::string>(gsd_hello), "hello") << "Should be hello";
        EXPECT_EQ(gs.default_value<std::string>(gsd_hello), "goodbye") << "Should be goodbye";

        gs.set_value(JsonStore("goodbye"), gsd_hello, false);
        EXPECT_EQ(gs.value<std::string>(gsd_hello), "goodbye") << "Should be goodbye";

        EXPECT_EQ(gs.value<int>(gsd_beast), 666) << "Should be 666";
        EXPECT_EQ(gs.description(gsd_beast), "Number of the beast") << "Number of the beast";

        EXPECT_EQ(gs.value<bool>(gsd_happy), true) << "Should be true";

        EXPECT_EQ(gs.value<bool>(gsd_nested_happy), true) << "Should be true";
        EXPECT_EQ(gs.value<bool>(gsd_nested_sad), false) << "Should be false";

        GlobalStoreDef gsd_runtime_test{"/runtime", "hello", "string", "Am I runtime"};
        gs.set(gsd_runtime_test, false);
        EXPECT_EQ(gs.value<std::string>(gsd_runtime_test), "hello") << "Should be hello";

    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }*/

    f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
}
