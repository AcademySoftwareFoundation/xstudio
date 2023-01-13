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