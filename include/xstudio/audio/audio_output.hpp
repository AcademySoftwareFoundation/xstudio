// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio::audio {

/**
 *  @brief Class for delivering audio to soundcard by maintaining a smoothed
 *  measurment of the playhead position and re-sampling audio sources as
 *  required
 */

class AudioOutputControl {

  public:
    /**
     *  @brief Constructor
     *
     */
    AudioOutputControl(const utility::JsonStore &prefs = utility::JsonStore()) {}

    ~AudioOutputControl() = default;

    /**
     *  @brief Use steady clock combined with soundcard latency to fill a
     *  buffer with sound samples, pulling samples from the queue of timestamped
     *  audio frames.
     *
     */
    void prepare_samples_for_soundcard(
        std::vector<int16_t> &samples,
        const long num_samps_to_push,
        const long microseconds_delay,
        const int num_channels,
        const int sample_rate);

    /**
     *  @brief The audio volume (range is 0-1)
     */
    [[nodiscard]] float volume() const { return volume_; }

    /**
     *  @brief The audio volume muted
     */
    [[nodiscard]] bool muted() const { return muted_; }

    /**
     *   @brief Queue audio buffer for streaming to the soundcard
     */
    void queue_samples_for_playing(
        const std::vector<media_reader::AudioBufPtr> &audio_buffers,
        const bool playing,
        const bool forwards,
        const float velocity);

    /**
     *   @brief Clear all queued audio buffers to immediately stop audio playback
     */
    void clear_queued_samples();

    enum Fade { NoFade = 0, DoFadeHead = 1, DoFadeTail = 2, DoFadeHeadAndTail = 3 };

    /**
     *   @brief Sets volume etc - these settings come from the global audio output
     *   module.
     */
    void set_attrs(
        const float volume,
        const bool muted,
        const bool audio_repitch,
        const bool audio_scrubbing) 
    {
      volume_ = volume;
      muted_ = muted;
      audio_repitch_ = audio_repitch;
      audio_scrubbing_ = audio_scrubbing;
    }

  private:

    media_reader::AudioBufPtr
    pick_audio_buffer(const utility::clock::time_point &tp, const bool drop_old_buffers);

    Fade check_if_buffer_is_contiguous_with_previous_and_next(
        const media_reader::AudioBufPtr &current_buf,
        const media_reader::AudioBufPtr &next_buf,
        const media_reader::AudioBufPtr &previous_buf_);

    std::map<utility::time_point, media_reader::AudioBufPtr> sample_data_;
    media_reader::AudioBufPtr current_buf_;
    media_reader::AudioBufPtr previous_buf_;
    long current_buf_pos_;
    float playback_velocity_ = {1.0f};

    int fade_in_out_ = {NoFade};

    bool audio_repitch_ = {false};
    bool audio_scrubbing_ = {false};
    float volume_ = {100.0f};
    bool muted_ = {false};

};
} // namespace xstudio::audio
