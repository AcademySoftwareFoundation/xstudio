// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <filesystem>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "blank.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

namespace fs = std::filesystem;


using namespace xstudio::media_reader;
using namespace xstudio::utility;
using namespace xstudio;

namespace {
static Uuid myshader_uuid{"b6e17a3d-ed81-4659-98a3-dee3afa45c6f"};
static Uuid s_plugin_uuid{"0a933ecc-d87f-4eee-ad21-987a0341d29e"};
static std::string myshader{R"(
#version 330 core
uniform int blank_width;
uniform int dummy;

// forward declaration
uvec4 get_image_data_4bytes(int byte_address);

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    int bytes_per_pixel = 4;
    int pixel_bytes_offset_in_texture_memory = (image_coord.x + image_coord.y*blank_width)*bytes_per_pixel;
    uvec4 c = get_image_data_4bytes(pixel_bytes_offset_in_texture_memory);
    return vec4(float(c.x)/255.0f,float(c.y)/255.0f,float(c.z)/255.0f,1.0f);
}
)"};

static ui::viewport::GPUShaderPtr
    blank_shader(new ui::opengl::OpenGLShader(myshader_uuid, myshader));

} // namespace

utility::Uuid BlankMediaReader::plugin_uuid() const { return s_plugin_uuid; }


std::array<std::byte, 3> BlankMediaReader::get_colour(const std::string &colour) const {

    if (colour == "gray")
        return std::array<std::byte, 3>{std::byte{0x10}, std::byte{0x10}, std::byte{0x10}};
    else if (colour == "green")
        return std::array<std::byte, 3>{std::byte{0x20}, std::byte{0xff}, std::byte{0x20}};

    return std::array<std::byte, 3>{std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

ImageBufPtr BlankMediaReader::image(const media::AVFrameID &mpr) {
    ImageBufPtr buf;
    int width             = 192;
    int height            = 108;
    size_t size           = width * height;
    int bytes_per_channel = 1;

    // using 4 bytes per pixel is a lot easier as this aligns with the texture
    // format which is 8 bit RGBA. If you chose 3 bytes per pixel your shader
    // has to do more work to access the RGB values that will be packed into
    // the RGBA layout in the texture.
    int bytes_per_pixel = 4 * bytes_per_channel;

    // The json dict is how we inject uniform values into the shader. You must
    // provide image_dims as a ivec2 (used by a part of the shader common to
    // all image types). Aside from that, you must only provide values that
    // correspond to uniform variables in your shader. If you put values in there
    // that aren't used in the shader you will see those warnings, as the shader
    // class dumbly loops over the values in the json trying to find their location
    // in the shader and will warn if it can't find it.
    //
    // One subtelty is if you declare a uniform variable in your shader, but the
    // execution path after compilation never references this variable, it will not
    // exist and you still get an error. I've use 'dummy' here to demonstrate... it's
    // in the shader but doesn't actually do anything.
    JsonStore jsn;
    jsn["blank_width"] = width;

    buf.reset(new ImageBuffer(myshader_uuid, jsn));
    buf->allocate(size * bytes_per_pixel);
    buf->set_shader(blank_shader);
    buf->set_image_dimensions(Imath::V2i(width, height));

    if (mpr.error() != "") {
        buf->set_error(mpr.error());
    }

    auto i = mpr.uri().query().find("colour");
    if (i != mpr.uri().query().end()) {
        auto c = get_colour(i->second);
        int i  = 0;
        for (byte *b = buf->buffer(); b < (buf->buffer() + size * bytes_per_pixel); b += 4) {
            if (((i / 16) & 1) == (i / (192 * 16) & 1)) {
                b[0] = (byte)c[0];
                b[1] = (byte)c[1];
                b[2] = (byte)c[2];
            } else {
                b[0] = (byte)0;
                b[1] = (byte)0;
                b[2] = (byte)0;
            }
            ++i;
        }
    } else {
        std::memset(buf->buffer(), 0, size * bytes_per_pixel);
    }

    return buf;
}

thumbnail::ThumbnailBufferPtr
BlankMediaReader::thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) {
    auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
        thumb_size, thumb_size, thumbnail::TF_RGB24);

    auto i = mpr.uri().query().find("colour");
    if (i != mpr.uri().query().end()) {
        auto c = get_colour(i->second);
        auto b = &(thumb->data()[0]);

        for (size_t i = 0; i < thumb->width() * thumb->height(); i++) {
            b[0] = c[0];
            b[1] = c[1];
            b[2] = c[2];
            b += 3;
        }
    } else {
        std::memset(&(thumb->data()[0]), 0, thumb->size());
    }

    return thumb;
}

MRCertainty BlankMediaReader::supported(const caf::uri &uri, const std::array<uint8_t, 16> &) {
    // we ignore the signature..
    // we cover so MANY...
    // but we're pretty good at movs..
    // spdlog::warn("{} {}", to_string(uri.scheme()), to_string(uri.authority()));

    if (to_string(uri.scheme()) == "xstudio" and to_string(uri.authority()) == "blank") {
        return MRC_FULLY;
    }

    return MRC_NO;
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaReaderPlugin<MediaReaderActor<BlankMediaReader>>>(
                s_plugin_uuid,
                "Blank",
                "xStudio",
                "Blank Media Reader",
                semver::version("1.0.0"))}));
}
}
