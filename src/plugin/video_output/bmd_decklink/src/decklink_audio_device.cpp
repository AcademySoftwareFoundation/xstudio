// SPDX-License-Identifier: Apache-2.0
#include "decklink_audio_device.hpp"
#include "decklink_output.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/logging.hpp"
#include <iostream>

using namespace xstudio::bm_decklink_plugin_1_0;
using namespace xstudio::global_store;

DecklinkAudioOutputDevice::DecklinkAudioOutputDevice(const utility::JsonStore &prefs, DecklinkOutput * bmd_output)
    : prefs_(prefs), bmd_output_(bmd_output) {}

DecklinkAudioOutputDevice::~DecklinkAudioOutputDevice() { 
    disconnect_from_soundcard(); 
}

void DecklinkAudioOutputDevice::disconnect_from_soundcard() {
    // not needed, video playback stops starts audio output
}

void DecklinkAudioOutputDevice::connect_to_soundcard() {
    // not needed, video playback stops starts audio output
}

long DecklinkAudioOutputDevice::desired_samples() { 
    // this sets the minumum chunk of samples that xstudio gives us to transfer
    // to the BMD audio buffer.
    return 2048; 
}

long DecklinkAudioOutputDevice::latency_microseconds() {
    return (bmd_output_->num_samples_in_buffer()*1000000)/sample_rate_;
}

bool DecklinkAudioOutputDevice::push_samples(const void *sample_data, const long num_samples) {
    bmd_output_->receive_samples_from_xstudio((int16_t *)sample_data, num_samples);
    return true;
}