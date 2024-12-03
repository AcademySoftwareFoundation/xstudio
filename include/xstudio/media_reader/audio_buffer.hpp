// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/audio/audio_output_device.hpp"
#include "xstudio/media_reader/buffer.hpp"

namespace xstudio {
namespace media_reader {

    class AudioBuffer : public Buffer {
      public:
        using Buffer::allocate;

        AudioBuffer(const utility::JsonStore &params = utility::JsonStore()) : Buffer(params) {}
        ~AudioBuffer() override = default;

        void allocate(
            const uint64_t _sample_rate,
            const int _num_channels,
            const uint64_t _num_samples,
            const audio::SampleFormat _sample_format) {
            sample_rate_   = _sample_rate;
            num_channels_  = _num_channels;
            num_samples_   = _num_samples;
            sample_format_ = _sample_format;

            size_t samp_size = 1;
            switch (_sample_format) {

            case audio::SampleFormat::UINT8:
                samp_size = 1;
                break;
            case audio::SampleFormat::INT16:
                samp_size = 2;
                break;
            case audio::SampleFormat::SFINT32:
                samp_size = 4;
                break;
            case audio::SampleFormat::FLOAT32:
                samp_size = 4;
                break;
            case audio::SampleFormat::INT64:
                samp_size = 8;
                break;
            case audio::SampleFormat::DOUBLE64:
                samp_size = 8;
                break;
            default:
                samp_size = 1;
            }

            Buffer::allocate(num_samples_ * num_channels_ * samp_size);
        }

        void extend_size(size_t size_extension) { Buffer::resize(size() + size_extension); }

        void extend(const AudioBuffer &o) {
            if (o.sample_format() != sample_format() || o.num_channels() != num_channels()) {
                throw std::runtime_error(
                    "AudioBuffer::extend mistmatch in audio buffer formats.");
            }
            extend_size(o.size());
            memcpy(buffer() + num_samples() * num_channels() * 2, o.buffer(), o.size());
            num_samples_ += o.num_samples();
        }

        [[nodiscard]] uint64_t sample_rate() const { return sample_rate_; }
        [[nodiscard]] int num_channels() const { return num_channels_; }
        [[nodiscard]] long num_samples() const { return long(num_samples_); }
        [[nodiscard]] audio::SampleFormat sample_format() const { return sample_format_; }
        [[nodiscard]] double duration_seconds() const {
            return sample_rate_ ? double(num_samples_) / double(sample_rate_) : 0.0;
        }
        [[nodiscard]] bool reversed() const { return reversed_; }
        [[nodiscard]] std::chrono::microseconds time_delta_to_video_frame() const {
            return time_delta_to_video_frame_;
        }

        void set_num_samples(const size_t n) { num_samples_ = n; }
        void set_reversed(const bool r) { reversed_ = r; }
        void set_time_delta_to_video_frame(const std::chrono::microseconds d) {
            time_delta_to_video_frame_ = d;
        }

        [[nodiscard]] const media::MediaKey &media_key() const { return media_key_; }
        void set_media_key(const media::MediaKey &key) { media_key_ = key; }

      private:
        uint64_t sample_rate_              = {0};
        int num_channels_                  = {0};
        uint64_t num_samples_              = {0};
        audio::SampleFormat sample_format_ = {audio::SampleFormat::UNSET};
        media::MediaKey media_key_;
        bool reversed_                                       = {false};
        std::chrono::microseconds time_delta_to_video_frame_ = {std::chrono::microseconds(0)};
    };

    /* Extending std::shared_ptr<AudioBufPtr> by adding a time point telling us
    when the audio samples should be sounded */
    class AudioBufPtr : public std::shared_ptr<AudioBuffer> {
      public:
        using Base = std::shared_ptr<AudioBuffer>;

        AudioBufPtr() = default;
        AudioBufPtr(AudioBuffer *imbuf) : Base(imbuf) {}
        AudioBufPtr(const AudioBufPtr &o)
            : Base(static_cast<const Base &>(o)), when_to_display_(o.when_to_display_), tts_(o.tts_) {}

        AudioBufPtr &operator=(const AudioBufPtr &o) {
            Base &b          = static_cast<Base &>(*this);
            b                = static_cast<const Base &>(o);
            when_to_display_ = o.when_to_display_;
            tts_ = o.tts_;
            return *this;
        }

        ~AudioBufPtr() = default;

        // equivalence operator based on the audio samples only
        bool operator==(const AudioBufPtr &o) const { return this->get() == o.get(); }

        bool operator<(const AudioBufPtr &o) const {
            return when_to_display_ < o.when_to_display_;
        }

        bool operator<(const utility::time_point &t) const { return when_to_display_ < t; }

        utility::time_point when_to_display_;

        [[nodiscard]] const timebase::flicks &timeline_timestamp() const { return tts_; }
        void set_timline_timestamp(const timebase::flicks tts) { tts_ = tts; }

      private:
        timebase::flicks tts_ = timebase::flicks{0};
    };

} // namespace media_reader
} // namespace xstudio