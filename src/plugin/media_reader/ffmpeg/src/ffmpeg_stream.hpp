// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/frame_rate.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// exception throwing convenience macro
#define xstr(a) __str(a)
#define __str(a) #a

#define CURRENT_FRAME_UNKNOWN -1

namespace xstudio {
namespace media_reader {
    namespace ffmpeg {

#include <chrono>

        class DebugTimer {
          public:
            DebugTimer(std::string p, int frame) : path_(std::move(p)), frame_(frame) {
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
            int64_t frame_to_pts(int frame) const;

            void set_virtual_frame_rate(const utility::FrameRate &vfr);

            const utility::FrameRate &frame_rate() const { return frame_rate_; }

            ImageBufPtr get_ffmpeg_frame_as_xstudio_image();

            AudioBufPtr get_ffmpeg_frame_as_xstudio_audio(const int soundcard_sample_rate);

            std::shared_ptr<thumbnail::ThumbnailBuffer>
            convert_av_frame_to_thumbnail(const size_t size_hint);

            int64_t receive_frame();

            void send_flush_packet();

            int send_packet(AVPacket *avc_packet_);

            void flush_buffers();

            size_t resample_audio(
                AVFrame *frame, AudioBufPtr &audio_buffer, int offset_into_output_buffer);

            [[nodiscard]] AVMediaType codec_type() const { return codec_type_; }

            [[nodiscard]] FFMpegStreamType stream_type() const { return stream_type_; }

            [[nodiscard]] int duration_frames() const;

            [[nodiscard]] bool is_single_frame() const { return duration_frames() < 2; }

            [[nodiscard]] int stream_index() const { return stream_index_; }

            [[nodiscard]] bool is_drop_frame_timecode() const {
                return is_drop_frame_timecode_;
            }

            [[nodiscard]] Imath::V2i resolution() const { return resolution_; }

            [[nodiscard]] float pixel_aspect() const { return pixel_aspect_; }

            [[nodiscard]] double duration_seconds() const;

            [[nodiscard]] AVDictionary *tags() { return avc_stream_->metadata; }

            [[nodiscard]] int64_t seconds_to_pts(const double secs) const {
                return (
                    !avc_stream_->time_base.num
                        ? 0
                        : int64_t(
                              secs * double(avc_stream_->time_base.den) /
                              double(avc_stream_->time_base.num)));
            }

            void set_current_frame_unknown() { current_frame_ = CURRENT_FRAME_UNKNOWN; }

          private:
            [[nodiscard]] int64_t stream_start_time() const {
                return avc_stream_->start_time != AV_NOPTS_VALUE ? avc_stream_->start_time : 0;
            }

            // void setup_frame(ImageStorePtr & video_frame);
            int stream_index_;
            AVCodecContext *codec_context_;
            AVFormatContext *format_context_;
            AVStream *avc_stream_;
            const AVCodec *codec_;
            AVFrame *frame;
            AVMediaType codec_type_;
            int64_t fpsNum_; // Numerator of FPS
            int64_t fpsDen_; // Denominator of FPS (if fractional)
            FFMpegStreamType stream_type_;
            bool is_drop_frame_timecode_;
            std::string source_path;
            bool format_conversion_warning_issued = {false};
            bool using_own_frame_allocation       = {false};
            bool nothing_decoded_yet_             = {true};
            int current_frame_                    = {CURRENT_FRAME_UNKNOWN};
            Imath::V2i resolution_                = {Imath::V2i(0, 0)};
            float pixel_aspect_                   = 1.0f;

            // for video rescaling
            SwsContext *sws_context_ = {nullptr};

            // for audio resampling
            AVSampleFormat target_sample_format_ = {AV_SAMPLE_FMT_NONE};
            int target_sample_rate_              = 0;
            int target_audio_channels_           = 0;
            AVSampleFormat src_audio_fmt_        = {AV_SAMPLE_FMT_NONE};
            int src_audio_sample_rate_           = {0};
            int src_audio_channel_layout_        = {0};
            SwrContext *audio_resampler_ctx_     = {0};

            utility::FrameRate frame_rate_;
        };
    } // namespace ffmpeg
} // namespace media_reader
} // namespace xstudio
