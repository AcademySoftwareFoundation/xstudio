// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/timeline/gap.hpp"
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
