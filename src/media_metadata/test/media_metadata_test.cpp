// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_metadata/media_metadata.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include <gtest/gtest.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_metadata;

ACTOR_TEST_MINIMAL()

TEST(MediaMetadataTest, Test) {

    // std::array<uint8_t, 16> sig;
    // caf::uri test_uri1 = utility::posix_path_to_uri(TEST_RESOURCE "/media/test.mov");
    // caf::uri test_uri2 = utility::posix_path_to_uri(TEST_RESOURCE "/media/flibble");
    // caf::uri test_uri3 = utility::posix_path_to_uri(TEST_RESOURCE "/media/test.0001.exr");

    // MediaMetadataManager mmm;

    // mmm.add_path(xstudio_root("/plugin"));

    // // should return NO
    // std::shared_ptr<MediaMetadata> dmm = mmm.supported(test_uri2);
    // EXPECT_FALSE(dmm.get());

    // ASSERT_EQ(mmm.load_plugins(), unsigned(2));
    // EXPECT_EQ(mmm.load_plugins(), unsigned(0));
    // EXPECT_EQ(mmm.names()[0], "FFProbe");

    // std::unique_ptr<MediaMetadata> mm = mmm.construct("FFProbe");
    // ASSERT_NE(mm, nullptr);
    // EXPECT_EQ(mmm.construct("JUNK"), nullptr);

    // EXPECT_EQ(mm->name(), "FFProbe");


    // EXPECT_EQ(mm->supported(test_uri1, sig), MMC_YES);
    // EXPECT_EQ(mm->supported(test_uri2, sig), MMC_MAYBE);

    // EXPECT_THROW(auto i = mm->metadata(test_uri2), std::runtime_error) << "Should be
    // exception";

    // nlohmann::json meta = mm->metadata(test_uri1);

    // EXPECT_EQ(meta["format"]["size"], "95566");

    // dmm = mmm.supported(test_uri1);
    // EXPECT_TRUE(dmm.get());
    // EXPECT_EQ(dmm->name(), "FFProbe");

    // meta = dmm->metadata(test_uri1);
    // EXPECT_EQ(meta["format"]["size"], "95566");

    // meta = mmm.metadata(test_uri1);
    // EXPECT_EQ(meta["format"]["size"], "95566");

    // meta = mmm.metadata(test_uri3);
    // EXPECT_EQ(meta["headers"][0]["capDate"]["value"], "2020:01:21 09:38:59");

    // EXPECT_NO_THROW(auto i = mmm.metadata(test_uri1, "FFProbe")) << "Should work exception";
    // EXPECT_THROW(auto i = mmm.metadata(test_uri1, "OpenEXR"), std::exception)
    //     << "Should throw exception";
    // EXPECT_THROW(auto i = mmm.metadata(test_uri1, "JUNK"), std::runtime_error)
    //     << "Should throw exception";
}
