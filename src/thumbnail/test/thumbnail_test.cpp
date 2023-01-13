// SPDX-License-Identifier: Apache-2.0
#include "xstudio/thumbnail/thumbnail.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;
using namespace xstudio::thumbnail;

TEST(ThumbnailTest, Test) {
    EXPECT_EQ(ThumbnailKey("wibble", 10).hash_str().size(), sizeof(size_t) * 2);
}
