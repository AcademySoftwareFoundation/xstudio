// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/audio/audio_output.hpp"
#include "xstudio/audio/audio_output_device.hpp"

namespace xstudio::audio {

template <typename OutputClassType>
class AudioOutputDeviceActor : public caf::event_based_actor {

  public:

    AudioOutputDeviceActor(
        caf::actor_config &cfg,
        caf::actor samples_actor)
    : caf::event_based_actor(cfg),
      playing_(false),
      waiting_for_samples_(false),
      audio_samples_actor_(samples_actor) {

      //spdlog::info("Created {} {}", "AudioOutputDeviceActor", OutputClassType::name());
      //utility::print_on_exit(this, OutputClassType::name());

      try {
          auto prefs = global_store::GlobalStoreHelper(system());
          utility::JsonStore j;
          utility::join_broadcast(this, prefs.get_group(j));
          open_output_device(j);
      } catch (...) {
          open_output_device(utility::JsonStore());
      }

          behavior_.assign(

              [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

              [=](json_store::update_atom,
                  const utility::JsonStore & /*change*/,
                  const std::string & /*path*/,
                  const utility::JsonStore &full) {
                  delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
              },
              [=](json_store::update_atom, const utility::JsonStore & /*j*/) {
                  // TODO: restart soundcard connection with new prefs
              },
              [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
                  if (!is_playing && output_device_) {
                      // this stops the loop pushing samples to the soundcard
                      playing_ = false;
                      output_device_->disconnect_from_soundcard();
                  } else if (is_playing && !playing_) {
                      // start loop
                      playing_ = true;
                      if (output_device_) output_device_->connect_to_soundcard();
                      anon_send(actor_cast<caf::actor>(this), push_samples_atom_v);
                  }
              },
              [=](push_samples_atom) {
                  if (!output_device_) return;
                  // The 'waiting_for_samples_' flag allows us to ensure that we
                  // don't have multiple requests for samples to play in flight -
                  // since each response to a request then sends another
                  // 'push_samples_atom' atom (to keep playback running), having multiple
                  // requests in flight completely messes up the audio playback as
                  // essentially we have two loops running within the single actor.
                  if (waiting_for_samples_ || !playing_)
                      return;
                  waiting_for_samples_ = true;

                  const long num_samps_soundcard_wants = (long)output_device_->desired_samples();
                  auto tt                              = utility::clock::now();
                  request(
                      audio_samples_actor_,
                      infinite,
                      get_samples_for_soundcard_atom_v,
                      num_samps_soundcard_wants,
                      (long)output_device_->latency_microseconds(),
                      (int)output_device_->num_channels(),
                      (int)output_device_->sample_rate())
                      .then(
                          [=](const std::vector<int16_t> &samples_to_play) mutable {

                              output_device_->push_samples(
                                  (const void *)samples_to_play.data(), samples_to_play.size());

                              waiting_for_samples_ = false;

                              if (playing_) {
                                  anon_send(actor_cast<caf::actor>(this), push_samples_atom_v);
                              }
                          },
                          [=](caf::error &err) mutable { waiting_for_samples_ = false; });
              }

          );
      }

      void open_output_device(const utility::JsonStore &prefs) {
          try {
              output_device_ = std::make_unique<OutputClassType>(prefs);
          } catch (std::exception &e) {
              spdlog::error(
                  "{} Failed to connect to an audio device: {}", __PRETTY_FUNCTION__, e.what());
          }
      }

    ~AudioOutputDeviceActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return name_.c_str(); }

  protected:

    std::unique_ptr<AudioOutputDevice> output_device_;

  private:

    caf::behavior behavior_;
    std::string name_;
    bool playing_;
    caf::actor audio_samples_actor_;
    bool waiting_for_samples_;
};

template <typename OutputClassType>
class AudioOutputActor : public caf::event_based_actor, AudioOutputControl {

  public:
    AudioOutputActor(
        caf::actor_config &cfg)
        : caf::event_based_actor(cfg) {
      init();
    }

    ~AudioOutputActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

  private:

    caf::actor audio_output_device_;

    void init();
    void
    get_audio_buffer(caf::actor media_actor, const utility::Uuid uuid, const int source_frame);

    caf::behavior behavior_;
    const utility::JsonStore params_;
    bool playing_       = {false};
    int video_frame_    = {0};
    int retry_on_error_ = {0};
    utility::Uuid uuid_ = {utility::Uuid::generate()};
    utility::Uuid sub_playhead_uuid_;
};

/* Singleton class that receives audio sample buffers from the current 
playhead during playback. It re-broadcasts these samples to any AudioOutputActor
that has been instanced. */
class GlobalAudioOutputActor : public caf::event_based_actor, module::Module {

  public:
    GlobalAudioOutputActor(
        caf::actor_config &cfg);
    ~GlobalAudioOutputActor() override = default;

    void on_exit() override;

    void attribute_changed(const utility::Uuid &attr_uuid, const int role);

    caf::behavior make_behavior() override { return behavior_.or_else(module::Module::message_handler()); }

  private:

    caf::actor event_group_;
    caf::message_handler behavior_;
    module::BooleanAttribute *audio_repitch_;
    module::BooleanAttribute *audio_scrubbing_;
    module::FloatAttribute *volume_;
    module::BooleanAttribute *muted_;

};

template <typename OutputClassType>
void AudioOutputActor<OutputClassType>::init() {

    //spdlog::debug("Created AudioOutputControlActor {}", OutputClassType::name());
    utility::print_on_exit(this, "AudioOutputControlActor");

    audio_output_device_ = spawn<AudioOutputDeviceActor<OutputClassType>>(caf::actor_cast<caf::actor>(this));
    link_to(audio_output_device_);

    auto global_audio_actor = system().registry().template get<caf::actor>(audio_output_registry);
    utility::join_event_group(this, global_audio_actor);

    behavior_.assign(

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
            send(audio_output_device_, utility::event_atom_v, playhead::play_atom_v, is_playing);
        },

        [=](get_samples_for_soundcard_atom,
            const long num_samps_to_push,
            const long microseconds_delay,
            const int num_channels,
            const int sample_rate) -> result<std::vector<int16_t>> {
            std::vector<int16_t> samples;
            try {

                prepare_samples_for_soundcard(
                    samples, num_samps_to_push, microseconds_delay, num_channels, sample_rate);

            } catch (std::exception &e) {

                return caf::make_error(xstudio_error::error, e.what());
            }
            return samples;
        },
        [=](
          utility::event_atom,
          module::change_attribute_event_atom,
          const float volume,
          const bool muted,
          const bool repitch,
          const bool scrubbing) {
            set_attrs(volume, muted, repitch, scrubbing);
        },
        [=](utility::event_atom,
            playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const utility::Uuid &sub_playhead,
            const bool playing,
            const bool forwards,
            const float velocity) {

            if (!playing) {
                clear_queued_samples();
            } else {
                if (sub_playhead != sub_playhead_uuid_) {
                    // sound is coming from a different source to
                    // previous time
                    clear_queued_samples();
                    sub_playhead_uuid_ = sub_playhead;
                }
                queue_samples_for_playing(audio_buffers, playing, forwards, velocity);
            }
        }

    );

}

} // namespace xstudio::audio
