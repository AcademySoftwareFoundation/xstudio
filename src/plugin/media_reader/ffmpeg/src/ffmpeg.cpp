// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sys/time.h>

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"

#include "ffmpeg.hpp"
#include "ffmpeg_decoder.hpp"

namespace fs = std::filesystem;


using namespace xstudio;
using namespace xstudio::global_store;
using namespace xstudio::media_reader::ffmpeg;
using namespace xstudio::media_reader;
using namespace xstudio::utility;

namespace {

static Uuid s_plugin_uuid("87557f93-55f8-4650-8905-4834f1f4b78d");
static Uuid ffmpeg_shader_uuid{"1c9259fc-ffee-11ea-87fe-989096adb429"};
static std::string the_shader = {R"(
#version 430 core
uniform ivec2 image_dims;
uniform ivec2 texture_dims;
uniform int rgb;
uniform int y_linesize;
uniform int u_linesize;
uniform int v_linesize;
uniform int a_linesize;
uniform int y_plane_bytes_offset;
uniform int u_plane_bytes_offset;
uniform int v_plane_bytes_offset;
uniform int a_plane_bytes_offset;
uniform bool half_scale_uvy;
uniform bool half_scale_uvx;
uniform int bits_per_channel;
uniform mat3 yuv_conv;
uniform ivec3 yuv_offsets;
uniform float norm_coeff;

uvec4 get_image_data_4bytes(int byte_address);

int get_image_data_2bytes(int byte_address);

int get_image_data_1byte(int byte_address);

int yuv_tex_lookup_10bit(
	ivec2 image_coord,
	int offset,
	int linestride)
{
    int address = offset + image_coord.x*2 + image_coord.y*linestride;
    return get_image_data_2bytes(address);
}

int yuv_tex_lookup_8bit(
	ivec2 image_coord,
	int offset,
	int linestride)
{
    int address = offset + image_coord.x + image_coord.y*linestride;
    return get_image_data_1byte(address);
}

vec4 fetch_rgba_pixel_from_rgb24(ivec2 image_coord, bool bgr)
{
	int address = image_coord.x*3 + image_coord.y*y_linesize;
	uvec3 rgb = bgr ? get_image_data_4bytes(address).zyx : get_image_data_4bytes(address).xyz;
	return vec4(vec3(rgb) * norm_coeff, 1.0);
}

vec4 fetch_rgba_pixel_from_rgba32(ivec2 image_coord)
{
	int address = image_coord.x*4 + image_coord.y*y_linesize;
	uvec4 rgba = get_image_data_4bytes(address);
	if (rgb == 3) { // AV_PIX_FMT_ARGB
		rgba.xyzw = rgba.wxyz;
	} else if (rgb == 4) { // AV_PIX_FMT_RGBA
        //nope
	} else if (rgb == 5) { // AV_PIX_FMT_ABGR
		rgba.xyzw = rgba.wzyx;
	} else if (rgb == 6) { // AV_PIX_FMT_BGRA
		rgba.xyzw = rgba.zyxw;
	}

	return vec4(rgba) * norm_coeff;
}

vec4 fetch_rgba_pixel_from_yuv(ivec2 image_coord)
{
	ivec3 yuv;
	float a = 1.0;

	ivec2 uv_coord = ivec2(
		half_scale_uvx ? image_coord.x >> 1 : image_coord.x,
		half_scale_uvy ? image_coord.y >> 1 : image_coord.y
		);

	if (bits_per_channel == 10 || bits_per_channel == 12) {

		yuv = ivec3(yuv_tex_lookup_10bit(image_coord, y_plane_bytes_offset, y_linesize),
			yuv_tex_lookup_10bit(uv_coord, u_plane_bytes_offset, u_linesize),
			yuv_tex_lookup_10bit(uv_coord, v_plane_bytes_offset, v_linesize)
			);

		if (half_scale_uvx && ((image_coord.x & 1) == 1) && image_coord.x*2 < image_dims.x) {

			uv_coord.x = uv_coord.x + 1;
			ivec3 yuv2 = ivec3(yuv.x,
				yuv_tex_lookup_10bit(uv_coord, u_plane_bytes_offset, u_linesize),
				yuv_tex_lookup_10bit(uv_coord, v_plane_bytes_offset, v_linesize)
				);

			yuv = (yuv + yuv2) >> 1;

		}

		if (a_linesize != 0) {
			a = float(yuv_tex_lookup_10bit(image_coord, a_plane_bytes_offset, a_linesize))*norm_coeff;
		}

	} else {

		yuv = ivec3( yuv_tex_lookup_8bit(image_coord, y_plane_bytes_offset, y_linesize),
				yuv_tex_lookup_8bit(uv_coord, u_plane_bytes_offset, u_linesize),
				yuv_tex_lookup_8bit(uv_coord, v_plane_bytes_offset, v_linesize));

	}

	yuv -= yuv_offsets;

	return vec4(vec3(yuv) * yuv_conv * norm_coeff, a); //divide by 1023.0

}

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
	if (rgb == 0) {
		return fetch_rgba_pixel_from_yuv(image_coord);
	} else if (rgb > 2) {
		return fetch_rgba_pixel_from_rgba32(image_coord);
	} else {
		return fetch_rgba_pixel_from_rgb24(image_coord, rgb == 2);
	}
}
)"};

static ui::viewport::GPUShaderPtr
    ffmpeg_shader(new ui::viewport::GPUShader(ffmpeg_shader_uuid, the_shader));


} // namespace


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaReaderPlugin<MediaReaderActor<FFMpegMediaReader>>>(
                s_plugin_uuid,
                "FFMPEG",
                "xStudio",
                "FFMPEG Media Reader",
                semver::version("1.0.0"))}));
}
}

FFMpegMediaReader::FFMpegMediaReader(const utility::JsonStore &prefs)
    : MediaReader("FFMPEG", prefs) {
    readers_per_source_ = 1;
    update_preferences(prefs);
}

utility::Uuid FFMpegMediaReader::plugin_uuid() const { return s_plugin_uuid; }

void FFMpegMediaReader::update_preferences(const utility::JsonStore &prefs) {
    try {
        readers_per_source_ =
            preference_value<int>(prefs, "/plugin/media_reader/FFMPEG/readers_per_source");
        soundcard_sample_rate_ =
            preference_value<int>(prefs, "/core/audio/pulse_audio_prefs/sample_rate");
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

ImageBufPtr FFMpegMediaReader::image(const media::AVFrameID &mptr) {
    std::string path = uri_to_posix_path(mptr.uri_);

    if (!decoder || decoder->path() != path) {
        decoder.reset(
            new FFMpegDecoder(path, soundcard_sample_rate_, VIDEO_STREAM, mptr.stream_id_));
    }

    ImageBufPtr rt;
    decoder->decode_video_frame(mptr.frame_, rt);

    if (rt) {
        rt->set_shader(ffmpeg_shader);
    }

    return rt;
}

AudioBufPtr FFMpegMediaReader::audio(const media::AVFrameID &mptr) {

    try {

        std::string path = uri_to_posix_path(mptr.uri_);

        if (!audio_decoder || audio_decoder->path() != path) {
            audio_decoder.reset(new FFMpegDecoder(
                path, soundcard_sample_rate_, AUDIO_STREAM, mptr.stream_id_, mptr.rate_));
        }

        AudioBufPtr rt;

        ImageBufPtr virtual_vid_frame;

        // xstudio stores a frame of audio samples for every video frame for any
        // given source (if the source has no video it is assigned a 'virtual' video
        // frame rate to maintain this approach). However, audio frames generally
        // do not have the same duration as video frames, so there is always some
        // offset between when the video frame is shown and when the audio samples
        // associated with that frame should sound.

        // For example, it's common for ffmpeg to return audio in blocks of
        // 1024 samples and it does so out of sync with delivery of video farmes
        // The FFMpegDecoder will approximately match each block to the video
        // frame using the display timestamp, it also concatenates blocks where
        // necessary as you will often find the blocks coming from ffmpeg at a
        // higher frequency than video frames.

        audio_decoder->decode_video_frame(mptr.frame_, virtual_vid_frame);

        if (virtual_vid_frame->audio_) {
            rt                = virtual_vid_frame->audio_;
            const float delta = rt->display_timestamp_seconds() -
                                virtual_vid_frame->display_timestamp_seconds();
            rt->set_time_delta_to_video_frame(
                std::chrono::microseconds((int)round(delta * 1000000.0)));
        } else {
            rt.reset(new AudioBuffer());
        }

        // if (rt) rt->set_shader_id(ffmpeg_shader_uuid);

        return rt;

    } catch (std::exception &e) {
        throw;
    }
    return AudioBufPtr();
}


xstudio::media::MediaDetail FFMpegMediaReader::detail(const caf::uri &uri) const {

    FFMpegDecoder t_decoder(uri_to_posix_path(uri), soundcard_sample_rate_);
    // N.B. MediaDetail needs frame duration, so invert frame rate
    std::vector<media::StreamDetail> streams;

    bool have_video = false;
    bool have_audio = false;

    for (auto &p : t_decoder.streams()) {
        if (p.second->codec_type() == AVMEDIA_TYPE_VIDEO ||
            p.second->codec_type() == AVMEDIA_TYPE_AUDIO) {
            have_video |= p.second->codec_type() == AVMEDIA_TYPE_VIDEO;
            have_audio |= p.second->codec_type() == AVMEDIA_TYPE_AUDIO;

            streams.emplace_back(media::StreamDetail(
                utility::FrameRateDuration(
                    static_cast<int>(t_decoder.duration_frames()),
                    t_decoder.frame_rate(p.first)),
                fmt::format("stream {}", p.first),
                (p.second->codec_type() == AVMEDIA_TYPE_VIDEO ? media::MT_IMAGE
                                                              : media::MT_AUDIO),
                "{0}@{1}/{2}"));
        }
    }

    if (!have_video && have_audio) {
        // this file is audio only. As an interim 'hack' to allow straightforward
        // compatibility with our (currently) inherently video frame based timeline, we add
        // a fake video stream running at 24 fps. In the ffmpeg decoder class, when
        // it is asked for a video frame it creates an empty one with the appropriate
        // pts for the frame number and then attaches decoded audio samples to
        // it in the same way we do with sources that have both audio and video
        for (auto &p : t_decoder.streams()) {
            if (p.second->codec_type() == AVMEDIA_TYPE_AUDIO) {

                streams.emplace_back(media::StreamDetail(
                    utility::FrameRateDuration(
                        static_cast<int>(t_decoder.duration_frames()),
                        t_decoder.frame_rate(p.first)),
                    fmt::format("stream {}", p.first),
                    media::MT_IMAGE,
                    "{0}@{1}/{2}"));
            }
        }
    }

    return xstudio::media::MediaDetail(name(), streams, t_decoder.first_frame_timecode());
}

MRCertainty FFMpegMediaReader::supported(const caf::uri &uri, const std::array<uint8_t, 16> &) {
    // we ignore the signature..
    // we cover so MANY...
    // but we're pretty good at movs..
    try {
        if (fs::path(uri.path().data()).extension() == ".mov")
            return MRC_FULLY;
        if (fs::path(uri.path().data()).extension() == ".exr")
            return MRC_NO;
    } catch (...) {
        return MRC_NO;
    }
    return MRC_MAYBE;
}

std::shared_ptr<thumbnail::ThumbnailBuffer>
FFMpegMediaReader::thumbnail(const media::AVFrameID &mptr, const size_t thumb_size) {
    try {

        std::string path = uri_to_posix_path(mptr.uri_);

        // DebugTimer d(path, mptr.frame_);
        if (!thumbnail_decoder || thumbnail_decoder->path() != path) {
            thumbnail_decoder.reset(
                new FFMpegDecoder(path, soundcard_sample_rate_, VIDEO_STREAM));
        }

        std::shared_ptr<thumbnail::ThumbnailBuffer> rt =
            thumbnail_decoder->decode_thumbnail_frame(mptr.frame_, thumb_size);

        // for now immediately deleting the decoder to prevent memory hogging
        // when generating many thumbnails. This could make the scrubbable
        // thumbnail feature slower since we might get consecutive calls to
        // the same FFMpegMediaReader. Need better management in the
        // MediaReaderActor
        thumbnail_decoder.reset();
        return rt;

    } catch (std::exception &e) {
        throw;
    }
}