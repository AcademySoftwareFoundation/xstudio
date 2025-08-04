#include <AudioToolbox/AudioQueue.h>
#include <iostream>
#include <string>
#include <sstream>

#include <chrono>
#include <thread>

#include "xstudio/audio/macos_audio_output_device.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::audio;
using namespace xstudio::global_store;

namespace xstudio {
namespace audio {
    class MacOSAudioOutData {
      public:
        MacOSAudioOutData() = default;

        ~MacOSAudioOutData() {
            for (auto &b : unused_buffers_) {
                AudioQueueFreeBuffer(audio_buffer_queue_, b);
            }
            AudioQueueDispose(audio_buffer_queue_, true);
        }

        void start() {
            if (running_)
                return;
            auto r                         = AudioQueueStart(audio_buffer_queue_, nullptr);
            last_sample_position_in_queue_ = 0;
            if (r) {
                throw r;
            }
            running_ = true;
        }

        void stop() {
            auto r = AudioQueueStop(audio_buffer_queue_, true);
            if (r) {
                throw r;
            }
            running_ = false;
        }

        [[nodiscard]] bool running() const { return running_; }

        AudioQueueBufferRef get_buffer_ref(long size) {
            AudioQueueBufferRef result = nullptr;
            std::lock_guard l(mutex_);
            auto b = unused_buffers_.begin();
            while (b != unused_buffers_.end()) {

                if ((*b)->mAudioDataBytesCapacity >= size) {
                    result = *b;
                    unused_buffers_.erase(b);
                    break;
                }
                b++;
            }
            if (!result) {
                // round up size to something sensible, in case we are getting
                // uneven blocks of audio samples from xstudio AudioControl.
                // This means buffers are more likely to be re-useable
                size = ((size / 4096) + 1) * 4096;
                AudioQueueAllocateBuffer(audio_buffer_queue_, size, &result);
            }
            return result;
        }

        void reuse_buffer(AudioQueueBufferRef r) {
            std::lock_guard l(mutex_);
            unused_buffers_.push_back(r);
        }

        AudioQueueRef audio_buffer_queue_ = {nullptr};
        std::mutex mutex_;
        std::vector<AudioQueueBufferRef> unused_buffers_;
        Float64 last_sample_position_in_queue_;
        bool running_ = {false};
    };
} // namespace audio
} // namespace xstudio

MacOSAudioOutputDevice::MacOSAudioOutputDevice(const utility::JsonStore &prefs)
    : AudioOutputDevice(), prefs_(prefs) {

    try {

        // note - we use the pulse_audio sample rate pref to set the sample rate on
        // our MacOS audio output. The reason is that this same preference tells the
        // media reader (ffmpeg) what sample rate to convert audio frames to when
        // they are delivered to xstudio. A better system for setting the intermediate
        // rate for audio buffers and delivery to the sound card could be forthcoming!
        buffer_size_ =
            preference_value<long>(prefs_, "/core/audio/pulse_audio_prefs/buffer_size");
        sample_rate_ =
            preference_value<long>(prefs_, "/core/audio/pulse_audio_prefs/sample_rate");

        // currently we only really support 2 channels but 5 and 7 for surround can be added
        // in easily but I don't have the gear to test this yet
        num_channels_ =
            preference_value<long>(prefs_, "/core/audio/pulse_audio_prefs/channels");

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

MacOSAudioOutputDevice::~MacOSAudioOutputDevice() {

    disconnect_from_soundcard();
    audio_out_data_.reset();
}

void output_buffer_cb(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    // we get this callback when the queue has finished with the given buffer.
    // The API docs tell us tha this callback won't get called after
    // AudioQueueDispose has returned - this is called in the AudioQueueDispose
    // destructor so we should be ok (i think!)
    ((MacOSAudioOutData *)inUserData)->reuse_buffer(inBuffer);
}

void MacOSAudioOutputDevice::disconnect_from_soundcard() {

    if (!audio_out_data_)
        return;

    try {

        audio_out_data_->stop();

    } catch (OSStatus &errcode) {
        // OSStatus codes are 4 chars packed, and if you convert to this
        // 4 char string they can be searched on the website osstatus.com
        // weird.
        char b[5];
        b[4] = 0;
        memcpy(b, &errcode, sizeof(errcode));
        audio_out_data_.reset();
        throw std::runtime_error(
            fmt::format("Failed to create output audio queue with OSStatus error code {}", b)
                .c_str());
    }
}

void MacOSAudioOutputDevice::connect_to_soundcard() {

    // already connected
    if (audio_out_data_) {

        if (!audio_out_data_->running()) {
            audio_out_data_->start();
        }
        return;
    }

    audio_out_data_.reset(new MacOSAudioOutData());

    AudioStreamBasicDescription desc;
    desc.mSampleRate       = sample_rate_;
    desc.mChannelsPerFrame = num_channels_;

    // xSTUDIO's audio sample buffer format is currently 16bit int. This is
    // currently set by our ffmpeg media reader. Again, we can add more
    // flexibility if we need to but for now hardcoding the format to
    // something sensible like 16 bit works fine.
    desc.mFormatID         = kAudioFormatLinearPCM;
    desc.mFormatFlags      = kAudioFormatFlagIsSignedInteger;
    desc.mBytesPerPacket   = 4;
    desc.mFramesPerPacket  = 1;
    desc.mBytesPerFrame    = 4;
    desc.mChannelsPerFrame = 2;
    desc.mBitsPerChannel   = 16;

    AudioQueueOutputCallback cb = output_buffer_cb;
    UInt32 flags                = 0;
    AudioQueueRef the_queue;
    CFRunLoopRef inCallbackRunLoop    = NULL;
    CFStringRef inCallbackRunLoopMode = NULL;

    try {
        auto r = AudioQueueNewOutput(
            &desc,
            cb,
            (void *)audio_out_data_.get(), // user data passed into output_buffer_cb
            inCallbackRunLoop,
            inCallbackRunLoopMode,
            flags,
            &(audio_out_data_->audio_buffer_queue_));
        if (r)
            throw r;

        /*UInt32 repitch = 1;
        AudioQueueSetProperty(audio_out_data_->audio_buffer_queue_,
                                    kAudioQueueProperty_EnableTimePitch,
                                    (const void*)&repitch,
                                    sizeof(repitch));

        AudioQueueParameterValue pitch = 1200.0f;
        AudioQueueSetParameter(audio_out_data_->audio_buffer_queue_, kAudioQueueParam_Pitch,
        pitch);*/
        audio_out_data_->start();

    } catch (OSStatus &errcode) {
        // OSStatus codes are 4 chars packed, and if you convert to this
        // 4 char string they can be searched on the website osstatus.com
        // weird.
        char b[5];
        b[4] = 0;
        memcpy(b, &errcode, sizeof(errcode));
        throw std::runtime_error(
            fmt::format("Failed to create output audio queue with OSStatus error code {}", b)
                .c_str());
    }
    //
}

long MacOSAudioOutputDevice::desired_samples() {

    // latency_microseconds tells us the number of samples sitting in the buffer
    // right now (measured as their a duration). If this amount is greater than
    // the duration of the number of samples in our chosen buffer_size_ parameter
    // then we return zero, meaning we don't want any more samples right now as
    // we prefer to let the samples in the buffer to drain somewhat.
    // The xstudio AudioController will wait for some amount of time (5ms) and then
    // call this again as part of the audio output loop.
    if (latency_microseconds() > aprx_buffer_water_level_microsecs_)
        return 0;

    // devide by 4 because 2 channels and 2 bytes per sample
    return buffer_size_ / 4;
}

long MacOSAudioOutputDevice::latency_microseconds() {

    // This is used to inform xstudio how many samples are queued for
    // playback at this instant in time - measured in microseconds
    // i.e. the 'watermark' of the buffer or just how much audio is
    // queued for playback.

    if (!audio_out_data_)
        return 0;

    AudioTimeStamp ts;
    // this call fetches us the current sample time (i.e. the 'number'
    // of the sample just played if the first sample since playback
    // started was sample zero), among other things
    AudioQueueGetCurrentTime(audio_out_data_->audio_buffer_queue_, nullptr, &ts, nullptr);

    if (audio_out_data_->last_sample_position_in_queue_ > ts.mSampleTime) {
        // the last time we enqueued a buffer, we got information about the sample time for the
        // first sample in that buffer - we subtract the current sample time from that an apply
        // the sample rate. We also add the duration of the last buffer ... this tells us the
        // total duration of the audio queued for playback
        auto v =
            ((audio_out_data_->last_sample_position_in_queue_ - ts.mSampleTime) * 1000000) /
            sample_rate_;
        return ((audio_out_data_->last_sample_position_in_queue_ - ts.mSampleTime) * 1000000) /
               sample_rate_;
    }

    return 0;
}


bool MacOSAudioOutputDevice::push_samples(const void *sample_data, const long num_samples) {

    if (!audio_out_data_)
        return false;

    // hardcoded for 16bits per channel
    const size_t num_bytes  = num_samples * 2;
    AudioQueueBufferRef buf = audio_out_data_->get_buffer_ref(num_bytes);
    buf->mAudioDataByteSize = num_bytes;
    memcpy(buf->mAudioData, sample_data, num_bytes);

    // Let's fetch the current time for the queue
    AudioTimeStamp ts, actual_play_time;
    AudioQueueGetCurrentTime(audio_out_data_->audio_buffer_queue_, nullptr, &ts, nullptr);

    bool underrun = false;

    if (audio_out_data_->last_sample_position_in_queue_ &&
        audio_out_data_->last_sample_position_in_queue_ < ts.mSampleTime) {
        // we have a buffer under-run situation ... the last samples we sent to
        // the queue have already played out.
        underrun = true;
    }

    // Note A: in underrun situation, we tell the queue to play the samples NOW. If we don't
    // do this, we never hear them because they are played after the last samples
    // we sent, which are now some point in the past
    AudioQueueEnqueueBufferWithParameters(
        audio_out_data_->audio_buffer_queue_,
        buf,
        0,
        nullptr,
        0,
        0,
        0,
        nullptr,
        underrun ? &ts : nullptr, // Note A
        &actual_play_time);

    // num_samples/2 = num_frames (2 chans per frame)
    audio_out_data_->last_sample_position_in_queue_ =
        actual_play_time.mSampleTime + num_samples / 2;

    // wait while most of the samples are played. The loop in xstudio that calls 'push_samples'
    // expects us to block here while the samples are actually played, though it doesn't need
    // to precisely match the duration time corresponding to num_samples because we want
    // something less than that to give us headroom in that loop to do other calculations like
    // resampling audio and so-on.
    // std::this_thread::sleep_for(std::chrono::milliseconds(audio_out_data_->last_buffer_duration_microsecs_
    // / 1200));
    return true;
}
