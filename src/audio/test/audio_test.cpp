// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media;
using namespace xstudio::global_store;
using namespace xstudio::global;
using namespace xstudio::media_cache;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace xstudio::playlist;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(AudioActorTest, Test) {

    FrameRate fr24(timebase::k_flicks_24fps);
    FrameRate fr30(timebase::k_flicks_one_thirtieth_second);
    FrameRate fr60(timebase::k_flicks_one_sixtieth_second);

    fixture f;
    auto gsa = f.self->spawn<GlobalActor>();

    auto playlist = f.self->spawn<PlaylistActor>("Test");
    auto srcuuid  = Uuid::generate();

    f.self->anon_send(
        playlist,
        add_media_atom_v,
        f.self->spawn<MediaActor>(
            "Media3",
            Uuid(),
            UuidActorVector({UuidActor(
                srcuuid,
                f.self->spawn<MediaSourceActor>(
                    "MediaSource3",
                    posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
                    utility::FrameRate(timebase::k_flicks_24fps),
                    srcuuid))})),
        Uuid());

    using namespace std::chrono_literals;

    // create the playhead
    try {

        caf::actor playhead;

        playhead = request_receive_wait<UuidActor>(
                       *(f.self),
                       playlist,
                       std::chrono::milliseconds(1000),
                       playlist::create_playhead_atom_v)
                       .actor();

        f.self->send(playhead, playhead::play_atom_v, true);

        std::this_thread::sleep_for(3000ms);

        f.self->send(playhead, playhead::play_atom_v, false);
        f.self->send_exit(playhead, caf::exit_reason::user_shutdown);

    } catch (std::exception &e) {

        EXPECT_TRUE(false) << " " << e.what() << "\n";
    }

    f.self->send_exit(playlist, caf::exit_reason::user_shutdown);
    f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}
