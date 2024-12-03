// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ffmpeg_stream.hpp"

namespace xstudio {
namespace media_reader {
    namespace ffmpeg {

        typedef std::shared_ptr<FFMpegStream> StreamPtr;

        class FFMpegDecoder {
          public:
            FFMpegDecoder(
                std::string path,
                const int soundcard_sample_rate,
                const utility::FrameRate default_rate,
                std::string stream_id = "");

            ~FFMpegDecoder();

            void decode_video_frame(
                const int64_t frame_num,
                ImageBufPtr &image_buffer,
                std::vector<unsigned int> stream_indeces = {1});

            void decode_audio_frame(
                const int64_t frame_num,
                AudioBufPtr &audio_buffer,
                std::vector<unsigned int> stream_indeces = {1});

            std::shared_ptr<thumbnail::ThumbnailBuffer>
            decode_thumbnail_frame(const int64_t frame_num, const size_t size_hint);

            const std::string &path() const { return movie_file_path_; }
            int64_t duration_frames() const { return duration_frames_; }
            utility::FrameRate frame_rate(unsigned int stream_idx = UINT_MAX) const;
            utility::Timecode first_frame_timecode();

            static int ffmpeg_threads;

            const std::map<unsigned int, StreamPtr> &streams() const { return streams_; };
            StreamPtr stream(unsigned int index) { return streams_[index]; }

          protected:
            void open_handles();
            void calc_duration_frames();
            void exclude_unwanted_streams();
            void close_handles();
            int64_t decode_and_store_next_frame();
            int64_t decode_next_frame();

            ImageBufPtr get_frame_and_release_from_mini_cache(const int frame_num);

            AudioBufPtr get_audio_frame_and_release_from_mini_cache(const int frame_num);

            bool have_video(const int frame_num) const;
            bool have_audio(const int frame_num) const;

            void pull_audio_buffer_from_stream(StreamPtr &audio_stream);
            void pull_video_buffer_from_stream(StreamPtr &video_stream);
            void pull_buffer_from_stream(StreamPtr &stream);

            void do_seek(const int seek_frame, const bool force = false);
            void empty_mini_caches(const int decoded_frame);
            bool is_single_frame() const;

            std::string movie_file_path_;
            int64_t last_requested_frame_;
            int64_t last_decoded_frame_ = {-1};
            int64_t requested_decode_frame_;
            AVPacket *avc_packet_;
            AVFormatContext *av_format_ctx_;
            StreamPtr decode_stream_;
            StreamPtr timecode_stream_;

            std::map<int, ImageBufPtr> video_frame_mini_cache_;
            std::map<int, AudioBufPtr> audio_frame_mini_cache_;

            std::map<unsigned int, StreamPtr> streams_;
            bool decoding_backwards_;
            const int soundcard_sample_rate_;
            int64_t duration_frames_;
            const std::string stream_id_;
            const utility::FrameRate default_rate_;
        };
    } // namespace ffmpeg
} // namespace media_reader
} // namespace xstudio
