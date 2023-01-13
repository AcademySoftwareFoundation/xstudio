// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/timeline/timeline.hpp"
#include "xstudio/timeline/gap.hpp"
#include "xstudio/timeline/clip.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;

using namespace std::chrono_literals;

TEST(TimelineTest, Test) {
    Timeline s;

    EXPECT_EQ(s.item().uuid(), s.uuid());
    EXPECT_EQ(s.item().item_type(), ItemType::IT_TIMELINE);

    EXPECT_TRUE(s.children().empty());
    EXPECT_EQ(find_uuid(s.children(), utility::Uuid()), s.children().cend());

    auto tuuid = Uuid::generate();
    s.children().emplace_back(Item(ItemType::IT_STACK, tuuid));
    EXPECT_FALSE(s.children().empty());
    EXPECT_EQ(find_uuid(s.children(), utility::Uuid()), s.children().cend());
    EXPECT_NE(find_uuid(s.children(), tuuid), s.children().cend());

    EXPECT_EQ(sum_trimmed_duration(s.children()), timebase::k_flicks_zero_seconds);

    Timeline s2(s.serialise());
}

TEST(TimelineTestFull, Test) {
    // start_logger();
    // mirror example from OTIO
    Clip c001("Clip-001");
    // 24fps start's frame 3, duration 3 frames
    c001.item().set_available_range(FrameRange(
        FrameRateDuration(3, timebase::k_flicks_24fps),
        FrameRateDuration(3, timebase::k_flicks_24fps)));

    // check duration
    EXPECT_EQ(c001.item().available_duration(), timebase::k_flicks_24fps * 3);

    Clip c003("Clip-003");
    c003.item().set_available_range(FrameRange(
        FrameRateDuration(100, timebase::k_flicks_24fps),
        FrameRateDuration(9, timebase::k_flicks_24fps)));

    Clip c004("Clip-004");
    c004.item().set_available_range(FrameRange(
        FrameRateDuration(100, timebase::k_flicks_24fps),
        FrameRateDuration(6, timebase::k_flicks_24fps)));

    Clip c005("Clip-005");
    c005.item().set_available_range(FrameRange(
        FrameRateDuration(100, timebase::k_flicks_24fps),
        FrameRateDuration(9, timebase::k_flicks_24fps)));

    Clip c006("Clip-006");
    c006.item().set_available_range(FrameRange(
        FrameRateDuration(3, timebase::k_flicks_24fps),
        FrameRateDuration(3, timebase::k_flicks_24fps)));

    Gap g001("Gap-001", FrameRateDuration(4, timebase::k_flicks_24fps));
    Gap g002("Gap-002", FrameRateDuration(7, timebase::k_flicks_24fps));

    Stack s001("Stack-001");
    Stack s002("Nested Stack-002");

    Track t001("Track-001");
    Track t002("Nested Track-002");
    Track t003("Nested Track-003");

    t003.item().set_active_range(FrameRange(
        FrameRateDuration(1, timebase::k_flicks_24fps),
        FrameRateDuration(10, timebase::k_flicks_24fps)));
    t003.item().set_available_range(FrameRange(
        FrameRateDuration(1, timebase::k_flicks_24fps),
        FrameRateDuration(10, timebase::k_flicks_24fps)));

    t003.children().push_back(c005.item());
    t003.children().push_back(c006.item());
    t003.refresh_item();

    EXPECT_EQ(t003.item().active_duration(), timebase::k_flicks_24fps * 10);
    EXPECT_EQ(t003.item().available_duration(), timebase::k_flicks_24fps * 12);


    t002.children().push_back(g002.item());
    t002.children().push_back(c003.item());
    t002.refresh_item();

    EXPECT_EQ(t002.item().trimmed_duration(), timebase::k_flicks_24fps * 16);
    EXPECT_EQ(*(t002.item().available_duration()), timebase::k_flicks_24fps * 16);

    s002.item().set_active_range(FrameRange(
        FrameRateDuration(2, timebase::k_flicks_24fps),
        FrameRateDuration(6, timebase::k_flicks_24fps)));
    s002.children().push_back(t002.item());
    s002.children().push_back(t003.item());
    s002.refresh_item();

    EXPECT_EQ(s002.item().active_duration(), timebase::k_flicks_24fps * 6);
    EXPECT_EQ(s002.item().available_duration(), timebase::k_flicks_24fps * 16);

    t001.children().push_back(c001.item());
    t001.children().push_back(s002.item());
    t001.children().push_back(g001.item());
    t001.children().push_back(c004.item());
    t001.refresh_item();

    EXPECT_EQ(t001.item().trimmed_duration(), timebase::k_flicks_24fps * 19);
    EXPECT_EQ(t001.item().available_duration(), timebase::k_flicks_24fps * 19);

    s001.children().push_back(t001.item());
    s001.refresh_item();

    EXPECT_EQ(s001.item().trimmed_duration(), timebase::k_flicks_24fps * 19);
    EXPECT_EQ(s001.item().available_duration(), timebase::k_flicks_24fps * 19);

    Timeline s;

    EXPECT_TRUE(s.item().empty());

    s.item().emplace_back(s001.item());
    s.refresh_item();

    EXPECT_EQ(s.item().trimmed_duration(), timebase::k_flicks_24fps * 19);
    EXPECT_EQ(s.item().available_duration(), timebase::k_flicks_24fps * 19);


    // test resolving timeline
    // spdlog::warn("{}", to_string(c001.uuid()));
    // spdlog::warn("{}", to_string(c003.uuid()));
    // spdlog::warn("{}", to_string(c004.uuid()));
    // spdlog::warn("{}", to_string(c005.uuid()));
    // spdlog::warn("{}", to_string(c006.uuid()));
    // spdlog::warn("");

    // test with gap
    EXPECT_FALSE(g001.item().resolve_time(FrameRate()));

    // test mapping from parent to child range.
    {
        auto [i, t] = *(c003.item().resolve_time(timebase::k_flicks_24fps * 0));
        EXPECT_EQ(i.uuid(), c003.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 100);
    }


    // test track 3(offset clips)
    {
        auto [i, t] = *(t003.item().resolve_time(timebase::k_flicks_24fps * 0));
        EXPECT_EQ(i.uuid(), c005.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 101);
    }

    {
        auto rt = t003.item().resolve_time(timebase::k_flicks_24fps * 8);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c006.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 3);
    }

    {
        auto rt = t003.item().resolve_time(timebase::k_flicks_24fps * 10);
        EXPECT_FALSE(rt);
    }

    // test second track
    // leads with gap..
    {
        auto rt = t002.item().resolve_time(timebase::k_flicks_24fps * 0);
        EXPECT_FALSE(rt);
    }

    {
        auto rt = t002.item().resolve_time(timebase::k_flicks_24fps * 7);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c003.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 100);
    }

    {
        auto rt = s002.item().resolve_time(timebase::k_flicks_24fps * 0);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c005.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 103);
    }

    {
        auto rt = s002.item().resolve_time(timebase::k_flicks_24fps * 5);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c003.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 100);
    }

    {
        auto rt = s002.item().resolve_time(timebase::k_flicks_24fps * 6);
        EXPECT_FALSE(rt);
    }
    // std::this_thread::sleep_for(30000ms);

    // spdlog::stopwatch sw;
    // for(auto i=0;i<100000;i++){
    //     for(auto ii=0;ii<300;ii++)
    //         s.item().resolve_time(timebase::k_flicks_24fps*ii);
    // }
    // spdlog::error("elapsed {:.3}", sw);

    // test timeline... FULLTEST!!
    // off the end
    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 19);
        EXPECT_FALSE(rt);
    }

    // check gap..
    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 9);
        EXPECT_FALSE(rt);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 12);
        EXPECT_FALSE(rt);
    }
    // check everything else..

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 0);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c001.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 3);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 0);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c001.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 3);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 3);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c005.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 103);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 8);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c003.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 100);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 13);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c004.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 100);
    }

    {
        auto rt = s.item().resolve_time(timebase::k_flicks_24fps * 18);
        EXPECT_TRUE(rt);
        auto [i, t] = *rt;
        EXPECT_EQ(i.uuid(), c004.item().uuid());
        EXPECT_EQ(t, timebase::k_flicks_24fps * 105);
    }
}
