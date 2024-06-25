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
#include "xstudio/audio/linux_audio_output_device.hpp"
#endif
#ifdef _WIN32
#include "xstudio/audio/windows_audio_output_device.hpp"
#endif

using namespace caf;
using namespace xstudio::audio;
using namespace xstudio::utility;
using namespace xstudio;

GlobalAudioOutputActor::GlobalAudioOutputActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg), module::Module("GlobalAudioOutputActor")
{

    audio_repitch_ = add_boolean_attribute("Audio Repitch", "Audio Repitch", false);
    audio_repitch_->set_role_data(
        module::Attribute::PreferencePath, "/core/audio/audio_repitch");

    audio_scrubbing_ = add_boolean_attribute("Audio Scrubbing", "Audio Scrubbing", false);
    audio_scrubbing_->set_role_data(
        module::Attribute::PreferencePath, "/core/audio/audio_scrubbing");

    volume_ = add_float_attribute("volume", "volume", 100.0f, 0.0f, 100.0f, 0.05f);
    volume_->set_role_data(module::Attribute::PreferencePath, "/core/audio/volume");

    // by setting static UUIDs on these module we only create them once in the UI
    volume_->set_role_data(module::Attribute::Groups, nlohmann::json{"audio_output"});

    muted_ = add_boolean_attribute("muted", "muted", false);
    muted_->set_role_data(module::Attribute::Groups, nlohmann::json{"audio_output"});
    muted_->set_role_data(module::Attribute::PreferencePath, "/core/audio/muted");

    spdlog::debug("Created GlobalAudioOutputActor");
    print_on_exit(this, "GlobalAudioOutputActor");

    system().registry().put(audio_output_registry, this);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

    behavior_.assign(

        [=](utility::get_event_group_atom) -> caf::actor { 
            return event_group_; 
        },

        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](playhead::play_atom, const bool is_playing) {
            send(event_group_, utility::event_atom_v, playhead::play_atom_v, is_playing);
        },
        [=](playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const Uuid &sub_playhead,
            const bool playing,
            const bool forwards,
            const float velocity) {

            send(event_group_,
                utility::event_atom_v,
                playhead::sound_audio_atom_v,
                audio_buffers,
                sub_playhead,
                playing,
                forwards,
                velocity);

        }

    );

    connect_to_ui();

    

}

void GlobalAudioOutputActor::on_exit() { system().registry().erase(audio_output_registry); }

void GlobalAudioOutputActor::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    // update and audio output clients with volume, mute etc.
    send(event_group_,
        utility::event_atom_v,
        module::change_attribute_event_atom_v,
        volume_->value(),
        muted_->value(),
        audio_repitch_->value(),
        audio_scrubbing_->value());

    Module::attribute_changed(attr_uuid, role);
}
