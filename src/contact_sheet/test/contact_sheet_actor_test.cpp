// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/contact_sheet/contact_sheet_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/utility/helpers.hpp"

#include "xstudio/media/media.hpp"
#include "xstudio/ui/viewport/viewport.hpp"

using namespace xstudio::utility;
using namespace xstudio::contact_sheet;
using namespace xstudio::media;
using namespace xstudio::playlist;

using namespace caf;
#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(ContactSheetActorTest, Test) {
    // fixture f;
    // auto tmp = f.self->spawn<PlaylistActor>("Test");
    // f.self->anon_mail(//     add_media_atom_v,
    //     f.self->spawn<MediaActor>(
    //         "test",
    //         Uuid(),
    //         f.self->spawn<MediaSourceActor>(
    //             "test",
    //             posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
    //             FrameList(1, 10))),
    //     Uuid()).send(//     tmp);
    // f.self->anon_mail(//     add_media_atom_v,
    //     f.self->spawn<MediaActor>(
    //         "test",
    //         Uuid(),
    //         f.self->spawn<MediaSourceActor>(
    //             "test", posix_path_to_uri(TEST_RESOURCE "/media/test.mov"))),
    //     Uuid()).send(//     tmp);

    // f.self->request(tmp, std::chrono::seconds(10), name_atom_v).receive(
    // 	[&](const std::string &name) {
    // 		EXPECT_EQ(name, "Test");
    // 	},
    // 	[&](const caf::error&) {
    // 		EXPECT_TRUE(false);
    // 	}
    // );

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
