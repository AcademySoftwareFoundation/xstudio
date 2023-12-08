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
        caf::actor_addr audio_playback_manager,
        const std::string name = "AudioOutputDeviceActor");

    ~AudioOutputDeviceActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return NAME.c_str(); }

  private:
    void open_output_device(const utility::JsonStore &prefs);

    std::unique_ptr<AudioOutputDevice> output_device_;

    inline static const std::string NAME = "AudioOutputDeviceActor";

    caf::behavior behavior_;
    std::string name_;
    bool playing_;
    caf::actor_addr audio_playback_manager_;
    bool waiting_for_samples_;
};


class AudioOutputControlActor : public caf::event_based_actor, AudioOutputControl {

  public:
    AudioOutputControlActor(
        caf::actor_config &cfg, const std::string name = "AudioOutputControlActor");
    ~AudioOutputControlActor() override = default;

    caf::behavior make_behavior() override { return message_handler().or_else(behavior_); }

    void on_exit() override;
    const char *name() const override { return NAME.c_str(); }

  private:
    caf::actor audio_output_device_;

    inline static const std::string NAME = "AudioOutputControlActor";
    void init();
    void
    get_audio_buffer(caf::actor media_actor, const utility::Uuid uuid, const int source_frame);

    caf::behavior behavior_;
    std::string name_;
    const utility::JsonStore params_;
    bool playing_       = {false};
    int video_frame_    = {0};
    int retry_on_error_ = {0};
    utility::Uuid uuid_ = {utility::Uuid::generate()};
    utility::Uuid sub_playhead_uuid_;
};
} // namespace xstudio::audio
