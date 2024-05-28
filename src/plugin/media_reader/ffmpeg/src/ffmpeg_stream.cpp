// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "ffmpeg_stream.hpp"
#include "xstudio/media/media_error.hpp"

#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace xstudio::media_reader::ffmpeg;
using namespace xstudio::media_reader;
using namespace xstudio;
// check for error code on an avcodec function and throw an exception
void xstudio::media_reader::ffmpeg::AVC_CHECK_THROW(int errorNum, const char *avc_command) {
    if (!errorNum)
        return;
    std::array<char, 4096> buf;

    if (!av_strerror(errorNum, buf.data(), buf.size())) {
        throw media_corrupt_error(
            std::string() + avc_command + " " + buf.data() + " (ffmpeg reader).");
    } else {
        throw media_corrupt_error(
            std::string("FFMPEG reader: ") + avc_command + " unknown error num " +
            std::to_string(errorNum) + ".");
    }
}

namespace {

static const std::set<int> shader_supported_pix_formats = {
    AV_PIX_FMT_RGB24,        AV_PIX_FMT_BGR24,       AV_PIX_FMT_RGB48LE,
    AV_PIX_FMT_RGBA64LE,     AV_PIX_FMT_ARGB,        AV_PIX_FMT_RGBA,
    AV_PIX_FMT_BGRA,         AV_PIX_FMT_YUV422P,     AV_PIX_FMT_YUVJ422P,
    AV_PIX_FMT_YUV420P,      AV_PIX_FMT_YUVJ420P,    AV_PIX_FMT_YUV422P10LE,
    AV_PIX_FMT_YUV420P10LE,  AV_PIX_FMT_YUV444P10LE, AV_PIX_FMT_YUVA444P10LE,
    AV_PIX_FMT_YUV422P12LE,  AV_PIX_FMT_YUV444P12LE, AV_PIX_FMT_YUVA422P12LE,
    AV_PIX_FMT_YUVA444P12LE, AV_PIX_FMT_GBRP10LE,    AV_PIX_FMT_GBRP12LE,
    AV_PIX_FMT_GBRAP10LE,    AV_PIX_FMT_GBRAP12LE,   AV_PIX_FMT_GBRAP16LE,
    AV_PIX_FMT_GBRAP16LE};

// Matrix generated with colour science for Python 0.3.15

// BT.601 (also see Poynton Digital Video and HD 2012 Eq 29.4)
static const Imath::M33f
    YCbCr_to_RGB_601(1., 0., 1.402, 1., -0.34413629, -0.71413629, 1., 1.772, -0.);
// BT.2020
static const Imath::M33f
    YCbCr_to_RGB_2020(1., -0., 1.4746, 1., -0.16455313, -0.57135313, 1., 1.8814, 0.);
// BT.709 (also see Poynton Digital Video and HD 2012 Eq 30.4)
static const Imath::M33f
    YCbCr_to_RGB_709(1., 0., 1.5748, 1., -0.18732427, -0.46812427, 1., 1.8556, 0.);

void set_shader_pix_format_info(
    xstudio::utility::JsonStore &jsn,
    AVCodecID codec_id,
    AVPixelFormat pix_fmt,
    AVColorRange color_range,
    AVColorSpace colorspace) {
    const AVPixFmtDescriptor *pixel_desc = av_pix_fmt_desc_get(pix_fmt);

    // Pixel format
    switch (pix_fmt) {
    case AV_PIX_FMT_RGB24:
        jsn["rgb"] = 1;
        break;
    case AV_PIX_FMT_BGR24:
        jsn["rgb"] = 2;
        break;
    case AV_PIX_FMT_ARGB:
        jsn["rgb"] = 3;
        break;
    case AV_PIX_FMT_RGBA:
        jsn["rgb"] = 4;
        break;
    case AV_PIX_FMT_ABGR:
        jsn["rgb"] = 5;
        break;
    case AV_PIX_FMT_BGRA:
        jsn["rgb"] = 6;
        break;
    case AV_PIX_FMT_GBRP10LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_GBRP12LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_GBRAP10LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_GBRAP12LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_GBRP16LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_GBRAP16LE:
        jsn["rgb"] = 7;
        break;
    case AV_PIX_FMT_RGB48LE:
        jsn["rgb"] = 8;
        break;
    case AV_PIX_FMT_RGBA64LE:
        jsn["rgb"] = 9;
        break;
    default:
        jsn["rgb"] = 0;
    };

    // Chroma subsampling
    jsn["half_scale_uvx"] = pixel_desc->log2_chroma_w;
    jsn["half_scale_uvy"] = pixel_desc->log2_chroma_h;

    // Bit depth
    const int bitdepth      = pixel_desc->comp[0].depth;
    const int max_cv        = std::floor(std::pow(2, bitdepth) - 1);
    jsn["bits_per_channel"] = bitdepth;
    jsn["norm_coeff"]       = 1.0f / max_cv;

    // YCbCr to RGB matrix and offset
    Imath::M33f yuv_to_rgb;

    switch (colorspace) {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        yuv_to_rgb = YCbCr_to_RGB_601;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        // TODO: ColSci
        // Handle BT2020 CL
        yuv_to_rgb = YCbCr_to_RGB_2020;
        break;
    case AVCOL_SPC_BT709:
    default:
        yuv_to_rgb = YCbCr_to_RGB_709;
    }

    // TODO: ColSci
    // Remove DNxHD Video range override when moved in MediaHook
    if (codec_id == AV_CODEC_ID_DNXHD)
        color_range = AVCOL_RANGE_MPEG;

    switch (color_range) {
    case AVCOL_RANGE_JPEG: {
        Imath::V3f offset(1, 128, 128);
        offset *= std::powf(2, bitdepth - 8);
        jsn["yuv_offsets"] = {"ivec3", 1, offset[0], offset[1], offset[2]};
    } break;
    case AVCOL_RANGE_MPEG:
    default: {
        Imath::V4f range(16, 235, 16, 240);
        range *= std::powf(2, bitdepth - 8);

        Imath::M33f scale;
        scale[0][0] = 1.f * max_cv / (range[1] - range[0]);
        scale[1][1] = 1.f * max_cv / (range[3] - range[2]);
        scale[2][2] = 1.f * max_cv / (range[3] - range[2]);
        yuv_to_rgb *= scale;

        Imath::V3f offset(16, 128, 128);
        offset *= std::powf(2, bitdepth - 8);
        jsn["yuv_offsets"] = {"ivec3", 1, offset[0], offset[1], offset[2]};
    }
    }

    jsn["yuv_conv"] = {
        "mat3",
        1,
        yuv_to_rgb[0][0],
        yuv_to_rgb[0][1],
        yuv_to_rgb[0][2],
        yuv_to_rgb[1][0],
        yuv_to_rgb[1][1],
        yuv_to_rgb[1][2],
        yuv_to_rgb[2][0],
        yuv_to_rgb[2][1],
        yuv_to_rgb[2][2]};
}


#define STRIDE_ALIGN 64
/*
 * The following function replaces avcodec_default_get_buffer2 and update_frame_pool
 * from libavcodec/decode.c in ffmpeg libs. This means we allocate buffers for
 * ffmpeg to decode into. The trick is we 'hide' an xstudio ImageBuffer object
 * on the AVFrame ... this ImageBuffer actually owns the allocated memory that
 * is given to ffmpeg for decoding. When we want to 'copy' the decoded image
 * from the AVFrame to an xstudio ImageBuffer, we just steal that hidden
 * ImageBuffer (via shared_ptr).
 *
 * FFMpeg does some sneaky stuff with AVFrames, creating them when it needs to
 * but not necessarily giving them to us, or giving them to us at some point
 * in the future. We therefore also need our own 'deleter' function that will
 * destroy our hidden ImageBuffer if it was never stolen by xstudio - i.e. in
 * the case when ffmpeg needed an AVFrame for it's own use but not as something
 * it would return to the client for image display.
 */

void image_buf_ptr_deleter(void *, uint8_t *data) {
    auto *buf_object_ptr = (ImageBufPtr *)data;
    if (buf_object_ptr) {
        delete buf_object_ptr;
    }
}

static int setup_video_buffer(AVCodecContext *ctx, AVFrame *frame, int /*flags*/) {
    // Code adapted from tools/target_dec_fuzzer.c

    std::array<ptrdiff_t, 4> linesizes;
    std::array<size_t, 4> planesizes;
    std::array<int, AV_NUM_DATA_POINTERS> linesizes_align;
    size_t total_size = 0;
    int i             = 0;
    int ret           = 0;
    int w             = frame->width;
    int h             = frame->height;

    avcodec_align_dimensions2(ctx, &w, &h, linesizes_align.data());
    ret = av_image_fill_linesizes(frame->linesize, ctx->pix_fmt, w);
    if (ret < 0)
        return ret;

    for (i = 0; i < 4 && frame->linesize[i]; i++)
        linesizes[i] = frame->linesize[i] = FFALIGN(frame->linesize[i], linesizes_align[i]);
    for (; i < 4; i++)
        linesizes[i] = 0;

    ret = av_image_fill_plane_sizes(planesizes.data(), ctx->pix_fmt, h, linesizes.data());
    if (ret < 0)
        return ret;

    for (i = 0; i < 4; i++)
        total_size += planesizes[i];

    // Data is backed by a single continuous buffer.
    ImageBufPtr buf_ptr(new ImageBuffer());
    auto *buffer = (uint8_t *)buf_ptr->allocate(total_size + 4096);

    frame->extended_data = frame->data;
    for (i = 0; i < 4 && planesizes[i]; i++) {
        // Create an AVBuffer from our existing data
        auto *buf_object_ptr = new ImageBufPtr(buf_ptr);
        frame->buf[i]        = av_buffer_create(
            (uint8_t *)buf_object_ptr, planesizes[i], image_buf_ptr_deleter, nullptr, 0);

        frame->data[i] = buffer;
        buffer += planesizes[i];
    }
    for (; i < AV_NUM_DATA_POINTERS; i++) {
        frame->data[i]     = nullptr;
        frame->linesize[i] = 0;
    }

    return 0;
}

} // namespace

ImageBufPtr FFMpegStream::get_ffmpeg_frame_as_xstudio_image() {

    ImageBufPtr image_buffer;
    // DebugTimer d("copy_avframe_to_xstudio_buffer", 1);

    int ffmpeg_pixel_format = codec_context_->pix_fmt;

    xstudio::utility::JsonStore jsn;

    if (shader_supported_pix_formats.find(ffmpeg_pixel_format) ==
        shader_supported_pix_formats.end()) {

        if (!format_conversion_warning_issued) {
            format_conversion_warning_issued = true;
            spdlog::warn(
                "Pixel format for {} of {} not supported in GPU shader, using CPU to convert. "
                "Playback performance may be affected.",
                source_path,
                ffmpeg_pixel_format);
        }

        image_buffer.reset(new ImageBuffer());
        auto buffer = (uint8_t *)image_buffer->allocate(4 * frame->width * frame->height);
        // not one of the ffmpeg pixel formats that our shader can deal with, so convert to
        // something we can
        sws_context_ = sws_getCachedContext(
            sws_context_,
            frame->width,
            frame->height,
            (AVPixelFormat)ffmpeg_pixel_format,
            frame->width,
            frame->height,
            AV_PIX_FMT_RGBA,
            0,
            nullptr,
            nullptr,
            nullptr);

        const std::array<int, 1> out_linesize({4 * frame->width});

        sws_scale(
            sws_context_,
            frame->data,
            frame->linesize,
            0,
            frame->height,
            &buffer,
            out_linesize.data());

        jsn["y_linesize"]           = out_linesize[0];
        jsn["u_linesize"]           = 0;
        jsn["v_linesize"]           = 0;
        jsn["a_linesize"]           = 0;
        jsn["y_plane_bytes_offset"] = 0;
        jsn["u_plane_bytes_offset"] = 0;
        jsn["v_plane_bytes_offset"] = 0;
        jsn["a_plane_bytes_offset"] = 0;

        ffmpeg_pixel_format = AV_PIX_FMT_RGBA;

    } else {

        std::array<ptrdiff_t, 4> linesizes;
        std::array<size_t, 4> planesizes;
        std::array<size_t, 4> offsets;
        size_t total_size = 0;

        // av_image_fill_plane_sizes wants pdtrdiff_t array, AVFrame gives int array.
        for (int i = 0; i < 4; i++)
            linesizes[i] = frame->linesize[i];

        int ret = av_image_fill_plane_sizes(
            planesizes.data(), (AVPixelFormat)frame->format, frame->height, linesizes.data());
        if (ret < 0) {
            spdlog::error("Error detecting decoded frame plane sizes");
        }

        // Decoder supports custom allocators (AV_CODEC_CAP_DR1)
        if (using_own_frame_allocation && frame->buf[0] && frame->buf[0]->data) {

            // Copy the shared ptr to our image buffer (see setup_video_buffer)
            image_buffer = *((ImageBufPtr *)frame->buf[0]->data);

            // Compute the offsets determined in setup_video_buffer using
            // avcodec_align_dimensions2().
            for (int i = 0; i < 4; ++i) {
                if (frame->data[i]) {
                    offsets[i] = (size_t)frame->data[i] - (size_t)frame->data[0];
                }
            }

            // Decoder manages video memory, so we don't have guaranteed ownership
            // of it and we need to copy it to xstudio image buffer.
        } else {

            offsets[0] = 0;
            for (int i = 1; i < 4; i++)
                offsets[i] = offsets[i - 1] + planesizes[i - 1];
            for (int i = 0; i < 4; i++)
                total_size += planesizes[i];

            image_buffer.reset(new ImageBuffer());
            auto buffer = (uint8_t *)image_buffer->allocate(total_size);

            for (int i = 0; i < 4; ++i) {
                if (frame->data[i]) {
                    std::memcpy(buffer + offsets[i], frame->data[i], planesizes[i]);
                }
            }
        }

        jsn["y_linesize"]           = frame->linesize[0];
        jsn["u_linesize"]           = frame->linesize[1];
        jsn["v_linesize"]           = frame->linesize[2];
        jsn["a_linesize"]           = frame->linesize[3];
        jsn["y_plane_bytes_offset"] = offsets[0];
        jsn["u_plane_bytes_offset"] = offsets[1];
        jsn["v_plane_bytes_offset"] = offsets[2];
        jsn["a_plane_bytes_offset"] = offsets[3];
    }

    image_buffer->set_image_dimensions(Imath::V2i(frame->width, frame->height));

    set_shader_pix_format_info(
        jsn,
        codec_->id,
        (AVPixelFormat)ffmpeg_pixel_format,
        frame->color_range,
        frame->colorspace);

    image_buffer->set_shader_params(jsn);

    image_buffer->set_display_timestamp_seconds(
        double(frame->pts) * double(avc_stream_->time_base.num) /
        double(avc_stream_->time_base.den));

    AVRational aspect = av_guess_sample_aspect_ratio(format_context_, avc_stream_, frame);

    if (aspect.den && aspect.num) {
        image_buffer->set_pixel_aspect(float(aspect.num) / float(aspect.den));
    } else {
        image_buffer->set_pixel_aspect(1.0f);
    }

    if (fpsNum_) {
        if (fpsDen_)
            image_buffer->set_duration_seconds(double(fpsDen_) / double(fpsNum_));
        else
            image_buffer->set_duration_seconds(1.0 / double(fpsNum_));
    }

    // determine if image has alpha - if planar, look for 'a_linesize' != 0.
    // Otherwise check for interleaved RGB pix formats that have an alpha
    int rgb_format_code = jsn.value("rgb", 0);
    int alpha_line_size = jsn.value("a_linesize", 0);

    if (rgb_format_code == 8 || rgb_format_code == 1 || rgb_format_code == 2) {
        image_buffer->set_has_alpha(false);
    } else if (rgb_format_code == 9 || (rgb_format_code > 2 && rgb_format_code < 7)) {
        image_buffer->set_has_alpha(true);
    } else {
        // rgb == 7 (RGB(A) planar) or rgb == 0 (i.e. YUV(A))
        image_buffer->set_has_alpha(alpha_line_size != 0);
    }

    image_buffer->set_decoder_frame_number(current_frame());

    return image_buffer;
}

std::shared_ptr<thumbnail::ThumbnailBuffer>
FFMpegStream::convert_av_frame_to_thumbnail(const size_t size_hint) {

    // to make the thumbnail image we use sws api to rescale the image into an unsigned 16 bit
    // int RGB buffer, which we convert to floating point. xstudio then uses the colour pipeline
    // to process the image into a display space as appropriate

    if (!frame->height || !frame->width)
        return std::shared_ptr<thumbnail::ThumbnailBuffer>();

    int ffmpeg_pixel_format = codec_context_->pix_fmt;

    // this hack stops sws emitting errors about deprecated pixel format
    if (ffmpeg_pixel_format == AV_PIX_FMT_YUVJ420P) {
        ffmpeg_pixel_format = AV_PIX_FMT_YUV420P;
    } else if (ffmpeg_pixel_format == AV_PIX_FMT_YUVJ422P) {
        ffmpeg_pixel_format = AV_PIX_FMT_YUV422P;
    } else if (ffmpeg_pixel_format == AV_PIX_FMT_YUVJ444P) {
        ffmpeg_pixel_format = AV_PIX_FMT_YUV444P;
    }

    int thumb_width =
        frame->width > frame->height ? size_hint : (size_hint * frame->width) / frame->height;
    int thumb_height =
        frame->height > frame->width ? size_hint : (size_hint * frame->height) / frame->width;

    auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
        thumb_width, thumb_height, thumbnail::TF_RGBF96);

    std::vector<uint8_t> t_data(thumb_width * thumb_height * 3 * 2);

    auto d      = reinterpret_cast<uint8_t *>(t_data.data());
    auto buffer = static_cast<uint8_t *const *>(&d);

    sws_context_ = sws_getCachedContext(
        sws_context_,
        frame->width,
        frame->height,
        (AVPixelFormat)ffmpeg_pixel_format,
        thumb_width,
        thumb_height,
        AV_PIX_FMT_RGB48, // 16 bits per channel
        0,
        nullptr,
        nullptr,
        nullptr);

    const std::array<int, 1> out_linesize({3 * 2 * thumb_width});

    sws_scale(
        sws_context_,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        buffer,
        out_linesize.data());

    // now convert from 16 bit to float:
    auto s = reinterpret_cast<uint16_t *>(t_data.data());
    auto f = reinterpret_cast<float *>(thumb->data().data());

    const size_t nv = thumb_width * thumb_height * 3;
    for (size_t i = 0; i < nv; i++) {
        *(f++) = float(*(s++)) / 65535.0f;
    }

    return thumb;
}

AudioBufPtr FFMpegStream::get_ffmpeg_frame_as_xstudio_audio(const int soundcard_sample_rate) {

    AudioBufPtr audio_buffer(new AudioBuffer());
    audio_buffer->allocate(
        soundcard_sample_rate,     // sample rate
        2,                         // num channels
        0,                         // num samples ... don't know yet
        audio::SampleFormat::INT16 // format
    );

    // N.B. We aren't supporting 'planar' audio data in xstudio (yet) - if a source codec_
    // supplies planar audio ffmpeg will convert to interleaved which is what soundcards want
    switch (audio_buffer->sample_format()) {
    case audio::SampleFormat::UINT8:
        target_sample_format_ = AV_SAMPLE_FMT_U8;
        break;
    case audio::SampleFormat::INT16:
        target_sample_format_ = AV_SAMPLE_FMT_S16;
        break;
    case audio::SampleFormat::INT32:
        target_sample_format_ = AV_SAMPLE_FMT_S32;
        break;
    case audio::SampleFormat::FLOAT32:
        target_sample_format_ = AV_SAMPLE_FMT_FLT;
        break;
    case audio::SampleFormat::DOUBLE64:
        target_sample_format_ = AV_SAMPLE_FMT_DBL;
        break;
    case audio::SampleFormat::INT64:
        target_sample_format_ = AV_SAMPLE_FMT_S64;
        break;
    default:
        throw media_corrupt_error("Audio buffer format is not set.");
    }

    target_sample_rate_    = audio_buffer->sample_rate();
    target_audio_channels_ = audio_buffer->num_channels();

    audio_buffer->set_display_timestamp_seconds(
        double(frame->pts) * double(avc_stream_->time_base.num) /
        double(avc_stream_->time_base.den));

    //spdlog::info(
    //    "Calculated display timestamp: {} seconds.",
    //    double(frame->pts) * double(avc_stream_->time_base.num) /
    //        double(avc_stream_->time_base.den));

    resample_audio(frame, audio_buffer, -1);

    return audio_buffer;
}

static int asd = 0;

FFMpegStream::FFMpegStream(
    AVFormatContext *fmt_ctx, AVStream *stream, int index, int thread_count, std::string path)
    : stream_index_(index),
      codec_context_(nullptr),
      format_context_(fmt_ctx),
      avc_stream_(stream),
      codec_(nullptr),
      frame(nullptr),
      source_path(std::move(path)) {

    codec_type_ = avc_stream_->codecpar->codec_type;
    frame       = av_frame_alloc();
    codec_      = avcodec_find_decoder(avc_stream_->codecpar->codec_id);

    codec_context_ = avcodec_alloc_context3(codec_);

    if (codec_type_ == AVMEDIA_TYPE_DATA) {

        AVC_CHECK_THROW(
            avcodec_parameters_to_context(codec_context_, avc_stream_->codecpar),
            "avcodec_parameters_to_context");
        if (avc_stream_->codecpar->codec_tag == MKTAG('t', 'm', 'c', 'd')) {
            stream_type_ = TIMECODE_STREAM;
            if (codec_context_->flags2 & AV_CODEC_FLAG2_DROP_FRAME_TIMECODE) {
                is_drop_frame_timecode_ = true;
            } else {
                is_drop_frame_timecode_ = false;
            }
        }

    } else if (codec_type_ == AVMEDIA_TYPE_VIDEO && codec_) {

        stream_type_ = VIDEO_STREAM;

        if (codec_->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            codec_context_->thread_count = thread_count;
            codec_context_->thread_type  = FF_THREAD_SLICE;
        }
        if (codec_->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
            codec_context_->thread_count = thread_count;
            codec_context_->thread_type |= FF_THREAD_FRAME;
        }

        /** initialize the stream parameters with demuxer information */
        AVC_CHECK_THROW(
            avcodec_parameters_to_context(codec_context_, avc_stream_->codecpar),
            "avcodec_parameters_to_context");
        AVC_CHECK_THROW(avcodec_open2(codec_context_, codec_, nullptr), "avcodec_open2");

        frame->width  = avc_stream_->codecpar->width;
        frame->height = avc_stream_->codecpar->height;
        frame->format = codec_context_->pix_fmt;

        // store resolution and pixel aspect
        resolution_ = Imath::V2f(avc_stream_->codecpar->width, avc_stream_->codecpar->height);
        auto sar    = av_guess_sample_aspect_ratio(format_context_, avc_stream_, nullptr);
        if (sar.num && sar.den) {
            pixel_aspect_ = float(sar.num) / float(sar.den);
        }

        if (codec_->capabilities & AV_CODEC_CAP_DR1) {

            // See Note 1 below
            // codec_ allows us to allocated AVFrame buffers
            codec_context_->get_buffer2 = setup_video_buffer;
            using_own_frame_allocation  = true;
        }
        codec_context_->opaque = this;


    } else if (codec_type_ == AVMEDIA_TYPE_AUDIO && codec_) {
        stream_type_ = AUDIO_STREAM;

        /** initialize the stream parameters with demuxer information */
        AVC_CHECK_THROW(
            avcodec_parameters_to_context(codec_context_, avc_stream_->codecpar),
            "avcodec_parameters_to_context");
        AVC_CHECK_THROW(avcodec_open2(codec_context_, codec_, nullptr), "avcodec_open2");
    } else {
        throw std::runtime_error("No decoder found.");
    }

    // Set the fps if it has been set correctly in the stream
    if (avc_stream_->avg_frame_rate.num != 0 && avc_stream_->avg_frame_rate.den != 0) {
        fpsNum_     = avc_stream_->avg_frame_rate.num;
        fpsDen_     = avc_stream_->avg_frame_rate.den;
        frame_rate_ = xstudio::utility::FrameRate(
            static_cast<double>(fpsDen_) / static_cast<double>(fpsNum_));
    } else if (avc_stream_->r_frame_rate.num != 0 && avc_stream_->r_frame_rate.den != 0) {
        fpsNum_     = avc_stream_->r_frame_rate.num;
        fpsDen_     = avc_stream_->r_frame_rate.den;
        frame_rate_ = xstudio::utility::FrameRate(
            static_cast<double>(fpsDen_) / static_cast<double>(fpsNum_));
    } else {
        fpsNum_     = 0;
        fpsDen_     = 0;
        frame_rate_ = xstudio::utility::FrameRate(timebase::k_flicks_24fps);
    }
}

FFMpegStream::~FFMpegStream() {

    if (audio_resampler_ctx_)
        swr_free(&audio_resampler_ctx_);
    if (frame) {
        av_frame_unref(frame);
        av_frame_free(&frame);
    }
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
    }
    if (sws_context_)
        sws_freeContext(sws_context_);
}

void FFMpegStream::set_virtual_frame_rate(const utility::FrameRate &vfr) { frame_rate_ = vfr; }

int64_t FFMpegStream::current_frame() {

    // no frame!
    if (nothing_decoded_yet_)
        return -1;

    if (current_frame_ != CURRENT_FRAME_UNKNOWN)
        return current_frame_;

    current_frame_ = 0;
    if (fpsNum_) {
        current_frame_ = int(floor(
            double((frame->best_effort_timestamp) * avc_stream_->time_base.num * fpsNum_) /
            double(avc_stream_->time_base.den * fpsDen_)));
    } else {
        current_frame_ = ((frame->best_effort_timestamp) * avc_stream_->time_base.num) /
                         (avc_stream_->time_base.den);
    }

    return current_frame_;
}

int64_t FFMpegStream::frame_to_pts(int frame) const {

    return seconds_to_pts(double(frame) * frame_rate().to_seconds());
}

size_t FFMpegStream::resample_audio(
    AVFrame *frame, AudioBufPtr &audio_buffer, int offset_into_output_buffer) {

    // N.B. this method is based loosely on the audio resampling in ffplay.c in ffmpeg source
    const int64_t target_channel_layout = av_get_default_channel_layout(2);

    av_samples_get_buffer_size(
        nullptr, frame->channels, frame->nb_samples, (AVSampleFormat)frame->format, 1);

    const int64_t dec_channel_layout =
        (frame->channel_layout &&
         frame->channels == av_get_channel_layout_nb_channels(frame->channel_layout))
            ? frame->channel_layout
            : av_get_default_channel_layout(frame->channels);

    const int wanted_nb_samples = frame->nb_samples * target_sample_rate_ / frame->sample_rate;

    if (!audio_resampler_ctx_ || frame->format != src_audio_fmt_ ||
        frame->sample_rate != src_audio_sample_rate_ ||
        dec_channel_layout != src_audio_channel_layout_) {

        swr_free(&audio_resampler_ctx_);
        audio_resampler_ctx_ = swr_alloc_set_opts(
            nullptr,
            target_channel_layout,
            target_sample_format_,
            target_sample_rate_,
            dec_channel_layout,
            (AVSampleFormat)frame->format,
            frame->sample_rate,
            0,
            nullptr);

        if (!audio_resampler_ctx_ || swr_init(audio_resampler_ctx_) < 0) {
            std::array<char, 4096> errbuf;

            // naughty naughty.. don't use sprintf when you can't guarantee len..
            // this is open to overflow..
            // this is safe..
            snprintf(
                errbuf.data(),
                errbuf.size(),
                "Cannot create sample rate converter for conversion of %d"
                " Hz %s %d channels to %d Hz %s %d channels!\n",
                frame->sample_rate,
                av_get_sample_fmt_name((AVSampleFormat)frame->format),
                frame->channels,
                target_sample_rate_,
                av_get_sample_fmt_name(target_sample_format_),
                target_audio_channels_);
            throw media_corrupt_error(errbuf.data());
        }

        src_audio_fmt_            = (AVSampleFormat)frame->format;
        src_audio_sample_rate_    = frame->sample_rate;
        src_audio_channel_layout_ = dec_channel_layout;
    }

    const auto in       = (const uint8_t **)frame->extended_data;
    int out_count       = wanted_nb_samples + 256;
    int target_out_size = av_samples_get_buffer_size(
        nullptr, target_audio_channels_, out_count, target_sample_format_, 0);

    if (target_out_size < 0) {
        throw media_corrupt_error("av_samples_get_buffer_size() failed.");
    }

    uint8_t *out;
    if (offset_into_output_buffer == -1) {
        // automatically extend the buffer the exact required amount
        // size_t sz = audio_buffer->size();
        // The multiplication by 2 * 2 seems to be an assumption based on specific audio data
        // properties. Here's a possible explanation:
        //
        // 2 Channels: The first 2 likely represents the fact that there are 2 channels. This
        // makes sense given that you've defined the target channel layout to be stereo
        // (av_get_default_channel_layout(2)). So, for each sample, there's data for both the
        // left and right channels.
        //
        // 2 Bytes per Sample (16-bit audio): The second 2 presumably represents 2 bytes per
        // sample, which corresponds to 16-bit audio samples. This is a common format for audio,
        // especially in CD-quality audio.
        //
        // By multiplying the number of samples by 2 * 2, is calculating the
        // offset in bytes to where the new data should be written in the buffer.
        //
        // However, this calculation has a couple of assumptions:
        //
        // - The audio always has 2 channels.
        // - The audio samples are always 16 bits.
        //
        // If either of these assumptions is violated (for example, if the audio is mono or if
        // the bit depth is different), then the calculation would be incorrect.
        //
        // It would be safer and clearer to derive these values from variables or constants that
        // explicitly state their purpose (like NUM_CHANNELS and BYTES_PER_SAMPLE), rather than
        // hardcoding them as 2 and 2. Alternatively, adding a comment to explain this
        // arithmetic can also help future maintainers understand the intent.
        audio_buffer->extend_size(target_out_size);
        out = (uint8_t *)(audio_buffer->buffer() + audio_buffer->num_samples() * 2 * 2);

    } else {

        if (size_t(target_out_size + offset_into_output_buffer) > audio_buffer->size()) {
            throw media_corrupt_error("Buffer too small for audio resampling.");
        }

        out = (uint8_t *)(audio_buffer->buffer() + offset_into_output_buffer);
    }

    int converted_n_samps =
        swr_convert(audio_resampler_ctx_, &out, out_count, in, frame->nb_samples);

    if (offset_into_output_buffer == -1) {
        audio_buffer->set_num_samples(audio_buffer->num_samples() + converted_n_samps);
    }

    if (converted_n_samps < 0) {
        throw media_corrupt_error("swr_convert() failed");
    }

    return converted_n_samps * target_audio_channels_ *
           av_get_bytes_per_sample(target_sample_format_);
}

int64_t FFMpegStream::receive_frame() {

    // for single frame video source, check if the frame has already been
    // decoded
    if (duration_frames() == 1 && !nothing_decoded_yet_) {
        return AVERROR_EOF;
    }

    av_frame_unref(frame);

    int rt = avcodec_receive_frame(codec_context_, frame);

    // we have decoded a frame, increment the frame counter
    // if our frame counter is set.
    if (!rt && current_frame_ != CURRENT_FRAME_UNKNOWN)
        current_frame_++;

    if (rt != AVERROR(EAGAIN) && rt != AVERROR_EOF) {
        AVC_CHECK_THROW(rt, "avcodec_receive_frame");
    }

    if (!rt && nothing_decoded_yet_)
        nothing_decoded_yet_ = false;

    // no error, but might need more packet data to receive aother frame or we've
    // hit the end of the file

    return rt;
}

void FFMpegStream::send_flush_packet() { avcodec_send_packet(codec_context_, nullptr); }

int FFMpegStream::send_packet(AVPacket *avc_packet_) {
    int rt = avcodec_send_packet(codec_context_, avc_packet_);
    av_packet_unref(avc_packet_);
    return rt;
}

void FFMpegStream::flush_buffers() { avcodec_flush_buffers(codec_context_); }

double FFMpegStream::duration_seconds() const {

    if (avc_stream_->time_base.num &&
        avc_stream_->duration != std::numeric_limits<int64_t>::lowest()) {

        return double(avc_stream_->duration * avc_stream_->time_base.num) /
               double(avc_stream_->time_base.den);

    } else {
        // stream duration is not known. Applying some rules based on trial and
        // error investigation.
        if (format_context_->duration > 0 &&
            avc_stream_->duration == std::numeric_limits<int64_t>::lowest()) {
            // I've found that on MKV/VP8 (webm) sources, if duration is set on the
            // AVFormatContext it is stated in microseconds (AV_TIME_BASE), not in the timebase
            // of the AVStream. Undocumented behaviour in ffmpeg, of course.
            return double(format_context_->duration) * 0.000001;
        } else if (avc_stream_->nb_frames > 0 && avc_stream_->avg_frame_rate.num) {
            // maybe we know the number of frames and frame rate
            return double(avc_stream_->nb_frames * avc_stream_->avg_frame_rate.den) /
                   double(avc_stream_->avg_frame_rate.num);
        } else if (avc_stream_->time_base.num) {
            // single frame source
            return double(avc_stream_->time_base.num) / double(avc_stream_->time_base.den);
        }
    }
    // fallback, assume single frame source
    return frame_rate().to_seconds();
}

int FFMpegStream::duration_frames() const {

    if (stream_type_ == VIDEO_STREAM && !fpsDen_) {
        // no average frame rate defined, assume it's a single pic
        return 1;
    }
    return (int)round(duration_seconds() / frame_rate().to_seconds());
}


/* Note 1: this experiment is disabled for now. The idea is that we circumvent
// ffmpeg's buffer allocation and insert our own pixel buffer (owned by video_frame)
// into which ffmpeg decodes. This will avoid the data copy happening in the
// call to FFMPegStream::copy_avframe_to_xstudio_buffer and does indeed speed up
// decoding. 2-3ms for typical HD res frame, more for UHD and so-on. However, the
// approach doesn't work when ffmpeg is doing multithreading on frames as the buffer
// allocation happens out of sync with the decode of a given video frame. Some more
// work could be done to fix that problem and gain a few ms per frame which may
// be needed for high res playback */
