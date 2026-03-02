// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/audio/audio_output_device.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace bm_decklink_plugin_1_0 {

    class DecklinkOutput;

    /**
     *  @brief DecklinkAudioOutputDevice class. Implements AudioOutputDevice
     *  base class to receive audio samples from xstudio and execute the 
     *  the Decklink API audio schduling calls etc.
     *
     *  @details
     *   See header for AudioOutputDevice in xstudio 
     */
    class DecklinkAudioOutputDevice : public audio::AudioOutputDevice {
      public:

        DecklinkAudioOutputDevice(const utility::JsonStore &prefs, DecklinkOutput * bmd_output);

        ~DecklinkAudioOutputDevice() override;

        void initialize_sound_card() override {}

        void connect_to_soundcard() override;

        void disconnect_from_soundcard() override;

        long desired_samples() override;

        bool push_samples(const void *sample_data, const long num_samples) override;

        /* This method should return the estimated number of samples in the 'soundcard'
        buffer waiting to be played multiplied by the same rate (as microseconds)*/
        long latency_microseconds() override;

        [[nodiscard]] long sample_rate() const override { return sample_rate_; }

        [[nodiscard]] int num_channels() const override { return num_channels_; }

        [[nodiscard]] audio::SampleFormat sample_format() const override { return sample_format_; }

        static std::string name() { return "DecklinkAudioOutputDevice"; }

      private:
        long sample_rate_           = {48000};
        int num_channels_           = {2};
        audio::SampleFormat sample_format_ = {audio::SampleFormat::INT16};
        const utility::JsonStore config_;
        const utility::JsonStore prefs_;
        DecklinkOutput * bmd_output_;
    };


} // namespace bm_decklink_plugin_1_0
} // namespace xstudio
