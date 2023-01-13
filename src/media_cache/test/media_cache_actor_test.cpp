// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/media_reader/media_reader.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_cache;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(MediaCacheActorTest, Test) {
    // fixture f;

    // auto tmp = f.self->spawn<GlobalImageCacheActor>();
    // auto c   = make_function_view(tmp);


    // f.self->request(tmp, infinite, retrieve_atom_v, "wibble")
    //     .receive(
    //         [&](media_reader::ImageBufPtr buf) { EXPECT_FALSE(buf); },
    //         [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    // f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
}
