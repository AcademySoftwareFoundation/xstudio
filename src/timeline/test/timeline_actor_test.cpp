// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;
using namespace xstudio::playlist;
using namespace xstudio::global;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

using namespace std::chrono_literals;

ACTOR_TEST_SETUP()


TEST(TimelineActorSerialiseTest, Test) {
    fixture f;
    // start_logger();
    auto t    = f.self->spawn<TimelineActor>();
    auto item = request_receive<Item>(*(f.self), t, item_atom_v);

    auto serialise = request_receive<JsonStore>(*(f.self), t, serialise_atom_v);

    auto t2 = f.self->spawn<TimelineActor>(serialise);

    EXPECT_EQ(item, request_receive<Item>(*(f.self), t2, item_atom_v));

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
    f.self->send_exit(t2, caf::exit_reason::user_shutdown);
}


TEST(TimelineActorHistoryTest, Test) {
    fixture f;

    start_logger(spdlog::level::debug);

    auto uuid       = utility::Uuid();
    auto t          = f.self->spawn<TimelineActor>();
    auto stack_item = request_receive<Item>(*(f.self), t, item_atom_v, 0);
    auto stack1     = stack_item.actor();

    // add track to stack
    uuid.generate_in_place();
    auto track1 = f.self->spawn<TrackActor>(
        "Track-001", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    auto hist1 = request_receive<JsonStore>(
        *(f.self), stack1, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, track1)}));

    // add clip to track..
    uuid.generate_in_place();
    auto clip1  = f.self->spawn<ClipActor>(UuidActor(), "Clip-001", uuid);
    auto result = request_receive<JsonStore>(
        *(f.self),
        clip1,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip1)}));

    // stack
    uuid.generate_in_place();
    auto stack2 = f.self->spawn<StackActor>("Nested Stack-002", utility::FrameRate(), uuid);
    result      = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, stack2)}));
    result = request_receive<JsonStore>(
        *(f.self),
        stack2,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(2, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));

    // gap
    uuid.generate_in_place();
    auto gap1 = f.self->spawn<GapActor>(
        "Gap-001", FrameRateDuration(4, timebase::k_flicks_24fps), uuid);
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, gap1)}));

    // clip 4
    uuid.generate_in_place();
    auto clip4 = f.self->spawn<ClipActor>(UuidActor(), "Clip-004", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip4,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip4)}));

    // now populate nested stack
    uuid.generate_in_place();
    auto track2 = f.self->spawn<TrackActor>(
        "Nested Track-002", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    result = request_receive<JsonStore>(
        *(f.self), stack2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, track2)}));

    uuid.generate_in_place();
    auto track3 = f.self->spawn<TrackActor>(
        "Nested Track-003", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    result = request_receive<JsonStore>(
        *(f.self), stack2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, track3)}));
    result = request_receive<JsonStore>(
        *(f.self),
        track3,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(1, timebase::k_flicks_24fps),
            FrameRateDuration(10, timebase::k_flicks_24fps)));

    // populate nested track 2
    uuid.generate_in_place();
    auto gap2 = f.self->spawn<GapActor>(
        "Gap-002", FrameRateDuration(7, timebase::k_flicks_24fps), uuid);
    result = request_receive<JsonStore>(
        *(f.self), track2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, gap2)}));

    uuid.generate_in_place();
    auto clip3 = f.self->spawn<ClipActor>(UuidActor(), "Clip-003", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip3,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip3)}));

    // populate nested track 3

    uuid.generate_in_place();
    auto clip5 = f.self->spawn<ClipActor>(UuidActor(), "Clip-005", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip5,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track3, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip5)}));


    auto clip6_uuid = Uuid::generate();
    auto clip6      = f.self->spawn<ClipActor>(UuidActor(), "Clip-006", clip6_uuid);
    result          = request_receive<JsonStore>(
        *(f.self),
        track3,
        insert_item_atom_v,
        -1,
        UuidActorVector({UuidActor(clip6_uuid, clip6)}));
    result = request_receive<JsonStore>(
        *(f.self),
        clip6,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));

    std::this_thread::sleep_for(500ms);

    {
        // validate clip 6 directly
        auto item = request_receive<Item>(*(f.self), clip6, item_atom_v);
        EXPECT_EQ(
            item.trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));

        auto titem = request_receive<Item>(*(f.self), t, item_atom_v);
        auto sitem = find_item(titem.children(), clip6_uuid);
        // validate cache in timeline
        EXPECT_EQ(
            (*sitem)->trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));
    }
    // timeline should be valid..
    // but might need to wait for the updates to bubble up
    // something not working... ordering of events ?

    auto history = request_receive<UuidActor>(*(f.self), t, history::history_atom_v);
    EXPECT_EQ(request_receive<int>(*(f.self), history.actor(), media_cache::count_atom_v), 15);


    // THE BIG ONE..
    // TEST SIMPLE CHANGE UNDO/REDO
    request_receive<bool>(*(f.self), t, history::undo_atom_v);
    // clip duration should now be reset..

    {
        // validate clip 6 directly
        auto item = request_receive<Item>(*(f.self), clip6, item_atom_v);
        EXPECT_EQ(
            item.trimmed_range(),
            FrameRange(
                FrameRateDuration(0, timebase::k_flicks_24fps),
                FrameRateDuration(0, timebase::k_flicks_24fps)));

        auto titem = request_receive<Item>(*(f.self), t, item_atom_v);
        auto sitem = find_item(titem.children(), clip6_uuid);
        // validate cache in timeline
        EXPECT_EQ(
            (*sitem)->trimmed_range(),
            FrameRange(
                FrameRateDuration(0, timebase::k_flicks_24fps),
                FrameRateDuration(0, timebase::k_flicks_24fps)));
    }

    request_receive<bool>(*(f.self), t, history::redo_atom_v);

    {
        // validate clip 6 directly
        auto item = request_receive<Item>(*(f.self), clip6, item_atom_v);
        EXPECT_EQ(
            item.trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));

        auto titem = request_receive<Item>(*(f.self), t, item_atom_v);
        auto sitem = find_item(titem.children(), clip6_uuid);
        // validate cache in timeline
        EXPECT_EQ(
            (*sitem)->trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));
    }

    // TEST MORE COMPLEX CHANGE.
    request_receive<bool>(*(f.self), t, history::undo_atom_v);
    request_receive<bool>(*(f.self), t, history::undo_atom_v);
    // undos insertion ?
    // clip6 is now invalid
    {
        auto titem = request_receive<Item>(*(f.self), t, item_atom_v);
        auto sitem = find_item(titem.children(), clip6_uuid);
        EXPECT_EQ((*sitem), titem.cend());
    }


    request_receive<bool>(*(f.self), t, history::redo_atom_v);
    request_receive<bool>(*(f.self), t, history::redo_atom_v);

    {
        auto titem = request_receive<Item>(*(f.self), t, item_atom_v);
        auto sitem = find_item(titem.children(), clip6_uuid);
        // validate cache in timeline
        EXPECT_EQ(
            (*sitem)->trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));

        // validate clip 6 directly
        auto item = request_receive<Item>(*(f.self), (*sitem)->actor(), item_atom_v);
        EXPECT_EQ(
            item.trimmed_range(),
            FrameRange(
                FrameRateDuration(3, timebase::k_flicks_24fps),
                FrameRateDuration(3, timebase::k_flicks_24fps)));
    }
    // FTW it actually worked..


    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}

// Mirror layout of timeline test..
TEST(TimelineActorChildTest, Test) {
    fixture f;

    // start_logger();

    auto uuid = utility::Uuid();
    auto t    = f.self->spawn<TimelineActor>();

    auto stack_item = request_receive<Item>(*(f.self), t, item_atom_v, 0);
    auto stack1     = stack_item.actor();

    // // add stack..
    // uuid.generate_in_place();
    // auto stack1 = f.self->spawn<StackActor>("Stack-001", uuid);
    // auto result = request_receive<bool>(*(f.self), t, insert_item_atom_v, 0,
    // UuidActor(uuid, stack1));

    // add track to stack
    uuid.generate_in_place();
    auto track1 = f.self->spawn<TrackActor>(
        "Track-001", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    auto result = request_receive<JsonStore>(
        *(f.self), stack1, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, track1)}));

    // add clip to track..
    uuid.generate_in_place();
    auto clip1 = f.self->spawn<ClipActor>(UuidActor(), "Clip-001", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip1,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip1)}));

    // stack
    uuid.generate_in_place();
    auto stack2 = f.self->spawn<StackActor>("Nested Stack-002", utility::FrameRate(), uuid);
    result      = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, stack2)}));
    result = request_receive<JsonStore>(
        *(f.self),
        stack2,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(2, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));

    // gap
    uuid.generate_in_place();
    auto gap1 = f.self->spawn<GapActor>(
        "Gap-001", FrameRateDuration(4, timebase::k_flicks_24fps), uuid);
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, gap1)}));

    // clip 4
    uuid.generate_in_place();
    auto clip4 = f.self->spawn<ClipActor>(UuidActor(), "Clip-004", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip4,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track1, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip4)}));

    // now populate nested stack
    uuid.generate_in_place();
    auto track2 = f.self->spawn<TrackActor>(
        "Nested Track-002", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    result = request_receive<JsonStore>(
        *(f.self), stack2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, track2)}));

    uuid.generate_in_place();
    auto track3 = f.self->spawn<TrackActor>(
        "Nested Track-003", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
    result = request_receive<JsonStore>(
        *(f.self), stack2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, track3)}));
    result = request_receive<JsonStore>(
        *(f.self),
        track3,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(1, timebase::k_flicks_24fps),
            FrameRateDuration(10, timebase::k_flicks_24fps)));

    // populate nested track 2
    uuid.generate_in_place();
    auto gap2 = f.self->spawn<GapActor>(
        "Gap-002", FrameRateDuration(7, timebase::k_flicks_24fps), uuid);
    result = request_receive<JsonStore>(
        *(f.self), track2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, gap2)}));

    uuid.generate_in_place();
    auto clip3 = f.self->spawn<ClipActor>(UuidActor(), "Clip-003", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip3,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track2, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip3)}));

    // populate nested track 3

    uuid.generate_in_place();
    auto clip5 = f.self->spawn<ClipActor>(UuidActor(), "Clip-005", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip5,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track3, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip5)}));


    uuid.generate_in_place();
    auto clip6 = f.self->spawn<ClipActor>(UuidActor(), "Clip-006", uuid);
    result     = request_receive<JsonStore>(
        *(f.self),
        clip6,
        active_range_atom_v,
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));
    result = request_receive<JsonStore>(
        *(f.self), track3, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, clip6)}));

    // timeline should be valid..
    // but might need to wait for the updates to bubble up
    // something not working... ordering of events ?
    std::this_thread::sleep_for(500ms);

    // validate trimmed ranges
    auto item = request_receive<Item>(*(f.self), clip6, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));
    item = request_receive<Item>(*(f.self), clip5, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), track3, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(1, timebase::k_flicks_24fps),
            FrameRateDuration(10, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), clip3, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(9, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), gap2, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(7, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), track2, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(16, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), stack2, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(2, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), clip1, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(3, timebase::k_flicks_24fps),
            FrameRateDuration(3, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), gap1, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(4, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), clip4, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(100, timebase::k_flicks_24fps),
            FrameRateDuration(6, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), track1, item_atom_v);
    // spdlog::warn("{}", item.serialise().dump(2));
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(19, timebase::k_flicks_24fps)));

    item = request_receive<Item>(*(f.self), stack1, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(19, timebase::k_flicks_24fps)));


    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(
        item.trimmed_range(),
        FrameRange(
            FrameRateDuration(0, timebase::k_flicks_24fps),
            FrameRateDuration(19, timebase::k_flicks_24fps)));

    // serialise test

    auto serialise = request_receive<JsonStore>(*(f.self), t, serialise_atom_v);
    auto t2        = f.self->spawn<TimelineActor>(serialise);
    item           = request_receive<Item>(*(f.self), t2, item_atom_v);
    EXPECT_EQ(
        item.trimmed_frame_duration().frames(),
        FrameRateDuration(19, timebase::k_flicks_24fps).frames());

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
    f.self->send_exit(t2, caf::exit_reason::user_shutdown);
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

// request_receive<UuidActor>(*(f.self), pl, add_media_atom_v, m, Uuid());

// auto clip1 = f.self->spawn<ClipActor>(pl, m);
// auto clip2 = f.self->spawn<ClipActor>(pl, m);

// auto vtrack1 = f.self->spawn<TrackActor>(pl, MediaType::MT_IMAGE, "VTrack");


// EXPECT_NO_THROW(
//     request_receive<UuidActor>(*(f.self), vtrack1, insert_clip_atom_v, clip1, Uuid()));

// EXPECT_NO_THROW(
//     request_receive<UuidActor>(*(f.self), vtrack1, insert_clip_atom_v, clip2, Uuid()));

// auto timeline1 = f.self->spawn<TimelineActor>(pl, "Timeline");
// auto edit_list =
//     request_receive<EditList>(*(f.self), timeline1, media::get_edit_list_atom_v, Uuid());
// EXPECT_EQ(edit_list.size(), unsigned(0));

// EXPECT_NO_THROW(
//     request_receive<UuidActor>(*(f.self), timeline1, insert_track_atom_v, vtrack1, Uuid()));

// edit_list =
//     request_receive<EditList>(*(f.self), timeline1, media::get_edit_list_atom_v, Uuid());
// // // check default, should map to original STL
// EXPECT_EQ(edit_list.size(), unsigned(2));
// EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED), unsigned(20));


// auto serial    = request_receive<JsonStore>(*(f.self), timeline1, serialise_atom_v);
// auto timeline2 = f.self->spawn<TimelineActor>(pl, serial);
// edit_list =
//     request_receive<EditList>(*(f.self), timeline2, media::get_edit_list_atom_v, Uuid());
// // // check default, should map to original STL
// EXPECT_EQ(edit_list.size(), unsigned(2));
// EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED), unsigned(20));

// auto duration = request_receive<FrameRateDuration>(*(f.self), clip1, duration_atom_v);
// duration.set_frames(5);
// request_receive<bool>(*(f.self), clip1, duration_atom_v, duration);
// duration.set_frames(1);
// request_receive<bool>(*(f.self), clip1, start_time_atom_v, duration);

// // wait for update to flow..
// std::this_thread::sleep_for(std::chrono::milliseconds(100));

// edit_list =
//     request_receive<EditList>(*(f.self), timeline1, media::get_edit_list_atom_v, Uuid());
// EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED), unsigned(15));
// EXPECT_EQ(edit_list.section_list()[0].frame_rate_and_duration_.frames(), 5);
// EXPECT_EQ(edit_list.section_list()[1].frame_rate_and_duration_.frames(), 10);

// EXPECT_EQ(
//     request_receive<media::AVFrameID>(
//         *(f.self), timeline1, media::get_media_pointer_atom_v, media::MT_IMAGE, 0)
//         .frame_,
//     2);
// EXPECT_EQ(
//     request_receive<media::AVFrameID>(
//         *(f.self), timeline1, media::get_media_pointer_atom_v, media::MT_IMAGE, 4)
//         .frame_,
//     6);
// EXPECT_EQ(
//     request_receive<media::AVFrameID>(
//         *(f.self), timeline1, media::get_media_pointer_atom_v, media::MT_IMAGE, 5)
//         .frame_,
//     1);

// f.self->send_exit(timeline1, caf::exit_reason::user_shutdown);
// f.self->send_exit(timeline2, caf::exit_reason::user_shutdown);
// f.self->send_exit(gsa, caf::exit_reason::user_shutdown);