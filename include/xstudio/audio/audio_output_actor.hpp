// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/audio/audio_output.hpp"
#include "xstudio/audio/audio_output_device.hpp"

namespace xstudio::audio {

class AudioOutputDeviceActor : public caf::event_based_actor {

  public:
    AudioOutputDeviceActor(
        caf::actor_config &cfg,
        caf::actor samples_actor,
        std::shared_ptr<AudioOutputDevice> output_device)
        : caf::event_based_actor(cfg),
          playing_(false),
          waiting_for_samples_(false),
          audio_samples_actor_(samples_actor),
          output_device_(output_device) {

        // spdlog::debug("Created {} {}", "AudioOutputDeviceActor", OutputClassType::name());
        // utility::print_on_exit(this, OutputClassType::name());

        /*try {
            auto prefs = global_store::GlobalStoreHelper(system());
            utility::JsonStore j;
            utility::join_broadcast(this, prefs.get_group(j));
            open_output_device(j);
        } catch (...) {
            open_output_device(utility::JsonStore());
        }*/

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
                if (output_device_) {
                    output_device_->initialize_sound_card();
                }
            },
            [=](utility::event_atom, playhead::play_atom) {
                // we get this message every time the AudioOutputActor has
                // received samples to play.
                // connect to the sound output device if necessary
                if (output_device_)
                    output_device_->connect_to_soundcard();

                if (!waiting_for_samples_) {
                    // start playback loop
                    anon_send(actor_cast<caf::actor>(this), push_samples_atom_v);
                }
            },
            [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
                if (!is_playing && output_device_) {
                    // this stops the loop pushing samples to the soundcard
                    if (playing_) {
                        // we've stopped playing, so clear samples in the
                        // buffer
                        output_device_->disconnect_from_soundcard();
                    }
                    playing_ = false;
                    //
                } else if (is_playing && !playing_) {
                    // start loop
                    playing_ = true;
                }
            },
            [=](push_samples_atom) {

                if (!output_device_)
                    return;

                /*if (!num_samps_soundcard_wants) {
                    // soundcard buffer is probably full. Wait 1ms, and continue
                    // continue the loop
                    if (playing_) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        anon_send(
                            actor_cast<caf::actor>(this), push_samples_atom_v);
                    }
                    return;
                }*/

                // The 'waiting_for_samples_' flag allows us to ensure that we
                // don't have multiple requests for samples to play in flight -
                // since each response to a request then sends another
                // 'push_samples_atom' atom (to keep playback running), having multiple
                // requests in flight completely messes up the audio playback as
                // essentially we have two loops running within the single actor.
                if (waiting_for_samples_)
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
                            waiting_for_samples_ = false;
                            if (output_device_->push_samples(
                                    (const void *)samples_to_play.data(),
                                    samples_to_play.size())) {

                                // continue the loop
                                if (playing_) {
                                    anon_send(
                                        actor_cast<caf::actor>(this), push_samples_atom_v);
                                }
                            }
                        },
                        [=](caf::error &err) mutable { waiting_for_samples_ = false; });
            }

        );
    }

    ~AudioOutputDeviceActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return name_.c_str(); }

  protected:
    std::shared_ptr<AudioOutputDevice> output_device_;

  private:
    caf::behavior behavior_;
    std::string name_;
    bool playing_;
    caf::actor audio_samples_actor_;
    bool waiting_for_samples_;
};

class AudioOutputActor : public caf::event_based_actor, AudioOutputControl {

  public:
    AudioOutputActor(caf::actor_config &cfg, std::shared_ptr<AudioOutputDevice> output_device)
        : caf::event_based_actor(cfg), output_device_(output_device) {
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
    std::shared_ptr<AudioOutputDevice> output_device_;
    caf::actor playhead_;
};

/* Singleton class that receives audio sample buffers from the current
playhead during playback. It re-broadcasts these samples to any AudioOutputActor
that has been instanced. */
class GlobalAudioOutputActor : public caf::event_based_actor, module::Module {

  public:
    GlobalAudioOutputActor(caf::actor_config &cfg);
    ~GlobalAudioOutputActor() override = default;

    void on_exit() override;

    void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

    caf::behavior make_behavior() override {
        return behavior_.or_else(module::Module::message_handler());
    }

    void hotkey_pressed(
        const utility::Uuid &hotkey_uuid,
        const std::string &context,
        const std::string &window) override;


  private:
    caf::actor event_group_;
    caf::message_handler behavior_;
    module::BooleanAttribute *audio_repitch_;
    module::BooleanAttribute *audio_scrubbing_;
    module::FloatAttribute *volume_;
    module::BooleanAttribute *muted_;
    utility::Uuid mute_hotkey_;
};

} // namespace xstudio::audio
