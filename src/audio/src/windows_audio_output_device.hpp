// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Audioclient.h>
#include <atlbase.h>

#include "xstudio/audio/audio_output_device.hpp"
#include "xstudio/utility/json_store.hpp"


namespace xstudio {
namespace audio {

    /**
     *  @brief WindowsAudioOutputDevice class, low level interface with audio output
     *
     *  @details
     *   See header for AudioOutputDevice
     */
    class WindowsAudioOutputDevice : public AudioOutputDevice {
      public:
        WindowsAudioOutputDevice(const utility::JsonStore &prefs);

        ~WindowsAudioOutputDevice() override;

        void connect_to_soundcard() override;

        void disconnect_from_soundcard() override;

        long desired_samples() override;

        void push_samples(const void *sample_data, const long num_samples, int channel_count) override;

        long latency_microseconds() override;

        [[nodiscard]] long sample_rate() const override { return sample_rate_; }

        [[nodiscard]] int num_channels() const override { return num_channels_; }

        [[nodiscard]] SampleFormat sample_format() const override { return sample_format_; }

      private:
        long sample_rate_ = {48000};
        int num_channels_ = {2};
        long buffer_size_ = {2048};
        SampleFormat sample_format_ = {SampleFormat::INT16};
        CComPtr<IAudioClient3> audio_client_;
        CComPtr<IAudioRenderClient> render_client_;
        const utility::JsonStore config_;
        const utility::JsonStore prefs_;

        HRESULT initializeAudioClient(
        const std::wstring &sound_card = L"",
        long sample_rate               = 48000,
        int num_channels               = 2);

    };
} // namespace audio
} // namespace xstudio
