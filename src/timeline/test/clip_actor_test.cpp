// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;
using namespace xstudio::playlist;
using namespace xstudio::global;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()


TEST(ClipActorTest, Test) {

    fixture f;

    auto c = f.self->spawn<ClipActor>(UuidActor(), "Clip1");

    auto item = request_receive<Item>(*(f.self), c, item_atom_v);

    auto serialise = request_receive<JsonStore>(*(f.self), c, serialise_atom_v);
    auto titem     = Item();
    auto cc        = f.self->spawn<ClipActor>(serialise, titem);
    EXPECT_EQ(item, request_receive<Item>(*(f.self), cc, item_atom_v));

    f.self->send_exit(c, caf::exit_reason::user_shutdown);
    f.self->send_exit(cc, caf::exit_reason::user_shutdown);
}
// fixture f;
// auto gsa = f.self->spawn<GlobalActor>();
// auto pl  = f.self->spawn<PlaylistActor>("Test");

// auto su1 = Uuid::generate();
// auto m   = f.self->spawn<MediaActor>(
//     "test",
//     Uuid(),

//     UuidActorVector({UuidActor(su1, f.self->spawn<MediaSourceActor>(
//         "test", posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
//         FrameList(1, 10), utility::FrameRate(timebase::k_flicks_24fps),
//         su1))})
// );

// start_logger(spdlog::level::debug);


// request_receive<UuidActor>(*(f.self), pl, add_media_atom_v, m, Uuid());


// auto clip1 = f.self->spawn<ClipActor>(pl, m);

// JsonStore serial;
// f.self->request(clip1, infinite, serialise_atom_v)
//     .receive(
//         [&](const JsonStore &jsn) { serial = jsn; },
//         [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

// auto clip2 = f.self->spawn<ClipActor>(pl, serial);

// auto frd1 = request_receive<FrameRateDuration>(*(f.self), clip1, duration_atom_v);

// auto frd2 = request_receive<FrameRateDuration>(*(f.self), clip2, duration_atom_v);

// EXPECT_EQ(frd1, frd2);

// EXPECT_EQ(
//     frd1,
//     request_receive<std::vector<MediaReference>>(
//         *(f.self), m, media::media_reference_atom_v)[0]
//         .duration());

// auto edit_list =
//     request_receive<EditList>(*(f.self), clip1, media::get_edit_list_atom_v, Uuid());

// // check default, should map to original STL
// EXPECT_EQ(edit_list.size(), unsigned(1));
// EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED), unsigned(10));
// int media_frame;
// edit_list.media_frame(0, media_frame);
// EXPECT_EQ(media_frame, 0);
// EXPECT_EQ(to_string(edit_list.section_list()[0].timecode_), "00:00:00:00");

// // now adjust clip duration
// auto duration   = request_receive<FrameRateDuration>(*(f.self), clip1, duration_atom_v);
// auto start_time = request_receive<FrameRateDuration>(*(f.self), clip1, start_time_atom_v);

// duration.set_frames(5);
// start_time.set_frames(1);

// request_receive<bool>(*(f.self), clip1, start_time_atom_v, start_time);
// request_receive<bool>(*(f.self), clip1, duration_atom_v, duration);

// edit_list =
//     request_receive<EditList>(*(f.self), clip1, media::get_edit_list_atom_v, Uuid());
// EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED), unsigned(5));
// EXPECT_EQ(to_string(edit_list.section_list()[0].timecode_), "00:00:00:01");

// // try and get frame 0.. should return frame 2 (sequence starts at 1).
// // try and get frame 4.. should return frame 6.
// // try and get frame 5 whould raise error.
// EXPECT_EQ(
//     request_receive<media::AVFrameID>(
//         *(f.self), clip1, media::get_media_pointer_atom_v, 0)
//         .frame_,
//     2);
// EXPECT_EQ(
//     request_receive<media::AVFrameID>(
//         *(f.self), clip1, media::get_media_pointer_atom_v, 4)
//         .frame_,
//     6);

// EXPECT_THROW(
//     request_receive<media::AVFrameID>(
//         *(f.self), clip1, media::get_media_pointer_atom_v, 5),
//     std::runtime_error);

// f.self->send_exit(clip1, caf::exit_reason::user_shutdown);
// f.self->send_exit(clip2, caf::exit_reason::user_shutdown);
// f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
