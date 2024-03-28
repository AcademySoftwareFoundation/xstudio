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

namespace {

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
void reverse_audio_buffer(const T *in, T *out, const int num_channels, const int num_samples);

template <typename T>
media_reader::AudioBufPtr
super_simple_respeed_audio_buffer(const media_reader::AudioBufPtr in, const float velocity);

void AudioOutputControl::prepare_samples_for_soundcard(
    std::vector<int16_t> &v,
    const long num_samps_to_push,
    const long microseconds_delay,
    const int num_channels,
    const int sample_rate) {

    try {

        v.resize(num_samps_to_push * num_channels);
        memset(v.data(), 0, v.size() * sizeof(int16_t));

        int16_t *d            = v.data();
        long n                = num_samps_to_push;
        long num_samps_pushed = 0;

        if (muted())
            return;

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
                    auto next_buf = pick_audio_buffer(
                        next_sample_play_time +
                            std::chrono::microseconds(
                                int(round(current_buf_->duration_seconds() * 1000000.0))),
                        false);

                    fade_in_out_ = check_if_buffer_is_contiguous_with_previous_and_next(
                        current_buf_, next_buf, previous_buf_);

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
                // current buf is exhausted
                previous_buf_ = current_buf_;
                current_buf_.reset();
            } else {
                break;
            }
        }

        const float vol       = volume();
        static float last_vol = vol;
        if (last_vol != vol) {
            changing_volume_adjust(v, vol / 100.0f, last_vol / 100.0f);
        } else if (vol != 100.0f) {
            static_volume_adjust(v, vol / 100.0f);
        }
        last_vol = vol;

    } catch (std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void AudioOutputControl::queue_samples_for_playing(
    const std::vector<media_reader::AudioBufPtr> &audio_frames,
    const bool playing,
    const bool forwards,
    const float velocity) {

    if (!playing) {

        return;
    }

    playback_velocity_ = audio_repitch_ ? std::max(0.1f, velocity) : 1.0f;

    for (const auto &a : audio_frames) {

        auto audio_frame = a;
        // if we're not playing and the last audio buffer played is the same as
        // the one we're receiving now we don't queue it for playing. This is because
        // a viewport refresh (for e.g. exposure scrubbing) results in the same
        // image frame being broadcast by the playhead
        if (!audio_frame ||
            (previous_buf_ && previous_buf_->media_key() == audio_frame->media_key()) ||
            (current_buf_ && current_buf_->media_key() == audio_frame->media_key()) ||
            !audio_frame->num_samples())
            continue;


        // xstudio stores a frame of audio samples for every video frame for any
        // given source (if the source has no video it is assigned a 'virtual' video
        // frame rate to maintain this approach). However, audio frames generally
        // do not have the same duration as video frames, so there is always some
        // offset between when the video frame is shown and when the audio samples
        // associated with that frame should sound.
        const auto when_to_sound_audio =
            audio_frame.when_to_display_ + audio_frame->time_delta_to_video_frame();

        // have we already got these audio samples in our queue? If so erase and
        // add back in to update the key
        for (auto p = sample_data_.begin(); p != sample_data_.end(); ++p) {
            if (p->second->media_key() == audio_frame->media_key()) {
                sample_data_.erase(p);
                break;
            }
        }

        if (audio_repitch_ && velocity != 1.0f) {
            audio_frame =
                super_simple_respeed_audio_buffer<int16_t>(audio_frame, fabs(velocity));
        }

        if (!forwards) {

            media_reader::AudioBufPtr reversed(
                new media_reader::AudioBuffer(audio_frame->params()));

            reversed->allocate(
                audio_frame->sample_rate(),
                audio_frame->num_channels(),
                audio_frame->num_samples(),
                audio_frame->sample_format());

            reverse_audio_buffer(
                (const int16_t *)audio_frame->buffer(),
                (int16_t *)reversed->buffer(),
                audio_frame->num_samples(),
                audio_frame->num_channels());

            sample_data_[when_to_sound_audio] = reversed;
            reversed->set_reversed(true);

            reversed->set_display_timestamp_seconds(audio_frame->display_timestamp_seconds());

        } else {
            sample_data_[when_to_sound_audio] = audio_frame;
        }
    }
}

void AudioOutputControl::clear_queued_samples() {
    sample_data_.clear();
    current_buf_.reset();
}

media_reader::AudioBufPtr AudioOutputControl::pick_audio_buffer(
    const utility::clock::time_point &tp, bool drop_old_buffers) {

    auto r = sample_data_.lower_bound(tp);

    if (r == sample_data_.end())
        return media_reader::AudioBufPtr();

    // gtp et the audio buf with a 'show' time that is CLOSEST
    // to now, need to look at the previous element to see if
    // it's nearer
    if (r != sample_data_.begin()) {
        auto r2 = r;
        r2--;
        const auto d2 = tp - r2->first;
        const auto d1 = r->first - tp;

        if (d1 > d2) {
            r = r2;
        }
    }

    media_reader::AudioBufPtr v = r->second;

    // if the audio buffer is not supposed to be played close to 'tp' we want to carry on and
    // play silence until soundcard and audio buffers are in sync
    const auto delta =
        double(std::chrono::duration_cast<std::chrono::microseconds>(r->first - tp).count()) /
        1000000.0;
    // if (0) {//fabs(delta) > v->duration_seconds()/2) {
    //     return media_reader::AudioBufPtr();
    // }

    if (drop_old_buffers) {
        r++;
        sample_data_.erase(sample_data_.begin(), r);
    }
    return v;
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

// when blocks of audio samples aren't contiguous then to avoid a transient
// at the border between the blocks we fade out samples at the tail of the
// current block and the head of the next block. The window for this fade out
// is defined here. It still causes distortion but much less bad.
#define FADE_FUNC_SAMPS 128

template <typename T>
void copy_from_xstudio_audio_buffer_to_soundcard_buffer(
    T *&stream,
    media_reader::AudioBufPtr &current_buf,
    long &current_buf_position,
    long &num_samples_to_copy,
    long &num_samps_pushed,
    const int num_channels,
    const int fade_in_out) {

    static std::vector<float> fade_coeffs;
    if (fade_coeffs.empty()) {
        fade_coeffs.resize(FADE_FUNC_SAMPS);
        for (int i = 0.0f; i < FADE_FUNC_SAMPS; ++i) {
            fade_coeffs[i] = (sin(float(i) * M_PI / float(FADE_FUNC_SAMPS)) + 1.0f) * 0.5f;
        }
    }

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
                    (*stream++) = T(round((*tt++) * fade_coeffs[current_buf_position]));
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
                const float f = i < FADE_FUNC_SAMPS ? fade_coeffs[i] : 1.0f;

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
