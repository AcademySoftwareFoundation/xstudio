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
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

using namespace std::chrono_literals;

TEST(StackActorMoveTest, Test) {
    fixture f;
    // start_logger();
    auto t     = f.self->spawn<StackActor>();
    auto valid = caf::actor();

    auto uuid1 = utility::Uuid::generate();
    valid      = f.self->spawn<GapActor>("Gap1", utility::FrameRateDuration(), uuid1);
    EXPECT_NO_THROW(request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid1, valid)})));
    auto uuid2 = utility::Uuid::generate();
    valid      = f.self->spawn<GapActor>("Gap2", utility::FrameRateDuration(), uuid2);
    EXPECT_NO_THROW(request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid2, valid)})));
    auto uuid3 = utility::Uuid::generate();
    valid      = f.self->spawn<GapActor>("Gap3", utility::FrameRateDuration(), uuid3);
    EXPECT_NO_THROW(request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid3, valid)})));
    auto uuid4 = utility::Uuid::generate();
    valid      = f.self->spawn<GapActor>("Gap4", utility::FrameRateDuration(), uuid4);
    EXPECT_NO_THROW(request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid4, valid)})));
    auto uuid5 = utility::Uuid::generate();
    valid      = f.self->spawn<GapActor>("Gap5", utility::FrameRateDuration(), uuid5);
    EXPECT_NO_THROW(request_receive<JsonStore>(
        *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid5, valid)})));


    auto item = request_receive<Item>(*(f.self), t, item_atom_v);
    // for(const auto &i:item.children())
    //     spdlog::warn("{}", to_string(i.uuid()));

    // spdlog::warn("");
    EXPECT_EQ(item.children().front().uuid(), uuid1);
    // test move..
    EXPECT_NO_THROW(request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 1, 1));
    // check move happened..
    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.children().front().uuid(), uuid2);

    // for(const auto &i:item.children())
    //     spdlog::warn("{}", to_string(i.uuid()));

    EXPECT_NO_THROW(request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 2, -1));
    // check move happened..
    item = request_receive<Item>(*(f.self), t, item_atom_v);
    EXPECT_EQ(item.children().back().uuid(), uuid1);

    // for(const auto &i:item.children())
    //     spdlog::warn("{}", to_string(i.uuid()));
    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}


TEST(StackActorTest, Test) {
    fixture f;
    // add stack
    auto s    = f.self->spawn<StackActor>();
    auto item = request_receive<Item>(*(f.self), s, item_atom_v);

    // check duration is zero.
    EXPECT_EQ(item.trimmed_frame_duration(), FrameRateDuration(0, timebase::k_flicks_24fps));

    // add gap
    auto guuid = Uuid::generate();
    auto g =
        f.self->spawn<GapActor>("Gap1", FrameRateDuration(10, timebase::k_flicks_24fps), guuid);
    auto result = request_receive<JsonStore>(
        *(f.self), s, insert_item_atom_v, 0, UuidActorVector({UuidActor(guuid, g)}));

    // stack duration should have changed.
    item = request_receive<Item>(*(f.self), s, item_atom_v);
    EXPECT_EQ(item.trimmed_frame_duration(), FrameRateDuration(10, timebase::k_flicks_24fps));

    // check update from child gap
    result = request_receive<JsonStore>(
        *(f.self),
        g,
        active_range_atom_v,
        FrameRange(FrameRateDuration(20, timebase::k_flicks_24fps)));
    // wait for update to bubble up
    std::this_thread::sleep_for(1000ms);
    item = request_receive<Item>(*(f.self), s, item_atom_v);
    EXPECT_EQ(item.available_frame_duration(), FrameRateDuration(20, timebase::k_flicks_24fps));

    // check serialise.
    auto serialise = request_receive<JsonStore>(*(f.self), s, serialise_atom_v);
    Item tmp;
    auto s2 = f.self->spawn<StackActor>(serialise, tmp);

    // validate
    EXPECT_EQ(item, request_receive<Item>(*(f.self), s2, item_atom_v));

    f.self->send_exit(s, caf::exit_reason::user_shutdown);
    f.self->send_exit(s2, caf::exit_reason::user_shutdown);
}


TEST(NestedStackActorTest, Test) {
    fixture f;
    // start_logger();

    auto s = f.self->spawn<StackActor>("Stack");

    auto cuuid = Uuid::generate();
    auto c     = f.self->spawn<StackActor>("ChildStack", utility::FrameRate(), cuuid);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), s, insert_item_atom_v, -1, UuidActorVector({UuidActor(cuuid, c)}));
    }

    std::this_thread::sleep_for(500ms);

    auto guuid1 = Uuid::generate();
    auto g1     = f.self->spawn<GapActor>(
        "Gap1", FrameRateDuration(10, timebase::k_flicks_24fps), guuid1);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid1, g1)}));
    }
    auto guuid2 = Uuid::generate();
    auto g2     = f.self->spawn<GapActor>(
        "Gap2", FrameRateDuration(20, timebase::k_flicks_24fps), guuid2);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid2, g2)}));
    }
    auto guuid3 = Uuid::generate();
    auto g3     = f.self->spawn<GapActor>(
        "Gap3", FrameRateDuration(30, timebase::k_flicks_24fps), guuid3);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid3, g3)}));
    }

    // Stack[ChildStack[Gap1,Gap2,Gap3]]
    // s item should hold info on children.
    std::this_thread::sleep_for(500ms);
    auto sitem = request_receive<Item>(*(f.self), s, item_atom_v);
    // s copy of c has three children
    EXPECT_EQ(sitem.children().front().size(), 3);

    // c has three children.
    auto citem = request_receive<Item>(*(f.self), c, item_atom_v);
    EXPECT_EQ(citem.size(), 3);

    // check our child is c
    EXPECT_EQ(cuuid, sitem.children().front().uuid());
    EXPECT_EQ(c, sitem.children().front().actor());

    // check citem first child
    EXPECT_EQ(guuid1, citem.children().front().uuid());
    EXPECT_EQ(g1, citem.children().front().actor());

    // check it exists in s
    EXPECT_EQ(guuid1, sitem.children().front().children().front().uuid());
    EXPECT_EQ(g1, sitem.children().front().children().front().actor());

    // leave..
    f.self->send_exit(s, caf::exit_reason::user_shutdown);
}


TEST(NestedTrackStackActorTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);

    auto s = f.self->spawn<StackActor>("Stack");

    auto cuuid = Uuid::generate();
    auto c     = f.self->spawn<TrackActor>(
        "ChildTrack", utility::FrameRate(), media::MediaType::MT_IMAGE, cuuid);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), s, insert_item_atom_v, -1, UuidActorVector({UuidActor(cuuid, c)}));
    }

    auto guuid1 = Uuid::generate();
    auto g1     = f.self->spawn<GapActor>(
        "Gap1", FrameRateDuration(10, timebase::k_flicks_24fps), guuid1);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid1, g1)}));
    }
    auto guuid2 = Uuid::generate();
    auto g2     = f.self->spawn<GapActor>(
        "Gap2", FrameRateDuration(20, timebase::k_flicks_24fps), guuid2);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid2, g2)}));
    }
    auto guuid3 = Uuid::generate();
    auto g3     = f.self->spawn<GapActor>(
        "Gap3", FrameRateDuration(30, timebase::k_flicks_24fps), guuid3);
    {
        auto result = request_receive<JsonStore>(
            *(f.self), c, insert_item_atom_v, -1, UuidActorVector({UuidActor(guuid3, g3)}));
    }

    // Stack[ChildStack[Gap1,Gap2,Gap3]]
    // s item should hold info on children.
    std::this_thread::sleep_for(500ms);
    auto sitem = request_receive<Item>(*(f.self), s, item_atom_v);
    // s copy of c has three children
    EXPECT_EQ(sitem.children().front().size(), 3);

    // c has three children.
    auto citem = request_receive<Item>(*(f.self), c, item_atom_v);
    EXPECT_EQ(citem.size(), 3);

    // check our child is c
    EXPECT_EQ(cuuid, sitem.children().front().uuid());
    EXPECT_EQ(c, sitem.children().front().actor());

    // check citem first child
    EXPECT_EQ(guuid1, citem.children().front().uuid());
    EXPECT_EQ(g1, citem.children().front().actor());

    // check it exists in s
    EXPECT_EQ(guuid1, sitem.children().front().children().front().uuid());
    EXPECT_EQ(g1, sitem.children().front().children().front().actor());

    // leave..
    f.self->send_exit(s, caf::exit_reason::user_shutdown);
    f.self->send_exit(c, caf::exit_reason::user_shutdown);
}
// test
TEST(StackActorAddTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t = f.self->spawn<StackActor>("Test Stack");

    {
        auto uuid = utility::Uuid::generate();
        auto invalid =
            f.self->spawn<TimelineActor>("Invalid Timeline", utility::FrameRate(), uuid);
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
        auto valid = f.self->spawn<TrackActor>(
            "Valid Track", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<StackActor>("Valid Stack", utility::FrameRate(), uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<ClipActor>(UuidActor(), "Valid Clip", uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<ClipActor>(UuidActor(), "Valid Clip", uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, -1, UuidActorVector({UuidActor(uuid, valid)})));
    }

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<GapActor>("Valid Gap", utility::FrameRateDuration(), uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}

// test
TEST(StackActorMoveTest2, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto t = f.self->spawn<StackActor>("Test Stack");

    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<TrackActor>(
            "Track 4", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }
    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<TrackActor>(
            "Track 3", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }
    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<TrackActor>(
            "Track 2", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }
    {
        auto uuid  = utility::Uuid::generate();
        auto valid = f.self->spawn<TrackActor>(
            "Track 1", utility::FrameRate(), media::MediaType::MT_IMAGE, uuid);
        EXPECT_NO_THROW(request_receive<JsonStore>(
            *(f.self), t, insert_item_atom_v, 0, UuidActorVector({UuidActor(uuid, valid)})));
    }

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 1");
    request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 1, 1);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 2");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 1");

    request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 1, 1, 0);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 1");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 2");

    request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 2, 1);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 3");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 1");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 2).name(), "Track 2");

    request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 1, 2, 0);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 1");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 2");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 2).name(), "Track 3");

    auto hist = request_receive<JsonStore>(*(f.self), t, move_item_atom_v, 0, 2, 1);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 3");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 1");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 2).name(), "Track 2");

    request_receive<bool>(*(f.self), t, history::undo_atom_v, hist);

    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 0).name(), "Track 1");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 1).name(), "Track 2");
    EXPECT_EQ(request_receive<Item>(*(f.self), t, item_atom_v, 2).name(), "Track 3");

    f.self->send_exit(t, caf::exit_reason::user_shutdown);
}
