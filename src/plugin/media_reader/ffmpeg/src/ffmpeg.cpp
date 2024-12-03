// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <iostream>
#include <map>
#include <mutex>
#include <regex>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

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
static Uuid ffmpeg_shader_uuid_yuv{"9854e7c0-2e32-4600-aedd-463b2a6de95a"};
static Uuid ffmpeg_shader_uuid_rgb{"20015805-0b83-426a-bf7e-f6549226bfef"};

static std::string the_shader_yuv = {R"(
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

vec4 fetch_rgba_pixel(ivec2 image_coord)
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
)"};

static std::string the_shader_rgb = {R"(
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
		rgba.xyzw = rgba.yzwx;
	} else if (rgb == 4) { // AV_PIX_FMT_RGBA
        //nope
	} else if (rgb == 5) { // AV_PIX_FMT_ABGR
		rgba.xyzw = rgba.wzyx;
	} else if (rgb == 6) { // AV_PIX_FMT_BGRA
		rgba.xyzw = rgba.zyxw;
	}

	return vec4(rgba) * norm_coeff;
}

vec4 fetch_rgba_pixel_from_rgb_48(ivec2 image_coord)
{
    // 2 bytes per channel, 3 channels per pix
	int address = image_coord.x*6 + image_coord.y*y_linesize;
    ivec4 rgba;
    rgba.x = get_image_data_2bytes(address);
    rgba.y = get_image_data_2bytes(address+2);
    rgba.z = get_image_data_2bytes(address+4);
    return vec4(vec3(rgba.xyz) * norm_coeff, 1.0f);
}

vec4 fetch_rgba_pixel_from_rgba_64(ivec2 image_coord)
{
    // 2 bytes per channel, 4 channels per pix
	int address = image_coord.x*8 + image_coord.y*y_linesize;
    ivec4 rgba;
    rgba.x = get_image_data_2bytes(address);
    rgba.y = get_image_data_2bytes(address+2);
    rgba.z = get_image_data_2bytes(address+4);
    rgba.w = get_image_data_2bytes(address+6);
    return vec4(rgba) * norm_coeff;
}


vec4 fetch_rgba_pixel_from_gbr_planar(ivec2 image_coord)
{
    // 2 bytes per channel, 3 channels per pix
	int address = image_coord.x*2 + image_coord.y*y_linesize;
	
    ivec4 rgba;
    rgba.x = get_image_data_2bytes(address+v_plane_bytes_offset);
    rgba.y = get_image_data_2bytes(address+y_plane_bytes_offset);
    rgba.z = get_image_data_2bytes(address+u_plane_bytes_offset);
    if (a_linesize != 0) {
        rgba.w = get_image_data_2bytes(address+a_plane_bytes_offset);
        return vec4(rgba) * norm_coeff;
    } else {
        return vec4(vec3(rgba.xyz) * norm_coeff, 1.0f);
    }
    
}

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
	if (rgb == 9) {
        return fetch_rgba_pixel_from_rgba_64(image_coord);
     }else if (rgb == 8) {
		return fetch_rgba_pixel_from_rgb_48(image_coord);
	} else if (rgb == 7) {
		return fetch_rgba_pixel_from_gbr_planar(image_coord);
	} else if (rgb > 2) {
		return fetch_rgba_pixel_from_rgba32(image_coord);
	} else {
		return fetch_rgba_pixel_from_rgb24(image_coord, rgb == 2);
	}
}
)"};

static ui::viewport::GPUShaderPtr
    ffmpeg_shader_yuv(new ui::opengl::OpenGLShader(ffmpeg_shader_uuid_yuv, the_shader_yuv));

static ui::viewport::GPUShaderPtr
    ffmpeg_shader_rgb(new ui::opengl::OpenGLShader(ffmpeg_shader_uuid_rgb, the_shader_rgb));

// See 'uri_convert' - I'm doing this because 'uri_to_posix_path' which is used
// in most places we need to go from uri to filsystem can't deal with uris like
// https://aswf.s3-accelerate.amazonaws.com/ALab_h264_MOVs/mk020_0220.mov. For
// FFMPEG reader, we might want to access media via https and other protocols
// so I have avoided using uri_to_posix_path
std::string uri_decode(const std::string &eString) {
    std::string ret;
    char ch;
    unsigned int i, j;
    for (i = 0; i < eString.length(); i++) {
        if (int(eString[i]) == 37) {
            sscanf(eString.substr(i + 1, 2).c_str(), "%x", &j);
            ch = static_cast<char>(j);
            ret += ch;
            i = i + 2;
        } else {
            ret += eString[i];
        }
    }
    return (ret);
}

std::string uri_convert(const caf::uri &uri) {

    // This may be a kettle of fish.

    // uri like https://aswf.s3-accelerate.amazonaws.com/ALab_h264_MOVs/mk020_0220.mov
    // can be passed through.
    // uri like file://localhost/user_data/my_vid.mov needs the 'localhost' removed.
    auto path = to_string(uri);
    utility::replace_string_in_place(path, "file://localhost", "file:");
    return uri_decode(path);
}

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
#ifdef __linux__
        soundcard_sample_rate_ =
            preference_value<int>(prefs, "/core/audio/pulse_audio_prefs/sample_rate");
#endif
#ifdef _WIN32
        soundcard_sample_rate_ =
            preference_value<int>(prefs, "/core/audio/windows_audio_prefs/sample_rate");
#endif

        default_rate_ = utility::FrameRate(
            preference_value<std::string>(prefs, "/core/session/media_rate"));

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

static std::mutex m;
static int ct = 0;

ImageBufPtr FFMpegMediaReader::image(const media::AVFrameID &mptr) {
    std::string path = uri_convert(mptr.uri());

    if (last_decoded_image_ && last_decoded_image_->media_key() == mptr.key()) {
        return last_decoded_image_;
    }

    if (!decoder || decoder->path() != path) {
        decoder.reset(
            new FFMpegDecoder(path, soundcard_sample_rate_, default_rate_, mptr.stream_id()));
    }

    ImageBufPtr rt;
    decoder->decode_video_frame(mptr.frame(), rt);

    if (rt && !rt->shader_params().is_null()) {
        if (rt->shader_params().value("rgb", 0) != 0) {
            rt->set_shader(ffmpeg_shader_rgb);
        } else {
            rt->set_shader(ffmpeg_shader_yuv);
        }
        last_decoded_image_ = rt;
    }

    return rt;
}

AudioBufPtr FFMpegMediaReader::audio(const media::AVFrameID &mptr) {

    try {

        // Set the path for the media file. Currently, it's hard-coded to a specific file.
        // This may be updated later to use the URI from the AVFrameID object.
        std::string path = uri_convert(mptr.uri());

        // If the audio_decoder object doesn't exist or the path it's using differs
        // from the one we're interested in, then create a new audio_decoder.
        if (!audio_decoder || audio_decoder->path() != path) {
            audio_decoder.reset(new FFMpegDecoder(
                path, soundcard_sample_rate_, default_rate_, mptr.stream_id()));
        }

        AudioBufPtr rt;

        // Decode the audio frame using the decoder and get the resulting audio buffer.
        audio_decoder->decode_audio_frame(mptr.frame(), rt);

        // If decoding didn't produce an audio buffer (i.e., rt is null), then initialize
        // a new empty audio buffer.
        if (!rt) {
            rt.reset(new AudioBuffer());
        }

        // Return the obtained/created audio buffer.
        return rt;

    } catch ([[maybe_unused]] std::exception &e) {
        // If an exception is encountered, rethrow it to be handled by the caller.
        throw;
    }

    // If everything else fails, return an empty shared pointer.
    return AudioBufPtr();
}


xstudio::media::MediaDetail FFMpegMediaReader::detail(const caf::uri &uri) const {

    FFMpegDecoder t_decoder(uri_convert(uri), soundcard_sample_rate_, default_rate_);
    // N.B. MediaDetail needs frame duration, so invert frame rate
    std::vector<media::StreamDetail> streams;

    for (auto &p : t_decoder.streams()) {
        if (p.second->codec_type() == AVMEDIA_TYPE_VIDEO ||
            p.second->codec_type() == AVMEDIA_TYPE_AUDIO) {

            auto frameRate = t_decoder.frame_rate(p.first);

            // If the stream has a duration of 1 then it is probably frame based.
            // FFMPEG assigns a default frame rate of 25fps to JPEGs, for example -
            // If this has happened, we want to ignore this and let xstudio apply
            // xSTUDIO's default frame rate preference instead.
            if ((t_decoder.duration_frames() == 1 &&
                 frameRate.to_flicks() == timebase::flicks(28224000))) {
                // setting a null frame rate will make xstudio use its own preference
                frameRate = utility::FrameRate();
            }

            streams.emplace_back(media::StreamDetail(
                utility::FrameRateDuration(
                    static_cast<int>(t_decoder.duration_frames()), frameRate),
                fmt::format("stream {}", p.first),
                (p.second->codec_type() == AVMEDIA_TYPE_VIDEO ? media::MT_IMAGE
                                                              : media::MT_AUDIO),
                "{0}@{1}/{2}",
                p.second->resolution(),
                p.second->pixel_aspect(),
                p.first));
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

        std::string path = uri_convert(mptr.uri());

        // DebugTimer d(path, mptr.frame());
        if (!thumbnail_decoder || thumbnail_decoder->path() != path) {
            thumbnail_decoder.reset(new FFMpegDecoder(
                path, soundcard_sample_rate_, default_rate_, mptr.stream_id()));
        }

        std::shared_ptr<thumbnail::ThumbnailBuffer> rt =
            thumbnail_decoder->decode_thumbnail_frame(mptr.frame(), thumb_size);

        // for now immediately deleting the decoder to prevent memory hogging
        // when generating many thumbnails. This could make the scrubbable
        // thumbnail feature slower since we might get consecutive calls to
        // the same FFMpegMediaReader. Need better management in the
        // MediaReaderActor
        thumbnail_decoder.reset();
        return rt;

    } catch ([[maybe_unused]] std::exception &e) {
        throw;
    }
}

#define ALPHA_UNSET -1e6f

PixelInfo FFMpegMediaReader::ffmpeg_buffer_pixel_picker(
    const ImageBuffer &buf, const Imath::V2i &pixel_location) {

    // This function is close to being a C++ implementation of the the
    // glsl shader(s) at the top of this file. It allows xstudio to inspect
    // individual pixel values from the frame buffer.
    try {

        PixelInfo r(pixel_location);
        if (buf.shader_params().is_null())
            return r;

        const int width  = buf.image_size_in_pixels().x;
        const int height = buf.image_size_in_pixels().y;

        if (pixel_location.x < 0 || pixel_location.x >= width || pixel_location.y < 0 ||
            pixel_location.y >= height) {
            return r;
        }

        const int rgb                  = buf.shader_params().value("rgb", 0);
        const int y_linesize           = buf.shader_params().value("y_linesize", 0);
        const int u_linesize           = buf.shader_params().value("u_linesize", 0);
        const int v_linesize           = buf.shader_params().value("v_linesize", 0);
        const int a_linesize           = buf.shader_params().value("a_linesize", 0);
        const int y_plane_bytes_offset = buf.shader_params().value("y_plane_bytes_offset", 0);
        const int u_plane_bytes_offset = buf.shader_params().value("u_plane_bytes_offset", 0);
        const int v_plane_bytes_offset = buf.shader_params().value("v_plane_bytes_offset", 0);
        const int a_plane_bytes_offset = buf.shader_params().value("a_plane_bytes_offset", 0);
        const int half_scale_uvy       = buf.shader_params().value("half_scale_uvy", 0);
        const int half_scale_uvx       = buf.shader_params().value("half_scale_uvx", 0);
        const int bits_per_channel     = buf.shader_params().value("bits_per_channel", 0);
        const Imath::M33f yuv_conv =
            buf.shader_params().value("yuv_conv", Imath::M33f());
        const Imath::V3i yuv_offsets = buf.shader_params().value("yuv_offsets", Imath::V3i());
        const float norm_coeff       = buf.shader_params().value("norm_coeff", 1.0f);

        auto get_image_data_4bytes = [&](const int address) -> std::array<float, 4> {
            if (address < 0 || address >= (int)buf.size())
                return std::array<float, 4>({0.0f, 0.0f, 0.0f, 0.0f});
            std::array<float, 4> r;
            memcpy(r.data(), (buf.buffer() + address), 4 * sizeof(float));
            return r;
        };

        auto get_image_data_2bytes = [&](const int address) -> int {
            if (address < 0 || address >= (int)buf.size())
                return 0;
            int *r = (int *)(buf.buffer() + address);
            return *r & 65535;
        };

        auto get_image_data_1byte = [&](const int address) -> int {
            if (address < 0 || address >= (int)buf.size())
                return 0;
            int *r = (int *)(buf.buffer() + address);
            return *r & 255;
        };

        auto yuv_tex_lookup_10bit =
            [&](const Imath::V2i image_coord, int offset, int linestride) -> int {
            int address = offset + image_coord.x * 2 + image_coord.y * linestride;
            return get_image_data_2bytes(address);
        };

        auto yuv_tex_lookup_8bit =
            [&](const Imath::V2i image_coord, int offset, int linestride) -> int {
            int address = offset + image_coord.x + image_coord.y * linestride;
            return get_image_data_1byte(address);
        };

        auto fetch_rgba_pixel_from_rgb24 = [&](const Imath::V2i image_coord,
                                               bool bgr) -> Imath::V4f {
            auto bytes4 = get_image_data_4bytes(image_coord.x * 3 + image_coord.y * y_linesize);
            Imath::V4f r = bgr ? Imath::V4f(bytes4[2], bytes4[1], bytes4[0], 0.0f)
                               : Imath::V4f(bytes4[0], bytes4[1], bytes4[2], 0.0f);
            r *= norm_coeff;
            r.z = 1.0f;
            return r;
        };

        auto fetch_rgba_pixel_from_rgba32 = [&](const Imath::V2i image_coord) -> Imath::V4f {
            int address = image_coord.x * 4 + image_coord.y * y_linesize;
            auto bytes4 = get_image_data_4bytes(address);
            Imath::V4f r;
            if (rgb == 3) { // AV_PIX_FMT_ARGB
                r = Imath::V4f(bytes4[1], bytes4[2], bytes4[3], bytes4[0]);
            } else if (rgb == 4) { // AV_PIX_FMT_RGBA
                // nope
            } else if (rgb == 5) { // AV_PIX_FMT_ABGR
                r = Imath::V4f(bytes4[3], bytes4[2], bytes4[1], bytes4[0]);
            } else if (rgb == 6) { // AV_PIX_FMT_BGRA
                r = Imath::V4f(bytes4[0], bytes4[1], bytes4[2], bytes4[3]);
            }

            return r * norm_coeff;
        };

        auto fetch_rgba_pixel_from_yuv = [&](const Imath::V2i image_coord) -> Imath::V4f {
            Imath::V3i yuv;
            float a = ALPHA_UNSET;

            Imath::V2i uv_coord = Imath::V2i(
                half_scale_uvx ? image_coord.x >> 1 : image_coord.x,
                half_scale_uvy ? image_coord.y >> 1 : image_coord.y);

            if (bits_per_channel == 10 || bits_per_channel == 12) {

                yuv = Imath::V3i(
                    yuv_tex_lookup_10bit(image_coord, y_plane_bytes_offset, y_linesize),
                    yuv_tex_lookup_10bit(uv_coord, u_plane_bytes_offset, u_linesize),
                    yuv_tex_lookup_10bit(uv_coord, v_plane_bytes_offset, v_linesize));

                if (half_scale_uvx && (image_coord.x & 1) == 1) {

                    uv_coord.x = uv_coord.x + 1;
                    Imath::V3i yuv2(
                        yuv.x,
                        yuv_tex_lookup_10bit(uv_coord, u_plane_bytes_offset, u_linesize),
                        yuv_tex_lookup_10bit(uv_coord, v_plane_bytes_offset, v_linesize));

                    yuv = (yuv + yuv2) / 2;
                }

                if (a_linesize != 0) {
                    a = float(yuv_tex_lookup_10bit(
                            image_coord, a_plane_bytes_offset, a_linesize)) *
                        norm_coeff;
                }

            } else {

                yuv = Imath::V3i(
                    yuv_tex_lookup_8bit(image_coord, y_plane_bytes_offset, y_linesize),
                    yuv_tex_lookup_8bit(uv_coord, u_plane_bytes_offset, u_linesize),
                    yuv_tex_lookup_8bit(uv_coord, v_plane_bytes_offset, v_linesize));
            }

            // record the code values
            r.add_code_value_info("Y", yuv.x);
            r.add_code_value_info("U", yuv.y);
            r.add_code_value_info("V", yuv.z);
            if (a != ALPHA_UNSET)
                r.add_code_value_info("A", a);

            yuv -= yuv_offsets;
            Imath::V3f yuvf(yuv.x, yuv.y, yuv.z);

            yuvf *= yuv_conv;
            yuvf *= norm_coeff;

            return Imath::V4f(yuvf.x, yuvf.y, yuvf.z, a); // divide by 1023.0
        };

        auto fetch_rgba_pixel_from_rgb_48 = [&](const Imath::V2i image_coord) -> Imath::V4f {
            // 2 bytes per channel, 3 channels per pix
            int address = image_coord.x * 6 + image_coord.y * y_linesize;
            Imath::V4f rgba;
            rgba.x = get_image_data_2bytes(address);
            rgba.y = get_image_data_2bytes(address + 2);
            rgba.z = get_image_data_2bytes(address + 4);

            // record the code values
            r.add_code_value_info("R", rgba.x);
            r.add_code_value_info("G", rgba.y);
            r.add_code_value_info("B", rgba.z);

            rgba *= norm_coeff;
            rgba.w = 0.0f;
            return rgba;
        };

        auto fetch_rgba_pixel_from_rgba_64 = [&](const Imath::V2i image_coord) -> Imath::V4f {
            // 2 bytes per channel, 4 channels per pix
            int address = image_coord.x * 8 + image_coord.y * y_linesize;
            Imath::V4f rgba;
            rgba.x = get_image_data_2bytes(address);
            rgba.y = get_image_data_2bytes(address + 2);
            rgba.z = get_image_data_2bytes(address + 4);
            rgba.w = get_image_data_2bytes(address + 6);

            // record the code values
            r.add_code_value_info("R", rgba.x);
            r.add_code_value_info("G", rgba.y);
            r.add_code_value_info("B", rgba.z);
            r.add_code_value_info("A", rgba.w);

            return rgba * norm_coeff;
        };


        auto fetch_rgba_pixel_from_gbr_planar =
            [&](const Imath::V2i image_coord) -> Imath::V4f {
            // 2 bytes per channel, 3 channels per pix
            int address = image_coord.x * 2 + image_coord.y * y_linesize;

            Imath::V4f rgba;
            rgba.x = get_image_data_2bytes(address + v_plane_bytes_offset);
            rgba.y = get_image_data_2bytes(address + y_plane_bytes_offset);
            rgba.z = get_image_data_2bytes(address + u_plane_bytes_offset);
            r.add_code_value_info("R", rgba.x);
            r.add_code_value_info("G", rgba.y);
            r.add_code_value_info("B", rgba.z);
            if (a_linesize != 0) {
                rgba.w = get_image_data_2bytes(address + a_plane_bytes_offset);
                r.add_code_value_info("A", rgba.w);
                return rgba * norm_coeff;
            } else {
                rgba *= norm_coeff;
                rgba.w = ALPHA_UNSET;
                return rgba;
            }
        };

        Imath::V4f rgba_pix;
        if (rgb == 0) {
            rgba_pix = fetch_rgba_pixel_from_yuv(pixel_location);
        } else if (rgb == 9) {
            rgba_pix = fetch_rgba_pixel_from_rgba_64(pixel_location);
        } else if (rgb == 8) {
            rgba_pix = fetch_rgba_pixel_from_rgb_48(pixel_location);
        } else if (rgb == 7) {
            rgba_pix = fetch_rgba_pixel_from_gbr_planar(pixel_location);
        } else if (rgb > 2) {
            rgba_pix = fetch_rgba_pixel_from_rgba32(pixel_location);
        } else {
            rgba_pix = fetch_rgba_pixel_from_rgb24(pixel_location, rgb == 2);
        }
        r.add_raw_channel_info("R", rgba_pix.x);
        r.add_raw_channel_info("G", rgba_pix.y);
        r.add_raw_channel_info("B", rgba_pix.z);
        if (rgba_pix.w != ALPHA_UNSET)
            r.add_raw_channel_info("A", rgba_pix.w);
        return r;

    } catch (std::exception &e) {

        std::cerr << "E " << e.what() << "\n";
    }
    return PixelInfo();
}
