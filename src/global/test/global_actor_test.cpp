// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

#include "xstudio/media/media.hpp"
#include "xstudio/ui/viewport/viewport.hpp"

using namespace xstudio;
using namespace xstudio::global;
using namespace xstudio::utility;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(GlobalActorTest, Test) {
    fixture f;
    // auto tmp = f.self->spawn<PlayerActor>("Test");

    // f.self->request(tmp, std::chrono::seconds(10), name_atom_v).receive(
    // 	[&](const std::string &name) {
    // 		EXPECT_EQ(name, "Test");
    // 	},
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    // );

    // f.self->request(tmp, std::chrono::seconds(10),
    // 		playlist::add_media_atom_v,
    // 		"test", posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"), 1,
    // 10).receive(
    // 	[&](caf::actor act) {
    // 		EXPECT_TRUE(act);
    // 	},
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    // );

    //    JsonStore serial;
    //    f.self->request(tmp, infinite, serialise_atom_v).receive(
    //      [&](const JsonStore &jsn) {
    //        serial = jsn;
    //      },
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    //    );

    // f.self->send_exit(tmp, caf::exit_reason::user_shutdown);

    // auto tmp2 = f.self->spawn<PlayerActor>(serial);

    // f.self->request(tmp2, std::chrono::seconds(10), name_atom_v).receive(
    // 	[&](const std::string &name) {
    // 		EXPECT_EQ(name, "Test");
    // 	},
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    // );

    // f.self->send_exit(tmp2, caf::exit_reason::user_shutdown);
}
