// SPDX-License-Identifier: Apache-2.0
#include <iostream>


#include "ffmpeg_decoder.hpp"
#include "xstudio/media/media_error.hpp"

#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define MIN_SEEK_FORWARD_FRAMES 16

using namespace xstudio::media_reader::ffmpeg;
using namespace xstudio;

namespace {
std::string make_stream_id(const int stream_index) {
    return fmt::format("stream {}", stream_index);
}
} // namespace

int FFMpegDecoder::ffmpeg_threads = 8;

FFMpegDecoder::FFMpegDecoder(
    std::string path, const int soundcard_sample_rate, std::string stream_id)
    : movie_file_path_(std::move(path)),
      last_requested_frame_(-100),
      avc_packet_(nullptr),
      av_format_ctx_(nullptr),
      soundcard_sample_rate_(soundcard_sample_rate),
      last_decoded_frame_(-100),
      stream_id_(std::move(stream_id))

{
    open_handles();
}

FFMpegDecoder::~FFMpegDecoder() { close_handles(); }


void FFMpegDecoder::open_handles() {

    if (not avc_packet_) {
        avc_packet_ = av_packet_alloc();
    }

    AVC_CHECK_THROW(
        avformat_open_input(&av_format_ctx_, movie_file_path_.c_str(), nullptr, nullptr),
        "avformat_open_input");
    AVC_CHECK_THROW(
        avformat_find_stream_info(av_format_ctx_, nullptr), "avformat_find_stream_info");

    av_format_ctx_->flags |= AVFMT_FLAG_GENPTS;
    // Find all of the video / data streams inside the file
    for (int i = 0; i < (int)av_format_ctx_->nb_streams; i++) {
        try {

            streams_[i] = std::make_shared<FFMpegStream>(
                av_format_ctx_,
                av_format_ctx_->streams[i],
                i,
                ffmpeg_threads,
                movie_file_path_);

        } catch ([[maybe_unused]] std::exception &e) {
        }
    }

    calc_duration_frames();

    // now remove streams that we aren't interested in
    exclude_unwanted_streams();
}

void FFMpegDecoder::calc_duration_frames() {

    // if we aren't loading a specific (known) stream, we load all streams to get
    // the details on the source. We also try and find out which video and/or
    // audio tracks are the 'best'
    int best_video_stream =
        stream_id_.empty()
            ? av_find_best_stream(av_format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0)
            : -1;

    int best_audio_stream =
        stream_id_.empty()
            ? av_find_best_stream(av_format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0)
            : -1;

    StreamPtr primary_video_stream;
    StreamPtr primary_audio_stream;

    for (auto &s : streams_) {
        auto &stream = s.second;
        if (stream->codec_type() == AVMEDIA_TYPE_VIDEO && !primary_video_stream) {
            if (best_video_stream == -1 || stream->stream_index() == best_video_stream) {
                primary_video_stream = stream;
            }
        } else if (stream->codec_type() == AVMEDIA_TYPE_AUDIO && !primary_audio_stream) {
            if (best_audio_stream == -1 || stream->stream_index() == best_audio_stream) {
                primary_audio_stream = stream;
            }
        } else if (stream->stream_type() == TIMECODE_STREAM && !timecode_stream_) {
            timecode_stream_ = stream;
        }
    }

    if (primary_video_stream && !primary_video_stream->is_single_frame()) {

        // xSTUDIO requires that streams within a given source have the same frame rate.
        // We force other streams to have a frame rate that matches the 'primary' video
        // stream. Particulary for audio streams, which typically deliver frames at a
        // different rate to video, we then re-parcel audio samples in a sort-of pull-down
        // to match the video rate. See the note immediately below for a bit more detail.
        for (auto &stream : streams_) {

            stream.second->set_virtual_frame_rate(primary_video_stream->frame_rate());
        }

    } else if (primary_audio_stream) {

        // What's happening here is for any audio only sources we enforce a frame rate of 60fps.
        // We do this because ffmpeg doesn't usually tell us the duration of audio frames (and
        // therefore the frame rate) until we start decoding. Audio is really a 'stream' and we
        // can't even be sure that the audio frame durations are constant. Thus, we force the
        // frame rate here. During decode we resolve xSTUDIO's virtual frame number into the
        // actual position in the audio stream. Audio frames coming from ffmpeg are assigned to
        // virtual frames as accurately as possible, but there will be a mismatch between the
        // PTS (presentation time stamp - when the samples should be played) of the ffmpeg audio
        // frame and the PTS of the xSTUDIO frame. We deal this by recording the offset between
        // the ffmpeg PTS and the xSTUDIO PTS. The audio playback engine will take this into
        // account as it streams audio frames to the soundcard.
        for (auto &stream : streams_) {

            stream.second->set_virtual_frame_rate(
                utility::FrameRate(timebase::k_flicks_one_sixtieth_second));
        }
    }

    duration_frames_ = 0;
    if (primary_video_stream) {

        if (primary_audio_stream && primary_video_stream->is_single_frame()) {
            // this can be the case with audio files that have a single image
            // video stream (album cover art, for example) so we fall back on
            // the audio stream duration
            duration_frames_ = primary_audio_stream->duration_frames();
        } else {
            duration_frames_ = primary_video_stream->duration_frames();
        }

    } else if (primary_audio_stream) {
        duration_frames_ = primary_audio_stream->duration_frames();
    }
}

void FFMpegDecoder::exclude_unwanted_streams() {

    if (stream_id_ != "") {
        for (auto p = streams_.begin(); p != streams_.end();) {
            if (make_stream_id(p->first) == stream_id_) {
                decode_stream_ = p->second;
                p++;
            } else {
                p = streams_.erase(p);
            }
        }
    }
}

void FFMpegDecoder::close_handles() {

    streams_.clear();

    if (av_format_ctx_) {
        avformat_close_input(&av_format_ctx_);
        av_format_ctx_ = nullptr;
    }

    if (avc_packet_) {
        av_packet_unref(avc_packet_);
        av_packet_free(&avc_packet_);
        avc_packet_ = nullptr;
    }
    decode_stream_.reset();
    timecode_stream_.reset();
}

// returns 0 if a frame was decoded. Returns AVERROR_EOF if we're at the end
// of a file, returns AVERROR(EAGAIN) if it needs more data, throws an exception
// if there are any other errors
int64_t FFMpegDecoder::decode_and_store_next_frame() {

    // does video stream have more video frames it can decode from the last
    // packet that it received?
    if (decode_stream_ && decode_stream_->receive_frame() == 0) {
        pull_buffer_from_stream(decode_stream_);
        return 0;
    }

    // try and read a frame from the stream into a packet
    int read_code = av_read_frame(av_format_ctx_, avc_packet_);

    if (read_code == AVERROR_EOF) {

        // end of file! We need to send a 'flush' packet to the
        // streams and keep decoding frames until exhausted.

        if (decode_stream_) {
            decode_stream_->send_flush_packet();
            while (decode_stream_->receive_frame() == 0) {
                pull_buffer_from_stream(decode_stream_);
            }
        }

        av_packet_unref(avc_packet_);
        return AVERROR_EOF;

    } else if (!read_code) { // frame read ok!

        if (decode_stream_ && avc_packet_->stream_index == decode_stream_->stream_index()) {
            int rx = decode_stream_->send_packet(avc_packet_);
            if (rx)
                AVC_CHECK_THROW(rx, "avcodec_send_packet");
            if (decode_stream_->receive_frame() == 0) {
                pull_buffer_from_stream(decode_stream_);
            }
        } else {
            av_packet_unref(avc_packet_);
        }

    } else {

        av_packet_unref(avc_packet_);
        AVC_CHECK_THROW(read_code, "av_read_frame");
    }
    return 0;
}

// returns 0 if a frame was decoded. Returns AVERROR_EOF if we're at the end
// of a file, returns AVERROR(EAGAIN) if it needs more data, throws an exception
// if there are any other errors
int64_t FFMpegDecoder::decode_next_frame() {

    // does video stream have more video frames it can decode from the last
    // packet that it received?
    if (decode_stream_ && decode_stream_->receive_frame() == 0) {
        return 0;
    }

    // try and read a frame from the stream into a packet
    int read_code = av_read_frame(av_format_ctx_, avc_packet_);

    if (read_code == AVERROR_EOF) {

        // end of file! We need to send a 'flush' packet to the
        // streams and keep decoding frames until exhausted.

        if (decode_stream_) {
            decode_stream_->send_flush_packet();
            int64_t rt = decode_stream_->receive_frame();
            while (rt == AVERROR(EAGAIN)) {
                rt = decode_stream_->receive_frame();
            }
            if (!rt)
                return 0;
        }

        return AVERROR_EOF;

    } else if (!read_code) { // frame read ok!

        if (decode_stream_ && avc_packet_->stream_index == decode_stream_->stream_index()) {
            int rx = decode_stream_->send_packet(avc_packet_);
            av_packet_unref(avc_packet_);
            if (rx)
                AVC_CHECK_THROW(rx, "avcodec_send_packet");
            return decode_stream_->receive_frame();
        }

    } else {

        av_packet_unref(avc_packet_);
        AVC_CHECK_THROW(read_code, "av_read_frame");
    }
    return 0;
}

xstudio::utility::Timecode FFMpegDecoder::first_frame_timecode() {
    if (!timecode_stream_) {

        // try and sniff from metadata, possibly unreliable
        // dump tags..
        if (av_format_ctx_) {
            if (av_format_ctx_->metadata) {
                AVDictionaryEntry *tag = nullptr;
                while (
                    (tag = av_dict_get(
                         av_format_ctx_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                    if (utility::to_lower(std::string(tag->key)) == "timecode") {
                        // invalid timecode string will cause an exception, which
                        // we will silently skip
                        try {
                            return xstudio::utility::Timecode(
                                tag->value,
                                (frame_rate().to_fps() ? frame_rate().to_fps() : 24.0));
                        } catch (...) {
                        }
                    }
                }
            }

            // try streams.
            for (auto &i : streams_) {
                if (i.second and i.second->tags()) {
                    AVDictionaryEntry *tag = nullptr;
                    while (
                        (tag = av_dict_get(i.second->tags(), "", tag, AV_DICT_IGNORE_SUFFIX))) {

                        if (utility::to_lower(std::string(tag->key)) == "timecode") {
                            try {
                                return xstudio::utility::Timecode(
                                    tag->value,
                                    (frame_rate().to_fps() ? frame_rate().to_fps() : 24.0));
                            } catch (...) {
                            }
                        }
                    }
                }
            }
        }

        // give up
        return xstudio::utility::Timecode(
            "00:00:00:00", (frame_rate().to_fps() ? frame_rate().to_fps() : 24.0));
    } else {

        int64_t timestamp = timecode_stream_->frame_to_pts(0);

        // avcodec_flush_buffers(timecode_stream_->codec_context_);
        std::stringstream msg;
        msg << "av_seek_frame to frame " << 0 << ", timestamp " << timestamp;

        AVC_CHECK_THROW(
            av_seek_frame(
                av_format_ctx_,
                timecode_stream_->stream_index(),
                timestamp,
                AVSEEK_FLAG_BACKWARD),
            msg.str().c_str());

        AVPacket *newPacket = av_packet_alloc();
        av_init_packet(newPacket);

        uint32_t timecodeOffset = 0;
        while (!av_read_frame(av_format_ctx_, newPacket)) {

            if (newPacket->stream_index == timecode_stream_->stream_index()) {

                for (int i = 0; i < newPacket->size; i++) {
                    int shift = (newPacket->size - 1 - i) * 8;
                    timecodeOffset += uint32_t(newPacket->data[i]) << shift;
                }
                av_packet_unref(newPacket);
                break;
            }
            av_packet_unref(newPacket);
        }
        av_packet_free(&newPacket);
        return xstudio::utility::Timecode(
            timecodeOffset, frame_rate().to_fps(), timecode_stream_->is_drop_frame_timecode());
    }
}


void FFMpegDecoder::decode_audio_frame(
    const int64_t frame_num,
    AudioBufPtr &audio_buffer,
    std::vector<unsigned int> /*stream_indeces*/
) {

    try {

        requested_decode_frame_ = frame_num;

        decoding_backwards_ = (last_requested_frame_ - frame_num) == 1 ||
                              (last_requested_frame_ - frame_num) == 2;

        if (have_audio(frame_num) && have_audio(frame_num + 1)) {
            // we've already decoded this audio frame but not used it
            audio_buffer          = get_audio_frame_and_release_from_mini_cache(frame_num);
            last_requested_frame_ = frame_num;
            return;
        }

        if (last_requested_frame_ != (frame_num - 1)) {
            do_seek(frame_num);
        }

        // this is the decode loop, we keep going until we have decoded the frame
        // that we want.
        while (last_decoded_frame_ <= frame_num) {

            // decoding of any and all streams happens in this call, and if
            // we get a complete video or audio frame it is also stored within
            // this function
            if (decode_and_store_next_frame() == AVERROR_EOF) {
                break;
            }
        }

        // re-check if we have the frame we want
        if (have_audio(frame_num)) {
            audio_buffer = get_audio_frame_and_release_from_mini_cache(frame_num);
            empty_mini_caches(frame_num);
            last_requested_frame_ = frame_num;
        } else {
            last_requested_frame_ = -100;
        }

    } catch ([[maybe_unused]] std::exception &e) {

        // some error has occurred ... force a fresh seek on next try
        last_requested_frame_ = -100;
        fflush(stderr);
        throw;
    }
}

void FFMpegDecoder::decode_video_frame(
    const int64_t frame_num,
    ImageBufPtr &image_buffer,
    std::vector<unsigned int> /*stream_indeces*/
) {

    try {

        requested_decode_frame_ = frame_num;

        decoding_backwards_ = (last_requested_frame_ - frame_num) == 1 ||
                              (last_requested_frame_ - frame_num) == 2;

        if (have_video(frame_num)) {
            // we've already decoded this video frame but not used it
            image_buffer          = get_frame_and_release_from_mini_cache(frame_num);
            last_requested_frame_ = frame_num;
            return;
        }

        if (decode_stream_ && decode_stream_->is_single_frame()) {

            // single image source like a JPEG. xstudio might say frame_num = 100 but
            // ffmpeg thinks its frame zero (and video length = 1 frame). This is because
            // for frame based sources that are decoded by FFMPEG, we do not treat the
            // sequence of files as a single stream for ffmpeg to decode but instead
            // each individual file will create an FFMpegDecoder instance referencing
            // that one file.
            requested_decode_frame_ = 0;
            while (decode_next_frame() != AVERROR_EOF) {
            }
            image_buffer = decode_stream_->get_ffmpeg_frame_as_xstudio_image();
            return;
        }

        if (last_requested_frame_ != (frame_num - 1)) {
            do_seek(frame_num);
        }

        // this is the decode loop, we keep going until we have decoded the frame
        // that we want.
        while (last_decoded_frame_ < frame_num) {

            // decoding of any and all streams happens in this call, and if
            // we get a complete video or audio frame it is also stored
            // within/hosts/hawley/user_data/QT_PIX_FORMATS_PROP/ this function
            if (decode_and_store_next_frame() == AVERROR_EOF) {
                break;
            }
        }

        // re-check if we have the frame we want
        if (have_video(frame_num)) {
            image_buffer = get_frame_and_release_from_mini_cache(frame_num);
        } else {
            throw media_corrupt_error(
                std::string("Failed to decode frame ") + std::to_string(frame_num) +
                " with AVERROR_EOF");
        }

        empty_mini_caches(frame_num);

        last_requested_frame_ = frame_num;

    } catch ([[maybe_unused]] std::exception &e) {

        // some error has occurred ... force a fresh seek on next try
        last_requested_frame_ = -100;
        fflush(stderr);
        throw;
    }
}

xstudio::utility::FrameRate FFMpegDecoder::frame_rate(unsigned int stream_idx) const {

    if (stream_idx == UINT_MAX && streams_.size()) {
        return streams_.begin()->second->frame_rate();
    }

    auto p = streams_.find(stream_idx);
    if (p == streams_.end()) {
        throw std::runtime_error("FFMpegDecoder::frame_rate, bad stream index");
    }

    return p->second->frame_rate();
}

std::shared_ptr<thumbnail::ThumbnailBuffer>
FFMpegDecoder::decode_thumbnail_frame(const int64_t frame_num, const size_t size_hint) {

    std::shared_ptr<thumbnail::ThumbnailBuffer> rt;
    if (!decode_stream_)
        return rt;
    try {

        if (decode_stream_->is_single_frame()) {

            while (decode_next_frame() != AVERROR_EOF) {
            }
            rt = decode_stream_->convert_av_frame_to_thumbnail(size_hint);

        } else {

            do_seek(frame_num, true);

            while (decode_stream_->current_frame() < frame_num &&
                   decode_next_frame() != AVERROR_EOF) {
            }
            rt = decode_stream_->convert_av_frame_to_thumbnail(size_hint);
        }

    } catch ([[maybe_unused]] std::exception &e) {

        // some error has occurred ... force a fresh seek on next try
        last_requested_frame_ = -100;
        fflush(stderr);
        throw;
    }
    return rt;
}

bool FFMpegDecoder::have_video(const int frame_num) const {
    auto p = video_frame_mini_cache_.find(frame_num);
    if (p != video_frame_mini_cache_.end()) {
        return true;
    }
    return false;
}

bool FFMpegDecoder::have_audio(const int frame_num) const {
    auto p = audio_frame_mini_cache_.find(frame_num);
    if (p != audio_frame_mini_cache_.end()) {
        return true;
    }
    return false;
}

media_reader::ImageBufPtr
FFMpegDecoder::get_frame_and_release_from_mini_cache(const int frame_num) {
    ImageBufPtr rt;
    auto p = video_frame_mini_cache_.find(frame_num);
    if (p != video_frame_mini_cache_.end()) {
        rt = p->second;
        video_frame_mini_cache_.erase(p);
    }
    return rt;
}

media_reader::AudioBufPtr
FFMpegDecoder::get_audio_frame_and_release_from_mini_cache(const int frame_num) {
    AudioBufPtr rt;
    auto p = audio_frame_mini_cache_.find(frame_num);
    if (p != audio_frame_mini_cache_.end()) {
        rt = p->second;
        audio_frame_mini_cache_.erase(p);
    }
    return rt;
}

void FFMpegDecoder::pull_buffer_from_stream(StreamPtr &stream) {

    if (stream->stream_type() == VIDEO_STREAM) {
        pull_video_buffer_from_stream(stream);
    } else if (stream->stream_type() == AUDIO_STREAM) {
        pull_audio_buffer_from_stream(stream);
    }
}

void FFMpegDecoder::pull_video_buffer_from_stream(StreamPtr &video_stream) {

    // Do we want the frame that was just decoded from video_stream ?
    // are we decoding forwards (after a seek) and haven't got to the frame we need?
    if (video_stream && video_stream->current_frame() < requested_decode_frame_ &&
        !decoding_backwards_)
        return;

    ImageBufPtr buf;
    if (video_stream) {

        buf = video_stream->get_ffmpeg_frame_as_xstudio_image();
    }

    if (!buf) {

        // If video_stream is null, we want to make a phoney video buffer with
        // an appropriate timestamp ... we can then attach audio frames to this
        // video buffer to send back to playback engine.
        buf.reset(new ImageBuffer("No Video"));
        buf->set_duration_seconds(frame_rate().to_seconds());
        buf->set_display_timestamp_seconds(requested_decode_frame_ * frame_rate().to_seconds());
        buf->set_decoder_frame_number(requested_decode_frame_);
    }

    video_frame_mini_cache_[buf->decoder_frame_number()] = buf;
    last_decoded_frame_                                  = buf->decoder_frame_number();
}

void FFMpegDecoder::pull_audio_buffer_from_stream(StreamPtr &audio_stream) {

    AudioBufPtr buf = audio_stream->get_ffmpeg_frame_as_xstudio_audio(soundcard_sample_rate_);
    if (buf) {

        // using the display timestamp of these audio samples, work out the
        // corresponding 'virtual' frame. Remember, audio might be delivered by
        // ffmpeg in chunks of 1024 samples with a sample rate of 44100Hz, say.
        // The video rate for the source might be 24fps, though. So audio and
        // video have very different frame rate. We force audio frames (in xSTUDIO)
        // to be at 24fps also - we do this by putting the 1024 chunks of samples
        // into the AudioBuffer (coming at 24fps) with the closest timestamp.

        int virtual_frame = int(floor(
            buf->display_timestamp_seconds() / (audio_stream->frame_rate().to_seconds())));

        // we are forcing audio frames into a 'virtual frame rate' that probably doesn't match
        // the rate of the audio frames coming from ffmpeg. The time that the virtual frame
        // should be sent to the soundcard slightly differs vs. the actual samples it contains.
        // Here we measure that difference, and it's used in the audio output to accurately pick
        // the samples in the audio output loop.
        double xstudio_frame_dts = virtual_frame * audio_stream->frame_rate().to_seconds();
        double ffmpeg_frame_dts  = buf->display_timestamp_seconds();
        buf->set_time_delta_to_video_frame(std::chrono::microseconds(
            (int)round((ffmpeg_frame_dts - xstudio_frame_dts) * 1000000.0)));


        auto p = audio_frame_mini_cache_.find(virtual_frame);
        if (p != audio_frame_mini_cache_.end()) {

            // these samepls have a DTS that falls into the period of a virtual
            // frame for which we already have some samples. We append these samples
            // to the existing samples.
            p->second->extend(*buf);
        } else {
            audio_frame_mini_cache_[virtual_frame] = buf;
        }

        if (last_decoded_frame_ != -100) {
            for (int f = (last_decoded_frame_ + 1); f < virtual_frame; ++f) {
                audio_frame_mini_cache_[f] = AudioBufPtr();
            }
        }

        last_decoded_frame_ = virtual_frame;
    }
}

void FFMpegDecoder::do_seek(const int seek_frame, bool force) {

    // tring a couple of tricks here ... the assumption is any codec that can't
    // seek to an exact frame can only decode forwards efficiently. If we are
    // decoding backwards, it's best to go a good few frames back and then
    // decode forwards until we get the frame we need. All the intermediate
    // frames will be put in our mini cache, so next time we need a frame
    // that is before the one we were just asked for it's already decoded.

    if (decode_stream_)
        decode_stream_->set_current_frame_unknown();

    if (decode_stream_ && (force || seek_frame <= last_requested_frame_ ||
                           seek_frame > (last_requested_frame_ + MIN_SEEK_FORWARD_FRAMES))) {

        // here, if we are going backwards frame by frames, we are going
        // to jump back by 16 frames
        int64_t timestamp = decode_stream_->frame_to_pts(
            decoding_backwards_ ? std::max(seek_frame - 16, 0) : std::max(seek_frame - 1, 0));

        decode_stream_->flush_buffers();
        if (decode_stream_)
            decode_stream_->flush_buffers();

        std::stringstream msg;
        msg << "av_seek_frame to frame " << seek_frame << ", timestamp " << timestamp;
        AVC_CHECK_THROW(
            av_seek_frame(
                av_format_ctx_,
                decode_stream_->stream_index(),
                timestamp,
                AVSEEK_FLAG_BACKWARD),
            msg.str().c_str());

        last_decoded_frame_ = -100;

        // clear our caches
        video_frame_mini_cache_.clear();
        audio_frame_mini_cache_.clear();
    }
}

void FFMpegDecoder::empty_mini_caches(const int decoded_frame) {

    const bool decoding_backwards = (last_requested_frame_ - decoded_frame) == 1 ||
                                    (last_requested_frame_ - decoded_frame) == 2;

    // if we are playing backwards, we don't want to clear our 'mini cache'
    // because many codecs are not seekable to a frame, so to access any frame
    // we seek to the nearest preceding frame and decode forwards. We want to
    // keep the intermediate frames ready for reverse playback
    if (decoding_backwards)
        return;

    // could use lower_bound here
    auto p = video_frame_mini_cache_.begin();
    while (p != video_frame_mini_cache_.end()) {
        if (p->first <= decoded_frame) {
            p = video_frame_mini_cache_.erase(p);
        } else {
            p++;
        }
    }

    // could use lower_bound here
    auto p2 = audio_frame_mini_cache_.begin();
    while (p2 != audio_frame_mini_cache_.end()) {
        if (p2->first <= decoded_frame) {
            p2 = audio_frame_mini_cache_.erase(p2);
        } else {
            p2++;
        }
    }
}
