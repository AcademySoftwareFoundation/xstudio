// SPDX-License-Identifier: Apache-2.0
#include "xstudio/audio/linux_audio_output_device.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/logging.hpp"
#include <iostream>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

using namespace xstudio::audio;
using namespace xstudio::global_store;

LinuxAudioOutputDevice::LinuxAudioOutputDevice(const utility::JsonStore &prefs)
    : prefs_(prefs) {}

LinuxAudioOutputDevice::~LinuxAudioOutputDevice() { disconnect_from_soundcard(); }

void LinuxAudioOutputDevice::disconnect_from_soundcard() {
    if (playback_handle_) {
        pa_simple_free(playback_handle_);
    }
    playback_handle_ = nullptr;
}

void LinuxAudioOutputDevice::connect_to_soundcard() {

    num_channels_ = 2;
    std::string sound_card("default");

    try {
        // format = static_cast<snd_pcm_format_t>(preference_value<int>(prefs_,
        // "/core/audio/pulse_audio_prefs/SAMPLE_FORMAT"));
        sample_rate_ =
            preference_value<int>(prefs_, "/core/audio/pulse_audio_prefs/sample_rate");
        buffer_size_ =
            preference_value<int>(prefs_, "/core/audio/pulse_audio_prefs/buffer_size");
        num_channels_ = preference_value<int>(prefs_, "/core/audio/pulse_audio_prefs/channels");
        sound_card =
            preference_value<std::string>(prefs_, "/core/audio/pulse_audio_prefs/sound_card");
    } catch (std::exception &e) {
        spdlog::warn("{} Failed to retrieve ALSA prefs : {} ", __PRETTY_FUNCTION__, e.what());
    }

    /* The Sample format to use */
    pa_sample_spec pa_ss;
    pa_ss.format   = PA_SAMPLE_S16LE;
    pa_ss.rate     = (uint32_t)sample_rate_;
    pa_ss.channels = (uint8_t)num_channels_;

    int error;

    /* Create a new playback stream */
    if (!(playback_handle_ = pa_simple_new(
              nullptr,
              "xstudio",
              PA_STREAM_PLAYBACK,
              sound_card == "default" ? nullptr : sound_card.c_str(),
              "playback",
              &pa_ss,
              nullptr,
              nullptr,
              &error))) {
        fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n", pa_strerror(error));
        std::stringstream ss;
        ss << __FILE__ ": pa_simple_new() failed: " << pa_strerror(error);
        throw std::runtime_error(ss.str().c_str());
    }

    spdlog::debug("{} Connected to soundcard : {} ", __PRETTY_FUNCTION__, sound_card.c_str());
}

long LinuxAudioOutputDevice::desired_samples() { return buffer_size_; }

long LinuxAudioOutputDevice::latency_microseconds() {

    pa_usec_t latency;
    int error;

    if (playback_handle_ &&
        (latency = pa_simple_get_latency(playback_handle_, &error)) == (pa_usec_t)-1) {
        std::stringstream ss;
        ss << __FILE__ ": pa_simple_get_latency() failed: " << pa_strerror(error);
        throw std::runtime_error(ss.str().c_str());
    }

    return (long)latency;
}


void LinuxAudioOutputDevice::push_samples(const void *sample_data, const long num_samples) {

    int error;
    if (playback_handle_ &&
        pa_simple_write(playback_handle_, sample_data, (size_t)num_samples * 2 * 2, &error) <
            0) {
        std::stringstream ss;
        ss << __FILE__ ": pa_simple_write() failed: " << pa_strerror(error);
        throw std::runtime_error(ss.str().c_str());
    }
}
