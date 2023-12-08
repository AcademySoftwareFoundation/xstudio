// SPDX-License-Identifier: Apache-2.0
#include <filesystem>
#include <algorithm>
#include <cctype>

#include <Iex.h>
#include <IexErrnoExc.h>
#include <IlmThreadMutex.h>
#include <Imath/ImathBox.h>
#include <ImfInputFile.h>
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>
#include <ImfMultiView.h>
#include <ImfPreviewImage.h>
#include <ImfRationalAttribute.h>
#include <ImfRgbaFile.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfVecAttribute.h>

#include "xstudio/media/media_error.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include <chrono>
#include "xstudio/ui/opengl/shader_program_base.hpp"

#include "openexr.hpp"
#include "simple_exr_sampler.hpp"

#define EXR_READ_BLOCK_HEIGHT 256

namespace fs = std::filesystem;


using namespace xstudio;
using namespace xstudio::global_store;
using namespace xstudio::media_reader;
using namespace xstudio::utility;


namespace {

static Uuid s_plugin_uuid("9fd34c7e-8b35-44c7-8976-387bae1e35e0");

/* Given an image display and data window and a maximum
overscan amount, compute the cropped data window that limits
the overscan in the data window.

Returns true if a crop is required to meet max overscan requirement
bool crop_data_window(
        Imath::Box2i &data_window,
        const Imath::Box2i &display_window,
        const float overscan_percent
)
{

        const int width = display_window.size().x+1;
        const int height = display_window.size().y+1;

        const Imath::Box2i in_data_window = data_window;

        data_window.min.x = std::max(
                data_window.min.x,
                (int)round(-float(width)*overscan_percent/100.0f)
                );

        data_window.max.x = std::min(
                data_window.max.x,
                (int)round(float(width)*(overscan_percent/100.0f + 1.0f))
                );

        data_window.min.y = std::max(
                data_window.min.y,
                (int)round(-float(height)*overscan_percent/100.0f)
                );

        data_window.max.y = std::min(
                data_window.max.y,
                (int)round(float(height)*(overscan_percent/100.0f + 1.0f))
                );

        return in_data_window != data_window;

}
*/


static Uuid openexr_shader_uuid{"1c9259fc-46a5-11ea-87fe-989096adb429"};
static std::string shader{R"(
#version 430 core
uniform int width;
uniform int height;
uniform int num_channels;
uniform int pix_type;
uniform ivec2 image_bounds_min;
uniform ivec2 image_bounds_max;

// we need to forward declare this function, which is defined by the base
// gl shader class
vec2 get_image_data_2floats(int byte_address);
float get_image_data_float32(int byte_address);

vec4 fetch_pixel_32bitfloat(ivec2 image_coord)
{
	if (image_coord.x < image_bounds_min.x || image_coord.x >= image_bounds_max.x) return vec4(0.0,0.0,0.0,0.0);
	if (image_coord.y < image_bounds_min.y || image_coord.y >= image_bounds_max.y) return vec4(0.0,0.0,0.0,0.0);

    int pixel_address_bytes = ((image_coord.x-image_bounds_min.x) + (image_coord.y-image_bounds_min.y)*(image_bounds_max.x-image_bounds_min.x))*num_channels*4;

    float R = get_image_data_float32(pixel_address_bytes);

    if (num_channels > 2) {

        float G = get_image_data_float32(pixel_address_bytes+4);
        float B = get_image_data_float32(pixel_address_bytes+8);
        if (num_channels == 3)
        {
            return vec4(R,G,B,1.0);
        }
        else
        {
            float A = get_image_data_float32(pixel_address_bytes+12);
            return vec4(R, G, B, A);
        }
    }
    else if (num_channels == 2)
	{
        // using Luminance/Alpha layout
        float A = get_image_data_float32(pixel_address_bytes+4);
		return vec4(R,R,R,A);
	}
	else if (num_channels == 1)
	{
		// 1 channels, assume luminance
        return vec4(R, R, R, 1.0);
	}

	return vec4(0.9,0.4,0.0,1.0);

}

vec4 fetch_pixel_16bitfloat(ivec2 image_coord)
{
	if (image_coord.x < image_bounds_min.x || image_coord.x >= image_bounds_max.x) return vec4(0.0,0.0,0.0,0.0);
	if (image_coord.y < image_bounds_min.y || image_coord.y >= image_bounds_max.y) return vec4(0.0,0.0,0.0,0.0);

    int pixel_address_bytes = ((image_coord.x-image_bounds_min.x) + (image_coord.y-image_bounds_min.y)*(image_bounds_max.x-image_bounds_min.x))*num_channels*2;

    vec2 pixRG = get_image_data_2floats(pixel_address_bytes);

    if (num_channels > 2) {

        vec2 pixBA = get_image_data_2floats(pixel_address_bytes+4);
        if (num_channels == 3)
        {
            return vec4(pixRG.x,pixRG.y,pixBA.x,1.0);
        }
        else
        {
            return vec4(pixRG,pixBA);
        }
    }
    else if (num_channels == 2)
	{
        // 2 channels, assume luminance/alpha
		return vec4(pixRG.x,pixRG.x,pixRG.x,pixRG.y);
	}
	else if (num_channels == 1)
	{
		// 1 channels, assume luminance
        return vec4(pixRG.x,pixRG.x,pixRG.x,1.0);
	}

	return vec4(0.9,0.4,0.0,1.0);

}

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    if (pix_type == 1) {
        return fetch_pixel_16bitfloat(image_coord);
    } else if (pix_type == 2) {
        return fetch_pixel_32bitfloat(image_coord);
    }
}
)"};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaReaderPlugin<MediaReaderActor<OpenEXRMediaReader>>>(
                s_plugin_uuid,
                "OpenEXR",
                "xStudio",
                "OpenEXR Media Reader",
                semver::version("1.0.0"))}));
}
}

static ui::viewport::GPUShaderPtr
    openexr_shader(new ui::opengl::OpenGLShader(openexr_shader_uuid, shader));
} // namespace

OpenEXRMediaReader::OpenEXRMediaReader(const utility::JsonStore &prefs)
    : MediaReader("OpenEXR", prefs) {
    Imf::setGlobalThreadCount(16);
    max_exr_overscan_percent_ = 5.0f;
    readers_per_source_       = 1;

    update_preferences(prefs);
}

utility::Uuid OpenEXRMediaReader::plugin_uuid() const { return s_plugin_uuid; }

void OpenEXRMediaReader::update_preferences(const utility::JsonStore &prefs) {
    try {
        max_exr_overscan_percent_ = preference_value<float>(
            prefs, "/plugin/media_reader/OpenEXR/max_exr_overscan_percent");
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    try {
        readers_per_source_ =
            preference_value<int>(prefs, "/plugin/media_reader/OpenEXR/readers_per_source");
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

ImageBufPtr OpenEXRMediaReader::image(const media::AVFrameID &mptr) {
    try {

        std::string path = uri_to_posix_path(mptr.uri_);

        // DebugTimer dd(path);

        Imf::MultiPartInputFile input(path.c_str());
        int parts    = input.parts();
        int part_idx = -1;
        Imf::PixelType pix_type;
        std::vector<std::string> exr_channels_to_load;

        for (int prt = 0; prt < parts; ++prt) {
            // skip incomplete parts - maybe better error/handling messaging required?
            if (!input.partComplete(prt))
                continue;
            const Imf::Header &part_header = input.header(prt);
            std::vector<std::string> stream_ids;
            stream_ids_from_exr_part(part_header, stream_ids);
            for (const auto &stream_id : stream_ids) {
                if (stream_id == mptr.stream_id_) {
                    pix_type = pick_exr_channels_from_stream_id(
                        part_header, mptr.stream_id_, exr_channels_to_load);
                    part_idx = prt;
                }
            }
        }

        if (part_idx == -1 && mptr.stream_id_ == "Main" && input.partComplete(0)) {
            // Older version of exr reader only provided a stream called "Main".
            // For backwards compatibility map this to the first stream from the
            // first 'part' (which is what you got with the old reader)
            const Imf::Header &part_header = input.header(0);
            std::vector<std::string> stream_ids;
            stream_ids_from_exr_part(part_header, stream_ids);
            if (stream_ids.empty()) {
                std::stringstream ss;
                ss << "Unable to find readable layer/stream in part 0 for file \"" << path
                   << "\"\n";
                throw std::runtime_error(ss.str().c_str());
            }
            pix_type = pick_exr_channels_from_stream_id(
                part_header, stream_ids[0], exr_channels_to_load);
            part_idx = 0;
        } else if (part_idx == -1) {
            std::stringstream ss;
            ss << "Failed to pick exr channels for file \"" << path << "\"\n";
            throw std::runtime_error(ss.str().c_str());
        }

        Imf::InputPart in(input, part_idx);

        Imath::Box2i data_window    = in.header().dataWindow();
        Imath::Box2i display_window = in.header().displayWindow();

        // decide the area of the image we want to load
        const bool cropped_data_window = false; /*crop_data_window(
                 data_window,
                 display_window,
                 max_exr_overscan_percent_
                 );*/

        // compute the size of the buffer we need
        const size_t n_pixels = (data_window.size().x + 1) * (data_window.size().y + 1);
        const size_t bytes_per_channel = (pix_type == Imf::PixelType::HALF ? 2 : 4);
        const size_t bytes_per_pixel   = bytes_per_channel * exr_channels_to_load.size();
        const size_t buf_size          = n_pixels * bytes_per_pixel;

        // const size_t gl_line_size = 8192*4;
        // const size_t padded_buf_size = (buf_size & (gl_line_size-1)) ?
        // ((buf_size/gl_line_size) + 1)*gl_line_size : buf_size;

        JsonStore jsn;
        jsn["num_channels"] = exr_channels_to_load.size();
        jsn["pix_type"]     = int(pix_type);
        // jsn["path"] = to_string(mptr.uri_);

        ImageBufPtr buf(new ImageBuffer(openexr_shader_uuid, jsn));
        buf->allocate(buf_size);
        buf->set_pixel_aspect(in.header().pixelAspectRatio());

        // 4th channel is always put into 'alpha' channel as per shader code
        // above
        buf->set_has_alpha(exr_channels_to_load.size() > 3);

        buf->set_shader(openexr_shader);
        buf->set_image_dimensions(
            display_window.size(),
            Imath::Box2i(
                data_window.min, Imath::V2i(data_window.max.x + 1, data_window.max.y + 1)));

        buf->params()["path"]          = to_string(mptr.uri_);
        buf->params()["channel_names"] = exr_channels_to_load;
        buf->params()["stream_id"]     = mptr.stream_id_;

        if (cropped_data_window) {
            // if we are not loading the whole data window, we need to provide a temporary
            // buffer that matches the EXR data window width for OpenEXR to load pixels into, we
            // then copy the pixels we want into our cropped image buffer. We do this in chunks
            // in the Y dimension to take advantage of OpenEXR decompress threads that are
            // (possibly) more efficient when decoding blocks of pixels at once
            const Imath::Box2i actual_data_window = in.header().dataWindow();
            const size_t actual_data_window_width = actual_data_window.size().x + 1;
            const size_t line_stride              = actual_data_window_width * bytes_per_pixel;
            const size_t cropped_line_stride = (data_window.size().x + 1) * bytes_per_pixel;
            Imf::FrameBuffer fb;

            std::vector<uint8_t> tmp_buf(
                bytes_per_pixel * actual_data_window_width * EXR_READ_BLOCK_HEIGHT);
            byte *buffer = buf->buffer();

            for (int chunk_y_min = data_window.min.y; chunk_y_min < data_window.max.y;
                 chunk_y_min += EXR_READ_BLOCK_HEIGHT) {

                uint8_t *fPtr = tmp_buf.data() - actual_data_window.min.x * bytes_per_pixel -
                                chunk_y_min * line_stride;

                Imf::FrameBuffer fb;
                std::for_each(
                    exr_channels_to_load.begin(),
                    exr_channels_to_load.end(),
                    [&](const std::string chan_name) {
                        fb.insert(
                            chan_name.c_str(),
                            Imf::Slice(
                                pix_type, (char *)fPtr, bytes_per_pixel, line_stride, 1, 1, 0));
                        fPtr += bytes_per_channel;
                    });
                in.setFrameBuffer(fb);

                const int ymax =
                    std::min(chunk_y_min + EXR_READ_BLOCK_HEIGHT - 1, data_window.max.y);
                in.readPixels(chunk_y_min, ymax);

                // TODO: this copy may benefit from threading
                fPtr = tmp_buf.data() +
                       (data_window.min.x - actual_data_window.min.x) * bytes_per_pixel;
                for (int l = chunk_y_min; l < ymax; ++l) {
                    memcpy(buffer, fPtr, cropped_line_stride);
                    buffer += cropped_line_stride;
                    fPtr += line_stride;
                }
            }

        } else {

            const size_t line_stride =
                (data_window.max.x - data_window.min.x + 1) * bytes_per_pixel;
            byte *buffer = buf->buffer() - data_window.min.x * bytes_per_pixel -
                           data_window.min.y * line_stride;

            Imf::FrameBuffer fb;
            std::for_each(
                exr_channels_to_load.begin(),
                exr_channels_to_load.end(),
                [&](const std::string chan_name) {
                    fb.insert(
                        chan_name.c_str(),
                        Imf::Slice(
                            pix_type, (char *)buffer, bytes_per_pixel, line_stride, 1, 1, 0));
                    buffer += bytes_per_channel;
                });
            in.setFrameBuffer(fb);
            in.readPixels(data_window.min.y, data_window.max.y);
        }
        return buf;
    } catch (const std::exception &err) {
        throw media_corrupt_error(err.what());
    }

    spdlog::error("SHouldn't reach this");
    return ImageBufPtr();
}

MRCertainty
OpenEXRMediaReader::supported(const caf::uri &, const std::array<uint8_t, 16> &sig) {
    if (sig[0] == 0x76 && sig[1] == 0x2f && sig[2] == 0x31 && sig[3] == 0x01)
        return MRC_FULLY;

    return MRC_NO;
}

void OpenEXRMediaReader::get_channel_names_by_layer(
    const Imf::Header &header,
    std::map<std::string, std::vector<std::string>> &channel_names_by_layer) const {

    // prepend the channels layer with the part name
    const std::string partname = header.hasName() ? header.name() + "." : "";
    const auto &channels       = header.channels();
    for (Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
        const std::string channel_name = i.name();
        const size_t dot_pos           = channel_name.find(".");
        if (dot_pos != std::string::npos && dot_pos) {
            // channel name has a dot separator - assume prefix is the 'layer' name which
            // we shall assign as a separate MediaStream
            std::string layer_name = std::string(channel_name, 0, dot_pos);
            channel_names_by_layer[partname + layer_name].push_back(channel_name);
        } else {
            static std::set<std::string> rgba_names(
                {"r", "g", "b", "a", "red", "green", "blue", "alpha"});
            static std::set<std::string> xyz_names({"x", "y", "z", "w"});
            std::string chan_lower_case = to_lower(channel_name);
            if (rgba_names.find(chan_lower_case) != rgba_names.end()) {
                channel_names_by_layer[partname + "RGBA"].push_back(channel_name);
            } else if (xyz_names.find(chan_lower_case) != xyz_names.end()) {
                channel_names_by_layer[partname + "XYZ"].push_back(channel_name);
            } else {
                channel_names_by_layer[partname + "Other"].push_back(channel_name);
            }
        }
    }

    // we have a part with a name, and only one layer - just use the part name
    // for this set of channels
    if (header.hasName() && channel_names_by_layer.size() == 1) {
        auto chans = channel_names_by_layer.begin()->second;
        channel_names_by_layer.clear();
        channel_names_by_layer[header.name()] = chans;
    }
}

void OpenEXRMediaReader::stream_ids_from_exr_part(
    const Imf::Header &header, std::vector<std::string> &stream_ids) const {

    std::map<std::string, std::vector<std::string>> channel_names_by_layer;
    get_channel_names_by_layer(header, channel_names_by_layer);

    if (channel_names_by_layer.find("RGBA") != channel_names_by_layer.end()) {
        // make sure RGBA layer is first Stream
        stream_ids.emplace_back("RGBA");
    }
    if (channel_names_by_layer.find("XYZ") != channel_names_by_layer.end()) {
        // make sure XYZ layer is first or second Stream
        stream_ids.emplace_back("XYZ");
    }
    for (const auto &p : channel_names_by_layer) {
        if (p.first == "RGBA" || p.first == "XYZ")
            continue;
        stream_ids.push_back(p.first);
    }
}

Imf::PixelType OpenEXRMediaReader::pick_exr_channels_from_stream_id(
    const Imf::Header &header,
    const std::string &stream_id,
    std::vector<std::string> &exr_channels_to_load) const {

    std::map<std::string, std::vector<std::string>> channel_names_by_layer;
    get_channel_names_by_layer(header, channel_names_by_layer);

    if (channel_names_by_layer.find(stream_id) == channel_names_by_layer.end()) {
        throw std::runtime_error("Unable to match stream ID with exr part/layer names.");
    }

    exr_channels_to_load = channel_names_by_layer[stream_id];

    if (exr_channels_to_load.empty()) {
        throw std::runtime_error("Unable to match stream ID with exr part/layer names.");
    }

    // try and work out if channels are *.x, *.y etc or just 'x' 'y' 'z' etc.
    // we want to be careful that we don't match a channel called 'xyz.red' though!
    int score = 0;
    static std::vector<std::string> xyz_tokens({".x", ".y", ".z", ".u", ".v"});
    static std::vector<std::string> xyz_tokens0({"x", "y", "z", "u", "v"});
    for (const auto &chan : exr_channels_to_load) {
        const std::string lower_name = to_lower(chan);
        for (const auto &token : xyz_tokens) {
            if (lower_name.find(token) != std::string::npos) {
                score++;
            }
        }
        for (const auto &token : xyz_tokens0) {
            if (lower_name == token) {
                score++;
            }
        }
    }
    const bool is_xyzuv = score >= 2;

    // now we need to sort the channels so that RGBA gets mapped into 0,1,2,3
    // so we can use reverse alphabetical sorting.
    // OR channels named like *.x, *.y, *.z will also get mapped to 0,1,2
    std::sort(
        exr_channels_to_load.begin(),
        exr_channels_to_load.end(),
        [&is_xyzuv](std::string a, std::string b) {
            if (is_xyzuv) {
                return a < b;
            } else {
                return a > b;
            }
        });


    // At the moment we can handle either all channels are float 16 or all channels
    // are float 32 - we can't have a mix of channel types

    const auto &channels = header.channels();

    int p_type_chk = -1;
    Imf::PixelType pix_type;

    // fetch the channel names for 16bit float channels
    for (Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
        if (std::find(exr_channels_to_load.begin(), exr_channels_to_load.end(), i.name()) !=
            exr_channels_to_load.end()) {
            if (p_type_chk == -1) {
                p_type_chk = (int)i.channel().type;
                pix_type   = i.channel().type;
            } else if (pix_type != i.channel().type) {
                throw std::runtime_error(
                    "EXR part/layer mixes pixel channel types. This is not supported.");
            }
        }
    }
    return pix_type;
}


xstudio::media::MediaDetail OpenEXRMediaReader::detail(const caf::uri &uri) const {

    const std::string path(uri_to_posix_path(uri));
    utility::FrameRateDuration frd;
    utility::Timecode tc("00:00:00:00");
    std::vector<media::StreamDetail> streams;

    try {

        Imf::MultiPartInputFile input(path.c_str());
        double fr = 0.0;

        int parts = input.parts();
        // bool fileComplete = true;

        // we use timecode from the primary 'part' only - xSTUDIO doesn't yet handle streams
        // with different frame rates
        const Imf::Header &h  = input.header(0);
        const auto rate       = h.findTypedAttribute<Imf::RationalAttribute>("framesPerSecond");
        const auto rate_bogus = h.findTypedAttribute<Imf::V2iAttribute>("framesPerSecond");
        const auto timecode   = h.findTypedAttribute<Imf::TimeCodeAttribute>("timeCode");
        const auto timecode_rate = h.findTypedAttribute<Imf::IntAttribute>("timecodeRate");
        if (rate)
            fr = static_cast<double>(rate->value());
        else if (rate_bogus and rate_bogus->value().y > 0)
            fr = static_cast<double>(rate_bogus->value().x) /
                 static_cast<double>(rate_bogus->value().y);
        else if (timecode_rate)
            fr = static_cast<double>(timecode_rate->value());

        if (timecode) {
            // note if frame rate is no known from metadata we use 24pfs
            // as a default
            tc = utility::Timecode(
                timecode->value().hours(),
                timecode->value().minutes(),
                timecode->value().seconds(),
                timecode->value().frame(),
                fr == 0.0 ? 24.0 : fr,
                timecode->value().dropFrame());
        } else if (rate) {
            tc = utility::Timecode("00:00:00:00", fr);
        }

        // if frame rate is not known, return null frame rate so xSTUDIO will
        // apply its global frame rate
        frd.set_rate(fr == 0.0 ? utility::FrameRate() : utility::FrameRate(1.0 / fr));

        std::vector<std::string> stream_ids;
        for (int prt = 0; prt < parts; ++prt) {
            // skip incomplete parts - maybe better error/handling messaging required?
            if (!input.partComplete(prt))
                continue;
            const Imf::Header &part_header = input.header(prt);
            stream_ids_from_exr_part(part_header, stream_ids);
        }

        for (const auto &stream_id : stream_ids) {
            streams.emplace_back(media::StreamDetail(frd, stream_id));
        }

    } catch (const std::exception &e) {
        throw media_corrupt_error(e.what());
    }

    return xstudio::media::MediaDetail(name(), streams, tc);
}

thumbnail::ThumbnailBufferPtr
OpenEXRMediaReader::thumbnail(const media::AVFrameID &mptr, const size_t thumb_size) {

    Imf::RgbaInputFile file(uri_to_posix_path(mptr.uri_).c_str());

    if (file.header().hasPreviewImage()) {
        const Imf::PreviewImage &preview = file.header().previewImage();
        auto thumb                       = std::make_shared<thumbnail::ThumbnailBuffer>(
            preview.width(), preview.height(), thumbnail::TF_RGB24);

        auto b = &(thumb->data()[0]);

        for (unsigned int i = 0; i < preview.width() * preview.height(); ++i) {
            b[0] = std::byte{preview.pixels()[i].r};
            b[1] = std::byte{preview.pixels()[i].g};
            b[2] = std::byte{preview.pixels()[i].b};
            b += 3;
        }
        return thumb;
    } else {

        ImageBufPtr full_image_buffer = image(mptr);

        int exr_width     = full_image_buffer->image_size_in_pixels().x;
        int exr_height    = full_image_buffer->image_size_in_pixels().y;
        int adj_exr_width = (int)round(float(exr_width) * full_image_buffer->pixel_aspect());

        int thumb_width =
            adj_exr_width > exr_height ? thumb_size : (thumb_size * adj_exr_width) / exr_height;
        int thumb_height =
            exr_height > adj_exr_width ? thumb_size : (thumb_size * exr_height) / adj_exr_width;

        auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
            thumb_width, thumb_height, thumbnail::TF_RGBF96);

        // auto d = reinterpret_cast<uint8_t *>(thumb->data().data());

        SimpleExrSampler ss(full_image_buffer, thumb);
        ss.fill_output();

        return thumb;
    }
    throw media_corrupt_error("Failed to read preview " + uri_to_posix_path(mptr.uri_));
}

/*
 *
 * Note how this looks a *lot* like the glsl unpack pixel shader!
 *
 */
PixelInfo OpenEXRMediaReader::exr_buffer_pixel_picker(
    const ImageBuffer &buf, const Imath::V2i &pixel_location) {
    int width                         = buf.image_size_in_pixels().x;
    int height                        = buf.image_size_in_pixels().y;
    int num_channels                  = buf.shader_params().value("num_channels", 0);
    int pix_type                      = buf.shader_params().value("pix_type", 0);
    const Imath::V2i image_bounds_min = buf.image_pixels_bounding_box().min;
    const Imath::V2i image_bounds_max = buf.image_pixels_bounding_box().max;

    std::vector<std::string> channel_names;
    auto chan_names = buf.params()["channel_names"];

    std::string stream_id = buf.params()["stream_id"];

    PixelInfo r(pixel_location, stream_id);
    stream_id += ".";

    // strip off the stream_id if it's part of the channel name
    if (chan_names.is_array()) {
        for (const auto &i : chan_names) {
            std::string chan_name = i.get<std::string>();
            if (chan_name.find(stream_id) == 0) {
                chan_name = std::string(chan_name, stream_id.size());
            }
            channel_names.push_back(chan_name);
        }
    }

    auto get_image_data_float32 = [&](const int address) -> float {
        if (address < 0 || address >= (int)buf.size())
            return 0.0f;
        return *((float *)(buf.buffer() + address));
    };

    auto get_image_data_2xhalf_float = [&](const int address) -> Imath::V2f {
        if (address < 0 || address >= (int)buf.size())
            return Imath::V2f(0.0f, 0.0f);
        half *v = (half *)(buf.buffer() + address);
        return Imath::V2f(v[0], v[1]);
    };

    auto fetch_pixel_32bitfloat = [&](const Imath::V2i image_coord) {
        if (image_coord.x < image_bounds_min.x || image_coord.x >= image_bounds_max.x)
            return;
        if (image_coord.y < image_bounds_min.y || image_coord.y >= image_bounds_max.y)
            return;

        int pixel_address_bytes =
            ((image_coord.x - image_bounds_min.x) +
             (image_coord.y - image_bounds_min.y) * (image_bounds_max.x - image_bounds_min.x)) *
            num_channels * 4;

        float R = get_image_data_float32(pixel_address_bytes);
        r.add_raw_channel_info(channel_names[0], R);

        if (num_channels > 2) {

            float G = get_image_data_float32(pixel_address_bytes + 4);
            r.add_raw_channel_info(channel_names[1], G);
            float B = get_image_data_float32(pixel_address_bytes + 8);
            r.add_raw_channel_info(channel_names[2], B);
            if (num_channels == 4) {
                float A = get_image_data_float32(pixel_address_bytes + 12);
                r.add_raw_channel_info(channel_names[3], A);
            }
        } else if (num_channels == 2) {
            // using Luminance/Alpha layout
            float A = get_image_data_float32(pixel_address_bytes + 4);
            r.add_raw_channel_info(channel_names[1], A);
        }
    };

    auto fetch_pixel_16bitfloat = [&](const Imath::V2i image_coord) {
        if (image_coord.x < image_bounds_min.x || image_coord.x >= image_bounds_max.x)
            return;
        if (image_coord.y < image_bounds_min.y || image_coord.y >= image_bounds_max.y)
            return;

        int pixel_address_bytes =
            ((image_coord.x - image_bounds_min.x) +
             (image_coord.y - image_bounds_min.y) * (image_bounds_max.x - image_bounds_min.x)) *
            num_channels * 2;

        Imath::V2f pixRG = get_image_data_2xhalf_float(pixel_address_bytes);

        if (num_channels > 2) {

            Imath::V2f pixBA = get_image_data_2xhalf_float(pixel_address_bytes + 4);
            r.add_raw_channel_info(channel_names[0], pixRG.x);
            r.add_raw_channel_info(channel_names[1], pixRG.y);
            r.add_raw_channel_info(channel_names[2], pixBA.x);
            if (num_channels == 4) {
                r.add_raw_channel_info(channel_names[3], pixBA.y);
            }
        } else if (num_channels == 2) {
            r.add_raw_channel_info(channel_names[0], pixRG.x);
            r.add_raw_channel_info(channel_names[1], pixRG.y);
        } else if (num_channels == 1) {
            // 1 channels, assume luminance
            r.add_raw_channel_info(channel_names[0], pixRG.x);
        }
    };

    if (pix_type == 1)
        fetch_pixel_16bitfloat(pixel_location);
    else
        fetch_pixel_32bitfloat(pixel_location);
    return r;
}