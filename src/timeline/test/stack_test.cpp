// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/timeline/gap.hpp"
#include "xstudio/timeline/clip.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;

TEST(StackTest, Test) {
    Stack s;
    // utility::start_logger();
    auto gd1 = utility::FrameRateDuration(10, timebase::k_flicks_24fps);
    auto g1  = Gap("Gap1", gd1);
    auto gd2 = utility::FrameRateDuration(15, timebase::k_flicks_24fps);
    auto g2  = Gap("Gap2", gd2);
    auto gd3 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto g3  = Gap("Gap3", gd3);

    auto ru1 = s.item().insert(s.item().end(), g1.item());
    auto ru2 = s.item().insert(s.item().end(), g2.item());
    auto ru3 = s.item().insert(s.item().end(), g3.item());

    auto it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    s.item().undo(ru3);
    s.item().undo(ru2);
    s.item().undo(ru1);

    EXPECT_TRUE(s.item().empty());

    s.item().redo(ru1);
    s.item().redo(ru2);
    s.item().redo(ru3);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    auto ru4 = s.item().erase(s.item().begin());
    it       = s.item().begin();
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    EXPECT_EQ(s.item().size(), 2);

    s.item().undo(ru4);
    EXPECT_EQ(s.item().size(), 3);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());


    auto start = std::next(s.item().begin(), 0);
    auto end   = std::next(s.item().begin(), 1);
    auto pos   = std::next(s.item().begin(), 3);

    auto ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    // move / undo
    start = std::next(s.item().begin(), 0);
    end   = std::next(s.item().begin(), 2);
    pos   = std::next(s.item().begin(), 3);

    ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    // move / undo
    start = std::next(s.item().begin(), 0);
    end   = std::next(s.item().begin(), 2);
    pos   = std::next(s.item().begin(), 3);

    ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());


    // do nested changes work ?
    auto ru6 = s.item().children().front().set_enabled(false);
    EXPECT_FALSE(s.item().children().front().enabled());
    s.item().undo(ru6);
    EXPECT_TRUE(s.item().children().front().enabled());
    s.item().redo(ru6);
    EXPECT_FALSE(s.item().children().front().enabled());
    s.item().undo(ru6);
    EXPECT_TRUE(s.item().children().front().enabled());

    // check no op
    s.item().set_enabled(true);
    auto ru7 = s.item().set_enabled(true);
    // no op
    s.item().undo(ru7);
    EXPECT_TRUE(s.item().enabled());

    // we should be able to replay events. on cloned trees
    Stack s2(s.serialise());
    s2.item().update(ru6);
    EXPECT_FALSE(s2.item().children().front().enabled());
}

TEST(StackBoxTest, Test) {
    Stack s;

    auto g1 = Gap("Gap1", utility::FrameRateDuration(10, timebase::k_flicks_24fps));
    auto g2 = Gap("Gap2", utility::FrameRateDuration(15, timebase::k_flicks_24fps));
    s.item().push_back(g1.item());
    s.item().push_back(g2.item());

    s.item().refresh();

    auto box = s.item().box();

    EXPECT_EQ(box.first.first, 0);
    EXPECT_EQ(box.first.second, 0);
    EXPECT_EQ(box.second.first, 15);
    EXPECT_EQ(box.second.second, 2);

    auto cbox = s.item().box(g1.uuid());
    EXPECT_EQ(cbox->first.first, 0);
    EXPECT_EQ(cbox->first.second, 0);
    EXPECT_EQ(cbox->second.first, 10);
    EXPECT_EQ(cbox->second.second, 1);

    cbox = s.item().box(g2.uuid());
    EXPECT_EQ(cbox->first.first, 0);
    EXPECT_EQ(cbox->first.second, 1);
    EXPECT_EQ(cbox->second.first, 15);
    EXPECT_EQ(cbox->second.second, 2);
}

TEST(StackRefreshTest, Test) {
    Stack s;

    // utility::start_logger();
    auto gd1 = utility::FrameRateDuration(10, timebase::k_flicks_24fps);
    auto g1  = Gap("Gap1", gd1);
    auto gd2 = utility::FrameRateDuration(15, timebase::k_flicks_24fps);
    auto g2  = Gap("Gap2", gd2);
    auto gd3 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto g3  = Gap("Gap3", gd3);

    auto ru1 = s.item().insert(s.item().end(), g1.item());
    auto ru2 = s.item().insert(s.item().end(), g2.item());
    auto ru3 = s.item().insert(s.item().end(), g3.item());

    auto av = s.item().available_range();
    EXPECT_FALSE(av);

    // we've added gaps so we need to refresh our state.
    auto ru4 = s.item().refresh(1);
    // this will modify our availiable range. So we capture that.
    // Or should it be part of the insertion?
    EXPECT_TRUE(s.item().available_range());
    EXPECT_NE(av, s.item().available_range());

    s.item().undo(ru4);
    s.item().undo(ru3);
    s.item().undo(ru2);
    s.item().undo(ru1);

    // we should be back to original state..
    EXPECT_FALSE(s.item().available_range());
}

TEST(StackResolveTest, Test) {
    Stack s;

    auto id1 = FrameRange(
        utility::FrameRateDuration(0, timebase::k_flicks_24fps),
        utility::FrameRateDuration(10, timebase::k_flicks_24fps));
    auto i1 = Clip("Clip1");
    i1.item().set_available_range(id1);

    auto id2 = FrameRange(
        utility::FrameRateDuration(0, timebase::k_flicks_24fps),
        utility::FrameRateDuration(15, timebase::k_flicks_24fps));
    auto i2 = Clip("Clip2");
    i2.item().set_available_range(id2);

    auto id3 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto i3  = Gap("Gap1", id3);

    s.item().insert(s.item().end(), i1.item());
    s.item().insert(s.item().end(), i2.item());
    s.item().insert(s.item().end(), i3.item());

    // we've added gaps so we need to refresh our state.
    s.item().refresh(1);

    auto r = s.item().resolve_time(timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE);
    EXPECT_TRUE(r);
    if (r) {
        auto [i, t] = *r;
        EXPECT_EQ(i.uuid(), i1.item().uuid());
    }


    auto focus = utility::UuidSet({i2.item().uuid()});

    // focus test
    r = s.item().resolve_time(timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, focus);
    EXPECT_TRUE(r);
    if (r) {
        auto [i, t] = *r;
        EXPECT_EQ(i.uuid(), i2.item().uuid());
    }

    // return normally if no focus match
    focus = utility::UuidSet({utility::Uuid::generate()});
    r = s.item().resolve_time(timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, focus);
    EXPECT_TRUE(r);
    if (r) {
        auto [i, t] = *r;
        EXPECT_EQ(i.uuid(), i1.item().uuid());
    }


    // test with focus not matching
    auto rr = s.item().resolve_time_raw(
        timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, focus);
    EXPECT_FALSE(rr.empty());
    EXPECT_EQ(rr.size(), 2);

    EXPECT_EQ(std::get<0>(rr[0]).uuid(), i1.item().uuid());
    EXPECT_EQ(std::get<0>(rr[1]).uuid(), i2.item().uuid());

    // test with focus matching
    focus = utility::UuidSet({i2.item().uuid()});
    rr    = s.item().resolve_time_raw(
        timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, focus);
    EXPECT_FALSE(rr.empty());
    EXPECT_EQ(rr.size(), 1);

    EXPECT_EQ(std::get<0>(rr[0]).uuid(), i2.item().uuid());

    // test with range matching one even with focus matching, but it's off the end
    focus = utility::UuidSet({i1.item().uuid()});
    rr    = s.item().resolve_time_raw(
        timebase::k_flicks_24fps * 14, media::MediaType::MT_IMAGE, focus);
    EXPECT_FALSE(rr.empty());
    EXPECT_EQ(rr.size(), 1);
    EXPECT_EQ(std::get<0>(rr[0]).uuid(), i2.item().uuid());
}

TEST(StackTrackResolveTest, Test) {
    Stack s;

    Track tv1("Track V1", MediaType::MT_IMAGE);
    Track tv2("Track V2", MediaType::MT_IMAGE);
    Track tv3("Track V3", MediaType::MT_IMAGE);

    Track ta1("Track A1", MediaType::MT_AUDIO);
    Track ta2("Track A2", MediaType::MT_AUDIO);
    Track ta3("Track A3", MediaType::MT_AUDIO);

    auto cd1 = FrameRange(
        utility::FrameRateDuration(0, timebase::k_flicks_24fps),
        utility::FrameRateDuration(10, timebase::k_flicks_24fps));
    auto c1 = Clip("Clip1");
    c1.item().set_available_range(cd1);

    auto c3 = Clip("Clip3");
    c3.item().set_available_range(cd1);

    auto c4 = Clip("Clip4");
    c4.item().set_available_range(cd1);

    auto cd2 = FrameRange(
        utility::FrameRateDuration(0, timebase::k_flicks_24fps),
        utility::FrameRateDuration(15, timebase::k_flicks_24fps));
    auto c2 = Clip("Clip2");
    c2.item().set_available_range(cd2);

    auto gd1 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto g1  = Gap("Gap1", gd1);
    auto g2  = Gap("Gap1", gd1);


    tv1.item().insert(tv1.item().end(), c1.item());
    tv1.item().insert(tv1.item().end(), g1.item());
    tv1.item().insert(tv1.item().end(), c2.item());
    tv1.item().refresh(1);

    tv2.item().insert(tv2.item().end(), c3.item());
    tv2.item().insert(tv2.item().end(), g2.item());
    tv2.item().insert(tv2.item().end(), c4.item());
    tv2.item().refresh(1);

    tv3.item().refresh(1);

    ta1.item().refresh(1);
    ta2.item().insert(ta2.item().end(), c1.item());
    ta2.item().insert(ta2.item().end(), g1.item());
    ta2.item().insert(ta2.item().end(), c2.item());
    ta2.item().refresh(1);
    ta3.item().refresh(1);

    s.item().insert(s.item().end(), tv1.item());
    s.item().insert(s.item().end(), tv2.item());
    s.item().insert(s.item().end(), tv3.item());
    s.item().insert(s.item().end(), ta1.item());
    s.item().insert(s.item().end(), ta2.item());
    s.item().insert(s.item().end(), ta3.item());
    s.item().refresh();

    auto rr =
        s.item().resolve_time_raw(timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE);
    EXPECT_FALSE(rr.empty());
    EXPECT_EQ(rr.size(), 2);

    rr = s.item().resolve_time_raw(timebase::k_flicks_24fps * 0, media::MediaType::MT_AUDIO);
    EXPECT_FALSE(rr.empty());
    EXPECT_EQ(rr.size(), 1);

    auto r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, utility::UuidSet({}), true);
    EXPECT_FALSE(r);

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c1.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c1.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 11,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c1.uuid()}),
        true);
    EXPECT_FALSE(r);

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({tv1.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c1.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 11,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({tv1.uuid()}),
        true);
    EXPECT_FALSE(r);

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({tv2.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c3.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({tv2.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c3.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c3.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c3.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c4.uuid()}),
        true);
    EXPECT_FALSE(r);
    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 1,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c3.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c3.uuid());

    r = s.item().resolve_time(
        timebase::k_flicks_24fps * 36,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({c4.uuid()}),
        true);
    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).uuid(), c4.uuid());
}
