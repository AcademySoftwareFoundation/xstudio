// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/utility/timecode.hpp"

using namespace xstudio::utility;

TEST(TimecodeTest, Constructor_Default) {
    Timecode t;
    Timecode s(0, 0, 0, 0, 30, false);
    ASSERT_EQ(t, s);
}

TEST(TimecodeTest, Constructor_String) {
    Timecode t("11;12;13;14", 29.97, true);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());

    Timecode u("11:12:13:14", 29.97, false);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());

    Timecode v("11.12.13.14", 29.97, false);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());

    Timecode w("11-12-13-14", 29.97, false);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());

    ASSERT_THROW(Timecode x("01000000", 30.0, false), std::invalid_argument);
    ASSERT_THROW(Timecode x("01:000:000", 30.0, false), std::invalid_argument);
    ASSERT_THROW(Timecode x("90", 30.0, false), std::invalid_argument);
}

// Converts frames to TC in constructor
TEST(TimecodeTest, Constructor_Int) {
    Timecode t(1208794, 29.97, true);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());
}

TEST(TimecodeTest, Constructor_IntFields) {
    Timecode t(11, 12, 13, 14, 29.97, true);
    ASSERT_EQ(unsigned(11), t.hours());
    ASSERT_EQ(unsigned(12), t.minutes());
    ASSERT_EQ(unsigned(13), t.seconds());
    ASSERT_EQ(unsigned(14), t.frames());
    ASSERT_EQ(t.total_frames(), unsigned(1208794));
}

// Converts TC to frames
TEST(TimecodeTest, Function_total_frames) {
    Timecode t("11:12:13;14", 29.97, true);
    ASSERT_EQ(unsigned(1208794), t.total_frames());
}

// Returns C-String with correct setSeparator
TEST(TimecodeTest, Function_to_string) {
    Timecode t("11:12:13:14", 29.97, false);
    Timecode u("11:12:13;14", 29.97, true);
    ASSERT_EQ("11:12:13:14", t.to_string());
    ASSERT_EQ("11:12:13;14", u.to_string());
}

// Cast to int
TEST(TimecodeTest, Operator_Int) {
    const int f = 1208794;
    Timecode t(f, 29.97, false);
    int i(f);
    ASSERT_EQ(f, i);
}

TEST(TimecodeTest, Operator_Relational) {
    Timecode t("11:12:13;14", 29.97, false);
    Timecode u("11:12:13;14", 29.97, false);
    ASSERT_EQ(u, t);
    ASSERT_GE(u, t);
    ASSERT_LE(u, t);
    u.minutes(13);
    ASSERT_NE(u, t);
    ASSERT_GT(u, t);
    ASSERT_LT(t, u);
}

TEST(TimecodeTest, Operator_Assignment) {
    Timecode t("11:12:13;14", 29.97, false);
    Timecode s;
    Timecode u = s = t;
    ASSERT_EQ(u, t);
    ASSERT_EQ(u, s);
    ASSERT_EQ(s, t);
}

TEST(TimecodeTest, Plus) {
    Timecode t("11:00:00:00", 29.97, true);
    Timecode u("00:12:13:14", 29.97, true);
    Timecode v("11:12:13:14", 29.97, true);

    Timecode w = t + u;
    ASSERT_EQ(v.total_frames(), w.total_frames());

    Timecode x("01:00:00:00", 29.97, true);
    Timecode y("01:00:00:15", 29.97, true);
    Timecode z = x + 15;
    ASSERT_EQ(z.total_frames(), y.total_frames());

    Timecode a("01:00:00:00", 29.97, true);
    Timecode b("00:01:00:00", 29.97, false);
    Timecode c = a + b;

    Timecode d("01:01:00:02", 29.97, true);
    ASSERT_EQ(c.total_frames(), d.total_frames());
    a = Timecode("23:59:59:29", 30.0, false);
    b = a + 1;
    c = Timecode("00:00:00:00", 30.0, false);
    ASSERT_EQ(b.total_frames(), c.total_frames());
}

TEST(TimecodeTest, Multiply) {
    Timecode t("03:00:00:00", 24, false);
    Timecode u("06:00:00:00", 24, false);
    Timecode v = t * 2;
    ASSERT_EQ(u, v);
}

TEST(TimecodeTest, InvalidTimecodes_RollOver) {
    ASSERT_EQ(Timecode("24:00:00:00", 30.0, false), Timecode("00:00:00:00", 30.0, false));
    ASSERT_EQ(Timecode("01:60:00:00", 30.0, false), Timecode("02:00:00:00", 30.0, false));
    ASSERT_EQ(Timecode("01:00:60:00", 30.0, false), Timecode("01:01:00:00", 30.0, false));
    ASSERT_EQ(Timecode("01:00:00:31", 30.0, false), Timecode("01:00:01:01", 30.0, false));
    ASSERT_EQ(Timecode("01:05:00:00", 29.97, true), Timecode("01:05:00:02", 29.97, true));
}
