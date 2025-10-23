// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <cmath>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"

// include for system (soundcard) audio output
#ifdef __linux__
#include "xstudio/audio/linux_audio_output_device.hpp"
#endif
#ifdef _WIN32
#include "xstudio/audio/windows_audio_output_device.hpp"
#endif
#ifdef __APPLE__
#include "xstudio/audio/macos_audio_output_device.hpp"
#endif

using namespace caf;
using namespace xstudio::audio;
using namespace xstudio::utility;
using namespace xstudio;

/* Notes:

In order to support multiple audio outputs (e.g. PC soundcard, SDI output card
etc.) receiving samples from multiple playing playheads, the audio output model
is a somewhat complex. The model for audio playback is described as follows:

During playback a PlayheadActor delivers audio samples to the singleton
GlobalAudioOutputActor instance.

The GlobalAudioOutputActor then forwards the samples data to AudioOutputActors
that then deliver the samples to sound devices.

For the active playhead in the xSTUDIO's main UI, the audio samples from that
playhead are forwarded via the event_group of the GlobalAudioOutputActor.
The default PC audio AudioOutputActor, plus AudioOutputActor from video output
plugins (e.g. SDI card AV output plugin) will receive and play audio from this
event_group.

For playheads belonging to the xSTUDIO 'quicview' light viewers, a separate
instance of the AudioOutputActor delivering to the PC audio is created and
samples from those playheads are delivered to these separate AudioOutputActor
instances. This allows independent audio playback in the quickviewer windows
from the main xSTUDIO UI.

*/


void AudioOutputActor::init() {

    // spdlog::debug("Created AudioOutputControlActor {}", OutputClassType::name());
    utility::print_on_exit(this, "AudioOutputControlActor");

    audio_output_device_ =
        spawn<AudioOutputDeviceActor>(caf::actor_cast<caf::actor>(this), output_device_);
    link_to(audio_output_device_);

    auto global_audio_actor =
        system().registry().template get<caf::actor>(audio_output_registry);

    if (is_global_) {
        // we only want to receive audio via the global_audio_actor event group
        // if we are marked as a global AudioOutputActor - this is how we play
        // audio from whatever is playing in the xstudio main UI.
        utility::join_event_group(this, global_audio_actor);
    }

    behavior_.assign(

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
            mail(utility::event_atom_v, playhead::play_atom_v, is_playing)
                .send(audio_output_device_);
            if (!is_playing)
                clear_queued_samples();
        },

        [=](utility::event_atom, playhead::play_atom) {
            // this message will force the audio_output_device_ actor to start
            // streaming samples to the soundcard (whether or not xSTUDIO playhead
            // is delivering audio samples to us). This is useful for some video
            // output plugins that might need to continually stream audio samples
            // regardless of whether there is actually audio to play. So we can
            // force it to stream silence this way.
            mail(utility::event_atom_v, playhead::play_atom_v).send(audio_output_device_);
        },

        [=](get_samples_for_soundcard_atom,
            const long num_samps_to_push,
            const long microseconds_delay,
            const int num_channels,
            const int sample_rate) -> result<std::vector<int16_t>> {
            // this message is a request from the AudioOutputDeviceActor to
            // get samples for the soundcard

            std::vector<int16_t> samples;

            if (audio_mode_on_silence_ == StopPushingSamplesOnSilence &&
                (muted() || (!playing_ && !audio_scrubbing_)))
                return samples;

            samples.resize(num_samps_to_push * num_channels);
            memset(samples.data(), 0, samples.size() * sizeof(int16_t));

            try {

                if (!playing_) {

                    long n = copy_samples_to_buffer_for_scrubbing(samples, num_samps_to_push);
                    if (audio_mode_on_silence_ == StopPushingSamplesOnSilence && !n) {
                        // we have no audio to play back (e.g. scrubbed audio)
                        // If more than 2s have elapsed since last samples then
                        // by clearing 'samples' here this stops the playback
                        // loop feeding samples to the soundcard
                        if (std::chrono::duration_cast<std::chrono::seconds>(
                                utility::clock::now() - last_audio_sounding_tp_)
                                .count() > 2) {
                            samples.clear();
                        }
                    } else {
                        last_audio_sounding_tp_ = utility::clock::now();
                    }

                } else {
                    prepare_samples_for_soundcard_playback(
                        samples,
                        num_samps_to_push,
                        microseconds_delay,
                        num_channels,
                        sample_rate);
                }

            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
            return samples;
        },
        [=](set_override_volume_atom, const float volume) { set_override_volume(volume); },
        [=](utility::event_atom,
            module::change_attribute_event_atom,
            const float volume,
            const bool muted,
            const bool repitch,
            const bool scrubbing) { set_attrs(volume, muted, repitch, scrubbing); },
        [=](utility::event_atom,
            playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const utility::Uuid &sub_playhead,
            const bool scrubbing,
            const timebase::flicks playhead_position) {
            if (scrubbing) {

                prepare_samples_for_audio_scrubbing(audio_buffers, playhead_position);
                mail(utility::event_atom_v, playhead::play_atom_v).send(audio_output_device_);

            } else {

                if (sub_playhead != sub_playhead_uuid_ || scrubbing) {
                    // sound is coming from a different source to
                    // previous time
                    clear_queued_samples();
                    sub_playhead_uuid_ = sub_playhead;
                }
                queue_samples_for_playing(audio_buffers);
                mail(utility::event_atom_v, playhead::play_atom_v).send(audio_output_device_);
            }
        },
        [=](utility::event_atom,
            playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed) {
            // these event messages are very fine-grained, so we know very accurately the
            // playhead position during playback
            playhead_position_changed(
                playhead_position, forward, velocity, playing, when_position_changed);
        },
        [=](utility::event_atom,
            audio::audio_samples_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            timebase::flicks playhead_position,
            const utility::Uuid &playhead_uuid) {

        });

    // kicks the global samples actor to update us with current volume etc.
    mail(module::change_attribute_event_atom_v, caf::actor_cast<caf::actor>(this))
        .send(global_audio_actor);
}

GlobalAudioOutputActor::GlobalAudioOutputActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg), module::Module("GlobalAudioOutputActor") {

    audio_repitch_ = add_boolean_attribute("Audio Repitch", "Audio Repitch", false);
    audio_repitch_->set_role_data(
        module::Attribute::PreferencePath, "/core/audio/audio_repitch");

    audio_scrubbing_ = add_boolean_attribute("Audio Scrubbing", "Audio Scrubbing", false);
    audio_scrubbing_->set_role_data(
        module::Attribute::PreferencePath, "/core/audio/audio_scrubbing");

    volume_ = add_float_attribute("volume", "volume", 100.0f, 0.0f, 100.0f, 0.05f);
    volume_->set_role_data(module::Attribute::PreferencePath, "/core/audio/volume");
    volume_->set_role_data(module::Attribute::UIDataModels, nlohmann::json{"audio_output"});

    muted_ = add_boolean_attribute("muted", "muted", false);
    muted_->set_role_data(module::Attribute::UIDataModels, nlohmann::json{"audio_output"});
    muted_->set_role_data(module::Attribute::PreferencePath, "/core/audio/muted");

    spdlog::debug("Created GlobalAudioOutputActor");
    print_on_exit(this, "GlobalAudioOutputActor");

    system().registry().put(audio_output_registry, this);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

    mute_hotkey_ = register_hotkey("/", "Mute Audio", "Mute/Un-mute audio.", true, "Playback");

    behavior_.assign(

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](playhead::play_atom,
            const bool is_playing,
            bool global,
            const utility::Uuid &playhead_uuid) {
            auto dest = global ? event_group_ : independent_output(playhead_uuid);
            if (dest) {
                mail(utility::event_atom_v, playhead::play_atom_v, is_playing).send(dest);
            }
        },
        [=](playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed,
            bool global,
            const utility::Uuid &playhead_uuid) {
            auto dest = global ? event_group_ : independent_output(playhead_uuid);

            if (dest) {
                mail(
                    utility::event_atom_v,
                    playhead::position_atom_v,
                    playhead_position,
                    forward,
                    velocity,
                    playing,
                    when_position_changed)
                    .send(dest);
            }
        },
        [=](playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const Uuid &sub_playhead_id,
            bool global,
            const utility::Uuid &playhead_uuid,
            const bool scrubbing,
            const timebase::flicks playhead_position) {
            auto dest = global ? event_group_ : independent_output(playhead_uuid);

            if (dest) {
                mail(
                    utility::event_atom_v,
                    playhead::sound_audio_atom_v,
                    audio_buffers,
                    sub_playhead_id,
                    scrubbing,
                    playhead_position)
                    .send(dest);
            }
        },
        [=](module::change_attribute_event_atom, caf::actor requester) {
            mail(
                utility::event_atom_v,
                module::change_attribute_event_atom_v,
                volume_->value(),
                muted_->value(),
                audio_repitch_->value(),
                audio_scrubbing_->value())
                .send(requester);
        },
        [=](audio::audio_samples_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            timebase::flicks playhead_position,
            const utility::Uuid &playhead_uuid) {
            mail(
                utility::event_atom_v,
                audio::audio_samples_atom_v,
                audio_buffers,
                playhead_position,
                playhead_uuid)
                .send(event_group_);
        },
        [=](playhead::sound_audio_atom, const utility::Uuid &playhead_uuid, bool) {
            // playhead is exiting, if the playhead has its own audid output
            // actor, kill it now
            auto p = independent_outputs_.find(playhead_uuid);
            if (p != independent_outputs_.end()) {
                send_exit(p->second, caf::exit_reason::user_shutdown);
                independent_outputs_.erase(p);
            }
        }

    );

    connect_to_ui();
}

void GlobalAudioOutputActor::on_exit() { system().registry().erase(audio_output_registry); }

void GlobalAudioOutputActor::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    // update and audio output clients with volume, mute etc.
    mail(
        utility::event_atom_v,
        module::change_attribute_event_atom_v,
        volume_->value(),
        muted_->value(),
        audio_repitch_->value(),
        audio_scrubbing_->value())
        .send(event_group_);

    for (auto &p : independent_outputs_) {
        mail(
            utility::event_atom_v,
            module::change_attribute_event_atom_v,
            volume_->value(),
            muted_->value(),
            audio_repitch_->value(),
            audio_scrubbing_->value())
            .send(p.second);
    }

    Module::attribute_changed(attr_uuid, role);
}

void GlobalAudioOutputActor::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {

    if (hotkey_uuid == mute_hotkey_) {
        muted_->set_value(!muted_->value());
    }
}

caf::actor GlobalAudioOutputActor::independent_output(const utility::Uuid &playhead_uuid) {

    auto p = independent_outputs_.find(playhead_uuid);
    if (p != independent_outputs_.end()) {
        return p->second;
    }

    try {
        JsonStore prefs_jsn;
        auto prefs = global_store::GlobalStoreHelper(system());
        join_broadcast(this, prefs.get_group(prefs_jsn));

#ifdef __linux__
        auto output_actor = spawn<audio::AudioOutputActor>(
            std::shared_ptr<audio::AudioOutputDevice>(
                new audio::LinuxAudioOutputDevice(prefs_jsn)),
            false);
#elif defined(__APPLE__)
        // TO DO
        auto output_actor = spawn<audio::AudioOutputActor>(
            std::shared_ptr<audio::AudioOutputDevice>(
                new audio::MacOSAudioOutputDevice(prefs_jsn)),
            false);
#elif defined(_WIN32)
        auto output_actor = spawn<audio::AudioOutputActor>(
            std::shared_ptr<audio::AudioOutputDevice>(
                new audio::WindowsAudioOutputDevice(prefs_jsn)),
            false);
#endif

        link_to(output_actor);
        independent_outputs_[playhead_uuid] = output_actor;
        return output_actor;
    } catch (std::exception &e) {
        independent_outputs_[playhead_uuid] = caf::actor();
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return caf::actor();
}