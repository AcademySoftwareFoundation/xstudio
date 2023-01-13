// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/sequence.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(SequenceTest, Test) {}

TEST(PadTest, Test) {
    EXPECT_EQ(pad_spec(pad_size("-1")), "%00d");
    EXPECT_EQ(pad_spec(pad_size("-01")), "%03d");
    EXPECT_EQ(pad_spec(pad_size("01")), "%02d");
    EXPECT_EQ(pad_spec(pad_size("001")), "%03d");
    EXPECT_EQ(pad_spec(pad_size("100")), "%00d");
    EXPECT_EQ(pad_spec(pad_size("1")), "%00d");
}
