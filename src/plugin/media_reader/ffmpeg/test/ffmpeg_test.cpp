// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "ffmpeg_decoder.hpp"
#include "ffmpeg.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;
using namespace xstudio::media_reader::ffmpeg;

ACTOR_TEST_MINIMAL()

// TEST(FFMpegMediaReaderTest, Test) {
//     FFMpegMediaReader ffmmr;
//     caf::uri good = posix_path_to_uri(TEST_RESOURCE "/media/test.mov");

//     EXPECT_EQ(ffmmr.supported(good, get_signature(good)), MRC_FULLY) << "Should be
//     supported"; EXPECT_EQ(
//         ffmmr.supported(
//             posix_path_to_uri(TEST_RESOURCE "/media/test.0001."), get_signature(good)),
//         MRC_MAYBE)
//         << "Should be supported";

//     EXPECT_TRUE(ffmmr.image(media::AVFrameID(good, 0))) << "Should be supported";
// }


// TEST(FFMPEGLeaker, TEST) {
//    auto path = "/jobs/UAP/IO/london/incoming/2022_06/2022_06_17/client/"
//                "Graded_Media_for_DNEG_Showreel_Aspera_Package/DNE/EP6/106_acc_0120_GRADED.mov";

//    auto decoder = new FFMpegDecoder(path, 44100, VIDEO_STREAM, "stream 0");

//    ImageBufPtr rt;

//    for (auto i = 0; i < 261; i++) {
//        decoder->decode_video_frame(i, rt);
//        rt.reset();
//    }
//    for (auto i = 0; i < 261; i++) {
//        decoder->decode_video_frame(i, rt);
//        rt.reset();
//    }

//    delete decoder;
//}

// Formats the shader can already unpack need no repacking at all.
TEST(FFMpegShaderFriendlyPixFormat, ShaderSupportedFormatsNeedNoRepack) {
    // These go straight down the planar path, so the helper is never consulted -
    // but a lossless target for them must still be themselves if it ever is.
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_YUV422P), AV_PIX_FMT_YUV422P);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_YUV444P10LE), AV_PIX_FMT_YUV444P10LE);
}

// Packed YUV must be repacked to planar at the same bit depth and subsampling, so
// the original code values survive for the pixel probe.
TEST(FFMpegShaderFriendlyPixFormat, PackedYUVRepacksToPlanarAtSameDepth) {
    // uyvy422 - 8 bit 4:2:2, as written by Resolve's uncompressed 8 bit movies
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_UYVY422), AV_PIX_FMT_YUV422P);
    // v210 - 10 bit 4:2:2
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_YUV422P10LE), AV_PIX_FMT_YUV422P10LE);
}

// v408/uyva is 8 bit 4:4:4 with alpha. It used to have no planar target and fell
// back to 8 bit RGBA, losing the YUV code values and taking sws's BT.601 path.
TEST(FFMpegShaderFriendlyPixFormat, EightBitYUVWithAlphaKeepsAlphaAndDepth) {
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_UYVA), AV_PIX_FMT_YUVA444P);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_YUVA420P), AV_PIX_FMT_YUVA420P);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_YUVA422P), AV_PIX_FMT_YUVA422P);
}

// Promoting 8 bit to a deeper format would rescale the code values, so the target
// for 8 bit input must stay 8 bit.
TEST(FFMpegShaderFriendlyPixFormat, EightBitIsNeverPromotedToDeeperFormat) {
    for (const auto fmt :
         {AV_PIX_FMT_UYVY422, AV_PIX_FMT_UYVA, AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV444P}) {
        const auto dst = shader_friendly_pix_format(fmt);
        ASSERT_NE(dst, AV_PIX_FMT_NONE) << av_get_pix_fmt_name(fmt);
        EXPECT_EQ(av_pix_fmt_desc_get(dst)->comp[0].depth, 8) << av_get_pix_fmt_name(fmt);
    }
}

// Big-endian RGB only needs a byte swap to reach a format the shader handles.
TEST(FFMpegShaderFriendlyPixFormat, BigEndianRGBRepacksToLittleEndian) {
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_RGB48BE), AV_PIX_FMT_RGB48LE);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_RGBA64BE), AV_PIX_FMT_RGBA64LE);
}

// 8 bit RGB is already handled by the interleaved RGB/RGBA shader paths.
TEST(FFMpegShaderFriendlyPixFormat, EightBitRGBHasNoPlanarTarget) {
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_RGB24), AV_PIX_FMT_NONE);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_RGBA), AV_PIX_FMT_NONE);
}

// Formats with no sensible lossless target fall back to a full conversion.
TEST(FFMpegShaderFriendlyPixFormat, UnsupportedFormatsFallBack) {
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_GRAY8), AV_PIX_FMT_NONE);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_PAL8), AV_PIX_FMT_NONE);
    EXPECT_EQ(shader_friendly_pix_format(AV_PIX_FMT_NONE), AV_PIX_FMT_NONE);
}

// sws defaults to BT.601 whatever the stream says, which is visibly wrong for
// BT.709 and BT.2020 media going through the RGBA fallback.
TEST(FFMpegSwsColourspace, MapsStreamColourspaceToCoefficients) {
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_BT709), SWS_CS_ITU709);
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_BT2020_NCL), SWS_CS_BT2020);
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_SMPTE240M), SWS_CS_SMPTE240M);
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_SMPTE170M), SWS_CS_ITU601);
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_BT470BG), SWS_CS_ITU601);
    // Unknown falls back to BT.601, matching sws's own default.
    EXPECT_EQ(sws_colourspace_from_av(AVCOL_SPC_UNSPECIFIED), SWS_CS_ITU601);
}

