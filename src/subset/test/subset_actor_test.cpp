// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/subset/subset_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace std::chrono_literals;

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::subset;
using namespace xstudio::media;
using namespace xstudio::playlist;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(SubsetActorTest, Test) {
    fixture f;
    start_logger(spdlog::level::debug);
    auto playlist = f.self->spawn<PlaylistActor>("Test");

    auto su1   = Uuid::generate();
    auto media = f.self->spawn<MediaActor>(
        "test",
        Uuid(),
        UuidActorVector({UuidActor(
            su1,
            f.self->spawn<MediaSourceActor>(
                "test",
                posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
                FrameList(1, 10),
                utility::FrameRate(timebase::k_flicks_24fps),
                su1))}));

    f.self->anon_send(playlist, add_media_atom_v, media, Uuid());

    auto media_uuid = request_receive<Uuid>(*(f.self), media, uuid_atom_v);

    auto su2    = Uuid::generate();
    auto media2 = f.self->spawn<MediaActor>(
        "test",
        Uuid(),
        UuidActorVector({UuidActor(
            su2,
            f.self->spawn<MediaSourceActor>(
                "test",
                posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
                utility::FrameRate(timebase::k_flicks_24fps),
                su2))}));

    f.self->anon_send(playlist, add_media_atom_v, media2, Uuid());

    // spawn subset..
    auto subset = f.self->spawn<SubsetActor>(playlist, "Test");

    request_receive<bool>(*(f.self), subset, add_media_atom_v, media, Uuid());

    request_receive<bool>(*(f.self), playlist, remove_media_atom_v, media_uuid);

    std::this_thread::sleep_for(1s);

    // f.self->send(tmp, add_media_atom_v,
    // f.self->spawn<MediaActor>(posix_path_to_uri(TEST_RESOURCE "/media/test.0001.exr")),
    // Uuid()); f.self->send(tmp, add_media_atom_v,
    // f.self->spawn<MediaActor>(posix_path_to_uri(TEST_RESOURCE "/media/test.mov")), Uuid());

    //    JsonStore serial;
    //    f.self->request(tmp, infinite, serialise_atom_v).receive(
    //      [&](const JsonStore &jsn) {
    //        serial = jsn;
    //      },
    //      [&](const caf::error&) {
    //        EXPECT_TRUE(false) << "Should return true";
    //      }
    //    );

    // auto tmp2 = f.self->spawn<PlaylistActor>(serial);
    // auto tmp3 = f.self->spawn<PlaylistActor>(serial);

    // f.self->request(tmp2, std::chrono::seconds(10), name_atom_v).receive(
    // 	[&](const std::string &name) {
    // 		EXPECT_EQ(name, "Test");
    // 	},
    // 	[&](const caf::error&) {
    // 		EXPECT_TRUE(false);
    // 	}
    // );
}
