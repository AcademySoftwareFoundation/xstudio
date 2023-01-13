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
}
