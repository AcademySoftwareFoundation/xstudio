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
#include "xstudio/utility/logging.hpp"

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

TEST(PlayheadActorTest, Test) {

    start_logger(spdlog::level::debug);

    FrameRate fr24(timebase::k_flicks_24fps);
    FrameRate fr30(timebase::k_flicks_one_thirtieth_second);
    FrameRate fr60(timebase::k_flicks_one_sixtieth_second);

    fixture f;
    auto gsa = f.self->spawn<GlobalActor>();

    auto playlist = f.self->spawn<PlaylistActor>("Test");

    auto s1u     = Uuid::generate();
    auto media_1 = f.self->spawn<MediaActor>(
        "Media1",
        Uuid(),
        UuidActorVector({UuidActor(
            s1u,
            f.self->spawn<MediaSourceActor>(
                "MediaSource1",
                posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
                FrameList(1, 10),
                fr24,
                s1u))}));

    auto s2u     = Uuid::generate();
    auto media_2 = f.self->spawn<MediaActor>(
        "Media2",
        Uuid(),
        UuidActorVector({UuidActor(
            s2u,
            f.self->spawn<MediaSourceActor>(
                "MediaSource2",
                posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
                FrameList(1, 10),
                fr60,
                s2u))}));

    auto s3u     = Uuid::generate();
    auto media_3 = f.self->spawn<MediaActor>(
        "Media3",
        Uuid(),
        UuidActorVector({UuidActor(
            s2u,
            f.self->spawn<MediaSourceActor>(
                "MediaSource3",
                posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
                utility::FrameRate(timebase::k_flicks_24fps),
                s3u))}) // 100 frames
    );

    auto media1_uuid = request_receive_wait<Uuid>(
        *(f.self), media_1, std::chrono::milliseconds(1000), uuid_atom_v);
    auto media2_uuid = request_receive_wait<Uuid>(
        *(f.self), media_2, std::chrono::milliseconds(1000), uuid_atom_v);
    auto media3_uuid = request_receive_wait<Uuid>(
        *(f.self), media_3, std::chrono::milliseconds(1000), uuid_atom_v);

    f.self->send(playlist, add_media_atom_v, media_1, Uuid());
    f.self->send(playlist, add_media_atom_v, media_2, Uuid());
    f.self->send(playlist, add_media_atom_v, media_3, Uuid());

    using namespace std::chrono_literals;

    utility::UuidVector uuids = {media1_uuid, media2_uuid, media3_uuid};

    std::this_thread::sleep_for(100ms);

    // create the playhead
    caf::actor playhead = request_receive_wait<UuidActor>(
                              *(f.self),
                              playlist,
                              std::chrono::milliseconds(1000),
                              playlist::create_playhead_atom_v)
                              .actor();

    caf::actor playhead_selection =
        request_receive_wait<UuidActor>(
            *(f.self), playhead, std::chrono::milliseconds(1000), playhead::source_atom_v)
            .actor();

    try {

        request_receive_wait<bool>(
            *(f.self),
            playhead_selection,
            std::chrono::milliseconds(1000),
            playlist::select_media_atom_v,
            uuids);

        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::duration_frames_atom_v),
            120);

        // set the compare mode to AB, duration should be length of longest source
        /*request_receive_wait<bool>(
            *(f.self),
            playhead,
            std::chrono::milliseconds(1000),
            playhead::compare_mode_atom_v,
            playhead::CM_AB);

        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::duration_frames_atom_v),
            100);

        f.self->send(playhead, playhead::compare_mode_atom_v, playhead::CM_STRING);*/

        // playhead should start on frame zero

        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            0);
        std::this_thread::sleep_for(100ms);

        // step 10 frames forwards
        for (int i = 0; i < 10; ++i) {
            f.self->send(playhead, playhead::step_atom_v, 1);
        }

        std::this_thread::sleep_for(100ms);


        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            10);

        // should be set to play forward
        EXPECT_EQ(
            request_receive_wait<bool>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::play_forward_atom_v),
            true);

        // set reverse
        f.self->send(playhead, playhead::play_forward_atom_v, false);

        EXPECT_EQ(
            request_receive_wait<bool>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::play_forward_atom_v),
            false);

        // step 5 frames backweards
        for (int i = 0; i < 5; ++i)
            f.self->send(playhead, playhead::step_atom_v, -1);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            5);

        // play for 1 second
        f.self->send(playhead, playhead::play_forward_atom_v, true);
        f.self->send(playhead, playhead::play_atom_v, true);
        std::this_thread::sleep_for(1s);
        f.self->send(playhead, playhead::play_atom_v, false);

        // started on frame 5, play 5 more frames at 24fps, then 10 frames at 60fps, then
        // remainder of the 1 second at 24fps
        int expected_frame = 5 + 5 + 10 + int(round((1.0 - 5 / 24.0 - 10 / 60.0) * 24.0f));

        // allow +/- 1 frame
        EXPECT_NEAR(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            expected_frame,
            1);

        // play backwards for 1 second
        f.self->send(playhead, playhead::play_forward_atom_v, false);
        f.self->send(playhead, playhead::play_atom_v, true);
        std::this_thread::sleep_for(1s);
        f.self->send(playhead, playhead::play_atom_v, false);
        std::this_thread::sleep_for(100ms); // wait for last frame to flush

        EXPECT_NEAR(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            5,
            1);

        // loop range defaults to off
        EXPECT_EQ(
            request_receive_wait<bool>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::use_loop_range_atom_v),
            false);
        f.self->send(playhead, playhead::use_loop_range_atom_v, true);

        EXPECT_EQ(
            request_receive_wait<bool>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::use_loop_range_atom_v),
            true);

        f.self->send(playhead, playhead::simple_loop_start_atom_v, 50);
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::simple_loop_start_atom_v),
            50);

        // setting simple loop start should force position to jump if it's before the start
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            50);

        f.self->send(playhead, playhead::simple_loop_end_atom_v, 80);
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::simple_loop_end_atom_v),
            80);

        f.self->send(playhead, playhead::simple_loop_end_atom_v, 40);
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::simple_loop_end_atom_v),
            40);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            40);

        // set 24 frame range
        f.self->send(playhead, playhead::play_forward_atom_v, true);
        f.self->send(playhead, playhead::simple_loop_end_atom_v, 74);
        f.self->send(playhead, playhead::simple_loop_start_atom_v, 50);
        f.self->send(playhead, playhead::jump_atom_v, (int)62);
        std::this_thread::sleep_for(10ms);

        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::simple_loop_start_atom_v),
            50);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::simple_loop_end_atom_v),
            74);
        EXPECT_EQ(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            62);

        // play for 1 second, should lopp back to same position
        f.self->send(playhead, playhead::play_atom_v, true);
        std::this_thread::sleep_for(1s);
        f.self->send(playhead, playhead::play_atom_v, false);
        EXPECT_NEAR(
            request_receive_wait<int>(
                *(f.self),
                playhead,
                std::chrono::milliseconds(1000),
                playhead::logical_frame_atom_v),
            62,
            1);


    } catch (std::exception &e) {

        EXPECT_TRUE(false) << " " << e.what() << "\n";
    }

    f.self->send_exit(playhead, caf::exit_reason::user_shutdown);
    f.self->send_exit(playlist, caf::exit_reason::user_shutdown);
    f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}
