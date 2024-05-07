// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/audio/audio_output_device.hpp"
#include "xstudio/utility/json_store.hpp"

#include <pulse/simple.h>

namespace xstudio {
namespace audio {

    /**
     *  @brief LinuxAudioOutputDevice class, low level interface with audio output
     *
     *  @details
     *   See header for AudioOutputDevice
     */
    class LinuxAudioOutputDevice : public AudioOutputDevice {
      public:
        LinuxAudioOutputDevice(const utility::JsonStore &prefs);

        ~LinuxAudioOutputDevice() override;

        void connect_to_soundcard() override;

        void disconnect_from_soundcard() override;

        long desired_samples() override;

        void push_samples(const void *sample_data, const long num_samples, int channel_count) override;

        long latency_microseconds() override;

        [[nodiscard]] long sample_rate() const override { return sample_rate_; }

        [[nodiscard]] int num_channels() const override { return num_channels_; }

        [[nodiscard]] SampleFormat sample_format() const override { return sample_format_; }

      private:
        long sample_rate_           = {44100};
        int num_channels_           = {2};
        long buffer_size_           = {2048};
        SampleFormat sample_format_ = {SampleFormat::INT16};
        pa_simple *playback_handle_ = {nullptr};
        const utility::JsonStore config_;
        const utility::JsonStore prefs_;
    };
} // namespace audio
} // namespace xstudio
