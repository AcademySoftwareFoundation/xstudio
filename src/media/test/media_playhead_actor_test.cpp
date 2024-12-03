// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::media_reader;
using namespace xstudio::media;
using namespace xstudio::global;
using namespace xstudio::playhead;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(MediaPlayheadActorTest, Test) {
    fixture f;
    // need support actors..
    // auto gsa = f.self->spawn<GlobalActor>();

    // auto media = f.self->spawn<MediaActor>(
    //   f.self->spawn<MediaSourceActor>("test", posix_path_to_uri(TEST_RESOURCE
    //   "/media/test.{:04d}.ppm"), FrameList(1, 10))
    // );

    // caf::actor pa;
    // f.self->request(media, infinite, create_playhead_atom_v).receive(
    //   [&](caf::actor a) {
    //       pa = a;
    //     },
    //   [&](const caf::error&err) {
    //     EXPECT_TRUE(false) << to_string(err);
    //   }
    //   );

    // f.self->request(pa, infinite, buffer_atom_v).receive(
    //   [&](ImageBufPtr buf) {
    //       EXPECT_FALSE(buf);
    //     },
    //   [&](const caf::error&err) {
    //     EXPECT_TRUE(false) << to_string(err);
    //   }
    //   );

    // f.self->request(pa, infinite, move_atom_v, FrameRateDuration(0,24)).receive(
    //   [&](ShowParams buf) {
    //       EXPECT_TRUE(std::get<0>(buf)) << "Failed to create buffer";
    //     },
    //   [&](const caf::error&err) {
    //     EXPECT_TRUE(false) << to_string(err);
    //   }
    //   );

    // f.self->send_exit(pa, caf::exit_reason::user_shutdown);
    // f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}
