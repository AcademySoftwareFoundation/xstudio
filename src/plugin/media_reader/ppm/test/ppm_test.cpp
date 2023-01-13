// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "ppm.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"


using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;

ACTOR_TEST_MINIMAL()

TEST(PPMMediaReaderTest, Test) {
    PPMMediaReader ppm;
    caf::uri good = posix_path_to_uri(TEST_RESOURCE "/media/test.0001.ppm");

    EXPECT_EQ(ppm.supported(good, get_signature(good)), MRC_FULLY) << "Should be supported";
    EXPECT_EQ(
        ppm.supported(
            posix_path_to_uri(TEST_RESOURCE "/media/test.0001."), get_signature(good)),
        MRC_FULLY)
        << "Should be supported";
    EXPECT_EQ(ppm.supported(good, std::array<uint8_t, 16>{'P', '7', '\n'}), MRC_NO)
        << "Should not be supported";


    EXPECT_TRUE(ppm.image(media::AVFrameID(good))) << "Should be supported";
}
