// SPDX-License-Identifier: Apache-2.0
#include <iostream>


#include "ffmpeg_decoder.hpp"
#include "xstudio/media/media_error.hpp"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

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
    std::string path,
    const int soundcard_sample_rate,
    const FFMpegStreamType wanted_stream_types,
    std::string stream_id,
    const utility::FrameRate video_frame_rate)
    : movie_file_path_(std::move(path)),
      last_requested_frame_(-100),
      avc_packet_(nullptr),
      av_format_ctx_(nullptr),
      soundcard_sample_rate_(soundcard_sample_rate),
      last_decoded_frame_(-100),
      wanted_stream_type_(wanted_stream_types),
      stream_id_(std::move(stream_id)),
      video_frame_rate_(std::move(video_frame_rate))

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

    // Find all of the video / data streams inside the file
    for (int i = 0; i < (int)av_format_ctx_->nb_streams; i++) {
        try {

            streams_[i] = std::make_shared<FFMpegStream>(
                av_format_ctx_,
                av_format_ctx_->streams[i],
                i,
                ffmpeg_threads,
                movie_file_path_);

            if (streams_[i]->codec_type() == AVMEDIA_TYPE_VIDEO && !primary_video_stream_) {
                primary_video_stream_ = streams_[i];
            }
            if (streams_[i]->stream_type() == TIMECODE_STREAM && !timecode_stream_) {
                timecode_stream_ = streams_[i];
            }
            if (streams_[i]->codec_type() == AVMEDIA_TYPE_AUDIO && !primary_audio_stream_) {
                primary_audio_stream_ = streams_[i];
            }

        } catch (std::exception &e) {
        }
    }

    // store the video frame rate, as we might only be decoding audio but
    // we need a reference video frame rate
    if (primary_video_stream_) {
        video_frame_rate_ = primary_video_stream_->frame_rate();
    }

    calc_duration_frames();

    // now remove streams that we aren't interested in
    exclude_unwanted_streams();
}

void FFMpegDecoder::calc_duration_frames() {

    duration_frames_ = 0;
    if (primary_video_stream_) {

        if (primary_audio_stream_ && primary_video_stream_->is_single_frame()) {
            // this can be the case with audio files that have a single image
            // video stream (album cover art, for example) so we fall back on
            // the audio stream duration
            duration_frames_ = primary_audio_stream_->duration_frames();
        } else {
            duration_frames_ = primary_video_stream_->duration_frames();
        }

    } else if (primary_audio_stream_) {
        duration_frames_ = primary_audio_stream_->duration_frames();
    }
}

void FFMpegDecoder::exclude_unwanted_streams() {

    for (auto p = streams_.begin(); p != streams_.end();) {

        if (stream_id_ != "") {
            if (make_stream_id(p->first) != stream_id_) {
                p = streams_.erase(p);
            } else {
                p++;
            }
        } else if (!(p->second->stream_type() & wanted_stream_type_)) {
            p = streams_.erase(p);
        } else {
            p++;
        }
    }
    if (!(wanted_stream_type_ & AUDIO_STREAM))
        primary_audio_stream_.reset();
    if (!(wanted_stream_type_ & VIDEO_STREAM))
        primary_video_stream_.reset();
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
    primary_video_stream_.reset();
    primary_audio_stream_.reset();
    timecode_stream_.reset();
}

// returns 0 if a frame was decoded. Returns AVERROR_EOF if we're at the end
// of a file, returns AVERROR(EAGAIN) if it needs more data, throws an exception
// if there are any other errors
int64_t FFMpegDecoder::decode_and_store_next_frame() {

    // does video stream have more video frames it can decode from the last
    // packet that it received?
    if (primary_video_stream_ && primary_video_stream_->receive_frame() == 0) {
        pull_video_buffer_from_stream(primary_video_stream_);
        return 0;
    }

    // likewise does audeo stream have more video frames it can decode from the last
    // packet that it received?
    if (primary_audio_stream_ && primary_audio_stream_->receive_frame() == 0) {
        pull_audio_buffer_from_stream(primary_audio_stream_);
        return 0;
    }

    // try and read a frame from the stream into a packet
    int read_code = av_read_frame(av_format_ctx_, avc_packet_);

    if (read_code == AVERROR_EOF) {

        // end of file! We need to send a 'flush' packet to the
        // streams and keep decoding frames until exhausted.

        if (primary_video_stream_) {
            primary_video_stream_->send_flush_packet();
            while (primary_video_stream_->receive_frame() == 0) {
                pull_video_buffer_from_stream(primary_video_stream_);
            }
        }

        if (primary_audio_stream_) {
            primary_audio_stream_->send_flush_packet();
            while (primary_audio_stream_->receive_frame() == 0) {
                pull_audio_buffer_from_stream(primary_audio_stream_);
            }
        }
        av_packet_unref(avc_packet_);
        return AVERROR_EOF;

    } else if (!read_code) { // frame read ok!

        if (primary_video_stream_ &&
            avc_packet_->stream_index == primary_video_stream_->stream_index()) {
            int rx = primary_video_stream_->send_packet(avc_packet_);
            if (rx)
                AVC_CHECK_THROW(rx, "avcodec_send_packet");
            if (primary_video_stream_->receive_frame() == 0) {
                pull_video_buffer_from_stream(primary_video_stream_);
            }
        } else if (
            primary_audio_stream_ &&
            avc_packet_->stream_index == primary_audio_stream_->stream_index()) {
            int rx = primary_audio_stream_->send_packet(avc_packet_);
            if (rx)
                AVC_CHECK_THROW(rx, "avcodec_send_packet");
            if (primary_audio_stream_->receive_frame() == 0) {
                pull_audio_buffer_from_stream(primary_audio_stream_);
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
    if (primary_video_stream_ && primary_video_stream_->receive_frame() == 0) {
        return 0;
    }

    // try and read a frame from the stream into a packet
    int read_code = av_read_frame(av_format_ctx_, avc_packet_);

    if (read_code == AVERROR_EOF) {

        // end of file! We need to send a 'flush' packet to the
        // streams and keep decoding frames until exhausted.

        if (primary_video_stream_) {
            primary_video_stream_->send_flush_packet();
            int64_t rt = primary_video_stream_->receive_frame();
            while (rt == AVERROR(EAGAIN)) {
                rt = primary_video_stream_->receive_frame();
            }
            if (!rt)
                return 0;
        }

        return AVERROR_EOF;

    } else if (!read_code) { // frame read ok!

        if (primary_video_stream_ &&
            avc_packet_->stream_index == primary_video_stream_->stream_index()) {
            int rx = primary_video_stream_->send_packet(avc_packet_);
            av_packet_unref(avc_packet_);
            if (rx)
                AVC_CHECK_THROW(rx, "avcodec_send_packet");
            return primary_video_stream_->receive_frame();
        }

    } else {

        av_packet_unref(avc_packet_);
        AVC_CHECK_THROW(read_code, "av_read_frame");
    }
    return 0;
}

xstudio::utility::Timecode FFMpegDecoder::first_frame_timecode() {
    if (!timecode_stream_) {
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


void FFMpegDecoder::decode_video_frame(
    const int64_t frame_num,
    ImageBufPtr &image_buffer,
    std::vector<unsigned int> /*stream_indeces*/
) {

    try {

        requested_video_frame_ = frame_num;

        decoding_backwards_ = (last_requested_frame_ - frame_num) == 1 ||
                              (last_requested_frame_ - frame_num) == 2;

        if (have_video_and_audio(frame_num)) {
            // we've already decoded this video frame but not used it
            image_buffer          = get_frame_and_release_from_mini_cache(frame_num);
            last_requested_frame_ = frame_num;
            return;
        }

        if (primary_video_stream_ && primary_video_stream_->is_single_frame()) {

            // single image source like a JPEG. xstudio might say frame_num = 100 but
            // ffmpeg thinks its frame zero (and video length = 1 frame). This is because
            // for frame based sources that are decoded by FFMPEG, we do not treat the
            // sequence of files as a single stream for ffmpeg to decode but instead
            // each individual file will create an FFMpegDecoder instance referencing
            // that one file.
            requested_video_frame_ = 0;
            while (decode_next_frame() != AVERROR_EOF) {
            }
            image_buffer = primary_video_stream_->get_ffmpeg_frame_as_xstudio_image();
            return;
        }

        if (last_requested_frame_ != (frame_num - 1)) {
            do_seek(frame_num);
        }

        if (!primary_video_stream_) {
            // if we don't have a video stream, we need to make an empty video
            // frame for us to attach our audio to using our 'virtual' video
            // frame rate.
            pull_video_buffer_from_stream(primary_video_stream_);
        }

        // this is the decode loop, we keep going until we have decoded the frame
        // that we want. Note that audio and video often come in a pretty 'lumpy'
        // way so we have to hang onto audio until video has caught up or vice
        // versa
        while (!have_video_and_audio(frame_num)) {

            if (last_decoded_frame_ > frame_num) {
                // this shouldn't happen thanks to seek, but if the seek
                // didn't work and we're already past 'frame_num' then we
                // exit the loop here
                break;
            }

            // decoding of any and all streams happens in this call, and if
            // we get a complete video or audio frame it is also stored within
            // this function
            if (decode_and_store_next_frame() == AVERROR_EOF) {
                break;
            }
        }

        // re-check if we have the frame we want
        if (have_video_and_audio(frame_num)) {
            image_buffer = get_frame_and_release_from_mini_cache(frame_num);
        } else if (have_video(frame_num)) {
            // we might have more video frames than audio, it seems common that
            // the audio and video streams in movie files don't match in duration.
            // This picks the video frame from the stored frames that have no audio
            image_buffer = get_frame_and_release_from_mini_cache(frame_num, true);
        } else {
            throw media_corrupt_error(
                std::string("Failed to decode frame ") + std::to_string(frame_num) +
                " with AVERROR_EOF");
        }

        empty_mini_caches(frame_num);

        last_requested_frame_ = frame_num;

    } catch (std::exception &e) {

        // some error has occurred ... force a fresh seek on next try
        last_requested_frame_ = -100;
        fflush(stderr);
        throw;
    }
}

xstudio::utility::FrameRate FFMpegDecoder::frame_rate(unsigned int /*stream_idx*/) const {

    // Currently we are not supporting per-stream frame rates. This is because
    // the 'meaning' of frame rate for audio vs video streams is quite different
    // and will over-complicate the xstudio playback engine.
    return video_frame_rate_;

    /*auto format_name = std::string(av_format_ctx_->iformat->name);
    if (format_name == "image2" || format_name == "png_pipe") {
        return xstudio::utility::FrameRate();
    }

    if (stream_idx == UINT_MAX && primary_video_stream_) {
        return primary_video_stream_->frame_rate();
    } else if (!primary_video_stream_) {// && primary_audio_stream_) {
        // we have no video stream, presume only audio. Provide a fallback
        // video frame rate
        return xstudio::utility::FrameRate(timebase::k_flicks_24fps);
    }

    auto p = streams_.find(stream_idx);
    if (p == streams_.end()) {
        throw std::runtime_error("FFMpegDecoder::frame_rate, bad stream index");
    }

    return p->second->frame_rate();*/
}

std::shared_ptr<thumbnail::ThumbnailBuffer>
FFMpegDecoder::decode_thumbnail_frame(const int64_t frame_num, const size_t size_hint) {

    std::shared_ptr<thumbnail::ThumbnailBuffer> rt;
    if (!primary_video_stream_)
        return rt;
    try {

        if (primary_video_stream_->is_single_frame()) {

            while (decode_next_frame() != AVERROR_EOF) {
            }
            rt = primary_video_stream_->convert_av_frame_to_thumbnail(size_hint);

        } else {

            do_seek(frame_num, true);

            while (primary_video_stream_->current_frame() < frame_num &&
                   decode_next_frame() != AVERROR_EOF) {
            }
            rt = primary_video_stream_->convert_av_frame_to_thumbnail(size_hint);
        }

    } catch (std::exception &e) {

        // some error has occurred ... force a fresh seek on next try
        last_requested_frame_ = -100;
        fflush(stderr);
        throw;
    }
    return rt;
}

bool FFMpegDecoder::have_video_and_audio(const int frame_num) {
    auto p = video_frame_mini_cache_.find(frame_num);
    if (p != video_frame_mini_cache_.end()) {
        return true;
    }
    return false;
}

bool FFMpegDecoder::have_video(const int frame_num) {
    auto p = video_frame_mini_cache_.find(frame_num);
    if (p != video_frame_mini_cache_.end()) {
        return true;
    }
    auto pp = video_frames_with_incomplete_audio_.begin();
    while (pp != video_frames_with_incomplete_audio_.end()) {
        if (pp->second->decoder_frame_number() == frame_num)
            return true;
        pp++;
    }
    return false;
}

media_reader::ImageBufPtr
FFMpegDecoder::get_frame_and_release_from_mini_cache(const int frame_num, const bool force) {
    ImageBufPtr rt;
    auto p = video_frame_mini_cache_.find(frame_num);
    if (p != video_frame_mini_cache_.end()) {
        rt = p->second;
        video_frame_mini_cache_.erase(p);
    }
    if (force) {
        auto pp = video_frames_with_incomplete_audio_.begin();
        while (pp != video_frames_with_incomplete_audio_.end()) {
            if (pp->second->decoder_frame_number() == frame_num) {
                rt = pp->second;
                video_frames_with_incomplete_audio_.erase(pp);
                break;
            }
            pp++;
        }
    }
    return rt;
}

void FFMpegDecoder::pull_audio_buffer_from_stream(StreamPtr &audio_stream) {

    AudioBufPtr buf = audio_stream->get_ffmpeg_frame_as_xstudio_audio(soundcard_sample_rate_);

    if (!buf)
        return;

    last_audio_frame_pts_ = buf->display_timestamp_seconds();
    audio_frames_unnattached_to_video_[last_audio_frame_pts_] = buf;
    attach_audio_to_video();
}

void FFMpegDecoder::pull_video_buffer_from_stream(StreamPtr &video_stream) {

    // Do we want the frame that was just decoded from video_stream ?
    // are we decoding forwards (after a seek) and haven't got to the frame we need?
    if (video_stream && video_stream->current_frame() < requested_video_frame_ &&
        !decoding_backwards_)
        return;

    ImageBufPtr buf;
    if (video_stream) {

        buf = video_stream->get_ffmpeg_frame_as_xstudio_image();
        if (!buf)
            return;

    } else {

        // If video_stream is null, we want to make a phoney video buffer with
        // an appropriate timestamp ... we can then attach audio frames to this
        // video buffer to send back to playback engine.
        buf.reset(new ImageBuffer("No Video"));
        buf->set_duration_seconds(video_frame_rate_.to_seconds());
        buf->set_display_timestamp_seconds(
            requested_video_frame_ * video_frame_rate_.to_seconds());
        buf->set_decoder_frame_number(requested_video_frame_);
    }

    if (primary_audio_stream_) {
        const double dts                         = buf->display_timestamp_seconds();
        video_frames_with_incomplete_audio_[dts] = buf;
        attach_audio_to_video();
    } else {
        video_frame_mini_cache_[buf->decoder_frame_number()] = buf;
        last_decoded_frame_                                  = buf->decoder_frame_number();
    }
}

void FFMpegDecoder::attach_audio_to_video() {

    auto pp = video_frames_with_incomplete_audio_.begin();
    while (pp != video_frames_with_incomplete_audio_.end()) {

        ImageBufPtr &video_buf = pp->second;

        // time point when this image goes OFF screen (for the next frame)
        const double edts =
            video_buf->display_timestamp_seconds() + video_buf->duration_seconds();

        // if the last decoded audio frame is ahead of the end of this video frame,
        // we must be able to attach audio to the video frame and 'deliver' it
        if (edts < last_audio_frame_pts_) {
            attach_audio_to_video_and_deliver(video_buf);
            pp = video_frames_with_incomplete_audio_.erase(pp);
        } else {
            break;
        }
    }
}

void FFMpegDecoder::attach_audio_to_video_and_deliver(ImageBufPtr buf) {

    const double dts  = buf->display_timestamp_seconds();
    const double edts = buf->display_timestamp_seconds() + buf->duration_seconds();
    // do we have audio whose timestamp overlaps with the duration of this video
    // frame
    auto p = audio_frames_unnattached_to_video_.lower_bound(dts);
    while (p != audio_frames_unnattached_to_video_.end()) {
        if (p->first >= dts && p->first < edts) {
            if (buf->audio_) {
                buf->audio_->extend(*(p->second.get()));
            } else {
                buf->audio_ = p->second;
            }
            audio_frames_unnattached_to_video_.erase(p);
            p = audio_frames_unnattached_to_video_.lower_bound(dts);
        } else {
            break;
        }
    }

    video_frame_mini_cache_[buf->decoder_frame_number()] = buf;
    last_decoded_frame_                                  = buf->decoder_frame_number();
}

void FFMpegDecoder::do_seek(const int seek_frame, bool force) {

    // tring a couple of tricks here ... the assumption is any codec that can't
    // seek to an exact frame can only decode forwards efficiently. If we are
    // decoding backwards, it's best to go a good few frames back and then
    // decode forwards until we get the frame we need. All the intermediate
    // frames will be put in our mini cache, so next time we need a frame
    // that is before the one we were just asked for it's already decoded.

    if (primary_video_stream_)
        primary_video_stream_->set_current_frame_unknown();

    if (!primary_video_stream_ && primary_audio_stream_) {

        // use our 'virtual' video frame rate to get to seconds
        double seek_secs  = seek_frame * video_frame_rate_.to_seconds();
        int64_t timestamp = primary_audio_stream_->seconds_to_pts(seek_secs);

        primary_audio_stream_->flush_buffers();

        std::stringstream msg;
        msg << "av_seek_frame to frame " << seek_frame << ", timestamp " << timestamp;
        AVC_CHECK_THROW(
            av_seek_frame(
                av_format_ctx_,
                primary_audio_stream_->stream_index(),
                timestamp,
                AVSEEK_FLAG_BACKWARD),
            msg.str().c_str());

        last_audio_frame_pts_ = -1.0;
        last_decoded_frame_   = -100;

        // clear our caches
        audio_frames_unnattached_to_video_.clear();
        video_frames_with_incomplete_audio_.clear();
        video_frame_mini_cache_.clear();

    } else if (
        primary_video_stream_ &&
        (force || seek_frame <= last_requested_frame_ ||
         seek_frame > (last_requested_frame_ + MIN_SEEK_FORWARD_FRAMES))) {

        // here, if we are going backwards frame by frames, we are going
        // to jump back by 16 frames
        int64_t timestamp = primary_video_stream_->frame_to_pts(
            decoding_backwards_ ? std::max(seek_frame - 16, 0) : seek_frame);

        primary_video_stream_->flush_buffers();
        if (primary_audio_stream_)
            primary_audio_stream_->flush_buffers();

        std::stringstream msg;
        msg << "av_seek_frame to frame " << seek_frame << ", timestamp " << timestamp;
        AVC_CHECK_THROW(
            av_seek_frame(
                av_format_ctx_,
                primary_video_stream_->stream_index(),
                timestamp,
                AVSEEK_FLAG_BACKWARD),
            msg.str().c_str());

        last_audio_frame_pts_ = -1.0;
        last_decoded_frame_   = -100;

        // clear our caches
        audio_frames_unnattached_to_video_.clear();
        video_frames_with_incomplete_audio_.clear();
        video_frame_mini_cache_.clear();
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

    // remove video frames that came before the frame that was requested
    auto pp = video_frames_with_incomplete_audio_.begin();
    while (pp != video_frames_with_incomplete_audio_.end()) {
        if (pp->second->decoder_frame_number() < decoded_frame) {
            pp = video_frames_with_incomplete_audio_.erase(pp);
        } else {
            pp++;
        }
    }

    // could use lower_bound here
    auto p = video_frame_mini_cache_.begin();
    while (p != video_frame_mini_cache_.end()) {
        if (p->first <= decoded_frame) {
            p = video_frame_mini_cache_.erase(p);
        } else {
            p++;
        }
    }
}
