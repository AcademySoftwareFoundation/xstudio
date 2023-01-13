// SPDX-License-Identifier: Apache-2.0

#include "openexr.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include <gtest/gtest.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;

ACTOR_TEST_MINIMAL()

TEST(OpenEXRMediaReaderTest, Test) {
    OpenEXRMediaReader mr;
    caf::uri good = posix_path_to_uri(TEST_RESOURCE "/media/test.0001.exr");

    EXPECT_EQ(mr.supported(good, get_signature(good)), MRC_FULLY) << "Should be supported";
    EXPECT_EQ(
        mr.supported(posix_path_to_uri(TEST_RESOURCE "/media/test.0001."), get_signature(good)),
        MRC_FULLY)
        << "Should not be supported";

    bool got_image = false;
    try {
        auto image = mr.image(media::AVFrameID(good));
        std::cerr << image->params() << "\n";
        got_image = bool(image);
    } catch (std::exception &e) {
        std::cerr << e.what() << "\n";
        got_image = false;
    }

    EXPECT_TRUE(got_image) << "Should be supported";
}
