// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio::audio {

enum class SampleFormat { UNSET, UINT8 = 1, INT16, SFINT32, FLOAT32, INT64, DOUBLE64 };

/**
 *  @brief AudioOutputDevice class, low level interface with audio output
 *
 *  @details
 *   Minimal, pure abstract base class for delivering audio samples to the soundcard
 */
class AudioOutputDevice {
  public:
    /**
     *  @brief Constructor
     *
     *  @details The implementation class is expected to open connections to the
     * soundcard in it's constructor, throwing an exception on any error.
     */
    AudioOutputDevice() = default;

    /**
     *  @brief Destructor
     *
     *  @details Close connection to soundcard in the destructor.
     */
    virtual ~AudioOutputDevice() = default;

    /**
     *  @brief Configure the sound card.
     *
     *  @details Should be called any time the sound card should be set up or changed
     */
    virtual void initialize_sound_card() = 0;

    /**
     *  @brief Open the connection to the sounding device
     *
     *  @details Note this will be called everytime audio playback starts
     */
    virtual void connect_to_soundcard() = 0;

    /**
     *  @brief Close the connection to the sounding device
     *
     *  @details Note this will be called everytime audio playback stops
     */
    virtual void disconnect_from_soundcard() = 0;

    /**
     *  @brief Query the soundcard for how many samples it would like to recieve for
     * playback
     *
     *  @details The caller should
     *  respond by calling push_samples with a sample buffer of the corresponding size
     */
    virtual long desired_samples() = 0;

    /**
     *  @brief Send a block of audio samples to the soundcard for playing
     *
     *  @details The sample data must already be formatted to match the sample format,
     *  number of channels and sample rate that the soundcard expects. This function may
     *  block while the soundcard consumes samples, depending on the implementation of
     *  the subclass.
     *
     *  @return (bool) returns true if samples were able to be consumed. Returning
     *  false will stop audio samples playback loop.
     */
    virtual bool push_samples(const void *sample_data, const long num_samples) = 0;

    /**
     *  @brief Query the audio pipeline delay from the last sample in the soundcard
     * buffer to when it actually sounds. Measured in microseconds.
     *
     *  @details If we write a sample to the soundcard buffer, how long (in
     * microseconds) before it is audible. Measures the current total audio output
     * latency.
     */
    virtual long latency_microseconds() = 0;

    /**
     *  @brief Query the sample rate
     */
    [[nodiscard]] virtual long sample_rate() const = 0;

    /**
     *  @brief Query the number of active audio channels e.g. 2 for stereo, 5 for Dolby
     * surround
     */
    [[nodiscard]] virtual int num_channels() const = 0;

    /**
     *  @brief Query the sample format that the soundcard will expect
     */
    [[nodiscard]] virtual SampleFormat sample_format() const = 0;
};
} // namespace xstudio::audio
