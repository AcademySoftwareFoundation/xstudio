// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <filesystem>

#include "ffmpeg_decoder.hpp"
#include "ffmpeg.hpp"
#include "vt_repack.hpp"
#ifdef XSTUDIO_HAS_VIDEOTOOLBOX
#include "vt_decoder.hpp"
#endif
#include "xstudio/global_store/global_store.hpp"
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


TEST(FFMpegVideoToolboxMode, ParsesPreferenceValues) {
    EXPECT_EQ(videotoolbox_mode_from_string("off"), VideoToolboxMode::Off);
    EXPECT_EQ(videotoolbox_mode_from_string("prefer"), VideoToolboxMode::Prefer);
    EXPECT_EQ(videotoolbox_mode_from_string("only"), VideoToolboxMode::Only);
    // Anything else gets the default.
    EXPECT_EQ(videotoolbox_mode_from_string(""), VideoToolboxMode::Prefer);
    EXPECT_EQ(videotoolbox_mode_from_string("Only"), VideoToolboxMode::Prefer);

    for (auto mode : {VideoToolboxMode::Off, VideoToolboxMode::Prefer, VideoToolboxMode::Only})
        EXPECT_EQ(videotoolbox_mode_from_string(to_string(mode)), mode);
}

// Media from test/videotoolbox_media/generate.sh. Tests that need it are skipped when
// it hasn't been generated.
static std::string vt_media(const std::string &name) {
    const std::string path = std::string(VT_TEST_MEDIA_DIR) + "/" + name;
    return std::filesystem::exists(path) ? path : std::string();
}

#define VT_MEDIA_OR_SKIP(var, name)                                                            \
    const std::string var = vt_media(name);                                                    \
    if (var.empty())                                                                           \
    GTEST_SKIP() << name << " not found in " VT_TEST_MEDIA_DIR                                 \
                 << " - run test/videotoolbox_media/generate.sh"

TEST(FFMpegVideoToolboxMode, DecoderCarriesMode) {
    VT_MEDIA_OR_SKIP(path, "h264_8bit_bframes.mov");
    for (auto mode :
         {VideoToolboxMode::Off, VideoToolboxMode::Prefer, VideoToolboxMode::Only}) {
        FFMpegDecoder decoder(
            path, 48000, utility::FrameRate(timebase::k_flicks_24fps), "", mode);
        EXPECT_EQ(decoder.videotoolbox_mode(), mode);
    }
}

// Decodes one frame through the full reader path (stream, shader params).
static ImageBufPtr decode_frame(
    const std::string &path,
    const int frame,
    const VideoToolboxMode mode = VideoToolboxMode::Off) {
    // Stream 0 is the video stream in all the test media.
    FFMpegDecoder decoder(
        path, 48000, utility::FrameRate(timebase::k_flicks_24fps), "stream 0", mode);
    ImageBufPtr buf;
    decoder.decode_video_frame(frame, buf);
    return buf;
}

// Y offset the shader subtracts: 0 for full range, 16 (8 bit) for video range.
static int luma_offset(const ImageBufPtr &buf) {
    return buf->shader_params()["yuv_offsets"][2].get<int>();
}

// DNxHD/HR in a .dneg.mov is always treated as video range, whatever it is tagged.
TEST(FFMpegColourRange, DnegMovForcesVideoRangeForDNxHD) {
    VT_MEDIA_OR_SKIP(plain, "dnxhr_fullrange.mov");
    VT_MEDIA_OR_SKIP(dneg, "dnxhr_fullrange.dneg.mov");

    auto plain_buf = decode_frame(plain, 0);
    auto dneg_buf  = decode_frame(dneg, 0);
    ASSERT_TRUE(plain_buf && dneg_buf);
    EXPECT_EQ(luma_offset(plain_buf), 0);
    EXPECT_EQ(luma_offset(dneg_buf), 16);
}

// ---------------------------------------------------------------------------------
// vt_repack: VideoToolbox pixel layouts to planar formats the shader reads.
// Checked against av_read_image_line2 reading the same bytes as the matching
// FFmpeg format, so FFmpeg's own pixel descriptors are the reference.

namespace {

using namespace xstudio::media_reader::ffmpeg::vt_repack;

// A source plane with padded rows, filled with pseudo-random values plus the
// extremes 0 and 0xFFFF (or 0xFF) at the start of every row.
struct TestPlane {
    std::vector<uint8_t> bytes;
    size_t stride;
    TestPlane(const int row_bytes, const int rows, const int bytes_per_sample, uint32_t seed)
        : stride(size_t(row_bytes) + 40) {
        bytes.resize(stride * rows);
        for (auto &b : bytes) {
            seed = seed * 1664525u + 1013904223u;
            b    = uint8_t(seed >> 24);
        }
        for (int r = 0; r < rows; ++r) {
            uint8_t *row = bytes.data() + stride * r;
            std::memset(row, 0, bytes_per_sample);
            std::memset(row + bytes_per_sample, 0xff, bytes_per_sample);
        }
    }
    SrcPlane src() const { return {bytes.data(), stride}; }
};

struct TestImage {
    Layout layout;
    std::vector<uint8_t> bytes;
    TestImage(const AVPixelFormat fmt, const int w, const int h)
        : layout(planar_layout(fmt, w, h)), bytes(layout.total_size) {}
    DstPlane dst(const int i) {
        return {bytes.data() + layout.offsets[i], layout.linesizes[i]};
    }
    // component c at (x, y) via FFmpeg's pixel descriptor
    int value(const int c, const int x, const int y) const {
        const uint8_t *data[4];
        int linesize[4];
        for (int i = 0; i < 4; ++i) {
            data[i]     = bytes.data() + layout.offsets[i];
            linesize[i] = int(layout.linesizes[i]);
        }
        uint16_t v = 0;
        av_read_image_line2(
            &v, data, linesize, av_pix_fmt_desc_get(layout.format), x, y, c, 1, 0, 2);
        return v;
    }
};

// component c at (x, y) of source planes interpreted as FFmpeg format 'fmt'
int reference_value(
    const AVPixelFormat fmt,
    const std::vector<const TestPlane *> &planes,
    const int c,
    const int x,
    const int y) {
    const uint8_t *data[4] = {nullptr, nullptr, nullptr, nullptr};
    int linesize[4]        = {0, 0, 0, 0};
    for (size_t i = 0; i < planes.size(); ++i) {
        data[i]     = planes[i]->bytes.data();
        linesize[i] = int(planes[i]->stride);
    }
    uint16_t v = 0;
    av_read_image_line2(&v, data, linesize, av_pix_fmt_desc_get(fmt), x, y, c, 1, 0, 2);
    return v;
}

} // namespace

TEST(VtRepack, PlanarLayoutIsAlignedAndContiguous) {
    const auto l = planar_layout(AV_PIX_FMT_YUV420P10LE, 1921, 1081);
    ASSERT_EQ(l.format, AV_PIX_FMT_YUV420P10LE);
    ASSERT_EQ(l.planes, 3);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(l.linesizes[i] % 64, 0u);
    EXPECT_GE(l.linesizes[0], 1921u * 2);
    EXPECT_GE(l.linesizes[1], 961u * 2);
    EXPECT_EQ(l.offsets[1], l.linesizes[0] * 1081);
    EXPECT_EQ(l.offsets[2], l.offsets[1] + l.linesizes[1] * 541);
    EXPECT_EQ(l.total_size, l.offsets[2] + l.linesizes[2] * 541);

    // packed formats have no plane layout
    EXPECT_EQ(planar_layout(AV_PIX_FMT_UYVY422, 64, 64).format, AV_PIX_FMT_NONE);
}

// Every VideoToolbox biplanar layout against FFmpeg's description of it: 8 bit
// (nv12/16/24), 10 bit in the top bits (p010/210/410) and 16 bit (p016/216/416,
// rounded to 12 bits).
TEST(VtRepack, BiplanarToPlanarAllChromaFormats) {
    enum Kind { Bits8, Bits10, Bits16 };
    struct Case {
        Kind kind;
        Chroma chroma;
        AVPixelFormat src, dst;
    };
    const Case cases[] = {
        {Bits8, Chroma::C420, AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P},
        {Bits8, Chroma::C422, AV_PIX_FMT_NV16, AV_PIX_FMT_YUV422P},
        {Bits8, Chroma::C444, AV_PIX_FMT_NV24, AV_PIX_FMT_YUV444P},
        {Bits10, Chroma::C420, AV_PIX_FMT_P010LE, AV_PIX_FMT_YUV420P10LE},
        {Bits10, Chroma::C422, AV_PIX_FMT_P210LE, AV_PIX_FMT_YUV422P10LE},
        {Bits10, Chroma::C444, AV_PIX_FMT_P410LE, AV_PIX_FMT_YUV444P10LE},
        {Bits16, Chroma::C420, AV_PIX_FMT_P016LE, AV_PIX_FMT_YUV420P12LE},
        {Bits16, Chroma::C422, AV_PIX_FMT_P216LE, AV_PIX_FMT_YUV422P12LE},
        {Bits16, Chroma::C444, AV_PIX_FMT_P416LE, AV_PIX_FMT_YUV444P12LE},
    };
    for (const auto &cs : cases) {
        for (auto [w, h] : {std::pair{64, 32}, std::pair{33, 17}}) {
            const int bps = cs.kind == Bits8 ? 1 : 2;
            const int cw = chroma_width(w, cs.chroma), ch = chroma_height(h, cs.chroma);
            TestPlane y(w * bps, h, bps, 3), uv(cw * 2 * bps, ch, bps, 4);
            if (cs.kind == Bits10) {
                // VideoToolbox leaves the low 6 bits zero; so does a real P010 encoder.
                for (auto *pl : {&y, &uv})
                    for (size_t i = 0; i + 1 < pl->bytes.size(); i += 2)
                        pl->bytes[i] &= 0xc0;
            }

            TestImage out(cs.dst, w, h);
            switch (cs.kind) {
            case Bits8:
                biplanar8_to_planar8(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            case Bits10:
                biplanar10_to_planar10(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            case Bits16:
                biplanar16_to_planar12(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            }

            auto expected = [&](const int c, const int x, const int yy) {
                const int ref = reference_value(cs.src, {&y, &uv}, c, x, yy);
                return cs.kind == Bits16 ? int(round_16_to_12(uint16_t(ref))) : ref;
            };
            for (int c = 0; c < 3; ++c) {
                const int pw = c ? cw : w, ph = c ? ch : h;
                for (int yy = 0; yy < ph; ++yy)
                    for (int x = 0; x < pw; ++x)
                        ASSERT_EQ(out.value(c, x, yy), expected(c, x, yy))
                            << av_get_pix_fmt_name(cs.src) << " " << w << "x" << h << " c" << c
                            << " " << x << "," << yy;
            }
        }
    }
}

// s4as = p416le (16 bit Y + interleaved CbCr) plus a 16 bit alpha plane.
TEST(VtRepack, S4asToYuva444p12RoundsAndKeepsAlpha) {
    for (auto [w, h] : {std::pair{64, 32}, std::pair{33, 17}}) {
        TestPlane y(w * 2, h, 2, 5), uv(w * 4, h, 2, 6), a(w * 2, h, 2, 7);
        TestImage out(AV_PIX_FMT_YUVA444P12LE, w, h);
        s4as_to_yuva444p12(
            y.src(), uv.src(), a.src(), out.dst(0), out.dst(1), out.dst(2), out.dst(3), w, h);

        for (int yy = 0; yy < h; ++yy)
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 3; ++c) {
                    const int ref = reference_value(AV_PIX_FMT_P416LE, {&y, &uv}, c, x, yy);
                    ASSERT_EQ(out.value(c, x, yy), round_16_to_12(uint16_t(ref)))
                        << "c" << c << " " << x << "," << yy;
                }
                const uint16_t alpha =
                    *reinterpret_cast<const uint16_t *>(a.bytes.data() + a.stride * yy + x * 2);
                ASSERT_EQ(out.value(3, x, yy), round_16_to_12(alpha)) << "A " << x << "," << yy;
            }
    }
    EXPECT_EQ(round_16_to_12(0), 0);
    EXPECT_EQ(round_16_to_12(0xffff), 4095);
    EXPECT_EQ(round_16_to_12(7), 0);
    EXPECT_EQ(round_16_to_12(8), 1);
    EXPECT_EQ(round_16_to_12(0xfff0), 4095);
}

// Not run by default (timing-dependent). Run with --gtest_also_run_disabled_tests.
// Budget (design D5, measured on an M2 Max): at most 3 ms per 4K frame, single
// threaded, and at most 2.5x a plain copy of the same source bytes. swscale, which
// this replaced, took 7.7-9.6 ms.
TEST(VtRepack, DISABLED_BenchmarkAgainstMemcpy4K) {
    const int w = 3840, h = 2160, reps = 30;
    using clock  = std::chrono::steady_clock;
    auto time_ms = [&](auto &&fn) {
        fn(); // warm up
        const auto t0 = clock::now();
        for (int i = 0; i < reps; ++i)
            fn();
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count() / reps;
    };
    auto memcpy_ms = [&](const std::vector<const TestPlane *> &planes) {
        size_t total = 0;
        for (auto *p : planes)
            total += p->bytes.size();
        std::vector<uint8_t> dst(total);
        return time_ms([&]() {
            size_t off = 0;
            for (auto *p : planes) {
                std::memcpy(dst.data() + off, p->bytes.data(), p->bytes.size());
                off += p->bytes.size();
            }
        });
    };
    auto report = [](const char *name, const double ms, const double copy_ms) {
        std::cout << name << ": " << ms << " ms, memcpy " << copy_ms << " ms, ratio "
                  << ms / copy_ms << "\n";
        EXPECT_LE(ms, 3.0) << name;
        EXPECT_LE(ms / copy_ms, 2.5) << name;
    };

    // every biplanar loop: name, bytes per sample, chroma, output format
    enum Kind { Bits8, Bits10, Bits16 };
    struct Case {
        const char *name;
        Kind kind;
        Chroma chroma;
        AVPixelFormat dst;
    };
    const Case cases[] = {
        {"420v -> yuv420p", Bits8, Chroma::C420, AV_PIX_FMT_YUV420P},
        {"422v -> yuv422p", Bits8, Chroma::C422, AV_PIX_FMT_YUV422P},
        {"444v -> yuv444p", Bits8, Chroma::C444, AV_PIX_FMT_YUV444P},
        {"x420 -> yuv420p10le", Bits10, Chroma::C420, AV_PIX_FMT_YUV420P10LE},
        {"x422 -> yuv422p10le", Bits10, Chroma::C422, AV_PIX_FMT_YUV422P10LE},
        {"x444 -> yuv444p10le", Bits10, Chroma::C444, AV_PIX_FMT_YUV444P10LE},
        {"sv22 -> yuv422p12le", Bits16, Chroma::C422, AV_PIX_FMT_YUV422P12LE},
        {"sv44 -> yuv444p12le", Bits16, Chroma::C444, AV_PIX_FMT_YUV444P12LE},
    };
    for (const auto &cs : cases) {
        const int bps = cs.kind == Bits8 ? 1 : 2;
        const int cw = chroma_width(w, cs.chroma), ch = chroma_height(h, cs.chroma);
        TestPlane y(w * bps, h, bps, 1), uv(cw * 2 * bps, ch, bps, 2);
        TestImage out(cs.dst, w, h);
        const double ms = time_ms([&]() {
            switch (cs.kind) {
            case Bits8:
                biplanar8_to_planar8(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            case Bits10:
                biplanar10_to_planar10(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            case Bits16:
                biplanar16_to_planar12(
                    y.src(), uv.src(), out.dst(0), out.dst(1), out.dst(2), w, h, cs.chroma);
                break;
            }
        });
        report(cs.name, ms, memcpy_ms({&y, &uv}));
    }
    {
        TestPlane y(w * 2, h, 2, 1), uv(w * 4, h, 2, 2), a(w * 2, h, 2, 3);
        TestImage out(AV_PIX_FMT_YUVA444P12LE, w, h);
        const double ms = time_ms([&]() {
            s4as_to_yuva444p12(
                y.src(),
                uv.src(),
                a.src(),
                out.dst(0),
                out.dst(1),
                out.dst(2),
                out.dst(3),
                w,
                h);
        });
        report("s4as -> yuva444p12le", ms, memcpy_ms({&y, &uv, &a}));
    }
}

// ---------------------------------------------------------------------------------
// VtDecoder, driven directly with packets from libavformat.
#ifdef XSTUDIO_HAS_VIDEOTOOLBOX

namespace {

struct Demuxer {
    AVFormatContext *ctx = nullptr;
    AVStream *stream     = nullptr;
    AVPacket *pkt        = av_packet_alloc();
    explicit Demuxer(const std::string &path) {
        if (avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) == 0 &&
            avformat_find_stream_info(ctx, nullptr) >= 0) {
            const int i = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (i >= 0)
                stream = ctx->streams[i];
        }
    }
    ~Demuxer() {
        av_packet_free(&pkt);
        avformat_close_input(&ctx);
    }
    // next video packet, or null at the end
    AVPacket *next() {
        av_packet_unref(pkt);
        while (av_read_frame(ctx, pkt) >= 0) {
            if (pkt->stream_index == stream->index)
                return pkt;
            av_packet_unref(pkt);
        }
        return nullptr;
    }
};

struct DecodeResult {
    std::vector<int64_t> packet_pts;
    std::vector<int64_t> frame_pts;
    int send_errors                   = 0;
    AVPixelFormat format              = AV_PIX_FMT_NONE;
    AVColorSpace colorspace           = AVCOL_SPC_UNSPECIFIED;
    AVColorPrimaries primaries        = AVCOL_PRI_UNSPECIFIED;
    AVColorTransferCharacteristic trc = AVCOL_TRC_UNSPECIFIED;
    AVColorRange range                = AVCOL_RANGE_UNSPECIFIED;
};

// Decodes the whole stream with VideoToolbox, avcodec-style send/receive loop.
DecodeResult vt_decode_all(const std::string &path, const int max_frames = 1 << 30) {
    DecodeResult r;
    Demuxer dm(path);
    std::string error;
    auto vt = VtDecoder::create(dm.stream, error);
    EXPECT_TRUE(vt) << error;
    if (!vt)
        return r;
    AVFrame *frame = av_frame_alloc();
    auto take      = [&]() {
        if (r.frame_pts.empty()) {
            r.format     = AVPixelFormat(frame->format);
            r.colorspace = frame->colorspace;
            r.primaries  = frame->color_primaries;
            r.trc        = frame->color_trc;
            r.range      = frame->color_range;
        }
        r.frame_pts.push_back(frame->pts);
    };
    while (AVPacket *pkt = dm.next()) {
        if (int(r.frame_pts.size()) >= max_frames)
            break;
        r.packet_pts.push_back(pkt->pts);
        if (vt->send_packet(pkt) < 0)
            r.send_errors++;
        while (vt->receive_frame(frame) == 0)
            take();
    }
    vt->send_packet(nullptr);
    while (vt->receive_frame(frame) == 0)
        take();
    av_frame_free(&frame);
    return r;
}

// Same, with FFmpeg's software decoder - the reference.
DecodeResult sw_decode_all(const std::string &path) {
    DecodeResult r;
    Demuxer dm(path);
    const AVCodec *codec = avcodec_find_decoder(dm.stream->codecpar->codec_id);
    AVCodecContext *ctx  = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, dm.stream->codecpar);
    avcodec_open2(ctx, codec, nullptr);
    AVFrame *frame = av_frame_alloc();
    auto take      = [&]() {
        if (r.frame_pts.empty()) {
            r.format     = AVPixelFormat(frame->format);
            r.colorspace = frame->colorspace;
            r.primaries  = frame->color_primaries;
            r.trc        = frame->color_trc;
            r.range      = frame->color_range;
        }
        r.frame_pts.push_back(frame->pts);
    };
    while (AVPacket *pkt = dm.next()) {
        r.packet_pts.push_back(pkt->pts);
        avcodec_send_packet(ctx, pkt);
        while (avcodec_receive_frame(ctx, frame) == 0)
            take();
    }
    avcodec_send_packet(ctx, nullptr);
    while (avcodec_receive_frame(ctx, frame) == 0)
        take();
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    return r;
}

const std::vector<std::pair<std::string, std::string>> k_vt_media_and_format = {
    {"h264_8bit_bframes.mov", "420v"},
    {"hevc_8bit_opengop.mov", "420v"},
    {"hevc_main10_bframes.mov", "x420"},
    {"hevc_422_10bit_bframes.mov", "x422"},
    {"hevc_main10_bt2020_pq.mov", "x420"},
    {"prores_422_proxy.mov", "x422"},
    {"prores_422_lt.mov", "x422"},
    {"prores_422_standard.mov", "x422"},
    {"prores_422_hq.mov", "x422"},
    {"prores_4444_alpha.mov", "s4as"},
    {"prores_4444xq.mov", "s4as"},
    // variants (spike 1.8); the 12 bit ones need FFMPEG_HIGHBIT when generating
    {"variant_h264_10bit_420.mov", "x420"},
    {"variant_h264_8bit_422.mov", "422v"},
    {"variant_h264_10bit_422.mov", "x422"},
    {"variant_h264_8bit_444.mov", "444v"},
    {"variant_h264_10bit_444.mov", "x444"},
    {"variant_hevc_8bit_422.mov", "422v"},
    {"variant_hevc_8bit_444.mov", "444v"},
    {"variant_hevc_10bit_444.mov", "x444"},
    {"variant_hevc_12bit_422.mov", "sv22"},
    {"variant_hevc_12bit_444.mov", "sv44"},
};

// Is the clip there? Variant clips are optional; the core set is required.
bool have_clip(const std::string &name) {
    if (!vt_media(name).empty())
        return true;
    EXPECT_EQ(name.rfind("variant_", 0), 0u) << name << " missing";
    return false;
}

} // namespace

// Task 4.1 / 4.2
TEST(VtDecoder, EligibilityAndOutputFormat) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    for (const auto &[name, expected] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        Demuxer dm(vt_media(name));
        ASSERT_TRUE(dm.stream) << name;
        std::string reason;
        EXPECT_TRUE(VtDecoder::eligible(dm.stream, reason)) << name << ": " << reason;
        auto vt = VtDecoder::create(dm.stream, reason);
        ASSERT_TRUE(vt) << name << ": " << reason;
        EXPECT_EQ(vt->output_format_name(), expected) << name;
    }

    for (const auto *name :
         {"h264_annexb.h264", "dnxhr_fullrange.mov", "variant_hevc_12bit_420.mov"}) {
        if (vt_media(name).empty())
            continue; // the 12 bit clip is optional
        Demuxer dm(vt_media(name));
        ASSERT_TRUE(dm.stream) << name;
        std::string reason;
        EXPECT_FALSE(VtDecoder::eligible(dm.stream, reason)) << name;
        EXPECT_FALSE(reason.empty()) << name;
        EXPECT_FALSE(VtDecoder::create(dm.stream, reason)) << name;
    }
}

// Task 4.3 / 4.4: every packet decodes, frames come out once each in presentation
// order, and the last frames aren't lost in the reorder delay.
TEST(VtDecoder, DecodesEveryPacketInPresentationOrder) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    for (const auto &[name, fmt] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        const auto r = vt_decode_all(vt_media(name));
        EXPECT_EQ(r.send_errors, 0) << name;
        ASSERT_EQ(r.frame_pts.size(), r.packet_pts.size()) << name;
        EXPECT_TRUE(std::is_sorted(r.frame_pts.begin(), r.frame_pts.end())) << name;
        EXPECT_EQ(std::adjacent_find(r.frame_pts.begin(), r.frame_pts.end()), r.frame_pts.end())
            << name << " repeats a frame";
        auto sorted_packets = r.packet_pts;
        std::sort(sorted_packets.begin(), sorted_packets.end());
        EXPECT_EQ(r.frame_pts, sorted_packets) << name;
    }
}

// Task 4.5 / 4.6: planar output format and colour metadata match FFmpeg's decoder.
TEST(VtDecoder, FormatAndColourMatchSoftwareDecoder) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    for (const auto &[name, fmt] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        const auto vt = vt_decode_all(vt_media(name), 1);
        const auto sw = sw_decode_all(vt_media(name));
        ASSERT_FALSE(vt.frame_pts.empty()) << name;
        EXPECT_EQ(vt.format, sw.format) << name << ": " << av_get_pix_fmt_name(vt.format)
                                        << " vs " << av_get_pix_fmt_name(sw.format);
        EXPECT_EQ(vt.colorspace, sw.colorspace) << name;
        EXPECT_EQ(vt.primaries, sw.primaries) << name;
        EXPECT_EQ(vt.trc, sw.trc) << name;
        // FFmpeg reports MPEG range for ProRes; unspecified elsewhere means MPEG too.
        auto norm = [](AVColorRange r) {
            return r == AVCOL_RANGE_UNSPECIFIED ? AVCOL_RANGE_MPEG : r;
        };
        EXPECT_EQ(norm(vt.range), norm(sw.range)) << name;
    }
}

// Task 4.5: decode 1000+ frames; run under 'leaks' to check no CVPixelBuffer or
// session objects are left behind.
TEST(VtDecoder, LongDecodeForLeakCheck) {
    VT_MEDIA_OR_SKIP(path, "h264_8bit_bframes.mov");
    size_t frames = 0;
    for (int i = 0; i < 21; ++i)
        frames += vt_decode_all(path).frame_pts.size();
    EXPECT_GE(frames, 1000u);
}


// ---------------------------------------------------------------------------------
// Through FFMpegDecoder / FFMpegStream: the path the reader uses.

namespace {

// Planar YUV(A) samples of a decoded image, read via the shader parameters the
// reader produced (so this also checks those are right).
struct Samples {
    int bits = 0;
    std::array<std::vector<uint16_t>, 4> planes;
};

Samples samples_of(const ImageBufPtr &buf) {
    Samples out;
    const auto &p        = buf->shader_params();
    out.bits             = p.value("bits_per_channel", 8);
    const int bps        = out.bits > 8 ? 2 : 1;
    const auto size      = buf->image_size_in_pixels();
    const int cw         = p.value("half_scale_uvx", 0) ? (size.x + 1) / 2 : size.x;
    const int ch         = p.value("half_scale_uvy", 0) ? (size.y + 1) / 2 : size.y;
    const char *names[4] = {"y", "u", "v", "a"};
    for (int i = 0; i < 4; ++i) {
        const int linesize = p.value(std::string(names[i]) + "_linesize", 0);
        if (!linesize)
            continue;
        const size_t offset = p.value(std::string(names[i]) + "_plane_bytes_offset", size_t(0));
        const int w         = (i == 1 || i == 2) ? cw : size.x;
        const int h         = (i == 1 || i == 2) ? ch : size.y;
        auto &plane         = out.planes[i];
        plane.reserve(size_t(w) * h);
        const auto *base = reinterpret_cast<const uint8_t *>(buf->buffer()) + offset;
        for (int y = 0; y < h; ++y) {
            const uint8_t *row = base + size_t(linesize) * y;
            for (int x = 0; x < w; ++x)
                plane.push_back(
                    bps == 2 ? reinterpret_cast<const uint16_t *>(row)[x] : uint16_t(row[x]));
        }
    }
    return out;
}

// Largest absolute sample difference between two images, -1 if their layouts differ.
int max_difference(const ImageBufPtr &a, const ImageBufPtr &b) {
    const Samples sa = samples_of(a), sb = samples_of(b);
    if (sa.bits != sb.bits)
        return -1;
    int worst = 0;
    for (int i = 0; i < 4; ++i) {
        if (sa.planes[i].size() != sb.planes[i].size())
            return -1;
        for (size_t k = 0; k < sa.planes[i].size(); ++k)
            worst = std::max(worst, std::abs(int(sa.planes[i][k]) - int(sb.planes[i][k])));
    }
    return worst;
}

// Scoped environment variable
struct ScopedEnv {
    std::string name;
    ScopedEnv(const char *n, const char *v) : name(n) { setenv(n, v, 1); }
    ~ScopedEnv() { unsetenv(name.c_str()); }
};

FFMpegDecoder make_decoder(const std::string &path, const VideoToolboxMode mode) {
    return FFMpegDecoder(
        path, 48000, utility::FrameRate(timebase::k_flicks_24fps), "stream 0", mode);
}

} // namespace

// Task 5.1 (and the second half of 4.5): frames come from VideoToolbox, as planar
// YUV the shader reads directly - no CPU conversion on the way.
TEST(VtStream, PlaysThroughVideoToolboxWithoutCpuConversion) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    for (const auto &[name, fmt] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        auto decoder = make_decoder(vt_media(name), VideoToolboxMode::Prefer);
        auto stream  = decoder.stream(0);
        ASSERT_TRUE(stream) << name;
        EXPECT_TRUE(stream->decoded_by_videotoolbox()) << name;
        // Task 3.2: FFmpeg's decoder is never even looked up, so builds without
        // one for this codec work the same.
        EXPECT_FALSE(stream->uses_ffmpeg_decoder()) << name;
        for (int f : {0, 1, 2, 10}) {
            ImageBufPtr buf;
            decoder.decode_video_frame(f, buf);
            ASSERT_TRUE(buf) << name << " frame " << f;
            EXPECT_EQ(buf->shader_params().value("pix_fmt", -1), 0)
                << name << ": YUV shader path";
            EXPECT_EQ(buf->decoder_frame_number(), f) << name;
        }
        EXPECT_FALSE(stream->converted_on_cpu()) << name;
    }
}

// Spec: frame-accurate presentation order (task 7.1). A fixed jumbled sequence of
// frame requests - forwards, backwards, first and last - must give the same
// pictures as FFmpeg's decoder: bit-exact for H.264/HEVC, within the ProRes
// tolerance measured in the spike (max 4 code values).
TEST(VtStream, RandomAccessMatchesSoftwareDecoder) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    const std::vector<int> sequence = {
        0, 1, 2, 47, 12, 13, 5, 30, 29, 28, 46, 23, 24, 0, 37, 11};
    for (const auto &[name, fmt] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        const bool prores = name.rfind("prores", 0) == 0;
        auto vt           = make_decoder(vt_media(name), VideoToolboxMode::Prefer);
        auto sw           = make_decoder(vt_media(name), VideoToolboxMode::Off);
        ASSERT_TRUE(vt.stream(0)->decoded_by_videotoolbox()) << name;
        ASSERT_FALSE(sw.stream(0)->decoded_by_videotoolbox()) << name;
        for (const int f : sequence) {
            ImageBufPtr a, b;
            std::string vt_error, sw_error;
            try {
                vt.decode_video_frame(f, a);
            } catch (const std::exception &e) {
                vt_error = e.what();
            }
            try {
                sw.decode_video_frame(f, b);
            } catch (const std::exception &e) {
                sw_error = e.what();
            }
            // FFMpegDecoder can't reach the frame just before an open-GOP keyframe
            // after a backward jump (frame 23 of the open-GOP clips here) with FFmpeg's
            // decoder - a seek issue predating VideoToolbox support. VideoToolbox may do
            // better, but must never fail where FFmpeg's decoder succeeds.
            if (vt_error.empty() && !sw_error.empty()) {
                ASSERT_TRUE(a) << name << " frame " << f;
                EXPECT_EQ(a->decoder_frame_number(), f) << name;
                continue;
            }
            EXPECT_EQ(vt_error, sw_error) << name << " frame " << f;
            if (!vt_error.empty() || !sw_error.empty())
                continue;
            ASSERT_TRUE(a && b) << name << " frame " << f;
            EXPECT_EQ(a->decoder_frame_number(), f) << name;
            EXPECT_EQ(a->has_alpha(), b->has_alpha()) << name;
            const int diff = max_difference(a, b);
            if (name.find("12bit") != std::string::npos) {
                EXPECT_GE(diff, 0) << name << " frame " << f << ": layouts differ";
                EXPECT_LE(diff, 1) << name << " frame " << f;
            } else if (prores) {
                EXPECT_GE(diff, 0) << name << " frame " << f << ": layouts differ";
                EXPECT_LE(diff, 4) << name << " frame " << f;
            } else {
                EXPECT_EQ(diff, 0) << name << " frame " << f;
            }
        }
    }
}

// Task 5.2: when VideoToolbox can't create a session, 'prefer' falls back to
// FFmpeg's decoder and 'only' fails each frame with a message naming the codec.
TEST(VtStream, SessionFailureFallsBackOrFails) {
    VT_MEDIA_OR_SKIP(path, "hevc_main10_bframes.mov");
    ScopedEnv force("XSTUDIO_VIDEOTOOLBOX_FORCE_FAILURE", "1");

    {
        auto decoder = make_decoder(path, VideoToolboxMode::Prefer);
        EXPECT_FALSE(decoder.stream(0)->decoded_by_videotoolbox());
        ImageBufPtr buf;
        decoder.decode_video_frame(3, buf);
        ASSERT_TRUE(buf);
        EXPECT_EQ(buf->decoder_frame_number(), 3);
    }
    {
        auto decoder = make_decoder(path, VideoToolboxMode::Only);
        ImageBufPtr buf;
        try {
            decoder.decode_video_frame(3, buf);
            ADD_FAILURE() << "expected videotoolbox_unavailable_error";
        } catch (const videotoolbox_unavailable_error &e) {
            EXPECT_NE(std::string(e.what()).find("hevc"), std::string::npos) << e.what();
            EXPECT_NE(std::string(e.what()).find("VideoToolbox"), std::string::npos)
                << e.what();
        }
        // and keeps failing cleanly rather than crashing
        EXPECT_THROW(decoder.decode_video_frame(4, buf), videotoolbox_unavailable_error);
    }
}

// Task 5.3: streams VideoToolbox doesn't take are decoded by FFmpeg, in any mode,
// exactly as with VideoToolbox off. (A raw Annex-B stream has no container timing,
// so the reader gives it no duration or frame numbers either way.)
TEST(VtStream, IneligibleStreamsUseSoftware) {
    VT_MEDIA_OR_SKIP(annexb, "h264_annexb.h264");
    VT_MEDIA_OR_SKIP(dnx, "dnxhr_fullrange.mov");
    for (const auto &path : {annexb, dnx}) {
        auto reference = make_decoder(path, VideoToolboxMode::Off);
        ImageBufPtr ref;
        reference.decode_video_frame(5, ref);
        ASSERT_TRUE(ref) << path;
        for (auto mode : {VideoToolboxMode::Prefer, VideoToolboxMode::Only}) {
            auto decoder = make_decoder(path, mode);
            EXPECT_FALSE(decoder.stream(0)->decoded_by_videotoolbox()) << path;
            ImageBufPtr buf;
            decoder.decode_video_frame(5, buf);
            ASSERT_TRUE(buf) << path;
            EXPECT_EQ(buf->decoder_frame_number(), ref->decoder_frame_number()) << path;
            EXPECT_EQ(max_difference(buf, ref), 0) << path;
        }
    }
}

// Task 5.4: thumbnails of VideoToolbox-decoded media.
TEST(VtStream, Thumbnails) {
    VT_MEDIA_OR_SKIP(first, "h264_8bit_bframes.mov");
    for (const auto &[name, fmt] : k_vt_media_and_format) {
        if (!have_clip(name))
            continue;
        auto decoder = make_decoder(vt_media(name), VideoToolboxMode::Prefer);
        ASSERT_TRUE(decoder.stream(0)->decoded_by_videotoolbox()) << name;
        auto thumb = decoder.decode_thumbnail_frame(24, 256);
        ASSERT_TRUE(thumb) << name;
        EXPECT_GT(thumb->width(), 0u) << name;
        EXPECT_GT(thumb->height(), 0u) << name;
    }
}

// Changing the preference at runtime must not hand back a frame decoded under the
// previous mode (FFMpegMediaReader keeps the last decoded image to answer repeat
// requests).
TEST(VtStream, PreferenceChangeDropsFramesDecodedUnderOldMode) {
    VT_MEDIA_OR_SKIP(path, "prores_422_hq.mov");

    utility::JsonStore prefs;
    global_store::set_preference_value(
        prefs, std::string("prefer"), "/plugin/media_reader/FFMPEG/videotoolbox");
    FFMpegMediaReader reader(prefs);

    const media::AVFrameID frame(
        posix_path_to_uri(path),
        10,
        0,
        media::FS_ON_DISK,
        0,
        1.0f,
        utility::FrameRate(timebase::k_flicks_24fps),
        "stream 0");

    const ImageBufPtr vt = reader.image(frame);
    ASSERT_TRUE(vt);
    vt->set_media_key(frame.key()); // as the reader actor does
    EXPECT_EQ(reader.image(frame), vt) << "repeat request is answered from memory";

    global_store::set_preference_value(
        prefs, std::string("off"), "/plugin/media_reader/FFMPEG/videotoolbox");
    reader.update_preferences(prefs);

    const ImageBufPtr sw = reader.image(frame);
    ASSERT_TRUE(sw);
    EXPECT_NE(sw, vt);
    // Apple's and FFmpeg's ProRes decoders differ slightly, so a fresh decode shows it
    EXPECT_GT(max_difference(sw, vt), 0);
    EXPECT_LE(max_difference(sw, vt), 4);
}

// ---------------------------------------------------------------------------------
// Task 6.1: AAC through AudioToolbox (aac_at)

namespace {

struct DecodedAudio {
    int64_t first_pts = AV_NOPTS_VALUE;
    std::vector<float> samples; // first channel
};

DecodedAudio decode_audio_with(const std::string &path, const char *decoder_name) {
    DecodedAudio out;
    AVFormatContext *ctx = nullptr;
    if (avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) < 0)
        return out;
    avformat_find_stream_info(ctx, nullptr);
    const int index      = av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const AVCodec *codec = avcodec_find_decoder_by_name(decoder_name);
    AVCodecContext *dec  = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec, ctx->streams[index]->codecpar);
    avcodec_open2(dec, codec, nullptr);
    AVPacket *pkt  = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    auto take      = [&]() {
        if (out.first_pts == AV_NOPTS_VALUE)
            out.first_pts = frame->pts;
        for (int i = 0; i < frame->nb_samples; ++i) {
            switch (frame->format) {
            case AV_SAMPLE_FMT_FLTP:
            case AV_SAMPLE_FMT_FLT:
                out.samples.push_back(reinterpret_cast<const float *>(frame->data[0])[i]);
                break;
            case AV_SAMPLE_FMT_S16P:
            case AV_SAMPLE_FMT_S16:
                out.samples.push_back(
                    reinterpret_cast<const int16_t *>(frame->data[0])[i] / 32768.0f);
                break;
            default:
                break;
            }
        }
    };
    while (av_read_frame(ctx, pkt) >= 0) {
        if (pkt->stream_index == index) {
            avcodec_send_packet(dec, pkt);
            while (avcodec_receive_frame(dec, frame) == 0)
                take();
        }
        av_packet_unref(pkt);
    }
    avcodec_send_packet(dec, nullptr);
    while (avcodec_receive_frame(dec, frame) == 0)
        take();
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec);
    avformat_close_input(&ctx);
    return out;
}

} // namespace

TEST(AacAudioToolbox, StreamUsesAacAt) {
    VT_MEDIA_OR_SKIP(path, "h264_aac.mov");
    ASSERT_TRUE(avcodec_find_decoder_by_name("aac_at")) << "FFmpeg built without AudioToolbox";
    for (auto mode : {VideoToolboxMode::Off, VideoToolboxMode::Prefer}) {
        FFMpegDecoder decoder(
            path, 48000, utility::FrameRate(timebase::k_flicks_24fps), "stream 1", mode);
        auto stream = decoder.stream(1);
        ASSERT_TRUE(stream);
        EXPECT_EQ(stream->ffmpeg_decoder_name(), "aac_at") << to_string(mode);
        AudioBufPtr audio;
        decoder.decode_audio_frame(10, audio);
        ASSERT_TRUE(audio);
        EXPECT_GT(audio->num_samples(), 0u);
    }
}

// aac_at must line up sample for sample with FFmpeg's own AAC decoder - same first
// timestamp, same length, no priming offset - or audio sync would change.
TEST(AacAudioToolbox, MatchesFFmpegAacTiming) {
    VT_MEDIA_OR_SKIP(path, "h264_aac.mov");
    const auto ref = decode_audio_with(path, "aac");
    const auto at  = decode_audio_with(path, "aac_at");
    ASSERT_FALSE(ref.samples.empty());
    ASSERT_FALSE(at.samples.empty());
    EXPECT_EQ(at.first_pts, ref.first_pts);
    EXPECT_EQ(at.samples.size(), ref.samples.size());

    // best alignment within +-4096 samples, on the chirp (non-periodic)
    const size_t n  = std::min(at.samples.size(), ref.samples.size());
    int best_lag    = 0;
    double best_err = 1e30;
    for (int lag = -4096; lag <= 4096; lag += 1) {
        double err  = 0;
        size_t used = 0;
        for (size_t i = 8192; i + 8192 < n; i += 7) {
            const long j   = long(i) + lag;
            const double d = at.samples[size_t(j)] - ref.samples[i];
            err += d * d;
            used++;
        }
        err /= double(used);
        if (err < best_err) {
            best_err = err;
            best_lag = lag;
        }
    }
    EXPECT_EQ(best_lag, 0);
    EXPECT_LT(std::sqrt(best_err), 1e-3) << "RMS difference between AAC decoders";
}

#endif // XSTUDIO_HAS_VIDEOTOOLBOX


// Not run by default (slow). Run with --gtest_also_run_disabled_tests.
// Memory must stay flat however many frames are decoded: VideoToolbox's frames live
// in IOSurfaces, which 'leaks' can't see but the process footprint includes.
#ifdef XSTUDIO_HAS_VIDEOTOOLBOX
#include <mach/mach.h>

static double footprint_mb() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
        return -1;
    return double(info.phys_footprint) / (1024.0 * 1024.0);
}

TEST(VtStream, DISABLED_MemoryStaysFlatOverLongDecode) {
    VT_MEDIA_OR_SKIP(path, "bench_uhd_prores_4444_grain.mov");
    for (auto mode : {VideoToolboxMode::Off, VideoToolboxMode::Prefer}) {
        std::vector<double> mb;
        for (int pass = 0; pass < 6; ++pass) {
            // a new decoder each pass, like the reader for thumbnails / reopened media
            auto decoder = make_decoder(path, mode);
            for (int f = 0; f < 120; ++f) {
                ImageBufPtr buf;
                decoder.decode_video_frame(f, buf);
            }
            mb.push_back(footprint_mb());
        }
        std::cout << to_string(mode) << " footprint MB after each 120 frame pass:";
        for (auto v : mb)
            std::cout << " " << int(v);
        std::cout << "\n";
        // allow some slack for allocator pools; a leaked 4K 4444 frame is ~66MB
        EXPECT_LT(mb.back() - mb[1], 200.0) << to_string(mode);
    }
}

#endif
