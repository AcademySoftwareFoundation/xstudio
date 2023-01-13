// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"

#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist.hpp"
#include "xstudio/utility/serialise_headers.hpp"

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::plugin_manager;

ACTOR_TEST_SETUP()

TEST(PluginManagerTest, Test) {
    fixture f;

    start_logger(spdlog::level::debug);

    PluginManager pm(std::list<std::string>({{PLUGIN_DIR}}));
    pm.load_plugins();

    utility::Uuid test_uuid1("17e4323c-8ee7-4d9c-b74a-57ba805c10e8");
    utility::Uuid test_uuid2("e4e1d569-2338-4e6e-b127-5a9688df161a");

    for (auto &i : pm.factories()) {
        if (i.first == test_uuid1) {
            EXPECT_EQ(i.second.factory()->uuid(), test_uuid1);
            EXPECT_EQ(i.second.factory()->name(), "hello");
        }
        if (i.first == test_uuid2) {
            EXPECT_EQ(i.second.factory()->uuid(), test_uuid2);
            EXPECT_EQ(i.second.factory()->name(), "template_test");
        }
    }

    auto actor1 = pm.spawn(*(f.self), test_uuid1);
    EXPECT_TRUE(actor1);
    if (actor1) {
        auto name = request_receive<std::string>(*(f.self), actor1, name_atom_v);
        EXPECT_EQ(name, "hello");
    }

    auto actor2 = pm.spawn(*(f.self), test_uuid2);
    EXPECT_TRUE(actor2);
    if (actor2) {
        auto name = request_receive<std::string>(*(f.self), actor2, name_atom_v);
        EXPECT_EQ(name, "hello");
    }
}
