// SPDX-License-Identifier: Apache-2.0

#include "openexr.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include <gtest/gtest.h>

#include <vector>

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

TEST(OpenEXRMediaReaderTest, BlenderMultilayerSinglePart) {
    OpenEXRMediaReader reader;
    const caf::uri uri =
        posix_path_to_uri(TEST_RESOURCE "/media/blender_multilayer.exr");
    media::MediaDetail detail;
    ASSERT_NO_THROW(detail = reader.detail(uri));

    std::vector<std::string> stream_names;
    for (const auto &stream : detail.streams_) {
        stream_names.emplace_back(stream.name_);
    }
    EXPECT_EQ(
        stream_names,
        (std::vector<std::string>{
            "beauty.Combined",
            "beauty.CryptoAsset00",
            "beauty.CryptoAsset01",
            "beauty.CryptoAsset02",
            "beauty.CryptoMaterial00",
            "beauty.CryptoMaterial01",
            "beauty.CryptoMaterial02",
            "beauty.CryptoObject00",
            "beauty.CryptoObject01",
            "beauty.CryptoObject02",
            "beauty.Depth",
            "beauty.Emission",
            "beauty.Noisy Image",
            "volume_high.Combined",
            "volume_high.Noisy Image",
            "volume_low.Combined",
            "volume_low.Noisy Image"}));

    const media::AVFrameID frame(
        uri,
        1,
        1,
        media::FS_ON_DISK,
        0,
        1.0f,
        utility::FrameRate(timebase::k_flicks_24fps),
        "beauty.Combined");
    ImageBufPtr image;
    ASSERT_NO_THROW(image = reader.image(frame));

    ASSERT_TRUE(image);
    EXPECT_EQ(image->shader_params()["num_channels"].get<int>(), 4);
    EXPECT_EQ(
        image->params()["channel_names"].get<std::vector<std::string>>(),
        (std::vector<std::string>{
            "beauty.Combined.R",
            "beauty.Combined.G",
            "beauty.Combined.B",
            "beauty.Combined.A"}));
}
