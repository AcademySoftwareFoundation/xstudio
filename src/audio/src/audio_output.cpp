// SPDX-License-Identifier: Apache-2.0

#include <caf/policy/select_all.hpp>
#include <cmath>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::audio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio;

// when blocks of audio samples aren't contiguous then to avoid a transient
// at the border between the blocks we fade out samples at the tail of the
// current block and the head of the next block. The window for this fade out
// is defined here. It still causes distortion but much less bad.
#define FADE_FUNC_SAMPS 256

namespace {

// sine envelope for fading/blending samples to avoid transients heading
// to the soundcard
static std::vector<float> fade_in_coeffs;
static std::vector<float> fade_out_coeffs;

struct MakeCoeffs {
    MakeCoeffs() {
        if (fade_in_coeffs.empty()) {
            fade_in_coeffs.resize(FADE_FUNC_SAMPS);
            fade_out_coeffs.resize(FADE_FUNC_SAMPS);
            for (int i = 0; i < FADE_FUNC_SAMPS; ++i) {
                fade_out_coeffs[i] =
                    (cos(float(i) * M_PI / float(FADE_FUNC_SAMPS)) + 1.0f) * 0.5f;
                fade_in_coeffs[i] = 1.0f - fade_out_coeffs[i];
            }
        }
    }
};

static MakeCoeffs s_mk_coeffs;

template <typename T>
void changing_volume_adjust(
    std::vector<T> &samples, const double volume, const double last_vol) {
    // Assuming int mult and bitshift is quicker than cast to float but haven't verified
    // this
    int ct         = samples.size();
    T *p           = samples.data();
    const float dv = (volume - last_vol) / ct;
    float vx       = last_vol;
    while (ct--) {

        const int factor =
            static_cast<int>(round(vx * static_cast<double>(std::numeric_limits<T>::max())));

        *p = static_cast<T>((int(*p) * factor) >> 16);
        p++;
        vx += dv;
    }
}

template <typename T> void static_volume_adjust(std::vector<T> &samples, const double volume) {
    // Assuming int mult and bitshift is quicker than cast to float but haven't verified
    // this
    int ct = samples.size();
    T *p   = samples.data();
    const int factor =
        static_cast<int>(round(volume * static_cast<double>(std::numeric_limits<T>::max())));
    while (ct--) {
        *p = static_cast<T>((int(*p) * factor) >> 16);
        p++;
    }
}

/*void do_blend(int16_t* &d, media_reader::AudioBufPtr &current_buf,
media_reader::AudioBufPtr &previous_buf, int &n, int &current_buf_pos) {

    // we've got two buffers that aren't contiguous so we need to do something to prevent
transients. The strategy is to take the gradient
    // at the last sample from the previous buffer, continue from the last sample changing
with the gradient and blend this path with
    // the incoming samples of the new buffer.

    int16_t * last_sample = (int16_t *)(previous_buf->buffer()) +
(previous_buf->num_samples()-1)*previous_buf->num_channels(); int16_t * pen_sample =
(int16_t
*)(previous_buf->buffer()) + (previous_buf->num_samples()-2)*previous_buf->num_channels();

    int16_t * next_samp = (int16_t *)(current_buf->buffer()) +
(current_buf_pos)*previous_buf->num_channels();

    const float delta_r = last_sample[0] - pen_sample[0];
    const float delta_l= last_sample[1] - pen_sample[1];

    float r_sample = last_sample[0];
    float l_sample = last_sample[1];

    float rfac = 1.0f;

    while (n) {

        r_sample += delta_r;
        l_sample += delta_l;

        float rr = float(*next_samp++)*(1.0f-rfac) + rfac*r_sample;
        float ll = float(*next_samp++)*(1.0f-rfac) + rfac*l_sample;

        *(d++) = (int16_t)std::max(
            std::min(
                int(round(rr)), 32767
                ),
            -32767);
        *(d++) = (int16_t)std::max(
            std::min(
                int(round(ll)), 32767
                ),
            -32767);

        n--;
        current_buf_pos++;
        rfac -= 1.0f/128.0f;
        if (rfac < 0.0f) break;

    }


}*/
} // namespace

template <typename T>
void copy_from_xstudio_audio_buffer_to_soundcard_buffer(
    T *&stream,
    media_reader::AudioBufPtr &current_buf,
    long &current_buf_position,
    long &num_samples_to_copy,
    long &num_samps_pushed,
    const int num_channels,
    const int fade_in_out);

template <typename T>
bool copy_samples_to_scrub_buffer(
    T *&stream,
    const long total_samps_needed,
    long &total_samps_pushed,
    media_reader::AudioBufPtr &current_buf,
    const long buffer_offset_sample,
    const long num_channels);

template <typename T>
void reverse_audio_buffer(const T *in, T *out, const int num_channels, const int num_samples);

template <typename T>
media_reader::AudioBufPtr
super_simple_respeed_audio_buffer(const media_reader::AudioBufPtr in, const float velocity);

void AudioOutputControl::prepare_samples_for_soundcard_playback(
    std::vector<int16_t> &v,
    const long num_samps_to_push,
    const long microseconds_delay,
    const int num_channels,
    const int sample_rate) {

    try {

        int16_t *d            = v.data();
        long n                = num_samps_to_push;
        long num_samps_pushed = 0;

        while (n > 0) {

            if (!current_buf_ && sample_data_.size()) {

                // when is the next sample that we copy into the buffer going to get played?
                // We assume there are 'samples_in_soundcard_buffer + num_samps_pushed' audio
                // samples already either in our soundcard buffer or that are already in our
                // new buffer ready to be pushed into the soundcard buffer
                auto next_sample_play_time =
                    utility::clock::now() + std::chrono::microseconds(microseconds_delay) +
                    std::chrono::microseconds((num_samps_pushed * 1000000) / sample_rate);

                current_buf_ = pick_audio_buffer(next_sample_play_time, true);

                if (current_buf_) {

                    current_buf_pos_ = 0;

                    // is audio playback stable ? i.e. is the next sample buffer
                    // continuous with the one we are about to play?
                    next_buf_ = pick_audio_buffer(
                        next_sample_play_time +
                            std::chrono::microseconds(
                                int(round(current_buf_->duration_seconds() * 1000000.0))),
                        false);

                    fade_in_out_ = check_if_buffer_is_contiguous_with_previous_and_next(
                        current_buf_, next_buf_, previous_buf_);

                    /*if (fade_in_out_ != NoFade) {
                        if (previous_buf_)
                            std::cerr << "P " << to_string(previous_buf_->media_key()) << "\n";
                        if (current_buf_)
                            std::cerr << "C " << to_string(current_buf_->media_key()) << "\n";
                        if (next_buf_)
                            std::cerr << "N " << to_string(next_buf_->media_key()) << "\n\n";

                    }*/

                } else {
                    fade_in_out_ = DoFadeHeadAndTail;
                    break;
                }

            } else if (!current_buf_ && sample_data_.empty()) {
                break;
            }

            copy_from_xstudio_audio_buffer_to_soundcard_buffer(
                d,
                current_buf_,
                current_buf_pos_,
                n,
                num_samps_pushed,
                num_channels,
                fade_in_out_);

            if (current_buf_pos_ == (long)current_buf_->num_samples()) {
                // current buf is exhausted, clear current_buf_ so we pick
                // a new one on next pass through this loop
                previous_buf_ = current_buf_;
                current_buf_.reset();
            } else {
                break;
            }
        }

        const float vol = volume();
        if (last_volume_ != vol) {
            changing_volume_adjust(v, vol / 100.0f, last_volume_ / 100.0f);
        } else if (vol != 100.0f) {
            static_volume_adjust(v, vol / 100.0f);
        }
        last_volume_ = vol;

    } catch (std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void AudioOutputControl::prepare_samples_for_audio_scrubbing(
    const std::vector<media_reader::AudioBufPtr> &audio_frames,
    const timebase::flicks playhead_position) {

    playhead_position_ = playhead_position;

    // audio_frames is unlikely to have a frame with a timestamp that exactly
    // matches playhead_position. So we pick the first frame with a timestamp
    // that is less than playhead position and then start copying samples from
    // the appropriate position.

    // Also note that the timestamps in audio_frames correspond exactly to video
    // frame timestamps ... however, because ffmpeg typically delivers audio in
    // chunks that don't line up with video frames (for example, often an audio
    // frame from ffmpeg is 1024 samples, regardless of sample rate and video
    // frame rate). In the xstudio ffmpeg reader we are just gluing together
    // the smaller audio frames to roughly match the beat of the video frames
    // but when we do this we store the offset between the first audio stamp
    // and the 'timeline_timestamp' so we can take account of the difference.

    // store bufs in our map
    sample_data_.clear();
    for (const auto &_frame : audio_frames) {

        if (!_frame)
            continue;
        const auto adjusted_timeline_timestamp = std::chrono::duration_cast<timebase::flicks>(
            _frame.timeline_timestamp() + _frame->time_delta_to_video_frame());

        sample_data_[adjusted_timeline_timestamp] = _frame;
    }

    // now pick nearest buffer
    auto r = sample_data_.lower_bound(playhead_position_);
    if (r != sample_data_.begin()) {
        r--;
    }
    if (r == sample_data_.end())
        return;

    const long num_channels = r->second->num_channels();

    // time diff from first audio sample to playhead position
    auto delta = playhead_position_ - r->first;

    long samples_offset = long(
        round(std::max(0.0, timebase::to_seconds(delta)) * double(r->second->sample_rate())));


    if (samples_offset < 0 || samples_offset >= r->second->num_samples()) {
        // the offset into our 'best' audio buffer from the playhead_position_
        // to the timestamp for the audio buf is outside the duration of the
        // audio buffer
        // we haven't found an audio buffer that corresponds to the playhead_position_
        // so must clear the buffer (i.e. play no audio)
        scrubbing_samples_buf_.clear();
        return;
    }

    // what's the video frame rate ? We can deduce this from the timeline
    // timestamps which match corresponding video frames for the scrub. We
    // should have been delivered 2 or more coniguouse audio frames for the
    // scrub. We use the audio buffer duration as a fallback
    timebase::flicks video_frame_duration = timebase::to_flicks(r->second->duration_seconds());
    auto r_plus                           = r;
    r_plus++;
    if (r_plus != sample_data_.end()) {
        video_frame_duration = r_plus->first - r->first;
    }

    // how many samples do we want to sound for this scrub event?
    long num_scrub_samps = long(
        round(timebase::to_seconds(video_frame_duration) * double(r->second->sample_rate())));

    // now we fill our scrub samples buffer.

    // What if there are samples left in the buffer from the previous scrub event that
    // haven't played out yet? We want to overwrite them, but we need to do a
    // blend so there isn't a discontinuity with the last samples that were
    // dispatched to the soundcard
    const size_t data_in_buffer = scrubbing_samples_buf_.size();
    scrubbing_samples_buf_.resize(num_scrub_samps * num_channels);

    if (data_in_buffer < scrubbing_samples_buf_.size()) {
        // any new samples added to buffer must be zero
        memset(
            scrubbing_samples_buf_.data() + data_in_buffer,
            0,
            (scrubbing_samples_buf_.size() - data_in_buffer) * sizeof(int16_t));
    }

    int16_t *samps = scrubbing_samples_buf_.data();
    long n         = 0;
    while (r != sample_data_.end() && r->second &&
           copy_samples_to_scrub_buffer(
               samps, num_scrub_samps, n, r->second, samples_offset, num_channels)) {
        samples_offset = 0;
        r++;
    }

    // we need to modulate the last samples put into the buffer with an envelope
    // that smoothly takes the amplitude to zero so we don't get transients
    // going to the soundcard should we not be streaming more samples to
    // the soundcard after this buffer is exhausted

    long fade_samps = std::min(long(FADE_FUNC_SAMPS), n);
    samps           = scrubbing_samples_buf_.data() + (n - fade_samps) * num_channels;
    float *f        = fade_out_coeffs.data() + long(FADE_FUNC_SAMPS) - fade_samps;
    while (fade_samps) {
        for (int chn = 0; chn < num_channels; ++chn) {
            (*samps++) = round((*samps) * (*f));
        }
        f++;
        fade_samps--;
    }
}

long AudioOutputControl::copy_samples_to_buffer_for_scrubbing(
    std::vector<int16_t> &v, const long num_samps_to_push) {

    size_t nn = std::min(v.size(), scrubbing_samples_buf_.size());
    if (!nn)
        return 0;
    memcpy(v.data(), scrubbing_samples_buf_.data(), nn * sizeof(int16_t));

    if (nn < scrubbing_samples_buf_.size()) {
        std::vector<int16_t> remaining_samps(scrubbing_samples_buf_.size() - nn);
        memcpy(
            remaining_samps.data(),
            scrubbing_samples_buf_.data() + nn,
            (scrubbing_samples_buf_.size() - nn) * sizeof(int16_t));
        scrubbing_samples_buf_ = std::move(remaining_samps);
    } else {
        scrubbing_samples_buf_.clear();
    }

    if (volume() != 100.0f) {
        static_volume_adjust(v, volume() / 100.0f);
    }

    return (long)nn;
}

void AudioOutputControl::queue_samples_for_playing(
    const std::vector<media_reader::AudioBufPtr> &audio_frames) {

    timebase::flicks t0;
    if (audio_frames.size()) {
        t0 = audio_frames[0].timeline_timestamp();
    }
    timebase::flicks o;
    for (const auto &_frame : audio_frames) {

        // xstudio stores a frame of audio samples for every video frame for any
        // given source (if the source has no video it is assigned a 'virtual' video
        // frame rate to maintain this approach).
        //
        // However, audio frames generally
        // do not have the same duration as video frames, so there is always some
        // offset between when the video frame is shown and when the audio samples
        // associated with that frame should sound. When building sample_data_
        // map we take account of this difference, which was calculate in
        // the fffmpeg reader when the audio samples are packaged up.
        const auto adjusted_timeline_timestamp = std::chrono::duration_cast<timebase::flicks>(
            _frame.timeline_timestamp() +
            (_frame ? _frame->time_delta_to_video_frame() : std::chrono::microseconds(0)));

        if (_frame)
            t0 += timebase::to_flicks(_frame->duration_seconds());
        else
            continue;

        // skip empty frames
        if (!_frame->num_samples()) continue;

        media_reader::AudioBufPtr frame =
            audio_repitch_ && playback_velocity_ != 1.0f
                ? super_simple_respeed_audio_buffer<int16_t>(_frame, playback_velocity_)
                : _frame;

        if (!playing_forward_) {

            media_reader::AudioBufPtr reversed(new media_reader::AudioBuffer(frame->params()));

            reversed->allocate(
                frame->sample_rate(),
                frame->num_channels(),
                frame->num_samples(),
                frame->sample_format());

            reverse_audio_buffer(
                (const int16_t *)frame->buffer(),
                (int16_t *)reversed->buffer(),
                frame->num_samples(),
                frame->num_channels());

            sample_data_[adjusted_timeline_timestamp] = reversed;
            reversed->set_reversed(true);
            reversed->set_display_timestamp_seconds(frame->display_timestamp_seconds());

        } else {
            sample_data_[adjusted_timeline_timestamp] = frame;
        }
    }
}

void AudioOutputControl::playhead_position_changed(
    const timebase::flicks playhead_position,
    const bool forward,
    const float velocity,
    const bool playing,
    utility::time_point when_position_changed) {
    if (!playing_ && playhead_position == playhead_position_) {
        // playhead hasn't moved
    }
    playhead_position_           = playhead_position;
    playback_velocity_           = std::max(0.1f, velocity);
    playing_                     = playing;
    playing_forward_             = forward;
    playhead_position_update_tp_ = when_position_changed;
}

void AudioOutputControl::clear_queued_samples() {
    sample_data_.clear();
    current_buf_.reset();
}

media_reader::AudioBufPtr AudioOutputControl::pick_audio_buffer(
    const utility::clock::time_point &tp, bool drop_old_buffers) {

    // The idea here is we pick an audio buffer from sample_data_ to draw
    // samples off and stream to the soundcard.

    // sample_data_ is a map of audio buffers stored against their play timestamp
    // in the playhead timeline. So for example frame zero in the timeline
    // has timestamp = 0. Frame 2 has timestamp = 41ms (for a 24fps source).

    // 'tp' here is a system clock timepoint that tells us when the last
    // audio sample currently in the soundcard sample buffer will actually
    // get played.

    // based on 'tp' - we estimate the playhead position when the
    // first sample of the next audio buffer that we pick to put into the
    // soundcard buffer will sound.

    // predict where we will be at the timepoint 'next_video_refresh' ...
    const timebase::flicks delta =
        std::chrono::duration_cast<timebase::flicks>(tp - playhead_position_update_tp_);
    const double v = (playing_forward_ ? 1.0f : -1.0f) * playback_velocity_;

    auto future_playhead_position =
        timebase::to_flicks(v * timebase::to_seconds(delta)) + playhead_position_;


    // during playback, we just pick the audio buffer immediately after
    // the one that we last used. However, we check if this new buffer's
    // position in the playback timeline matches well with our estimate of
    // where the playhead will be when the audio samples hit the speakers.
    //
    // If it doesn't we continue and use a best match search below

    // let's step from the last audio buffer we used to the next...
    auto p = sample_data_.find(last_buffer_pts_);
    if (p != sample_data_.end()) {
        if (playing_forward_) {
            p++;
        } else if (!playing_forward_ && p != sample_data_.begin()) {
            p--;
        }

        auto drift = timebase::to_seconds(future_playhead_position - p->first);
        if (fabs(drift) < 0.05) {
            if (drop_old_buffers)
                last_buffer_pts_ = p->first;
            return p->second;
        }
    }

    auto r = sample_data_.lower_bound(future_playhead_position);

    if (r == sample_data_.end()) {
        auto r = media_reader::AudioBufPtr();
        return r;
    }

    // gtp et the audio buf with a 'show' time that is CLOSEST
    // to now, need to look at the previous element to see if
    // it's nearer
    if (r != sample_data_.begin()) {
        auto r2 = r;
        r2--;
        const auto d2 = future_playhead_position - r2->first;
        const auto d1 = r->first - future_playhead_position;

        if (d1 > d2) {
            r = r2;
        }
    }

    media_reader::AudioBufPtr buf = r->second;
    if (drop_old_buffers)
        last_buffer_pts_ = r->first;

    // what if our 'best' buffer, i.e. the one nearest to 'future_playhead_position'
    // is still not close. Some innaccuracy is happening, e.g. buffers that we need
    // haven't been delivered. We must play silence instead,
    const auto t_mismatch = double(std::chrono::duration_cast<std::chrono::microseconds>(
                                       r->first - future_playhead_position)
                                       .count()) /
                            1000000.0;

    // so if we are more than half the duration of the buffer out, return empty ptr
    if (!buf || fabs(t_mismatch) > buf->duration_seconds() / 2) {
        return media_reader::AudioBufPtr();
    }

    if (drop_old_buffers && playing_forward_) {
        sample_data_.erase(sample_data_.begin(), r);
    } else if (drop_old_buffers) {
        sample_data_.erase(r, sample_data_.end());
    }
    return buf;
}

AudioOutputControl::Fade
AudioOutputControl::check_if_buffer_is_contiguous_with_previous_and_next(
    const media_reader::AudioBufPtr &current_buf,
    const media_reader::AudioBufPtr &next_buf,
    const media_reader::AudioBufPtr &previous_buf_) {

    int result = 0;
    if (current_buf->reversed()) {

        if (next_buf && next_buf->reversed()) {
            const double delta = (current_buf_->display_timestamp_seconds() -
                                  next_buf->display_timestamp_seconds()) /
                                     playback_velocity_ -
                                 next_buf->duration_seconds();

            if (fabs(delta) > 0.001) {
                result |= DoFadeTail;
            }
        } else {
            result |= DoFadeTail;
        }

        if (previous_buf_ && previous_buf_->reversed()) {

            const double delta = (previous_buf_->display_timestamp_seconds() -
                                  current_buf_->display_timestamp_seconds()) /
                                     playback_velocity_ -
                                 current_buf_->duration_seconds();

            if (fabs(delta) > 0.001) {
                result |= DoFadeHead;
            }

        } else {
            result |= DoFadeHead;
        }

    } else {

        if (next_buf && !next_buf->reversed()) {
            const double delta = (next_buf->display_timestamp_seconds() -
                                  current_buf_->display_timestamp_seconds()) /
                                     playback_velocity_ -
                                 current_buf_->duration_seconds();
            if (fabs(delta) > 0.001) {
                result |= DoFadeTail;
            }
        } else {
            result |= DoFadeTail;
        }

        if (previous_buf_ && !previous_buf_->reversed()) {

            const double delta = (current_buf_->display_timestamp_seconds() -
                                  previous_buf_->display_timestamp_seconds()) /
                                     playback_velocity_ -
                                 previous_buf_->duration_seconds();

            if (fabs(delta) > 0.001) {
                result |= DoFadeHead;
            }

        } else {
            result |= DoFadeHead;
        }
    }

    return (AudioOutputControl::Fade)result;
}


template <typename T>
void copy_from_xstudio_audio_buffer_to_soundcard_buffer(
    T *&stream,
    media_reader::AudioBufPtr &current_buf,
    long &current_buf_position,
    long &num_samples_to_copy,
    long &num_samps_pushed,
    const int num_channels,
    const int fade_in_out) {

    if (fade_in_out == AudioOutputControl::NoFade) {

        if (((long)current_buf->num_samples() - (long)current_buf_position) >
            num_samples_to_copy) {
            // remaining samples in current buf exceeds num samples we need to put into the
            // stream
            memcpy(
                stream,
                current_buf->buffer() + current_buf_position * num_channels * sizeof(T),
                num_samples_to_copy * num_channels * sizeof(T));
            stream += num_samples_to_copy;
            num_samps_pushed += num_samples_to_copy;
            current_buf_position += num_samples_to_copy;
            num_samples_to_copy = 0;
        } else {
            // fewer samples left in current buffer than the soundcard wants
            memcpy(
                stream,
                current_buf->buffer() + current_buf_position * num_channels * sizeof(T),
                ((long)current_buf->num_samples() - current_buf_position) * num_channels *
                    sizeof(T));
            num_samples_to_copy -= (current_buf->num_samples() - current_buf_position);
            stream += (current_buf->num_samples() - current_buf_position) * num_channels;
            num_samps_pushed += (current_buf->num_samples() - current_buf_position);
            current_buf_position = current_buf->num_samples();
        }

    } else {

        T *tt = ((T *)current_buf->buffer()) + current_buf_position * num_channels;

        if (fade_in_out & AudioOutputControl::DoFadeHead) {
            while (current_buf_position < FADE_FUNC_SAMPS && num_samples_to_copy &&
                   current_buf_position < current_buf->num_samples()) {

                for (int chn = 0; chn < num_channels; ++chn) {
                    (*stream++) = T(round((*tt++) * fade_in_coeffs[current_buf_position]));
                }
                num_samples_to_copy--;
                current_buf_position++;
                num_samps_pushed++;
            }
        }

        const long bpos = std::max(
            std::min(
                long(current_buf->num_samples() - current_buf_position) -
                    ((fade_in_out & AudioOutputControl::DoFadeTail) ? 32l : 0l),
                num_samples_to_copy),
            0l);

        memcpy(stream, tt, bpos * num_channels * sizeof(T));
        num_samples_to_copy -= bpos;
        num_samps_pushed += bpos;
        current_buf_position += bpos;
        stream += bpos * num_channels;
        tt += bpos * num_channels;

        if (fade_in_out & AudioOutputControl::DoFadeTail) {

            while (num_samples_to_copy && current_buf_position < current_buf->num_samples()) {

                const int i   = current_buf->num_samples() - current_buf_position - 1;
                const float f = i < FADE_FUNC_SAMPS ? fade_in_coeffs[i] : 1.0f;

                for (int chn = 0; chn < num_channels; ++chn) {
                    (*stream++) = T(round((*tt++) * f));
                }

                num_samples_to_copy--;
                current_buf_position++;
                num_samps_pushed++;
            }
        }
    }
}

template <typename T>
bool copy_samples_to_scrub_buffer(
    T *&stream,
    const long total_samps_needed,
    long &total_samps_pushed,
    media_reader::AudioBufPtr &current_buf,
    const long buffer_offset_sample,
    const long num_channels) {


    const long buffer_size_samples = current_buf->num_samples();
    long buffer_position           = buffer_offset_sample;

    T *tt = ((T *)current_buf->buffer()) + buffer_offset_sample * num_channels;

    long n = total_samps_needed;
    // when we fill the first samples of 'stream' we blend with any samples
    // already in the buffer to avoid the transients
    while (total_samps_pushed < FADE_FUNC_SAMPS && buffer_position < buffer_size_samples && n) {

        const float f = fade_in_coeffs[total_samps_pushed];
        // blend incoming samps with whatever samps are already in the buffer
        for (int chn = 0; chn < num_channels; ++chn) {
            (*stream++) = T(round((*tt++) * f)) + T(round((*stream) * (1.0f - f)));
        }
        buffer_position++;
        total_samps_pushed++;
        n--;
    }

    long remaining_samples = std::min(
        buffer_size_samples - buffer_position, total_samps_needed - total_samps_pushed);
    memcpy(stream, tt, remaining_samples * sizeof(T) * num_channels);
    stream += remaining_samples * num_channels;
    total_samps_pushed += remaining_samples;

    return total_samps_pushed < total_samps_needed;
}

template <typename T>
void reverse_audio_buffer(const T *in, T *out, const int num_samples, const int num_channels) {

    int c = num_channels;
    while (c--) {

        const T *inc = in++;
        int n        = num_samples;
        T *outc      = (out++) + (n - 1) * num_channels;

        while (n--) {

            *(outc) = *(inc);
            outc -= num_channels;
            inc += num_channels;
        }
    }
}

template <typename T>
media_reader::AudioBufPtr
super_simple_respeed_audio_buffer(const media_reader::AudioBufPtr in, const float velocity) {

    media_reader::AudioBufPtr resampled(new media_reader::AudioBuffer(in->params()));

    resampled->set_display_timestamp_seconds(in->display_timestamp_seconds());

    resampled->allocate(
        in->sample_rate(),
        in->num_channels(),
        (int)round(float(in->num_samples()) / velocity),
        in->sample_format());

    const T *in_buf = (const T *)in->buffer();
    T *out_buf      = (T *)resampled->buffer();

    int n               = resampled->num_samples() - 1;
    float ff            = 0.0f;
    const float ff_step = float(in->num_samples()) / float(resampled->num_samples());
    const int chans     = in->num_channels();

    // no doubt some SSE stuff would help here?

    while (n--) {

        const T *isamp0 = in_buf + int(ff) * chans;
        const T *isamp1 = isamp0 + chans;
        const float v   = ff - floor(ff);

        int c = chans;
        while (c--) {
            *(out_buf++) = T(float(*(isamp0++)) * (1.0 - v) + float(*(isamp1++)) * (v));
        }
        ff += ff_step;
    }

    int c  = chans;
    in_buf = (const T *)in->buffer() + (in->num_samples() - 1) * chans;
    while (c--) {
        *(out_buf++) = *(in_buf++);
    }

    return resampled;
}
