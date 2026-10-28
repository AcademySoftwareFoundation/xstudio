// SPDX-License-Identifier: Apache-2.0
#include "vt_repack.hpp"

#include <cstring>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace xstudio::media_reader::ffmpeg::vt_repack {

namespace {
    // The loops below are kept simple (contiguous rows, no branches) so the compiler
    // can vectorise them.

    template <typename T> inline const T *src_row(const SrcPlane &p, const int y) {
        return reinterpret_cast<const T *>(p.data + p.stride * size_t(y));
    }

    template <typename T> inline T *dst_row(const DstPlane &p, const int y) {
        return reinterpret_cast<T *>(p.data + p.stride * size_t(y));
    }
} // namespace

Layout planar_layout(const AVPixelFormat format, const int width, const int height) {

    Layout layout;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
    if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_PLANAR))
        return layout;

    std::array<int, 4> linesizes = {0, 0, 0, 0};
    if (av_image_fill_linesizes(linesizes.data(), format, width) < 0)
        return layout;

    std::array<ptrdiff_t, 4> aligned = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i)
        aligned[i] = FFALIGN(linesizes[i], 64);

    std::array<size_t, 4> plane_sizes = {0, 0, 0, 0};
    if (av_image_fill_plane_sizes(plane_sizes.data(), format, height, aligned.data()) < 0)
        return layout;

    layout.format = format;
    for (int i = 0; i < 4; ++i) {
        if (!plane_sizes[i])
            break;
        layout.offsets[i]   = layout.total_size;
        layout.linesizes[i] = size_t(aligned[i]);
        layout.total_size += plane_sizes[i];
        layout.planes++;
    }
    return layout;
}

void biplanar8_to_planar8(
    const SrcPlane y,
    const SrcPlane cbcr,
    const DstPlane dst_y,
    const DstPlane dst_u,
    const DstPlane dst_v,
    const int width,
    const int height,
    const Chroma chroma) {

    for (int row = 0; row < height; ++row)
        std::memcpy(dst_y.data + dst_y.stride * row, y.data + y.stride * row, size_t(width));

    const int cw = chroma_width(width, chroma);
    const int ch = chroma_height(height, chroma);
    for (int row = 0; row < ch; ++row) {
        const uint8_t *__restrict in = src_row<uint8_t>(cbcr, row);
        uint8_t *__restrict u        = dst_row<uint8_t>(dst_u, row);
        uint8_t *__restrict v        = dst_row<uint8_t>(dst_v, row);
        for (int x = 0; x < cw; ++x) {
            u[x] = in[2 * x];
            v[x] = in[2 * x + 1];
        }
    }
}

void biplanar10_to_planar10(
    const SrcPlane y,
    const SrcPlane cbcr,
    const DstPlane dst_y,
    const DstPlane dst_u,
    const DstPlane dst_v,
    const int width,
    const int height,
    const Chroma chroma) {

    for (int row = 0; row < height; ++row) {
        const uint16_t *__restrict in = src_row<uint16_t>(y, row);
        uint16_t *__restrict out      = dst_row<uint16_t>(dst_y, row);
        for (int x = 0; x < width; ++x)
            out[x] = in[x] >> 6;
    }

    const int cw = chroma_width(width, chroma);
    const int ch = chroma_height(height, chroma);
    for (int row = 0; row < ch; ++row) {
        const uint16_t *__restrict in = src_row<uint16_t>(cbcr, row);
        uint16_t *__restrict u        = dst_row<uint16_t>(dst_u, row);
        uint16_t *__restrict v        = dst_row<uint16_t>(dst_v, row);
        for (int x = 0; x < cw; ++x) {
            u[x] = in[2 * x] >> 6;
            v[x] = in[2 * x + 1] >> 6;
        }
    }
}

void biplanar16_to_planar12(
    const SrcPlane y,
    const SrcPlane cbcr,
    const DstPlane dst_y,
    const DstPlane dst_u,
    const DstPlane dst_v,
    const int width,
    const int height,
    const Chroma chroma) {

    for (int row = 0; row < height; ++row) {
        const uint16_t *__restrict in = src_row<uint16_t>(y, row);
        uint16_t *__restrict out      = dst_row<uint16_t>(dst_y, row);
        for (int x = 0; x < width; ++x)
            out[x] = round_16_to_12(in[x]);
    }

    const int cw = chroma_width(width, chroma);
    const int ch = chroma_height(height, chroma);
    for (int row = 0; row < ch; ++row) {
        const uint16_t *__restrict in = src_row<uint16_t>(cbcr, row);
        uint16_t *__restrict u        = dst_row<uint16_t>(dst_u, row);
        uint16_t *__restrict v        = dst_row<uint16_t>(dst_v, row);
        for (int x = 0; x < cw; ++x) {
            u[x] = round_16_to_12(in[2 * x]);
            v[x] = round_16_to_12(in[2 * x + 1]);
        }
    }
}

void s4as_to_yuva444p12(
    const SrcPlane y,
    const SrcPlane cbcr,
    const SrcPlane a,
    const DstPlane dst_y,
    const DstPlane dst_u,
    const DstPlane dst_v,
    const DstPlane dst_a,
    const int width,
    const int height) {

    if (!dst_a.data) {
        // no alpha wanted: the same as sv44
        biplanar16_to_planar12(y, cbcr, dst_y, dst_u, dst_v, width, height, Chroma::C444);
        return;
    }

    // One pass writing all four planes measured faster than a pass per plane.
    for (int row = 0; row < height; ++row) {
        const uint16_t *__restrict in_y  = src_row<uint16_t>(y, row);
        const uint16_t *__restrict in_uv = src_row<uint16_t>(cbcr, row);
        const uint16_t *__restrict in_a  = src_row<uint16_t>(a, row);
        uint16_t *__restrict out_y       = dst_row<uint16_t>(dst_y, row);
        uint16_t *__restrict out_u       = dst_row<uint16_t>(dst_u, row);
        uint16_t *__restrict out_v       = dst_row<uint16_t>(dst_v, row);
        uint16_t *__restrict out_a       = dst_row<uint16_t>(dst_a, row);
        for (int x = 0; x < width; ++x) {
            out_y[x] = round_16_to_12(in_y[x]);
            out_u[x] = round_16_to_12(in_uv[2 * x]);
            out_v[x] = round_16_to_12(in_uv[2 * x + 1]);
            out_a[x] = round_16_to_12(in_a[x]);
        }
    }
}

} // namespace xstudio::media_reader::ffmpeg::vt_repack
