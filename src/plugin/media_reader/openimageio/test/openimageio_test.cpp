// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

#include <OpenImageIO/imageio.h>

#include "openimageio.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;

namespace fs = std::filesystem;

ACTOR_TEST_MINIMAL()

namespace {

// OIIO expands integer data up to the next byte boundary by replicating the high
// bits into the low bits, so to store a given 10 or 12 bit code value we have to
// hand it the equivalent expanded value. See BaseTypeConvertU10ToU16 in OIIO's
// libdpx, and the matching shift in oiio_buffer_pixel_picker.
uint16_t expand_10_to_16(const uint16_t v) { return (uint16_t)((v << 6) | (v >> 4)); }

fs::path temp_media_path(const std::string &filename) {
    return fs::temp_directory_path() / ("xstudio_oiio_probe_test_" + filename);
}

// Write a tiny single colour image, optionally declaring a native bit depth
// smaller than the stored one (as a 10 bit DPX does).
void write_test_image(
    const std::string &path,
    const OIIO::TypeDesc format,
    const std::array<uint16_t, 3> &rgb,
    const int native_bits_per_sample = 0) {

    const int width = 4, height = 4;

    auto out = OIIO::ImageOutput::create(path);
    ASSERT_TRUE(out) << "Could not create ImageOutput for " << path;

    OIIO::ImageSpec spec(width, height, 3, format);
    if (native_bits_per_sample)
        spec.attribute("oiio:BitsPerSample", native_bits_per_sample);

    ASSERT_TRUE(out->open(path, spec)) << out->geterror();

    if (format == OIIO::TypeDesc::UINT8) {
        std::vector<uint8_t> pixels(width * height * 3);
        for (int i = 0; i < width * height; ++i) {
            pixels[i * 3 + 0] = (uint8_t)rgb[0];
            pixels[i * 3 + 1] = (uint8_t)rgb[1];
            pixels[i * 3 + 2] = (uint8_t)rgb[2];
        }
        ASSERT_TRUE(out->write_image(OIIO::TypeDesc::UINT8, pixels.data()));
    } else {
        std::vector<uint16_t> pixels(width * height * 3);
        for (int i = 0; i < width * height; ++i) {
            pixels[i * 3 + 0] = rgb[0];
            pixels[i * 3 + 1] = rgb[1];
            pixels[i * 3 + 2] = rgb[2];
        }
        ASSERT_TRUE(out->write_image(OIIO::TypeDesc::UINT16, pixels.data()));
    }

    out->close();
}

// The reader expects the stream id the rest of xstudio gives it - an empty one
// makes parse_stream_name index off the end of an empty vector.
media::AVFrameID frame_id(const std::string &path) {
    return media::AVFrameID(
        posix_path_to_uri(path),
        0,
        0,
        media::FS_ON_DISK,
        0,
        1.0f,
        utility::FrameRate(timebase::k_flicks_24fps),
        "image");
}

// Read the image back through the reader and probe the pixel at (1,1).
PixelInfo probe_image(const std::string &path) {
    OIIOMediaReader reader;

    auto buf = reader.image(frame_id(path));
    EXPECT_TRUE(buf) << "Reader returned no image for " << path;
    if (!buf)
        return PixelInfo();

    auto picker = reader.pixel_picker_func();
    EXPECT_TRUE(picker != nullptr);

    return picker(*buf, buf->shader_params(), Imath::V2i(1, 1), {});
}

std::vector<int> code_values(const PixelInfo &info) {
    std::vector<int> r;
    for (const auto &c : info.code_values_info())
        r.push_back(c.code_value);
    return r;
}

std::vector<std::string> channel_names(const PixelInfo &info) {
    std::vector<std::string> r;
    for (const auto &c : info.code_values_info())
        r.push_back(c.channel_name);
    return r;
}

} // namespace

// A 10 bit DPX should report its own 10 bit code values, not the 16 bit values
// that OIIO expands them to internally.
TEST(OIIOPixelProbe, TenBitDPXReportsNativeCodeValues) {
    const auto path = temp_media_path("10bit.dpx").string();

    // SMPTE-ish values that are meaningful at 10 bits.
    write_test_image(
        path,
        OIIO::TypeDesc::UINT16,
        {expand_10_to_16(940), expand_10_to_16(64), expand_10_to_16(512)},
        10);

    const auto info = probe_image(path);

    EXPECT_EQ(channel_names(info), (std::vector<std::string>{"R", "G", "B"}));
    EXPECT_EQ(code_values(info), (std::vector<int>{940, 64, 512}));

    // The raw (normalised) values should still be the full range 0-1 values.
    ASSERT_EQ(info.raw_channels_info().size(), 3u);
    EXPECT_NEAR(info.raw_channels_info()[0].channel_value, 940.0f / 1023.0f, 0.001f);

    fs::remove(path);
}

// A genuinely 16 bit file has no reduced bit depth to undo.
TEST(OIIOPixelProbe, SixteenBitDPXReportsFullRangeCodeValues) {
    const auto path = temp_media_path("16bit.dpx").string();

    write_test_image(path, OIIO::TypeDesc::UINT16, {65535, 0, 32768}, 16);

    const auto info = probe_image(path);

    EXPECT_EQ(code_values(info), (std::vector<int>{65535, 0, 32768}));

    fs::remove(path);
}

// PNG doesn't set oiio:BitsPerSample, so the picker should fall back to the
// stored bit depth rather than mangling the values.
TEST(OIIOPixelProbe, SixteenBitPNGReportsFullRangeCodeValues) {
    const auto path = temp_media_path("16bit.png").string();

    write_test_image(path, OIIO::TypeDesc::UINT16, {65535, 1000, 32768});

    const auto info = probe_image(path);

    EXPECT_EQ(code_values(info), (std::vector<int>{65535, 1000, 32768}));

    fs::remove(path);
}

TEST(OIIOPixelProbe, EightBitPNGReportsEightBitCodeValues) {
    const auto path = temp_media_path("8bit.png").string();

    write_test_image(path, OIIO::TypeDesc::UINT8, {235, 16, 128});

    const auto info = probe_image(path);

    EXPECT_EQ(code_values(info), (std::vector<int>{235, 16, 128}));

    ASSERT_EQ(info.raw_channels_info().size(), 3u);
    EXPECT_NEAR(info.raw_channels_info()[0].channel_value, 235.0f / 255.0f, 0.001f);

    fs::remove(path);
}

// The colour pipeline needs at least three raw channels to derive the linear and
// display rows, so an empty picker result shows as N/A in the HUD.
TEST(OIIOPixelProbe, ProvidesThreeRawChannelsForColourPipeline) {
    const auto path = temp_media_path("raw_channels.png").string();

    write_test_image(path, OIIO::TypeDesc::UINT8, {10, 20, 30});

    const auto info = probe_image(path);

    EXPECT_GE(info.raw_channels_info().size(), 3u);

    fs::remove(path);
}

// Out of bounds probes should come back empty rather than reading stray memory.
TEST(OIIOPixelProbe, OutOfBoundsPixelReturnsNoValues) {
    const auto path = temp_media_path("bounds.png").string();

    write_test_image(path, OIIO::TypeDesc::UINT8, {10, 20, 30});

    OIIOMediaReader reader;
    auto buf = reader.image(frame_id(path));
    ASSERT_TRUE(buf);

    auto picker = reader.pixel_picker_func();

    for (const auto &coord :
         {Imath::V2i(-1, 0), Imath::V2i(0, -1), Imath::V2i(4, 0), Imath::V2i(0, 4)}) {
        const auto info = picker(*buf, buf->shader_params(), coord, {});
        EXPECT_TRUE(info.code_values_info().empty())
            << "Expected no code values at " << coord.x << "," << coord.y;
        EXPECT_TRUE(info.raw_channels_info().empty())
            << "Expected no raw values at " << coord.x << "," << coord.y;
    }

    fs::remove(path);
}

