// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/media/media.hpp"

using namespace xstudio;
using namespace xstudio::module;
using namespace xstudio::global;
using namespace xstudio::utility;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(ModuleTest, Test) {

    fixture f;

    start_logger(spdlog::level::debug);

    auto gsa = f.self->spawn<GlobalActor>();

    try {

    } catch (std::exception &e) {

        EXPECT_TRUE(false) << " " << e.what() << "\n";
    }

    f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}
