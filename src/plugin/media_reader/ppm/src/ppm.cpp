// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <filesystem>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ppm.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/media/media_error.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

namespace fs = std::filesystem;


using namespace xstudio::media_reader;
using namespace xstudio::utility;
using namespace xstudio;

namespace {
static Uuid myshader_uuid{"1c9259fc-46a5-11ea-9de9-989096adb429"};
static Uuid s_plugin_uuid{"c0465f96-901a-42bc-875b-ecf30f1eef14"};
static std::string myshader{R"(
#version 330 core
uniform int width;
uniform int bytes_per_channel;

// forward declaration
uvec4 get_image_data_4bytes(int byte_address);

vec4 fetch_pixel_8bit(ivec2 image_coord)
{
    int address = (image_coord.x + image_coord.y*width)*3;
    uvec4 c = get_image_data_4bytes(address);
    return vec4(float(c.x)/255.0f,float(c.y)/255.0f,float(c.z)/255.0f,1.0f);
}

vec4 fetch_pixel_16bit(ivec2 image_coord)
{
    int address = (image_coord.x + image_coord.y*width)*6;
    uvec4 c = get_image_data_4bytes(address);
    return vec4(float(c.x)/65535.0f,float(c.y)/65535.0f,float(c.z)/65535.0f,1.0f);
}

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    if (bytes_per_channel == 1) {
        return fetch_pixel_8bit(image_coord);
    } else {
        return fetch_pixel_16bit(image_coord);
    }
}
)"};

static ui::viewport::GPUShaderPtr
    ppm_shader(new ui::opengl::OpenGLShader(myshader_uuid, myshader));

} // namespace

ImageBufPtr PPMMediaReader::image(const media::AVFrameID &mptr) {
    ImageBufPtr buf;

    std::ifstream inp(uri_to_posix_path(mptr.uri()), std::ios::in | std::ios::binary);
    if (inp.is_open()) {
        size_t width;
        size_t height;
        size_t max_col_val;

        std::string line;

        std::getline(inp, line);
        if (line != "P6")
            throw media_corrupt_error(
                "Error. Unrecognized file format." + to_string(mptr.uri()));

        std::getline(inp, line);
        while (line[0] == '#') {
            std::getline(inp, line);
        }

        std::stringstream dimensions(line);
        try {
            dimensions >> width;
            dimensions >> height;
        } catch (std::exception &e) {
            throw media_corrupt_error(std::string("Header file format error. ") + e.what());
        }

        std::getline(inp, line);
        std::stringstream max_val(line);
        try {
            max_val >> max_col_val;
        } catch (std::exception &e) {
            throw media_corrupt_error(std::string("Header file format error. ") + e.what());
        }

        size_t size           = width * height;
        int bytes_per_channel = (max_col_val == 65535 ? 2 : 1);
        int bytes_per_pixel   = 3 * bytes_per_channel;

        JsonStore jsn;
        jsn["bytes_per_channel"] = bytes_per_channel;
        jsn["width"]             = width;
        jsn["height"]            = height;

        buf.reset(new ImageBuffer(myshader_uuid, jsn));
        buf->allocate(size * bytes_per_pixel);
        buf->set_shader(ppm_shader);
        buf->set_image_dimensions(Imath::V2i(width, height));

        byte *buffer = buf->buffer();
        inp.read((char *)buffer, size * bytes_per_pixel);
        inp.close();

    } else {
        throw media_unreadable_error("Unable to open " + to_string(mptr.uri()));
    }

    return buf;
}

MRCertainty PPMMediaReader::supported(const caf::uri &, const std::array<uint8_t, 16> &sig) {

    // we ignore the signature..
    // we cover so MANY...
    // but we're pretty good at movs..
    if (sig[0] == 'P' and sig[1] == '6' and sig[2] == '\n') {
        return MRC_FULLY;
    }

    return MRC_NO;
}

utility::Uuid PPMMediaReader::plugin_uuid() const { return s_plugin_uuid; }


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>({std::make_shared<
            MediaReaderPlugin<MediaReaderActor<PPMMediaReader>>>(
            s_plugin_uuid, "PPM", "xStudio", "PPM Media Reader", semver::version("1.0.0"))}));
}
}
