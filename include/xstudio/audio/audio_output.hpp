// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>

#include "xstudio/audio/enums.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio::audio {

enum AudioBehaviourOnSilence { StopPushingSamplesOnSilence, ContinuePushingSamplesOnSilence };


static inline std::map<std::string, ScrubBehaviour> scrubBehaviourMap = {
    {"1 Frame", OneFrame},
    {"1.25 Frames", OnePt25Frames},
    {"1.5 Frames", OnePt5Frames},
    {"2 Frames", TwoFrames},
    {"3 Frames", ThreeFrames},
    {"1/24th Sec", OneFrameAt24Fps},
    {"1/30th Sec", OneFrameAt30Fps},
    {"1/60th Sec", OneFrameAt60Fps},
    {"Custom Duration", Custom},
};

struct ScrubHelper {

    ScrubHelper() = default;

    void set_custom_duration_ms(const int ms) { scrub_window_millisecs_ = ms; }
    void set_behaviour(const std::string &behaviour) {
        const auto p = scrubBehaviourMap.find(behaviour);
        if (p != scrubBehaviourMap.end())
            scrub_behaviour_ = p->second;
        else
            scrub_behaviour_ = ScrubBehaviour::OneFrame;
    }

    timebase::flicks scrub_duration(const utility::FrameRate &media_rate) const;
    double scrub_duration_secs(const utility::FrameRate &media_rate) const {
        return timebase::to_seconds(scrub_duration(media_rate));
    }

  private:
    int scrub_window_millisecs_     = {50};
    ScrubBehaviour scrub_behaviour_ = {OneFrame};
};

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
    void prepare_samples_for_soundcard_playback(
        std::vector<int16_t> &samples,
        const long num_samps_to_push,
        const long microseconds_delay,
        const int num_channels,
        const int sample_rate);

    /**
     *  @brief Pick audio samples based on the current playhead position to sound
     *  audio during timeline scrubbing.
     *
     */
    long copy_samples_to_buffer_for_scrubbing(
        std::vector<int16_t> &samples, const long num_samps_to_push);

    /**
     *  @brief The audio volume (range is 0-100)
     */
    [[nodiscard]] float volume() const {
        return (override_volume_ == -1.0f ? volume_ : override_volume_) * playhead_volume_ /
               100.0f;
    }

    /**
     *  @brief The audio volume muted
     */
    [[nodiscard]] bool muted() const { return override_volume_ == -1.0f ? muted_ : false; }

    /**
     *   @brief Queue audio buffer for streaming to the soundcard
     */
    void queue_samples_for_playing(const std::vector<media_reader::AudioBufPtr> &audio_buffers);

    /**
     *   @brief Queue audio buffer for streaming to the soundcard during
     *   timeline scrubbing
     */
    void prepare_samples_for_audio_scrubbing(
        const std::vector<media_reader::AudioBufPtr> &audio_buffers,
        const timebase::flicks playhead_position);

    /**
     *   @brief Fine grained update of playhead position
     */
    void playhead_position_changed(
        const timebase::flicks playhead_position,
        const timebase::flicks playhead_loop_in,
        const timebase::flicks playhead_loop_out,
        const bool forward,
        const float velocity,
        const bool playing,
        utility::time_point when_position_changed);

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
        const bool audio_scrubbing,
        const std::string &scrub_behaviour,
        const int scrub_window_millisecs) {
        volume_          = volume;
        muted_           = muted;
        audio_repitch_   = audio_repitch;
        audio_scrubbing_ = audio_scrubbing;
        scrub_helper_.set_behaviour(scrub_behaviour);
        scrub_helper_.set_custom_duration_ms(scrub_window_millisecs);
    }

    void set_override_volume(const float override_volume) {
        override_volume_ = override_volume;
    }

  protected:
    media_reader::AudioBufPtr
    pick_audio_buffer(const utility::clock::time_point &tp, const bool drop_old_buffers);

    Fade check_if_buffer_is_contiguous_with_previous_and_next(
        const media_reader::AudioBufPtr &current_buf,
        const media_reader::AudioBufPtr &next_buf,
        const media_reader::AudioBufPtr &previous_buf_);

    // the actual sound samples that we are about to play, measured against
    // their timestamp in the xstudio plyhead timeline
    std::map<timebase::flicks, media_reader::AudioBufPtr> sample_data_;

    // a dynamic buffer of samples to be streamed to soundcard during
    // scrubbing.
    std::vector<int16_t> scrubbing_samples_buf_;

    media_reader::AudioBufPtr current_buf_;
    media_reader::AudioBufPtr previous_buf_;
    media_reader::AudioBufPtr next_buf_;
    long current_buf_pos_;
    float playback_velocity_ = {1.0f};

    int fade_in_out_ = {NoFade};

    timebase::flicks playhead_position_;
    timebase::flicks playhead_loop_in_ = {
        timebase::flicks(std::numeric_limits<timebase::flicks::rep>::lowest())};
    timebase::flicks playhead_loop_out_ = {
        timebase::flicks(std::numeric_limits<timebase::flicks::rep>::max())};
    bool playing_forward_ = {true};
    utility::time_point playhead_position_update_tp_;
    timebase::flicks last_buffer_pts_;

    bool audio_repitch_                = {false};
    bool audio_scrubbing_              = {false};
    float volume_                      = {100.0f};
    bool muted_                        = {false};
    bool playing_                      = {false};
    float override_volume_             = {-1.0f};
    float playhead_volume_             = {100.0f};
    float last_volume_                 = {100.0f};
    float scrub_chunk_duration_frames_ = {1.0f};
    bool apply_global_volume_          = {true};

    ScrubHelper scrub_helper_;
};
} // namespace xstudio::audio
