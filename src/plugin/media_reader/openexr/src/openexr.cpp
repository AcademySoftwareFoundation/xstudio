// SPDX-License-Identifier: Apache-2.0
#include <filesystem>


#include <Iex.h>
#include <IexErrnoExc.h>
#include <IlmThreadMutex.h>
#include <Imath/ImathBox.h>
#include <ImfChannelList.h>
#include <ImfHeader.h> // staticInitialize
#include <ImfInputFile.h>
#include <ImfMultiPartInputFile.h>
#include <ImfMultiView.h>
#include <ImfPreviewImage.h>
#include <ImfRationalAttribute.h>
#include <ImfRgbaFile.h>
#include <ImfTimeCodeAttribute.h>

#include "xstudio/media/media_error.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include <chrono>

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

/* Examine the channel data in an EXR input file and map channels in the
file to RGB(A) channels for display.

We have to do this somewhat heuristically as EXR channel names can be literally
anything. We want to look for regular channel names like 'r','g','b' or '*.x' '*.y' '*.z'

*/
Imf::PixelType
exr_channels_decision(Imf::InputFile &in, std::vector<std::string> &exr_channels_to_load) {

    const Imf::ChannelList channels = in.header().channels();
    Imf::PixelType pixelType;
    exr_channels_to_load.clear();

    // At the moment we can handle either all channels are float 16 or all channels
    // are float 32 - we can't have a mix of channel types

    // fetch the channel names for 16bit float channels
    for (Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
        if (i.channel().type == Imf::PixelType::HALF) {
            exr_channels_to_load.emplace_back(i.name());
        }
    }
    if (exr_channels_to_load.empty()) {
        // there were no 16bit float channels - look for float32 instead
        for (Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
            if (i.channel().type == Imf::PixelType::FLOAT) {
                exr_channels_to_load.emplace_back(i.name());
            }
        }
        pixelType = Imf::PixelType::FLOAT;
    } else {
        pixelType = Imf::PixelType::HALF;
    }

    // if we have 3 channels that include .x, .y and .z in their names we want
    // to map these to RGB when loading.
    int score = 0;
    std::vector<std::string> xyz_tokens({".x", ".y", ".z"});
    for (const auto &token : xyz_tokens) {
        for (const auto &c : exr_channels_to_load) {
            if (c.find(token) != std::string::npos) {
                score++;
                break;
            }
        }
    }
    const bool has_xyz_channels = score == 3;

    if (has_xyz_channels) {
        std::sort(
            exr_channels_to_load.begin(),
            exr_channels_to_load.end(),
            [](std::string a, std::string b) {
                // this test ensures channels with short names (e.g. 'R','G', 'B' etc) will
                // always come first - thus we filter out non-RGB channles (like DI mattes)
                // in the presense of regular RGB channels
                if (a.size() != b.size())
                    return a.size() < b.size();
                // this test alphabetically orders the channels to ensure X, Y, Z order (if
                // we have such channel names)
                return a < b;
            });
    } else {
        std::sort(
            exr_channels_to_load.begin(),
            exr_channels_to_load.end(),
            [](std::string a, std::string b) {
                // see above ..
                if (a.size() != b.size())
                    return a.size() < b.size();
                // this test reverse alphabetically orders the channels to ensure R, G, B, A
                // order (if we have such channel names)
                return a > b;
            });
    }

    // now we truncate the list of channels to max 4 channels to load
    while (exr_channels_to_load.size() > 4)
        exr_channels_to_load.pop_back();

    return pixelType;
}

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
    openexr_shader(new ui::viewport::GPUShader(openexr_shader_uuid, shader));
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

        Imf::InputFile in(path.c_str());

        // decide which channels we are going to load
        std::vector<std::string> exr_channels_to_load;
        Imf::PixelType pix_type = exr_channels_decision(in, exr_channels_to_load);

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
        buf->set_shader(openexr_shader);
        buf->set_image_dimensions(
            display_window.size(),
            Imath::Box2i(
                data_window.min, Imath::V2i(data_window.max.x + 1, data_window.max.y + 1)));

        buf->params()["path"] = to_string(mptr.uri_);

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

xstudio::media::MediaDetail OpenEXRMediaReader::detail(const caf::uri &uri) const {
    const std::string path(uri_to_posix_path(uri));
    utility::FrameRateDuration frd;
    utility::Timecode tc("00:00:00:00");

    try {
        Imf::MultiPartInputFile input(path.c_str());
        double fr = 30.0;
        // int parts = input.parts();
        // bool fileComplete = true;

        // for (int prt = 0; prt < parts && fileComplete; ++prt)
        // 	if (!input.partComplete (prt))
        // 		fileComplete = false;
        const Imf::Header &h = input.header(0);
        const auto rate      = h.findTypedAttribute<Imf::RationalAttribute>("framesPerSecond");
        const auto timecode  = h.findTypedAttribute<Imf::TimeCodeAttribute>("timeCode");

        if (rate) {
            fr = static_cast<double>(rate->value());
            frd.set_rate(utility::FrameRate(1.0 / fr));
        }

        if (timecode) {
            tc = utility::Timecode(
                timecode->value().hours(),
                timecode->value().minutes(),
                timecode->value().seconds(),
                timecode->value().frame(),
                fr,
                timecode->value().dropFrame());
        } else if (rate) {
            tc = utility::Timecode("00:00:00:00", fr);
        }

        spdlog::debug("{} {}", fr, frd.rate().to_fps());
    } catch (const std::exception &e) {
        throw media_corrupt_error(e.what());
    }

    return xstudio::media::MediaDetail(name(), {media::StreamDetail(frd)}, tc);
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