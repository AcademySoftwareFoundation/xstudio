// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
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

using namespace caf;
using namespace xstudio::audio;
using namespace xstudio::utility;
using namespace xstudio;

void AudioOutputActor::init() {

    // spdlog::debug("Created AudioOutputControlActor {}", OutputClassType::name());
    utility::print_on_exit(this, "AudioOutputControlActor");

    audio_output_device_ =
        spawn<AudioOutputDeviceActor>(caf::actor_cast<caf::actor>(this), output_device_);
    link_to(audio_output_device_);

    auto global_audio_actor =
        system().registry().template get<caf::actor>(audio_output_registry);
    utility::join_event_group(this, global_audio_actor);

    behavior_.assign(

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
            send(
                audio_output_device_, utility::event_atom_v, playhead::play_atom_v, is_playing);
            if (!is_playing)
                clear_queued_samples();
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
            const utility::Uuid &sub_playhead) {
            if (sub_playhead != sub_playhead_uuid_) {
                // sound is coming from a different source to
                // previous time
                clear_queued_samples();
                sub_playhead_uuid_ = sub_playhead;
            }
            queue_samples_for_playing(audio_buffers);
            send(audio_output_device_, utility::event_atom_v, playhead::play_atom_v);            
        },
        [=](utility::event_atom,
            playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed) {
            // these event messages are very fine-grained, so we know very accurately the playhead
            // position during playback
            playhead_position_changed(
                playhead_position, forward, velocity, playing, when_position_changed);
        });

    // kicks the global samples actor to update us with current volume etc.
    send(
        global_audio_actor,
        module::change_attribute_event_atom_v,
        caf::actor_cast<caf::actor>(this));
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

    // by setting static UUIDs on these module we only create them once in the UI
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

        [=](playhead::play_atom, const bool is_playing) {
            send(event_group_, utility::event_atom_v, playhead::play_atom_v, is_playing);
        },
        [=](playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed) {
            send(
                event_group_,
                utility::event_atom_v,
                playhead::position_atom_v,
                playhead_position,
                forward,
                velocity,
                playing,
                when_position_changed
                );
        },
        [=](playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const Uuid &sub_playhead_id) {
            send(
                event_group_,
                utility::event_atom_v,
                playhead::sound_audio_atom_v,                
                audio_buffers,
                sub_playhead_id);
        },
        [=](module::change_attribute_event_atom, caf::actor requester) {
            send(
                requester,
                utility::event_atom_v,
                module::change_attribute_event_atom_v,
                volume_->value(),
                muted_->value(),
                audio_repitch_->value(),
                audio_scrubbing_->value());
        }

    );

    connect_to_ui();
}

void GlobalAudioOutputActor::on_exit() { system().registry().erase(audio_output_registry); }

void GlobalAudioOutputActor::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    // update and audio output clients with volume, mute etc.
    send(
        event_group_,
        utility::event_atom_v,
        module::change_attribute_event_atom_v,
        volume_->value(),
        muted_->value(),
        audio_repitch_->value(),
        audio_scrubbing_->value());

    Module::attribute_changed(attr_uuid, role);
}

void GlobalAudioOutputActor::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {

    if (hotkey_uuid == mute_hotkey_) {
        muted_->set_value(!muted_->value());
    }
}
