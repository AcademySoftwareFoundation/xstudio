// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/csv.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(CSVTest, EscapeTest) {
    EXPECT_EQ(escape_csv(""), "");
    EXPECT_EQ(escape_csv(","), R"(",")");
    EXPECT_EQ(escape_csv("\n"), "\"\n\"");
    EXPECT_EQ(escape_csv("\""), R"("""")");
}

TEST(CSVTest, RowTest) {
    EXPECT_EQ(to_csv_row({"1"}), "1");
    EXPECT_EQ(to_csv_row({"1"}, 1), "1");
    EXPECT_EQ(to_csv_row({"1"}, 2), "1,");
    EXPECT_EQ(to_csv_row({"1", "2", "3"}, 2), "1,2");
    EXPECT_EQ(to_csv_row({"1", ",", "2"}, 3), "1,\",\",2");
}

TEST(CSVTest, CSV) {
    EXPECT_EQ(to_csv({}), "");
    EXPECT_EQ(to_csv({{}}), "\r\n");
    EXPECT_EQ(to_csv({{""}}), "\r\n");
    EXPECT_EQ(to_csv({{"1"}}), "1\r\n");
    EXPECT_EQ(to_csv({{"1", "2"}}), "1,2\r\n");
    EXPECT_EQ(to_csv({{"1", "2"}, {"1"}}), "1,2\r\n1,\r\n");
    EXPECT_EQ(to_csv({{"1", "2"}, {}, {"1"}}), "1,2\r\n,\r\n1,\r\n");
    EXPECT_EQ(to_csv({{"1"}, {"1", "2"}}), "1\r\n1\r\n");
}