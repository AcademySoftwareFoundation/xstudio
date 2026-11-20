// SPDX-License-Identifier: Apache-2.0
#pragma once

// VideoToolbox decode backend for FFMpegStream (macOS only).
//
// This header deliberately includes no Apple headers: MacTypes.h defines a 'nil'
// macro that breaks caf, and this header is included alongside caf ones.

#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace xstudio::media_reader::ffmpeg {

/* Decodes one video stream with a VideoToolbox decompression session.

The send_packet / receive_frame / flush methods follow the same rules as
avcodec_send_packet / avcodec_receive_frame / avcodec_flush_buffers, return codes
included, so FFMpegStream can use it in place of an AVCodecContext.

Frames come out in presentation order, in a planar YUV(A) format the xstudio
shader reads directly. Their pixel data lives in an xstudio ImageBuffer, which is
hidden in frame->buf[0] the same way setup_video_buffer() in ffmpeg_stream.cpp
does it. */
class VtDecoder {
  public:
    /* Whether the stream's codec and decoder configuration can go to VideoToolbox.
    If not, 'reason' says why. */
    static bool eligible(const AVStream *stream, std::string &reason);

    /* Creates a decoder for the stream. Returns null and sets 'error' if
    VideoToolbox can't create a session for it. Setting the environment variable
    XSTUDIO_VIDEOTOOLBOX_FORCE_FAILURE makes it always fail, for testing the
    fallback paths. */
    static std::unique_ptr<VtDecoder> create(const AVStream *stream, std::string &error);

    ~VtDecoder();

    VtDecoder(const VtDecoder &)            = delete;
    VtDecoder &operator=(const VtDecoder &) = delete;

    /* A null packet starts draining, as with avcodec_send_packet. Returns 0 or a
    negative AVERROR. The packet isn't unreferenced. */
    int send_packet(const AVPacket *packet);

    /* Returns 0 with 'frame' filled, AVERROR(EAGAIN) if more packets are needed,
    or AVERROR_EOF once fully drained. */
    int receive_frame(AVFrame *frame);

    /* Drops every frame in flight, e.g. before a seek. */
    void flush();

    /* The planar pixel format receive_frame() produces. */
    [[nodiscard]] AVPixelFormat output_pix_fmt() const;

    /* VideoToolbox's name for the session's output format, for logging. */
    [[nodiscard]] std::string output_format_name() const;

    [[nodiscard]] bool using_hardware() const;

  private:
    struct Impl;
    explicit VtDecoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace xstudio::media_reader::ffmpeg
