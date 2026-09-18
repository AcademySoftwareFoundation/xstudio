// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/gap.hpp"
#include "xstudio/timeline/clip.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;

TEST(TrackTest, Test) {
    Track t("Track", MediaType::MT_IMAGE);

    EXPECT_EQ(t.item().uuid(), t.uuid());
    EXPECT_EQ(t.item().item_type(), ItemType::IT_VIDEO_TRACK);

    Track t2(t.serialise());
    EXPECT_EQ(t2.item().uuid(), t.uuid());


    t.children().emplace_back(
        Gap("Gap", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());
    t.children().emplace_back(
        Gap("Gap", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());

    EXPECT_EQ(sum_trimmed_duration(t.children()), timebase::k_flicks_24fps * 20);
    t.refresh_item();
    EXPECT_EQ(t.item().trimmed_range().duration(), timebase::k_flicks_24fps * 20);

    auto iaf = t.item().item_at_frame(0);
    EXPECT_TRUE(iaf);
    EXPECT_EQ(iaf->first, t.item().cbegin());

    iaf = t.item().item_at_frame(20);
    EXPECT_FALSE(iaf);

    iaf = t.item().item_at_frame(9);
    EXPECT_TRUE(iaf);
    EXPECT_EQ(iaf->first, t.item().cbegin());
    EXPECT_EQ(iaf->second, 9);

    iaf = t.item().item_at_frame(10);
    EXPECT_TRUE(iaf);
    EXPECT_EQ(iaf->first, std::next(t.item().cbegin(), 1));
    EXPECT_EQ(iaf->second, 0);

    EXPECT_EQ(t.item().frame_at_index(0), t.item().trimmed_frame_start().frames());
    EXPECT_EQ(t.item().frame_at_index(1), t.item().trimmed_frame_start().frames() + 10);
    EXPECT_EQ(t.item().frame_at_index(2), t.item().trimmed_frame_start().frames() + 20);
    EXPECT_EQ(t.item().frame_at_index(3), t.item().trimmed_frame_start().frames() + 20);

    EXPECT_EQ(t.item().frame_at_index(0, 1), t.item().trimmed_frame_start().frames() + 1);
}

std::pair<JsonStore, JsonStore> doMoveTest(Track &t, int start, int count, int before) {
    auto sit = std::next(t.item().begin(), start);
    auto eit = std::next(sit, count);
    auto dit = std::next(t.item().begin(), before);

    auto changes = t.item().splice(dit, t.item().children(), sit, eit);
    auto more    = t.item().refresh();

    return std::make_pair(more, changes);
}

#define TESTMOVE(...)                                                                          \
    t.item().undo(more);                                                                       \
    t.item().undo(changes);                                                                    \
                                                                                               \
    EXPECT_EQ((*(t.item().item_at_index(0)))->name(), "Gap 1");                                \
    EXPECT_EQ((*(t.item().item_at_index(1)))->name(), "Gap 2");                                \
    EXPECT_EQ((*(t.item().item_at_index(2)))->name(), "Gap 3");                                \
    EXPECT_EQ((*(t.item().item_at_index(3)))->name(), "Gap 4");


TEST(TrackShapeTest, Test) {
    Track t("Track", MediaType::MT_IMAGE);

    auto gap1 = Gap("Gap 1", utility::FrameRateDuration(10, timebase::k_flicks_24fps));
    auto gap2 = Gap("Gap 2", utility::FrameRateDuration(10, timebase::k_flicks_24fps));

    t.children().push_back(gap1.item());
    t.children().push_back(gap2.item());

    t.refresh_item();

    auto box = t.item().box();
    EXPECT_EQ(box.first.first, 0);
    EXPECT_EQ(box.first.second, 0);

    EXPECT_EQ(box.second.first, 20);
    EXPECT_EQ(box.second.second, 1);

    auto s = Stack("Stack");

    auto sgap1 = Gap("Stack Gap 1", utility::FrameRateDuration(10, timebase::k_flicks_24fps));
    auto sgap2 = Gap("Stack Gap 2", utility::FrameRateDuration(10, timebase::k_flicks_24fps));

    s.children().push_back(sgap1.item());
    s.children().push_back(sgap2.item());
    s.refresh_item();

    t.children().push_back(s.item());
    t.refresh_item();

    box = t.item().box();
    EXPECT_EQ(box.first.first, 0);
    EXPECT_EQ(box.first.second, 0);
    EXPECT_EQ(box.second.first, 30);
    EXPECT_EQ(box.second.second, 2);

    auto cbox = t.item().box(gap1.uuid());
    EXPECT_EQ(cbox->first.first, 0);
    EXPECT_EQ(cbox->first.second, 0);
    EXPECT_EQ(cbox->second.first, 10);
    EXPECT_EQ(cbox->second.second, 1);

    cbox = t.item().box(gap2.uuid());
    EXPECT_EQ(cbox->first.first, 10);
    EXPECT_EQ(cbox->first.second, 0);
    EXPECT_EQ(cbox->second.first, 20);
    EXPECT_EQ(cbox->second.second, 1);

    cbox = t.item().box(sgap1.uuid());
    EXPECT_EQ(cbox->first.first, 20);
    EXPECT_EQ(cbox->first.second, 0);
    EXPECT_EQ(cbox->second.first, 30);
    EXPECT_EQ(cbox->second.second, 1);

    cbox = t.item().box(sgap2.uuid());
    EXPECT_EQ(cbox->first.first, 20);
    EXPECT_EQ(cbox->first.second, 1);
    EXPECT_EQ(cbox->second.first, 30);
    EXPECT_EQ(cbox->second.second, 2);
}


TEST(TrackMoveTest, Test) {
    Track t("Track", MediaType::MT_IMAGE);

    t.children().emplace_back(
        Gap("Gap 1", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());
    t.children().emplace_back(
        Gap("Gap 2", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());
    t.children().emplace_back(
        Gap("Gap 3", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());
    t.children().emplace_back(
        Gap("Gap 4", utility::FrameRateDuration(10, timebase::k_flicks_24fps)).item());

    EXPECT_EQ(t.item().item_at_frame(0)->first->name(), "Gap 1");
    EXPECT_EQ(t.item().item_at_frame(10)->first->name(), "Gap 2");
    EXPECT_EQ(t.item().item_at_frame(20)->first->name(), "Gap 3");
    EXPECT_EQ(t.item().item_at_frame(30)->first->name(), "Gap 4");

    {
        auto [more, changes] = doMoveTest(t, 0, 1, 3);

        EXPECT_EQ((*(t.item().item_at_index(0)))->name(), "Gap 2");
        EXPECT_EQ((*(t.item().item_at_index(1)))->name(), "Gap 3");
        EXPECT_EQ((*(t.item().item_at_index(2)))->name(), "Gap 1");
        EXPECT_EQ((*(t.item().item_at_index(3)))->name(), "Gap 4");

        TESTMOVE()
    }

    {
        auto [more, changes] = doMoveTest(t, 0, 2, 3);

        EXPECT_EQ((*(t.item().item_at_index(0)))->name(), "Gap 3");
        EXPECT_EQ((*(t.item().item_at_index(1)))->name(), "Gap 1");
        EXPECT_EQ((*(t.item().item_at_index(2)))->name(), "Gap 2");
        EXPECT_EQ((*(t.item().item_at_index(3)))->name(), "Gap 4");

        TESTMOVE()
    }

    {
        auto [more, changes] = doMoveTest(t, 1, 1, 0);

        EXPECT_EQ((*(t.item().item_at_index(0)))->name(), "Gap 2");
        EXPECT_EQ((*(t.item().item_at_index(1)))->name(), "Gap 1");
        EXPECT_EQ((*(t.item().item_at_index(2)))->name(), "Gap 3");
        EXPECT_EQ((*(t.item().item_at_index(3)))->name(), "Gap 4");

        TESTMOVE()
    }

    {
        auto [more, changes] = doMoveTest(t, 1, 3, 0);

        // should be 0,3,4
        EXPECT_EQ((*(t.item().item_at_index(0)))->name(), "Gap 2");
        EXPECT_EQ((*(t.item().item_at_index(1)))->name(), "Gap 3");
        EXPECT_EQ((*(t.item().item_at_index(2)))->name(), "Gap 4");
        EXPECT_EQ((*(t.item().item_at_index(3)))->name(), "Gap 1");


        TESTMOVE()
    }
}

TEST(TrackResolveTest, Test) {
    Track t("Track", MediaType::MT_IMAGE);

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

    t.item().insert(t.item().end(), i1.item());
    t.item().insert(t.item().end(), i2.item());
    t.item().insert(t.item().end(), i3.item());

    // we've added gaps so we need to refresh our state.
    t.item().refresh(1);

    // check get clip
    auto r = t.item().resolve_time(timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE);
    EXPECT_TRUE(r);

    // check get clip
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({t.uuid(), i1.uuid()}));
    EXPECT_TRUE(r);

    // check get nothing as GAPS don't count
    r = t.item().resolve_time(timebase::k_flicks_24fps * 26, media::MediaType::MT_IMAGE);
    EXPECT_FALSE(r);

    // neither in focus
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 0, media::MediaType::MT_IMAGE, utility::UuidSet({}), true);
    EXPECT_FALSE(r);

    // track is focused
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({t.uuid()}),
        true);
    EXPECT_TRUE(r);

    //  clip focused
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({i1.uuid()}),
        true);
    EXPECT_TRUE(r);

    //  both focused
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 0,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({t.uuid(), i1.uuid()}),
        true);
    EXPECT_TRUE(r);

    //  not clip focused
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 15,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({i1.uuid()}),
        true);
    EXPECT_FALSE(r);

    //  not clip focused
    r = t.item().resolve_time(
        timebase::k_flicks_24fps * 15,
        media::MediaType::MT_IMAGE,
        utility::UuidSet({i2.uuid()}),
        true);

    EXPECT_TRUE(r);
    EXPECT_EQ(std::get<0>(*r).name(), i2.name());
    EXPECT_EQ(std::get<0>(*r).uuid(), i2.uuid());
}
