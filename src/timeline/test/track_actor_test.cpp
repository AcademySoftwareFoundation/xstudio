// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

using namespace std::chrono_literals;

TEST(TrackActorAddTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t = f.self->spawn<TrackActor>("Top Track");

    {
        auto uuid = utility::Uuid::generate();
        auto invalid =
            f.self->spawn<TrackActor>("Invalid Track", media::MediaType::MT_IMAGE, uuid);
        EXPECT_THROW(
            request_receive<JsonStore>(
                *(f.self),
                t,
                insert_item_atom_v,
                0,
                UuidActorVector({UuidActor(uuid, invalid)})),
            std::runtime_error);
        f.self->send_exit(invalid, caf::exit_reason::user_shutdown);
    }

    {
        auto uuid    = utility::Uuid::generate();
        auto invalid = f.self->spawn<TimelineActor>("Invalid Timeline", uuid);
        EXPECT_THROW(
            request_receive<JsonStore>(
                *(f.self),
                t,
                insert_item_atom_v,
                0,
                UuidActorVector({UuidActor(uuid, invalid)})),
            std::runtime_error);
        f.self->send_exit(invalid, caf::exit_reason::user_shutdown);
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<StackActor>("Valid Stack", uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<GapActor>("Valid GAP", utility::FrameRateDuration(), uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<ClipActor>(UuidActor(), "Valid Clip", uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    auto item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.size(), 3);

    EXPECT_NO_THROW(request_receive<JsonStore>(*(f.self), t, erase_item_atom_v, 0));
    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.size(), 2);

    auto jitem = std::pair<JsonStore, std::vector<Item>>();
    EXPECT_NO_THROW(
        (jitem = request_receive<std::pair<JsonStore, std::vector<Item>>>(
             *(f.self), t, remove_item_atom_v, 0)));
    EXPECT_EQ(jitem.second[0].item_type(), ItemType::IT_GAP);
    f.self->send_exit(jitem.second[0].actor(), caf::exit_reason::user_shutdown);

    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.size(), 1);

    f.self->send_exit(item.children().begin()->actor(), caf::exit_reason::user_shutdown);
    // need delay for update to propagate.
    std::this_thread::sleep_for(500ms);

    //
    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.size(), 0);

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}

TEST(TrackActorTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t = f.self->spawn<TrackActor>();
    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<GapActor>("Valid GAP", utility::FrameRateDuration(), uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }


    auto item = request_receive<Item>(*(f.self), t, item_atom_v);
    // EXPECT_EQ(item.available_frame_duration(), FrameRateDuration(0, timebase::k_flicks_24fps)
    // );

    auto serialise = request_receive<JsonStore>(*(f.self), t, serialise_atom_v);
    Item tmp;
    auto t2 = f.self->spawn<TrackActor>(serialise, tmp);
    EXPECT_EQ(item, request_receive<Item>(*(f.self), t2, item_atom_v));

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
    f.self->send_exit(t2, caf::exit_reason::user_shutdown);
}

// [=](move_item_atom, const int src_index, const int count, const int dst_index)
//     -> result<JsonStore> {
//     auto rp = make_response_promise<JsonStore>();
//     move_items(src_index, count, dst_index, rp);
//     return rp;
// },


TEST(TrackMoveActorTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t        = f.self->spawn<TrackActor>();
    auto duration = FrameRateDuration(10, timebase::k_flicks_24fps);

    auto gap_uuid1  = utility::Uuid::generate();
    auto gap_actor1 = f.self->spawn<GapActor>("Gap1", duration, gap_uuid1);
    auto gap_uuid2  = utility::Uuid::generate();
    auto gap_actor2 = f.self->spawn<GapActor>("Gap2", duration, gap_uuid2);
    auto gap_uuid3  = utility::Uuid::generate();
    auto gap_actor3 = f.self->spawn<GapActor>("Gap3", duration, gap_uuid3);

    request_receive<JsonStore>(
        *(f.self),
        t,
        insert_item_atom_v,
        0,
        UuidActorVector({UuidActor(gap_uuid3, gap_actor3)}));
    request_receive<JsonStore>(
        *(f.self),
        t,
        insert_item_atom_v,
        0,
        UuidActorVector({UuidActor(gap_uuid2, gap_actor2)}));
    request_receive<JsonStore>(
        *(f.self),
        t,
        insert_item_atom_v,
        0,
        UuidActorVector({UuidActor(gap_uuid1, gap_actor1)}));

    auto item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ((*(item.item_at_index(0)))->uuid(), gap_uuid1);
    EXPECT_EQ((*(item.item_at_index(1)))->uuid(), gap_uuid2);
    EXPECT_EQ((*(item.item_at_index(2)))->uuid(), gap_uuid3);

    request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 1, 2);

    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ((*(item.item_at_index(0)))->uuid(), gap_uuid2);
    EXPECT_EQ((*(item.item_at_index(1)))->uuid(), gap_uuid1);
    EXPECT_EQ((*(item.item_at_index(2)))->uuid(), gap_uuid3);


    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}

TEST(TrackUndoActorTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t    = f.self->spawn<TrackActor>();
    auto item = request_receive<Item>(*(f.self), t, item_atom_v);

    // create gap, check duration.
    auto guuid1    = utility::Uuid::generate();
    auto duration1 = FrameRateDuration(10, timebase::k_flicks_24fps);
    auto range1    = FrameRange(duration1);
    auto gap1      = f.self->spawn<GapActor>("Gap1", duration1, guuid1);
    auto gitem     = request_receive<Item>(*(f.self), gap1, item_atom_v);
    EXPECT_EQ(gitem.trimmed_range(), range1);

    // insert gap.
    auto hist1 = request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(guuid1, gap1)}));

    // check gap in track
    auto item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item2.children().front().trimmed_range(), range1);

    // change gap ..
    auto range2 = FrameRange(FrameRateDuration(20, timebase::k_flicks_24fps));
    auto hist2  = request_receive<JsonStore>(*(f.self), gap1, active_range_atom_v, range2);
    gitem       = request_receive<Item>(*(f.self), gap1, item_atom_v);
    EXPECT_EQ(gitem.trimmed_range(), range2);

    // should match in track
    item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item2.children().front().trimmed_range(), range2);

    // we should be able to now use the histories..
    request_receive<bool>(*(f.self), t, history::undo_atom_v, hist2);
    // validate changes have been applied.
    // we may have a slight issue as we really needed to capture the change event at the track
    // level so our track availiable range will be off..
    gitem = request_receive<Item>(*(f.self), gap1, item_atom_v);
    EXPECT_EQ(gitem.trimmed_range(), range1);

    item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item2.children().front().trimmed_range(), range1);

    // wow that actually worked..
    // redo it..
    request_receive<bool>(*(f.self), t, history::redo_atom_v, hist2);
    gitem = request_receive<Item>(*(f.self), gap1, item_atom_v);
    EXPECT_EQ(gitem.trimmed_range(), range2);

    item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item2.children().front().trimmed_range(), range2);

    // try to undo insertion..
    request_receive<bool>(*(f.self), t, history::undo_atom_v, hist2);
    request_receive<bool>(*(f.self), t, history::undo_atom_v, hist1);
    item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_TRUE(item2.children().empty());

    // WOW * 2 it worked..

    // now to redo insertion... ack requires recreation of actor... erm..
    request_receive<bool>(*(f.self), t, history::redo_atom_v, hist1);

    // check item..
    item2 = request_receive<Item>(*(f.self), t, item_atom_v);
    // check gap in track
    EXPECT_EQ(item2.children().front().trimmed_range(), range1);

    // // check address..
    // auto serialise = request_receive<JsonStore>(*(f.self), t, serialise_atom_v);

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}
