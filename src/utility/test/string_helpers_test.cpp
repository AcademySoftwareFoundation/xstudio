// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio::utility;

TEST(StringHelpersTest, SnakeCase) {
    EXPECT_EQ(snake_case("a_b_1"), "a_b_1");
    EXPECT_EQ(snake_case("AB1"), "a_b_1");
    EXPECT_EQ(snake_case("A_B1"), "a__b_1");
    EXPECT_EQ(snake_case("AbC1"), "ab_c_1");
    EXPECT_EQ(snake_case("A B 1"), "a_b_1");
    EXPECT_EQ(snake_case("Ab  Bc 1"), "ab_bc_1");
}

TEST(StringHelpersTest, split_vector) {
    std::vector<int> int_vec{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto split1  = split_vector(int_vec, 1);
    auto split2  = split_vector(int_vec, 2);
    auto split3  = split_vector(int_vec, 3);
    auto split10 = split_vector(int_vec, 10);
    auto split11 = split_vector(int_vec, 11);

    EXPECT_EQ(split1.size(), 10);
    EXPECT_EQ(split1[0].size(), 1);
    EXPECT_EQ(split1[9].size(), 1);

    EXPECT_EQ(split2.size(), 5);
    EXPECT_EQ(split2[0].size(), 2);
    EXPECT_EQ(split2[4].size(), 2);
    EXPECT_EQ(split2[0][0], 1);
    EXPECT_EQ(split2[0][1], 2);
    EXPECT_EQ(split2[4][0], 9);
    EXPECT_EQ(split2[4][1], 10);

    EXPECT_EQ(split3.size(), 4);
    EXPECT_EQ(split3[0].size(), 3);
    EXPECT_EQ(split3[3].size(), 1);
    EXPECT_EQ(split3[0][0], 1);
    EXPECT_EQ(split3[0][2], 3);
    EXPECT_EQ(split3[3][0], 10);

    EXPECT_EQ(split10.size(), 1);
    EXPECT_EQ(split10[0].size(), 10);
    EXPECT_EQ(split10[0][0], 1);
    EXPECT_EQ(split10[0][9], 10);

    EXPECT_EQ(split11.size(), 1);
    EXPECT_EQ(split11[0].size(), 10);
    EXPECT_EQ(split11[0][0], 1);
    EXPECT_EQ(split11[0][9], 10);
}