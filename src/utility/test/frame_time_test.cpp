// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/frame_range.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

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

TEST(EditListTest1, Test) {
    FrameRateDuration r12_24(12l, 24.0);
    FrameRateDuration r6_12(6l, 12.0);
    FrameRateDuration r24_48(24l, 48.0);
    FrameRate r24(timebase::k_flicks_24fps);

    EditList edit_list(
        {EditListSection(Uuid(), r12_24, Timecode()),
         EditListSection(Uuid(), r6_12, Timecode()),
         EditListSection(Uuid(), r24_48, Timecode())});

    EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::FIXED, r24), unsigned(42));
    EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::DYNAMIC, r24), unsigned(42));
    EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::REMAPPED, r24), unsigned(36));

    EXPECT_EQ(edit_list.duration_seconds(TimeSourceMode::FIXED, r24), 42.0 / 24.0);
    EXPECT_EQ(edit_list.duration_seconds(TimeSourceMode::DYNAMIC, r24), 1.5f);
    EXPECT_EQ(edit_list.duration_seconds(TimeSourceMode::REMAPPED, r24), 1.5f);

    EXPECT_EQ(
        timebase::to_seconds(edit_list.duration_flicks(TimeSourceMode::FIXED, r24)),
        42.0 / 24.0);
    EXPECT_EQ(
        timebase::to_seconds(edit_list.duration_flicks(TimeSourceMode::DYNAMIC, r24)), 1.5f);
    EXPECT_EQ(
        timebase::to_seconds(edit_list.duration_flicks(TimeSourceMode::REMAPPED, r24)), 1.5f);
}

TEST(EditListTest2, Test) {
    FrameRateDuration r30_30(30l, 30.0);
    FrameRateDuration r30_60(30l, 60.0);
    FrameRateDuration r30_15(30l, 15.0);
    FrameRate r24(timebase::k_flicks_24fps);

    EditList edit_list(
        {EditListSection(Uuid(), r30_30, Timecode()),
         EditListSection(Uuid(), r30_60, Timecode()),
         EditListSection(Uuid(), r30_15, Timecode())});

    FrameRate fr30(timebase::k_flicks_one_thirtieth_second);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(0, 24.0)), 0);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(1, 24.0)), 1);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(29, 24.0)),
        29);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(30, 24.0)),
        30);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(31, 24.0)),
        32);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(32, 24.0)),
        34);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(45, 24.0)),
        60);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(46, 24.0)),
        61);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(47, 24.0)),
        61);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30, FrameRateDuration(104, 24.0)),
        89);
}

TEST(EditListTest3, Test) {
    FrameRateDuration r30_30(30l, 30.0);
    FrameRateDuration r30_60(30l, 60.0);
    FrameRateDuration r30_15(30l, 15.0);
    FrameRate r24(timebase::k_flicks_24fps);

    EditList edit_list(
        {EditListSection(Uuid(), r30_30, Timecode()),
         EditListSection(Uuid(), r30_60, Timecode()),
         EditListSection(Uuid(), r30_15, Timecode())});

    FrameRate fr15(timebase::k_flicks_one_fifteenth_second);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(0, 24.0)), 0);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(1, 24.0)), 2);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(14, 24.0)),
        28);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(15, 24.0)),
        30);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(16, 24.0)),
        34);
    // EXPECT_EQ(edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15,
    // FrameRateDuration(16, 24.0)), 34);
    EXPECT_EQ(edit_list.duration_frames(TimeSourceMode::REMAPPED, fr15), unsigned(52));
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, fr15, FrameRateDuration(51, 24.0)),
        89);
}

/*TEST(EditListTest4, Test) {
    FrameRateDuration r30_30(static_cast<int>(30), timebase::k_flicks_one_thirtieth_second);
    FrameRateDuration r30_60(static_cast<int>(30), timebase::k_flicks_one_sixtieth_second);
    FrameRateDuration r30_15(static_cast<int>(30), timebase::k_flicks_one_fifteenth_second);
    FrameRate r24(timebase::k_flicks_24fps);

    EditList edit_list(
        {EditListSection(Uuid(), r30_30, Timecode()),
         EditListSection(Uuid(), r30_60, Timecode()),
         EditListSection(Uuid(), r30_15, Timecode())});

    FrameRate fr15(timebase::k_flicks_one_fifteenth_second);

    EXPECT_EQ(
        edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 30).frames(),
        FrameRateDuration(static_cast<int>(30), r24).frames());

    // EXPECT_EQ(edit_list.logical_frame(TimeSourceMode::REMAPPED, fr30,
    // FrameRateDuration(36, 24.0)), 42);

    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 45).frames(),
    // FrameRateDuration(static_cast<int>(36), r24).frames());
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 30),
    // FrameRateDuration(static_cast<int>(30), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 31).frames(),
    // FrameRateDuration(static_cast<int>(31), r24).frames());
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 32).frames(),
    // FrameRateDuration(static_cast<int>(32), r24).frames());

    // EXPECT_EQ(edit_list.frame(TimeSourceMode::FIXED, fr15, r24, 1),
    // FrameRateDuration(static_cast<int>(1), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::DYNAMIC, fr15, r24, 1),
    // FrameRateDuration(static_cast<int>(1), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr15, r24, 4).seconds(),
    // FrameRateDuration(static_cast<int>(4), r24).seconds());


    // EXPECT_EQ(edit_list.frame(TimeSourceMode::FIXED, fr15, r24, 34),
    // FrameRateDuration(static_cast<int>(34), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::DYNAMIC, fr15, r24, 34),
    // FrameRateDuration(static_cast<int>(34), r24)); EXPECT_EQ(
    // 	edit_list.frame(TimeSourceMode::REMAPPED, fr15, r24, 29).frames(),
    // 	FrameRateDuration(static_cast<int>(15), r24).frames()
    // );

    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 1),
    // FrameRateDuration(static_cast<int>(1), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 30),
    // FrameRateDuration(static_cast<int>(30), r24));
    // EXPECT_EQ(edit_list.frame(TimeSourceMode::REMAPPED, fr30, r24, 31),
    // FrameRateDuration(static_cast<int>(32), r24));
}*/

TEST(EditListTest5, Test) {

    FrameRateDuration r30_24(30l, 24.0);
    FrameRateDuration r30_60(30l, 60.0);
    FrameRateDuration r30_30(30l, 30.0);

    FrameRate fr24(timebase::k_flicks_24fps);
    FrameRate fr30(timebase::k_flicks_one_thirtieth_second);
    FrameRate fr60(timebase::k_flicks_one_sixtieth_second);

    EditList edit_list(
        {EditListSection(Uuid(), r30_24, Timecode()),
         EditListSection(Uuid(), r30_60, Timecode()),
         EditListSection(Uuid(), r30_30, Timecode())});

    FrameRate fr15(timebase::k_flicks_one_fifteenth_second);

    EXPECT_EQ(edit_list.frame_rate_at_frame(15), fr24);
    EXPECT_EQ(edit_list.frame_rate_at_frame(45), fr60);
    EXPECT_EQ(edit_list.frame_rate_at_frame(75), fr30);

    try {

        static_cast<void>(edit_list.frame_rate_at_frame(150));
        FAIL() << "frame_rate_at_frame() should throw an out of frames error\n";

    } catch (...) {
    }

    EXPECT_EQ(edit_list.flicks_from_frame(TimeSourceMode::FIXED, 0, fr24), timebase::flicks(0));
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::REMAPPED, 0, fr24), timebase::flicks(0));
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::DYNAMIC, 0, fr24), timebase::flicks(0));

    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::FIXED, 10, fr24),
        10 * timebase::k_flicks_24fps);
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::REMAPPED, 10, fr24),
        10 * timebase::k_flicks_24fps);
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::DYNAMIC, 10, fr24),
        10 * timebase::k_flicks_24fps);

    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::FIXED, 40, fr24),
        40.0 * timebase::k_flicks_24fps);
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::REMAPPED, 40, fr24),
        40.0 * timebase::k_flicks_24fps);
    EXPECT_EQ(
        edit_list.flicks_from_frame(TimeSourceMode::DYNAMIC, 40, fr24),
        30.0 * timebase::k_flicks_24fps + 10.0 * timebase::k_flicks_one_sixtieth_second);

    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::FIXED, 10 * timebase::k_flicks_24fps, fr24),
        10);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::REMAPPED, 10 * timebase::k_flicks_24fps, fr24),
        10);
    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::DYNAMIC, 10 * timebase::k_flicks_24fps, fr24),
        10);

    EXPECT_EQ(
        edit_list.logical_frame(TimeSourceMode::FIXED, 40 * timebase::k_flicks_24fps, fr24),
        40);
    EXPECT_EQ(
        edit_list.logical_frame(
            TimeSourceMode::REMAPPED,
            30 * timebase::k_flicks_24fps + 10 * timebase::k_flicks_24fps,
            fr24),
        55); // 10 frames @ 24fps = (60/24)*10 @ 60fps = 25 @ 60fps
    EXPECT_EQ(
        edit_list.logical_frame(
            TimeSourceMode::DYNAMIC,
            30 * timebase::k_flicks_24fps + 10 * timebase::k_flicks_one_sixtieth_second,
            fr24),
        40);

    try {

        static_cast<void>(edit_list.logical_frame(
            TimeSourceMode::DYNAMIC,
            30 * timebase::k_flicks_24fps + 30 * timebase::k_flicks_one_sixtieth_second +
                30 * timebase::k_flicks_one_thirtieth_second +
                15 * timebase::k_flicks_one_thirtieth_second,
            fr24));
        FAIL() << "logical_frame() should throw an out of frames error\n";

    } catch (...) {
    }

    auto frames_max = std::numeric_limits<int>::max();
    auto frames_min = std::numeric_limits<int>::min();

    timebase::flicks step_period(0);
    EXPECT_EQ(
        edit_list.step(
            TimeSourceMode::FIXED, fr24, true, 1.0, 10, frames_min, frames_max, step_period),
        11);
    EXPECT_EQ(step_period, timebase::k_flicks_24fps);

    // step from last frame back to first
    step_period = timebase::flicks(0);
    EXPECT_EQ(
        edit_list.step(
            TimeSourceMode::FIXED, fr24, true, 1.0, 90, frames_min, frames_max, step_period),
        0);
    EXPECT_EQ(step_period, timebase::k_flicks_24fps);

    // 30fps rate remapping should step two frames in 60fps source at frame 40
    step_period = timebase::flicks(0);
    EXPECT_EQ(
        edit_list.step(
            TimeSourceMode::REMAPPED, fr30, true, 1.0, 40, frames_min, frames_max, step_period),
        42);
    EXPECT_EQ(step_period, timebase::k_flicks_one_thirtieth_second);

    //
    step_period = timebase::flicks(0);
    EXPECT_EQ(
        edit_list.step(
            TimeSourceMode::DYNAMIC, fr30, true, 1.0, 40, frames_min, frames_max, step_period),
        41);
    EXPECT_EQ(step_period, timebase::k_flicks_one_sixtieth_second);

    step_period = timebase::flicks(0);
    EXPECT_EQ(
        edit_list.step(
            TimeSourceMode::DYNAMIC, fr30, true, 1.0, 60, frames_min, frames_max, step_period),
        61);
    EXPECT_EQ(step_period, timebase::k_flicks_one_thirtieth_second);
}
