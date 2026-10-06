// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <map>
#include <vector>

extern "C" {
#include <libavutil/rational.h>
}

namespace xstudio::media_reader::ffmpeg {

/* Rescales a decoded audio frame's pts to the sample it starts at.
 *
 * A pts counts units of the stream's time_base. It is the muxer's own count
 * of samples from the start of the stream, written in that unit. There are
 * three ways to get the sample back, and the caller picks one per stream from
 * what the library states about it:
 *
 * use_pts()          The codec has no frames, its samples come straight from
 *                    the packet's bytes (av_get_exact_bits_per_sample() > 0),
 *                    and the time_base unit is no more than one sample. The
 *                    pts is the position. wav, aiff and PCM in mov.
 *
 * use_frame_size()   The library gives the codec a fixed frame size
 *                    (av_get_audio_frame_duration2() > 0). Frame k of the
 *                    stream starts at start + k * frame_size, and k is the
 *                    pts in samples, less the start, divided by frame_size
 *                    and rounded: a pts is never out by anything near half a
 *                    frame. AAC, AC-3, MP2, MP3, in any container.
 *
 * use_region_table() The library does not state a frame size, because the
 *                    format lets it change from packet to packet (vorbis
 *                    picks one of three per packet from the sound). Each
 *                    packet is then a region of the stream with its own size,
 *                    and the caller keeps a table of them: where each packet
 *                    is in the file and how many samples it decodes to, in
 *                    file order, as far into the file as anything has needed
 *                    to go. The sizes come from the library without decoding.
 *                    With every size before it known, where a region starts
 *                    is calculated, and so is the region that holds a given
 *                    sample, which is where to seek for it. No pts places
 *                    anything: real webm files carry pts tens of milliseconds
 *                    out around a change of size, and the pts of a packet
 *                    inside a matroska block or an ogg page is worked out by
 *                    the demuxer and comes out differently after a seek. A
 *                    region is named by where its packet is in the file,
 *                    AVPacket.pos, and which of the packets there it is.
 *
 * Frames must arrive whole for their sizes to mean anything, so the decoder is
 * opened with AV_CODEC_FLAG2_SKIP_MANUAL and the caller drops the priming the
 * frame's skip-samples side data names. Nothing here names a codec.
 */
class AudioPtsRescaler {
  public:
    struct Frame {
        int64_t pts;
        int nb_samples;
    };

    // What the stream's start shows, once at open: whole frames decoded from
    // its first packet. They give where the audio starts.
    struct StreamStart {
        std::vector<Frame> frames;
        // the first packet the decoder accepted
        int64_t first_packet_pts = 0;
        // priming the container declares, in samples, and whether it declares
        // any at all, by that value or by skip side data on the first packet
        int initial_padding   = 0;
        bool priming_declared = false;
        // samples the decoder asks dropped from the front of the first frame
        int64_t first_frame_skip = 0;
    };

    AudioPtsRescaler() = default;
    AudioPtsRescaler(AVRational pkt_timebase, int sample_rate);

    void set_stream_start(const StreamStart &start);

    void use_pts();
    void use_frame_size(int frame_size);
    void use_region_table();
    [[nodiscard]] bool uses_region_table() const { return method_ == Method::REGION_TABLE; }

    // use_pts() and use_frame_size(): where the frame with this pts starts
    [[nodiscard]] int64_t position(int64_t pts) const;

    // use_region_table(). Regions are added in file order from the stream's
    // first packet: `pos` is AVPacket.pos, nb_samples what the packet decodes
    // to, pts the packet's.
    void add_region(int64_t pos, int nb_samples, int64_t pts);
    [[nodiscard]] int64_t regions() const { return int64_t(region_pos_.size()); }
    // the first sample the table does not reach yet
    [[nodiscard]] int64_t covered_to() const { return next_region_start_ - ts_offset_; }
    // a packet is the ordinal-th, from 0, of the packets at its pos
    [[nodiscard]] bool has_region(int64_t pos, int ordinal) const;
    [[nodiscard]] int64_t region_index(int64_t pos, int ordinal) const;
    // where a region starts, and the region that holds a sample the table covers
    [[nodiscard]] int64_t region_start(int64_t index) const;
    [[nodiscard]] int64_t region_holding(int64_t sample) const;
    // a pts to seek to, backwards, to read the file from that region: the pts
    // of the nearest packet at or before it that is the first at its pos, which
    // is one the file stores
    [[nodiscard]] int64_t seek_pts_for_region(int64_t index) const;

    // a pts as a position, to the precision of the time_base only
    [[nodiscard]] int64_t rescale(int64_t pts) const;

  private:
    [[nodiscard]] int64_t pts_to_samples(int64_t pts) const;
    [[nodiscard]] int64_t pts_error(int64_t first, const std::vector<Frame> &frames) const;
    [[nodiscard]] int64_t estimate_start(const std::vector<Frame> &frames) const;

    AVRational pkt_timebase_ = {0, 1};
    int sample_rate_         = 0;

    int64_t start_samples_ = 0; // the stream's first frame, as its pts have it
    int64_t ts_offset_     = 0; // that, less where the first frame belongs

    enum class Method { PTS, FRAME_SIZE, REGION_TABLE };
    Method method_      = Method::PTS;
    int64_t frame_size_ = 1;
    // the region table, in file order: where each packet is, the sample it
    // starts at, and its pts if it is the first packet at its pos
    std::vector<int64_t> region_pos_;
    std::vector<int64_t> region_start_;
    std::vector<int64_t> region_stored_pts_;
    // the sample the next region added will start at
    int64_t next_region_start_ = 0;
    // the first region at each pos
    std::map<int64_t, int64_t> first_at_pos_;
};

} // namespace xstudio::media_reader::ffmpeg
