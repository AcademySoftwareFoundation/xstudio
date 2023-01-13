// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "blank.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;
ACTOR_TEST_MINIMAL()

TEST(BlankMediaReaderTest, Test) {
    BlankMediaReader bmr;
    caf::uri good = *caf::make_uri("xstudio://blank");

    EXPECT_EQ(bmr.supported(good, get_signature(good)), MRC_FULLY) << "Should be supported";

    EXPECT_TRUE(bmr.image(media::AVFrameID(good))) << "Should be supported";
}
