// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/frame_range.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(FrameRangeIntersect, Test) {
    auto fr1 = FrameRange(
        24 * timebase::k_flicks_24fps, 24 * timebase::k_flicks_24fps, timebase::k_flicks_24fps);

    // auto fr2 = FrameRange();

    EXPECT_EQ(fr1.intersect(fr1), fr1);

    // no overlap front
    EXPECT_EQ(
        fr1.intersect(FrameRange(
                          timebase::k_flicks_zero_seconds,
                          24 * timebase::k_flicks_24fps,
                          timebase::k_flicks_24fps))
            .duration(),
        timebase::k_flicks_zero_seconds);

    // no overlap end
    EXPECT_EQ(
        fr1.intersect(FrameRange(
                          48 * timebase::k_flicks_24fps,
                          24 * timebase::k_flicks_24fps,
                          timebase::k_flicks_24fps))
            .duration(),
        timebase::k_flicks_zero_seconds);


    // one frame
    EXPECT_EQ(
        fr1.intersect(FrameRange(
                          timebase::k_flicks_zero_seconds,
                          25 * timebase::k_flicks_24fps,
                          timebase::k_flicks_24fps))
            .duration(),
        timebase::k_flicks_24fps);

    // one frame
    EXPECT_EQ(
        fr1.intersect(FrameRange(
                          timebase::k_flicks_zero_seconds,
                          25 * timebase::k_flicks_24fps,
                          timebase::k_flicks_24fps))
            .start(),
        24 * timebase::k_flicks_24fps);

    // check duration truncation.

    EXPECT_EQ(
        fr1.intersect(FrameRange(
            timebase::k_flicks_zero_seconds,
            48 * timebase::k_flicks_24fps,
            timebase::k_flicks_24fps)),
        fr1);

    EXPECT_EQ(
        fr1.intersect(FrameRange(
            timebase::k_flicks_zero_seconds,
            49 * timebase::k_flicks_24fps,
            timebase::k_flicks_24fps)),
        fr1);

    auto avail = FrameRange(
        24 * timebase::k_flicks_24fps, 24 * timebase::k_flicks_24fps, timebase::k_flicks_24fps);
    auto active = FrameRange(
        0 * timebase::k_flicks_24fps, 48 * timebase::k_flicks_24fps, timebase::k_flicks_24fps);

    EXPECT_FALSE(avail.intersect(active) == active);

    auto fr_actjvn = FrameRange();
    from_json(
        R"({
        "duration": 3057600000,
        "rate": 29400000,
        "start": 30723000000
    })"_json,
        fr_actjvn);

    auto fr_avajvn = FrameRange();
    from_json(
        R"({
        "duration": 3057600000,
        "rate": 29400000,
        "start": 30723000000
    })"_json,
        fr_actjvn);

    EXPECT_FALSE(fr_avajvn.intersect(fr_actjvn) == fr_actjvn);
}

TEST(FrameRateTest, Test) {

    FrameRate ri24(24);
    EXPECT_EQ(ri24.to_seconds(), 24.0);

    FrameRate rs24(24.0);
    EXPECT_EQ(rs24.to_seconds(), 24.0);

    FrameRate r24(timebase::k_flicks_one_twenty_fourth_of_second);

    EXPECT_EQ(r24.to_microseconds().count(), std::chrono::microseconds(41666).count());


    EXPECT_EQ((FrameRate(1.0) / r24), 24);


    FrameRate r0;
    EXPECT_EQ(r0.to_fps(), 0.0);
    EXPECT_EQ(to_string(r0), "0.000000");
}

TEST(FrameRateDurationTest, Test) {
    FrameRateDuration r24(24, 24.0);
    FrameRateDuration r0;

    EXPECT_EQ(to_string(r0), "0.000000 0");


    EXPECT_EQ(FrameRateDuration(24, 24).frames(), 24);
    EXPECT_EQ(FrameRateDuration(24, 24).seconds(), 1.0);

    EXPECT_EQ(r24.frames(), 24);
    EXPECT_EQ(r24.seconds(), 1.0);
    r24.step();
    EXPECT_EQ(r24.frames(), 25);


    r24.set_seconds(2.0);
    EXPECT_EQ(r24.seconds(), 2.0);
    EXPECT_EQ(r24.frames(), 48);

    r24.set_frames(24 * 4);
    EXPECT_EQ(r24.seconds(), 4.0);
    EXPECT_EQ(r24.frames(), 24 * 4);
    EXPECT_EQ(r24.seconds(FrameRate(static_cast<double>(1.0 / 12.0))), 8.0);

    EXPECT_EQ(r24.frame(FrameRateDuration(23, 24.0)), 23);
    EXPECT_EQ(r24.frame(FrameRateDuration(0, 24.0)), 0);
    EXPECT_EQ(r24.frame(FrameRateDuration(23, 48.0)), 12);

    EXPECT_EQ(r24.frame(FrameRateDuration(0, 24.0)), 0);
    EXPECT_EQ(r24.frame(FrameRateDuration(6, 12.0)), 12);
    EXPECT_EQ(r24.frame(FrameRateDuration(6, 12.0), false), 6);

    FrameRateDuration r25(25, 25);

    EXPECT_EQ(r25.seconds(), 1.0);
    EXPECT_EQ(r25.frames(), 25);


    FrameRateDuration r24_1(24, 24);

    EXPECT_EQ((r24 - r24_1).seconds(), 3.0);

    r24_1.step();
    EXPECT_EQ(r24_1.frames(), 25);
    r24_1.step(true, 2.0f);
    EXPECT_EQ(r24_1.frames(), 27);
    r24_1.step(true, 0.4f);
    EXPECT_EQ(r24_1.frames(), 27);
    r24_1.step(true, 0.5f);
    EXPECT_EQ(r24_1.frames(), 27);

    r24_1.step(false);
    EXPECT_EQ(r24_1.frames(), 26);
}

// constexpr timebase::flicks k_flicks_one_twelve_of_second{
//     std::chrono::duration_cast<timebase::flicks>(
//         std::chrono::duration<timebase::flicks::rep, std::ratio<1, 12>>{1})};

TEST(FrameRateDurationTest2, Test) {
    FrameRateDuration r24(24, 24.0);
    FrameRateDuration r12(12, 24.0);
    FrameRateDuration r12_12(12, 12.0);
    FrameRateDuration r13_12(13, 12.0);

    EXPECT_EQ(r24.subtract_frames(r24).frames(), 0);
    EXPECT_EQ(r24.subtract_frames(r12, false).frames(), 12);
    EXPECT_EQ(r24.subtract_frames(r12).frames(), 12);

    EXPECT_EQ(r24.subtract_frames(r12_12).frames(), 12);
    EXPECT_EQ(r24.subtract_frames(r12_12, false).frames(), 0);

    EXPECT_EQ(r24.subtract_frames(r13_12).frames(), 11);

    EXPECT_EQ(r24.subtract_seconds(r12_12).frames(), 0);
    EXPECT_EQ(r24.subtract_seconds(r12_12, false).frames(), 12);

    FrameRateDuration r24_s(1.0f, timebase::k_flicks_one_twenty_fourth_of_second);
    EXPECT_EQ(r24.frames(), 24);

    for (auto i = 0; i < 60; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 24.0), false), i);
    }

    for (auto i = 0; i < 60; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 24.0)), i);
    }

    for (auto i = 0; i < 24; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 12.0), false), i) << i;
    }

    EXPECT_EQ(r24.frame(FrameRateDuration(0, 12.0), true), 0);
    EXPECT_EQ(r24.frame(FrameRateDuration(1, 12.0), true), 2);
    EXPECT_EQ(r24.frame(FrameRateDuration(2, 12.0), true), 4);

    for (auto i = 0; i < 24; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 12.0), true), i * 2) << i;
    }

    for (auto i = 0; i < 24; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 48.0), false), i) << i;
    }

    for (auto i = 0; i < 24; i++) {
        EXPECT_EQ(
            r24.frame(FrameRateDuration(i, 48.0), true),
            std::round(static_cast<float>(i) / 2.0))
            << i;
    }
}

TEST(FrameRateDurationTest3, Test) {
    FrameRateDuration r24(48, 24.0);
    FrameRateDuration r12(12, 24.0);

    for (auto i = 0; i < 48; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 24.0), true), i) << i;
    }
    for (auto i = 0; i < 48; i++) {
        EXPECT_EQ(r24.frame(FrameRateDuration(i, 24.0), false), i) << i;
    }
}

// TEST(FrameRangeTest1, Test) {
//     auto a = FrameRange();

//     EXPECT_EQ(a.start().seconds(), 0.0);
//     EXPECT_EQ(a.duration().seconds(), 0.0);

// }

