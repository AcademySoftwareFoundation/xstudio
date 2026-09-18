// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "audio_pts_rescaler.hpp"

#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// exception throwing convenience macro
#define xstr(a) __str(a)
#define __str(a) #a

#define CURRENT_FRAME_UNKNOWN -1

namespace xstudio::media_reader::ffmpeg {

#include <chrono>

class DebugTimer {
  public:
    DebugTimer(std::string p, int ffmpeg_frame_) : path_(std::move(p)), frame_(ffmpeg_frame_) {
        t1_ = utility::clock::now();
    }

    ~DebugTimer() {
        std::cerr << "Reading" << path_ << " @ " << frame_ << " done in "
                  << double(std::chrono::duration_cast<std::chrono::microseconds>(
                                utility::clock::now() - t1_)
                                .count()) /
                         1000000.0
                  << " seconds\n";
    }

  private:
    utility::time_point t1_;
    const std::string path_;
    const int frame_;
};

void AVC_CHECK_THROW(int errorNum, const char *avc_command);

typedef enum {
    VIDEO_STREAM     = 1,
    AUDIO_STREAM     = 2,
    TIMECODE_STREAM  = 4,
    ALL_STREAM_TYPES = 7
} FFMpegStreamType;

/* Class to manage data concerning a single audio/video/data stream
in an AVFormat ffpmeg object */
class FFMpegStream {

  public:
    FFMpegStream(
        AVFormatContext *fmt_ctx,
        AVStream *stream,
        int index,
        int thread_count,
        std::string path);

    virtual ~FFMpegStream();

    int64_t current_frame();
    int64_t frame_to_pts(int ffmpeg_frame_) const;

    void set_virtual_frame_rate(const utility::FrameRate &vfr);

    const utility::FrameRate &frame_rate() const { return frame_rate_; }

    ImageBufPtr get_ffmpeg_frame_as_xstudio_image();

    AudioBufPtr get_ffmpeg_frame_as_xstudio_audio();

    const ImageBufPtr &attached_pic() const { return attached_pic_; }

    std::shared_ptr<thumbnail::ThumbnailBuffer>
    convert_av_frame_to_thumbnail(const size_t size_hint);

    int64_t receive_frame();

    void send_flush_packet();

    int send_packet(AVPacket *avc_packet_);

    void flush_buffers();

    size_t resample_audio(
        AVFrame *ffmpeg_frame_,
        AudioBufPtr &audio_buffer,
        int offset_into_output_buffer,
        const int target_sample_rate);

    [[nodiscard]] AVMediaType codec_type() const { return codec_type_; }

    [[nodiscard]] FFMpegStreamType stream_type() const { return stream_type_; }

    [[nodiscard]] Imath::V2i resolution() const { return resolution_; }

    [[nodiscard]] float pixel_aspect() const { return pixel_aspect_; }

    [[nodiscard]] int duration_frames() const;

    [[nodiscard]] bool is_single_frame() const {
        return is_attached_pic_ || duration_frames() < 2;
    }

    [[nodiscard]] bool is_attached_pic() const { return is_attached_pic_; }

    [[nodiscard]] int stream_index() const { return stream_index_; }

    [[nodiscard]] bool is_drop_frame_timecode() const { return is_drop_frame_timecode_; }

    [[nodiscard]] double duration_seconds() const;

    [[nodiscard]] AVDictionary *tags() { return avc_stream_->metadata; }

    [[nodiscard]] int64_t seconds_to_pts(const double secs) const {
        return (
            !avc_stream_->time_base.num ? 0
                                        : int64_t(
                                              secs * double(avc_stream_->time_base.den) /
                                              double(avc_stream_->time_base.num)));
    }

    void set_current_frame_unknown() { current_frame_ = CURRENT_FRAME_UNKNOWN; }

    // Audio decoded after a seek and dropped, so the decoder has settled: RFC 7845
    // asks 80ms for opus, the most any codec here needs.
    static constexpr double AUDIO_SEEK_PREROLL_SECONDS = 0.08;

    // The timestamp of the first packet of the stream: the start time, or the
    // first index entry where that is earlier, as in an mp4 whose edit list
    // carries the AAC priming in a packet ahead of the pts zero the start time
    // reports. Never later than the start time: a format without an index of
    // its own gets entries added wherever a seek happened to probe.
    [[nodiscard]] int64_t first_packet_pts() const;

    // Whether the run about to be decoded starts with pre-roll to drop, and the
    // timestamp the seek was meant to reach: nothing from there on is dropped.
    // Not at the head of the stream, where the decoder's initial state is right.
    void set_audio_preroll(const bool preroll, const int64_t wanted_pts) {
        audio_preroll_pending_ = preroll;
        audio_wanted_pts_      = wanted_pts;
        audio_landed_late_     = false;
    }
    // whether the first frame decoded after the seek started after the
    // timestamp the seek was meant to reach: the demuxer landed late
    [[nodiscard]] bool audio_landed_late() const { return audio_landed_late_; }

    // Once per audio stream: seek to its first packet and decode the first
    // frames with a codec context of its own, to learn the frame sizes and
    // where the audio starts. Returns whether it ran, since running moves the
    // read position.
    bool probe_audio_start();
    // Where to seek to decode from target_pts. Differs from target_pts for a
    // stream whose packets are regions of differing size, see AudioPtsRescaler.
    int64_t audio_seek_pts(int64_t target_pts);

  private:
    [[nodiscard]] int64_t stream_start_time() const {
        return avc_stream_->start_time != AV_NOPTS_VALUE ? avc_stream_->start_time : 0;
    }

    void decode_attached_pic();

    // a fresh decoder, as the head of the stream gets: a flush does not reset
    // every decoder, and priming is trimmed by hand from whole frames
    void open_audio_decoder();
    // drop the samples the frame's skip-samples side data names; returns the
    // count dropped from the front
    int64_t trim_skipped_samples();

    // void setup_frame(ImageStorePtr & video_frame);
    int stream_index_;
    AVCodecContext *codec_context_{nullptr};
    AVFormatContext *format_context_{nullptr};
    AVStream *avc_stream_{nullptr};
    const AVCodec *codec_{nullptr};
    AVFrame *ffmpeg_frame_{nullptr};
    AVMediaType codec_type_;
    int64_t fpsNum_; // Numerator of FPS
    int64_t fpsDen_; // Denominator of FPS (if fractional)
    FFMpegStreamType stream_type_;
    bool is_drop_frame_timecode_;
    std::string source_path;
    bool format_conversion_warning_issued = {false};
    bool using_own_frame_allocation       = {false};
    bool nothing_decoded_yet_             = {true};
    int64_t current_frame_                = {CURRENT_FRAME_UNKNOWN};

    // decoded frames ending at or before this run position are pre-roll; the
    // lowest position of all means none is
    static constexpr int64_t NO_PREROLL = std::numeric_limits<int64_t>::min();
    int64_t audio_publish_from_         = {NO_PREROLL};
    bool audio_preroll_pending_         = {false};
    int64_t audio_wanted_pts_           = {0};
    bool audio_landed_late_             = {false};


    AudioPtsRescaler audio_pts_rescaler_;
    // the file position of the audio packet last sent to the decoder, and which
    // of the packets at that position it was
    static constexpr int64_t NO_PACKET_POS = std::numeric_limits<int64_t>::min();
    int64_t audio_packet_pos_              = {NO_PACKET_POS};
    int audio_packet_ordinal_              = {0};
    // sizing packets for the region table without decoding them: the codec's
    // parser, and the region of the packet it was fed last
    static constexpr int64_t NO_REGION  = -1;
    AVCodecParserContext *audio_parser_ = {nullptr};
    AVCodecContext *audio_parse_ctx_    = {nullptr};
    int64_t audio_sizer_fed_            = {NO_REGION};
    bool audio_duration_in_samples_     = {false};
    int64_t audio_first_packet_pts_     = {0};

    void reset_audio_packet_sizer();
    int size_audio_packet(const AVPacket *packet, int64_t region);
    void add_audio_region(const AVPacket *packet);
    void extend_audio_region_table(int64_t sample);
    bool audio_start_probed_ = {false};

    Imath::V2i resolution_ = {Imath::V2i(0, 0)};
    float pixel_aspect_    = 1.0f;
    bool is_attached_pic_  = {false};

    // for video rescaling
    SwsContext *sws_context_ = {nullptr};

    // for audio resampling
    AVSampleFormat target_sample_format_ = {AV_SAMPLE_FMT_NONE};
    int target_audio_channels_           = 0;
    AVSampleFormat src_audio_fmt_        = {AV_SAMPLE_FMT_NONE};
    int src_audio_sample_rate_           = {0};
    int src_audio_channel_layout_        = {0};
    int src_audio_chans_                 = {0};
    SwrContext *audio_resampler_ctx_     = {0};

    utility::FrameRate frame_rate_;
    ImageBufPtr attached_pic_;
};
} // namespace xstudio::media_reader::ffmpeg
