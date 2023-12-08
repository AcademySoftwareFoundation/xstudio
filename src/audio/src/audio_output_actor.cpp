// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <cmath>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"

// include for system (soundcard) audio output
#ifdef __linux__
#include "linux_audio_output_device.hpp"
#elif __APPLE__
// TO DO
#elif _WIN32
// TO DO
#endif


using namespace caf;
using namespace xstudio::audio;
using namespace xstudio::utility;
using namespace xstudio;

AudioOutputDeviceActor::AudioOutputDeviceActor(
    caf::actor_config &cfg, caf::actor_addr audio_playback_manager, const std::string name)
    : caf::event_based_actor(cfg),
      name_(name),
      playing_(false),
      audio_playback_manager_(std::move(audio_playback_manager)),
      waiting_for_samples_(false) {

    spdlog::debug("Created {} {}", NAME, name_);
    print_on_exit(this, "AudioOutputDeviceActor");

    try {
        auto prefs = global_store::GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        open_output_device(j);
    } catch (...) {
        open_output_device(JsonStore());
    }

    behavior_.assign(

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },
        [=](json_store::update_atom, const JsonStore & /*j*/) {
            // TODO: restart soundcard connection with new prefs
        },
        [=](playhead::play_atom, const bool is_playing) {
            if (!is_playing) {
                // this stops the loop pushing samples to the soundcard
                playing_ = false;
                output_device_->disconnect_from_soundcard();
            } else if (is_playing && !playing_) {
                // start loop
                playing_ = true;
                output_device_->connect_to_soundcard();
                anon_send(actor_cast<caf::actor>(this), push_samples_atom_v);
            }
        },
        [=](push_samples_atom) {
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
                actor_cast<caf::actor>(audio_playback_manager_),
                infinite,
                get_samples_for_soundcard_atom_v,
                num_samps_soundcard_wants,
                (long)output_device_->latency_microseconds(),
                (int)output_device_->num_channels(),
                (int)output_device_->sample_rate())
                .then(
                    [=](const std::vector<int16_t> &samples_to_play) mutable {
                        output_device_->push_samples(
                            (const void *)samples_to_play.data(), num_samps_soundcard_wants);

                        waiting_for_samples_ = false;

                        if (playing_) {
                            anon_send(actor_cast<caf::actor>(this), push_samples_atom_v);
                        }
                    },
                    [=](caf::error &err) mutable { waiting_for_samples_ = false; });
        }

    );
}

void AudioOutputDeviceActor::open_output_device(const utility::JsonStore &prefs) {

    try {
#ifdef __linux__
        output_device_ = std::make_unique<LinuxAudioOutputDevice>(prefs);
#elif __APPLE__
        // TO DO
#elif _WIN32
        // TO DO
#endif
    } catch (std::exception &e) {
        spdlog::debug(
            "{} Failed to connect to an audio device: {}", __PRETTY_FUNCTION__, e.what());
    }
}


AudioOutputControlActor::AudioOutputControlActor(caf::actor_config &cfg, const std::string name)
    : caf::event_based_actor(cfg), name_(name) {
    init();
}

void AudioOutputControlActor::init() {

    spdlog::debug("Created {} {}", NAME, name_);
    print_on_exit(this, "AudioOutputControlActor");

    system().registry().put(audio_output_registry, this);

    audio_output_device_ = spawn<AudioOutputDeviceActor>(actor_cast<caf::actor_addr>(this));
    link_to(audio_output_device_);
    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

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
        [=](playhead::play_atom, const bool is_playing) {
            if (!is_playing) {
                clear_queued_samples();
            }
            anon_send(audio_output_device_, playhead::play_atom_v, is_playing);
        },
        [=](playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const Uuid &sub_playhead,
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

    connect_to_ui();
}


void AudioOutputControlActor::on_exit() { system().registry().erase(audio_output_registry); }
