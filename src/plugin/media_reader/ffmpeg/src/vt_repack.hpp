// SPDX-License-Identifier: Apache-2.0
#pragma once

// Repacks the pixel layouts VideoToolbox decodes to into planar formats the xstudio
// YUV shader reads directly. Platform-neutral (just pointers and strides) so the
// loops can be unit tested anywhere. See design D5 in
// openspec/changes/add-videotoolbox-decode-backend.

#include <array>
#include <cstddef>
#include <cstdint>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace xstudio::media_reader::ffmpeg::vt_repack {

struct SrcPlane {
    const uint8_t *data;
    size_t stride; // bytes per row
};

struct DstPlane {
    uint8_t *data;
    size_t stride; // bytes per row
};

/* Plane layout of a planar destination image in one contiguous buffer. */
struct Layout {
    AVPixelFormat format            = AV_PIX_FMT_NONE;
    std::array<size_t, 4> offsets   = {0, 0, 0, 0};
    std::array<size_t, 4> linesizes = {0, 0, 0, 0};
    size_t total_size               = 0;
    int planes                      = 0;
};

/* Layout for a planar 'format' image of width x height. Line sizes are 64 byte
aligned, matching what setup_video_buffer() gives ffmpeg's own decoders. Returns a
layout with format AV_PIX_FMT_NONE for packed formats (AV_PIX_FMT_FLAG_PLANAR unset). */
Layout planar_layout(AVPixelFormat format, int width, int height);

/* Chroma subsampling of a source and its planar destination. */
enum class Chroma { C420, C422, C444 };

inline int chroma_width(const int width, const Chroma c) {
    return c == Chroma::C444 ? width : (width + 1) / 2;
}

inline int chroma_height(const int height, const Chroma c) {
    return c == Chroma::C420 ? (height + 1) / 2 : height;
}

/* 8 bit Y + interleaved CbCr (VideoToolbox '420v' / '422v' / '444v' and the
full range 'f' variants; ffmpeg nv12 / nv16 / nv24) to yuv420p / yuv422p /
yuv444p. */
void biplanar8_to_planar8(
    SrcPlane y,
    SrcPlane cbcr,
    DstPlane dst_y,
    DstPlane dst_u,
    DstPlane dst_v,
    int width,
    int height,
    Chroma chroma);

/* 10 bit samples in the top bits of 16 bit words, Y + interleaved CbCr
(VideoToolbox 'x420' / 'x422' / 'x444' and the full range variants; ffmpeg
p010le / p210le / p410le) to yuv420p10le / yuv422p10le / yuv444p10le. */
void biplanar10_to_planar10(
    SrcPlane y,
    SrcPlane cbcr,
    DstPlane dst_y,
    DstPlane dst_u,
    DstPlane dst_v,
    int width,
    int height,
    Chroma chroma);

/* 16 bit Y + interleaved CbCr (VideoToolbox 'sv20' / 'sv22' / 'sv44'; ffmpeg
p016le / p216le / p416le) to yuv420p12le / yuv422p12le / yuv444p12le. Samples are
rounded to 12 bits. */
void biplanar16_to_planar12(
    SrcPlane y,
    SrcPlane cbcr,
    DstPlane dst_y,
    DstPlane dst_u,
    DstPlane dst_v,
    int width,
    int height,
    Chroma chroma);

/* 16 bit 4:4:4 Y + interleaved CbCr + A (VideoToolbox 's4as') to yuva444p12le, or
to yuv444p12le if dst_a.data is null. Samples are rounded to 12 bits. */
void s4as_to_yuva444p12(
    SrcPlane y,
    SrcPlane cbcr,
    SrcPlane a,
    DstPlane dst_y,
    DstPlane dst_u,
    DstPlane dst_v,
    DstPlane dst_a,
    int width,
    int height);

/* 16 bit value rounded to 12 bits, clamped. Clamping first keeps every step within
16 bits ((65527 + 8) >> 4 == 4095), so loops vectorise at full width. */
inline uint16_t round_16_to_12(const uint16_t v) {
    const uint16_t clamped = v < 65527 ? v : uint16_t(65527);
    return uint16_t(uint16_t(clamped + 8) >> 4);
}

} // namespace xstudio::media_reader::ffmpeg::vt_repack
