#ifdef WINDOWS_AUDIO
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Audioclient.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <mmdeviceapi.h>
#include <iostream>
#include <string>

#include "windows_audio_output_device.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::audio;
using namespace xstudio::global_store;


WindowsAudioOutputDevice::WindowsAudioOutputDevice(const utility::JsonStore &prefs)
    : AudioOutputDevice(), prefs_(prefs) {
    // Get the default sound card and use default sample rate and number of channels
    HRESULT hr = initializeAudioClient(
        L"", 44100, 2); // Use empty string for sound card to pick the default
    if (FAILED(hr)) {
        // Just log the error and continue
        spdlog::error("Failed to initialize audio client: HRESULT 0x{:X}", hr);
    }
}

WindowsAudioOutputDevice::~WindowsAudioOutputDevice() {
    disconnect_from_soundcard();
}

void WindowsAudioOutputDevice::disconnect_from_soundcard() {
    if (audio_client_) {
        audio_client_->Stop();
        audio_client_ = nullptr;
    }
    render_client_ = nullptr;
}

HRESULT WindowsAudioOutputDevice::initializeAudioClient(
    const std::wstring &sound_card /* = L"" */,
    long sample_rate /* = 44100 */,
    int num_channels /* = 2 */) {

    CComPtr<IMMDeviceEnumerator> device_enumerator;
    CComPtr<IMMDevice> audio_device;
    HRESULT hr;

    // Create a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void **>(&device_enumerator));
    if (FAILED(hr)) {
        return hr;
    }

    // If sound_card is not provided, get the default audio-render device
    if (sound_card.empty()) {
        hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audio_device);
    } else {
        // Get the audio-render device based on the provided sound_card
        hr = device_enumerator->GetDevice(sound_card.c_str(), &audio_device);
    }

    if (FAILED(hr)) {
        return hr;
    }

    // Get an IAudioClient3 instance
    hr = audio_device->Activate(
        __uuidof(IAudioClient3),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **>(&audio_client_));
    if (FAILED(hr)) {
        return hr;
    }

    // Initialize the audio format
    WAVEFORMATEX* wave_format = nullptr;
    WAVEFORMATEXTENSIBLE wfext;
    wfext.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfext.Format.nChannels = num_channels;
    wfext.Format.nSamplesPerSec = sample_rate;
    wfext.Format.wBitsPerSample = 16;
    wfext.Format.nBlockAlign = wfext.Format.nChannels * (wfext.Format.wBitsPerSample / 8);
    wfext.Format.nAvgBytesPerSec = wfext.Format.nSamplesPerSec * wfext.Format.nBlockAlign;
    wfext.Format.cbSize = 22;
    wfext.Samples.wValidBitsPerSample = wfext.Format.wBitsPerSample;
    wfext.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    wfext.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    wave_format = reinterpret_cast<WAVEFORMATEX*>(&wfext);

    // Initialize the audio client
    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   buffer_size_ * 10000,  // buffer duration
                                   0,  // periodicity
                                   wave_format,
                                   nullptr);  // session GUID

    return hr;
}

void WindowsAudioOutputDevice::connect_to_soundcard() {

    sample_rate_ = 44100; //default values
    num_channels_ = 2;
    std::wstring sound_card(L"default");
    buffer_size_ = 2048; // Adjust to match your preferences

    // Replace with your method to get preference values
    try {
        sample_rate_ =
            preference_value<long>(prefs_, "/core/audio/windows_audio_prefs/sample_rate");
        buffer_size_ =
            preference_value<long>(prefs_, "/core/audio/windows_audio_prefs/buffer_size");
        num_channels_ = preference_value<int>(prefs_, "/core/audio/windows_audio_prefs/channels");
        sound_card =
            preference_value<std::wstring>(prefs_, "/core/audio/windows_audio_prefs/sound_card");
    } catch (std::exception &e) {
        spdlog::warn("{} Failed to retrieve WASAPI prefs : {} ", __PRETTY_FUNCTION__, e.what());
    }

    HRESULT hr = initializeAudioClient(sound_card, sample_rate_, num_channels_);
    if (FAILED(hr)) {
        spdlog::error("{} Failed to initialize audio client: HRESULT=0x{:08x}", __PRETTY_FUNCTION__, hr);
        throw std::runtime_error("Failed to initialize audio client");
    }

    // Get an IAudioRenderClient instance
    hr = audio_client_->GetService(__uuidof(IAudioRenderClient),
                                   reinterpret_cast<void**>(&render_client_));
    if (FAILED(hr)) {
        spdlog::error("{} Failed to get render client: HRESULT=0x{:08x}", __PRETTY_FUNCTION__, hr);
        throw std::runtime_error("Failed to get render client");
    }

    //spdlog::debug("{} Connected to soundcard : {} ", __PRETTY_FUNCTION__, to_utf8(sound_card).c_str());
}

long WindowsAudioOutputDevice::desired_samples() {
    // Note: WASAPI works with a fixed buffer size, so this will return the same
    // value for the duration of a playback session
    UINT32 bufferSize;
    HRESULT hr = audio_client_->GetBufferSize(&bufferSize);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get buffer size");
    }
    return bufferSize;
}

long WindowsAudioOutputDevice::latency_microseconds() {
    // Note: This will just return the latency that WASAPI reports,
    // which may not include all sources of latency
    REFERENCE_TIME defaultDevicePeriod, minimumDevicePeriod;
    HRESULT hr = audio_client_->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get device period");
    }
    return defaultDevicePeriod / 10; // convert 100-nanosecond units to microseconds
}

void WindowsAudioOutputDevice::push_samples(const void *sample_data, const long num_samples) {
    BYTE *buffer;
    HRESULT hr = render_client_->GetBuffer(num_samples, &buffer);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get buffer");
    }

    memcpy(buffer, sample_data, num_samples * sizeof(float));

    hr = render_client_->ReleaseBuffer(num_samples, 0);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to release buffer");
    }
}
#endif 