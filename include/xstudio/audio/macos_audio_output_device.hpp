// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/audio/audio_output_device.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace audio {

    class MacOSAudioOutData;

    /**
     *  @brief MacOSAudioOutputDevice class, low level interface with audio output
     *
     *  @details
     *   See header for AudioOutputDevice
     */
    class MacOSAudioOutputDevice : public AudioOutputDevice {

      public:

        MacOSAudioOutputDevice(const utility::JsonStore &prefs);

        ~MacOSAudioOutputDevice() override;

        void initialize_sound_card() override {}

        void connect_to_soundcard() override;

        void disconnect_from_soundcard() override;

        long desired_samples() override;

        bool push_samples(const void *sample_data, const long num_samples) override;

        long latency_microseconds() override;

        [[nodiscard]] long sample_rate() const override { return sample_rate_; }

        [[nodiscard]] int num_channels() const override { return num_channels_; }

        [[nodiscard]] SampleFormat sample_format() const override { return sample_format_; }

      private:

        long aprx_buffer_water_level_microsecs_ = {80000};
        long sample_rate_           = {48000};
        int num_channels_           = {2};
        long buffer_size_           = {2048};
        SampleFormat sample_format_ = {SampleFormat::INT16};

        const utility::JsonStore prefs_;

        std::unique_ptr<MacOSAudioOutData> audio_out_data_;

    };
} // namespace audio
} // namespace xstudio
