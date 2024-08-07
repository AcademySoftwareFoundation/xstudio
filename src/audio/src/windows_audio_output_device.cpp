#include <windows.h>
#include <Audioclient.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>
#include <sstream>

#include <chrono>
#include <thread>

#include "xstudio/audio/windows_audio_output_device.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::audio;
using namespace xstudio::global_store;


WindowsAudioOutputDevice::WindowsAudioOutputDevice(const utility::JsonStore &prefs)
    : AudioOutputDevice(), prefs_(prefs) {}

WindowsAudioOutputDevice::~WindowsAudioOutputDevice() { disconnect_from_soundcard(); }

void WindowsAudioOutputDevice::disconnect_from_soundcard() { return; }

HRESULT WindowsAudioOutputDevice::initializeAudioClient(
    const std::string &sound_card /* = L"" */,
    long sample_rate /* = 48000 */,
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

    // If sound_card is not provided, enumerate the devices and pick the first one
    if (sound_card.empty() || sound_card == "default") {
        hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &audio_device);
    } else {
        // Get the audio-render device based on the provided sound_card
        std::wstring sound_card_w = L"";
        std::wstringstream combiner;
        combiner << sound_card.c_str();
        sound_card_w = combiner.str();
        hr           = device_enumerator->GetDevice(sound_card_w.c_str(), &audio_device);
    }

    if (FAILED(hr)) {
        return hr;
    }

#if false
    // Print the device name
    CComPtr<IPropertyStore> property_store;
    hr = audio_device->OpenPropertyStore(STGM_READ, &property_store);
    if (SUCCEEDED(hr)) {
        PROPVARIANT var_name;
        PropVariantInit(&var_name);

        hr = property_store->GetValue(PKEY_Device_FriendlyName, &var_name);
        if (SUCCEEDED(hr)) {
            wprintf(L"Audio Device Name: %s\n", var_name.pwszVal);
            PropVariantClear(&var_name); // always clear the PROPVARIANT to release any
                                         // memory it might've allocated
        }
    }
#endif

    // Get an IAudioClient3 instance
    hr = audio_device->Activate(
        __uuidof(IAudioClient3),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **>(&audio_client_));
    if (FAILED(hr)) {
        return hr;
    }

    // Get the mix format from the audio client
    WAVEFORMATEX *pMixFormat = NULL;
    hr                       = audio_client_->GetMixFormat(&pMixFormat);
    if (FAILED(hr)) {
        spdlog::error("Failed to get mix format: HRESULT=0x{:08x}", hr);
        return hr;
    }


    sample_rate_ = pMixFormat->nSamplesPerSec;
#if false
    // Print the mix format details
    spdlog::info("Mix Format Details:");
    spdlog::info("Format Tag: {}", pMixFormat->wFormatTag);
    spdlog::info("Channels: {}", pMixFormat->nChannels);
    spdlog::info("Sample Rate: {}", pMixFormat->nSamplesPerSec);
    spdlog::info("Bits Per Sample: {}", pMixFormat->wBitsPerSample);
    spdlog::info("Block Align: {}", pMixFormat->nBlockAlign);
    spdlog::info("Average Bytes Per Second: {}", pMixFormat->nAvgBytesPerSec);

    if (pMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pMixFormat->cbSize >= 22) {
        WAVEFORMATEXTENSIBLE *pExtensible =
            reinterpret_cast<WAVEFORMATEXTENSIBLE *>(pMixFormat);
        spdlog::info("Valid Bits Per Sample: {}", pExtensible->Samples.wValidBitsPerSample);
        DWORD channel_mask = pExtensible->dwChannelMask;
        spdlog::info("Channel Mask: {}", channel_mask);
        // Add more fields if needed

        spdlog::info(
            "Channel Layout:\nFL {}  FLOC {} C {}  FROC {}  FR {}     LFE {}\n\n     TFL {}   TFC {}   TFR {}\n",
            (int)(bool)(SPEAKER_FRONT_LEFT & channel_mask),
            (int)(bool)(SPEAKER_FRONT_LEFT_OF_CENTER & channel_mask),
            (int)(bool)(SPEAKER_FRONT_CENTER & channel_mask),
            (int)(bool)(SPEAKER_FRONT_RIGHT_OF_CENTER & channel_mask),
            (int)(bool)(SPEAKER_FRONT_RIGHT & channel_mask),
            (int)(bool)(SPEAKER_LOW_FREQUENCY & channel_mask),
            (int)(bool)(SPEAKER_TOP_FRONT_LEFT & channel_mask),
            (int)(bool)(SPEAKER_TOP_FRONT_CENTER & channel_mask),
            (int)(bool)(SPEAKER_TOP_FRONT_RIGHT & channel_mask)
            
        );
    }
#endif

    // Fetch the currently active shared mode format
    WAVEFORMATEX *wavefmt = NULL;
    UINT32 current_period = 0;
    hr                    = audio_client_->GetCurrentSharedModeEnginePeriod(
        (WAVEFORMATEX **)&wavefmt, &current_period);
    if (FAILED(hr)) {
        spdlog::error("Failed to get current shared mode engine period: HRESULT=0x{:08x}", hr);
        CoTaskMemFree(pMixFormat);
        return hr;
    }

    // Fetch the minimum period supported by the current setup
    UINT32 DP, FP, MINP, MAXP;
    hr = audio_client_->GetSharedModeEnginePeriod(wavefmt, &DP, &FP, &MINP, &MAXP);
    if (FAILED(hr)) {
        spdlog::error("Failed to get shared mode engine period details: HRESULT=0x{:08x}", hr);
        CoTaskMemFree(pMixFormat);
        CoTaskMemFree(wavefmt);
        return hr;
    }

    // Initialize the audio client with the mix format
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        MINP,
        0,
        wavefmt,
        nullptr // session GUID
    );

    // Free the mix format and wave format after usage
    CoTaskMemFree(pMixFormat);
    CoTaskMemFree(wavefmt);

    return hr;
}


void WindowsAudioOutputDevice::initialize_sound_card() {
    sample_rate_  = 48000; // default values
    num_channels_ = 2;
    std::string sound_card("default");
    buffer_size_ = 2048; // Adjust to match your preferences

    // Replace with your method to get preference values
    try {
        sample_rate_ =
            preference_value<long>(prefs_, "/core/audio/windows_audio_prefs/sample_rate");
        buffer_size_ =
            preference_value<long>(prefs_, "/core/audio/windows_audio_prefs/buffer_size");
        num_channels_ =
            preference_value<int>(prefs_, "/core/audio/windows_audio_prefs/channels");
        sound_card =
            preference_value<std::string>(prefs_, "/core/audio/windows_audio_prefs/sound_card");
    } catch (std::exception &e) {
        spdlog::warn("{} Failed to retrieve WASAPI prefs : {} ", __PRETTY_FUNCTION__, e.what());
    }

    HRESULT hr = initializeAudioClient(sound_card, sample_rate_, num_channels_);
    if (FAILED(hr)) {
        spdlog::error(
            "{} Failed to initialize audio client: HRESULT=0x{:08x}", __PRETTY_FUNCTION__, hr);
        return; // or handle the error as appropriate
    }

    // Get an IAudioRenderClient instance
    hr = audio_client_->GetService(
        __uuidof(IAudioRenderClient), reinterpret_cast<void **>(&render_client_));
    if (FAILED(hr)) {
        spdlog::error("Failed to get IAudioRenderClient: HRESULT=0x{:08x}", hr);
        return; // or handle the error as appropriate
    }

    audio_client_->Start();
}

void WindowsAudioOutputDevice::connect_to_soundcard() {
    // We are already playing ;-D
}

long WindowsAudioOutputDevice::desired_samples() {
    // Note: WASAPI works with a fixed buffer size, so this will return the same
    // value for the duration of a playback session
    UINT32 bufferSize = 0; // initialize to 0
    HRESULT hr        = audio_client_->GetBufferSize(&bufferSize);

    if (FAILED(hr)) {
        spdlog::error("Failed to get buffer size from WASAPI with HRESULT: 0x{:08x}", hr);
        throw std::runtime_error("Failed to get buffer size");
    }

    UINT32 pad = 0;
    hr         = audio_client_->GetCurrentPadding(&pad);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get current padding from WASAPI");
    }

    return bufferSize - pad;
}

long WindowsAudioOutputDevice::latency_microseconds() {
    // Note: This will just return the latency that WASAPI reports,
    // which may not include all sources of latency
    REFERENCE_TIME defaultDevicePeriod = 0, minimumDevicePeriod = 0; // initialize to 0
    HRESULT hr = audio_client_->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod);
    if (FAILED(hr)) {
        spdlog::error("Failed to get device period from WASAPI with HRESULT: 0x{:08x}", hr);
        throw std::runtime_error("Failed to get device period");
    }
    return defaultDevicePeriod / 10; // convert 100-nanosecond units to microseconds
}

void WindowsAudioOutputDevice::push_samples(const void *sample_data, const long num_samples) {

    int channel_count = num_channels_;

    if (num_samples < 0 || num_samples % channel_count != 0) {
        spdlog::error(
            "Invalid number of samples provided: {}. Expected a multiple of {}",
            num_samples,
            channel_count);
        return;
    }

    // Ensure we have a valid render_client_
    if (!render_client_) {
        // spdlog::error("Invalid Render Client");
        return; // Exit if no render client is set
    }

    // Retrieve the size (maximum capacity) of the endpoint buffer.
    UINT32 buffer_framecount = 0;
    HRESULT hr               = audio_client_->GetBufferSize(&buffer_framecount);
    if (FAILED(hr)) {
        spdlog::error("Failed to get buffer size from WASAPI");
        return;
    }


    // Get the number of frames of padding in the endpoint buffer.
    UINT32 pad = 0;
    hr         = audio_client_->GetCurrentPadding(&pad);
    if (FAILED(hr)) {
        spdlog::error("Failed to get current padding from WASAPI");
        return;
    }

    // Calculate the number of frames we can safely write into the buffer without overflow.
    long available_frames = buffer_framecount - pad;
    long frames_to_write  = num_samples / channel_count;
    if (available_frames < frames_to_write) {
        frames_to_write = available_frames;
    }

    if (frames_to_write) {

        // Get a buffer from WASAPI for our audio data.
        BYTE *buffer;
        hr = render_client_->GetBuffer(available_frames, &buffer);
        if (FAILED(hr)) {
            spdlog::error("GetBuffer failed with HRESULT: 0x{:08x}", hr);
            throw std::runtime_error("Failed to get buffer from WASAPI");
        }

        // Convert int16_t PCM data to float samples considering the interleaved format.
        int16_t *pcmData     = (int16_t *)sample_data;
        float *floatBuffer   = (float *)buffer;
        const float maxInt16 = 32767.0f;

        long total_samples_to_process = frames_to_write * channel_count;
        for (long i = 0; i < total_samples_to_process; i++) {
            floatBuffer[i] = pcmData[i] / maxInt16;
        }

        // Release the buffer back to WASAPI to play.
        hr = render_client_->ReleaseBuffer(frames_to_write, 0);
        if (FAILED(hr)) {
            spdlog::error("Failed to release buffer to WASAPI with HRESULT: 0x{:08x}", hr);
            throw std::runtime_error("Failed to release buffer to WASAPI");
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds((long)(.5 / sample_rate_ * frames_to_write)));
    } else {
        // Avoid tight loop thrashing when we are out of samples.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
