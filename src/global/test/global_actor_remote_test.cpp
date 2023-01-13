// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

using namespace xstudio;
using namespace xstudio::global;
using namespace xstudio::utility;
using namespace caf;

TEST(GlobalActorRemoteTest, Test) {
    fixture f;

    auto remote = f.system.middleman().remote_actor("localhost", 45500);
    f.self->send(*remote, version_atom_v);
}
