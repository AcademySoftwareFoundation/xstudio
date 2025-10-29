#include <OpenImageIO/typedesc.h>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>

#include "openimageio.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

namespace fs = std::filesystem;

using namespace xstudio::media_reader;
using namespace xstudio::utility;
using namespace xstudio;

namespace {
// Unique UUID for this plugin
static Uuid s_plugin_uuid{"a1b2c3d4-e5f6-7890-abcd-ef1234567890"};

// UUID for the shader
static Uuid myshader_uuid{"b1c2d3e4-f5a6-7b98-cdef-123456789abc"};

// Channel name definitions
static const std::set<std::string> RED_CHANNEL_NAMES       = {"r", "red"};
static const std::set<std::string> GREEN_CHANNEL_NAMES     = {"g", "green"};
static const std::set<std::string> BLUE_CHANNEL_NAMES      = {"b", "blue"};
static const std::set<std::string> ALPHA_CHANNEL_NAMES     = {"a", "alpha"};
static const std::set<std::string> LUMINANCE_CHANNEL_NAMES = {
    "y", "luminance", "l", "gray", "grey", "mono"};

/**
 * @brief Specifies the type of image based on its channel composition.
 *
 * - IMAGE_GRAYSCALE:        An image containing a single luminance (Y) channel.
 * - IMAGE_GRAYSCALE_ALPHA:  An image containing a luminance (Y) channel plus an alpha channel
 * (Y+A).
 * - IMAGE_RGB:              An image containing three color channels: Red, Green, and Blue.
 * - IMAGE_RGBA:             An image containing three color channels (Red, Green, Blue) plus an
 * alpha channel (RGBA).
 */
enum ImageType {
    IMAGE_GRAYSCALE,       ///< 1 channel - luminance (Y) only
    IMAGE_GRAYSCALE_ALPHA, ///< 2 channels - luminance (Y) and alpha (A)
    IMAGE_RGB,             ///< 3 channels - red, green, blue (RGB)
    IMAGE_RGBA,            ///< 4 channels - red, green, blue, alpha (RGBA)
};

/**
 * @brief Channel identifiers for planar image layout.
 */
enum class Channel { RED, GREEN, BLUE, ALPHA };

// Supports: 8-bit, 16-bit UINT, 16-bit HALF, float32
static std::string oiio_shader_code{R"(
#version 410 core
uniform int width;
uniform int height;
uniform int bytes_per_channel;  // 1=8bit, 2=16bit, 4=float
uniform int is_half_float;      // 0=UINT16, 1=HALF float
uniform int has_alpha;          // 0=no alpha, 1=has alpha
uniform int channel_r_start;
uniform int channel_g_start;
uniform int channel_b_start;
uniform int channel_a_start;

// Forward declarations
int get_image_data_1byte(int byte_address);
int get_image_data_2bytes(int byte_address);
vec2 get_image_data_2floats(int byte_address);
float get_image_data_float32(int byte_address);

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
    
    int r_coord = (image_coord.x + image_coord.y * width) * bytes_per_channel + channel_r_start;
    int g_coord = (image_coord.x + image_coord.y * width) * bytes_per_channel + channel_g_start;
    int b_coord = (image_coord.x + image_coord.y * width) * bytes_per_channel + channel_b_start;
    int a_coord = (image_coord.x + image_coord.y * width) * bytes_per_channel + channel_a_start;

    if (bytes_per_channel == 1) {
        R = get_image_data_1byte(r_coord) / 255.0;
        G = get_image_data_1byte(g_coord) / 255.0;
        B = get_image_data_1byte(b_coord) / 255.0;
        if (has_alpha == 1) A = get_image_data_1byte(a_coord) / 255.0;
    }
    else if (bytes_per_channel == 2) {
        if (is_half_float == 1) {
            R = get_image_data_2floats(r_coord).x;
            G = get_image_data_2floats(g_coord).x;
            B = get_image_data_2floats(b_coord).x;
            if (has_alpha == 1) A = get_image_data_2floats(a_coord).x;
        }
        else {
            R = get_image_data_2bytes(r_coord) / 65535.0;
            G = get_image_data_2bytes(g_coord) / 65535.0;
            B = get_image_data_2bytes(b_coord) / 65535.0;
            if (has_alpha == 1) A = get_image_data_2bytes(a_coord) / 65535.0;
        }
    }
    else if (bytes_per_channel == 4) {
        R = get_image_data_float32(r_coord);
        G = get_image_data_float32(g_coord);
        B = get_image_data_float32(b_coord);
        if (has_alpha == 1) A = get_image_data_float32(a_coord);
    }

    return vec4(R, G, B, A);
}
)"};

static ui::viewport::GPUShaderPtr
    oiio_shader(new ui::opengl::OpenGLShader(myshader_uuid, oiio_shader_code));

} // namespace

OIIOMediaReader::OIIOMediaReader(const utility::JsonStore &prefs) : MediaReader("OIIO", prefs) {
    update_preferences(prefs);
}

void OIIOMediaReader::update_preferences(const utility::JsonStore &prefs) {
    try {
        // Set OIIO threading
        int threads =
            global_store::preference_value<int>(prefs, "/plugin/media_reader/OIIO/threads");
        OIIO::attribute("threads", threads);
    } catch (const std::exception &e) {
    }

    try {
        // Set OIIO EXR-specific threading
        int exr_threads =
            global_store::preference_value<int>(prefs, "/plugin/media_reader/OIIO/exr_threads");
        OIIO::attribute("exr_threads", exr_threads);
    } catch (const std::exception &e) {
    }

    try {
        // Enable or disable TBB in OIIO
        int use_tbb =
            global_store::preference_value<int>(prefs, "/plugin/media_reader/OIIO/use_tbb");
        OIIO::attribute("use_tbb", use_tbb);
    } catch (const std::exception &e) {
    }

    try {
        // Set OIIO log times preference
        // When the "log_times" attribute is nonzero, ImageBufAlgo functions are instrumented to
        // record the number of times they were called and the total amount of time spent
        // executing them.
        int log_times =
            global_store::preference_value<int>(prefs, "/plugin/media_reader/OIIO/log_times");
        OIIO::attribute("log_times", log_times);
    } catch (const std::exception &e) {
    }
}

utility::Uuid OIIOMediaReader::plugin_uuid() const { return s_plugin_uuid; }

/**
 * @brief Detects the image type (grayscale, RGB, etc.) from the given OIIO::ImageSpec.
 *
 * This function inspects the channel names of the provided ImageSpec to determine the
 * image type. It checks for recognized channel names corresponding to RGB, alpha,
 * and grayscale channels. If channel names are ambiguous, it falls back to the number
 * of channels to decide the type.
 *
 * @param spec The OpenImageIO ImageSpec describing the image's channel layout.
 * @return ImageType Enum indicating the detected image type.
 *         Possible values: IMAGE_GRAYSCALE, IMAGE_GRAYSCALE_ALPHA, IMAGE_RGB, IMAGE_RGBA
 */
ImageType detect_image_type(const OIIO::ImageSpec &spec) {
    static const std::set<std::string> rgb_names = []() {
        std::set<std::string> merged = RED_CHANNEL_NAMES;
        merged.insert(GREEN_CHANNEL_NAMES.begin(), GREEN_CHANNEL_NAMES.end());
        merged.insert(BLUE_CHANNEL_NAMES.begin(), BLUE_CHANNEL_NAMES.end());
        return merged;
    }();
    static const std::set<std::string> alpha_names = ALPHA_CHANNEL_NAMES;
    static const std::set<std::string> gray_names  = LUMINANCE_CHANNEL_NAMES;

    int rgb_count   = 0;
    int alpha_count = 0;
    int gray_count  = 0;

    // Iterate over each channel and classify its type according to the channel name.
    for (int i = 0; i < spec.nchannels; ++i) {
        std::string name = spec.channelnames[i];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (rgb_names.count(name)) {
            rgb_count++;
        } else if (alpha_names.count(name)) {
            alpha_count++;
        } else if (gray_names.count(name)) {
            gray_count++;
        }
    }

    // Prioritize RGB detection: if at least three RGB channels exist, it's RGB or RGBA.
    if (rgb_count >= 3) {
        return alpha_count > 0 ? IMAGE_RGBA : IMAGE_RGB;
    }

    // Handle grayscale images with or without alpha.
    if (gray_count == 1) {
        return alpha_count > 0 ? IMAGE_GRAYSCALE_ALPHA : IMAGE_GRAYSCALE;
    }

    // Fallback: deduce based purely on channel count.
    if (spec.nchannels == 1)
        return IMAGE_GRAYSCALE;
    if (spec.nchannels == 2)
        return IMAGE_GRAYSCALE_ALPHA;
    if (spec.nchannels == 3)
        return IMAGE_RGB;
    if (spec.nchannels == 4)
        return IMAGE_RGBA;

    // Default case: treat as RGB(A) depending on whether any alpha was found.
    return alpha_count > 0 ? IMAGE_RGBA : IMAGE_RGB;
}

/**
 * @brief Generic function to find a channel index by matching a set of possible names.
 *
 * Iterates through the channel names in the given ImageSpec and returns the index
 * of the first channel that matches any name in the provided set (case-insensitive).
 *
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @param channel_names Set of possible channel names to match.
 * @param default_value Default value to return if no match is found.
 * @return The index of the matching channel, or default_value if not found.
 */
int get_channel_index_by_names(
    const OIIO::ImageSpec &spec,
    const std::set<std::string> &channel_names,
    int default_value = -1) {

    for (int i = 0; i < spec.nchannels; ++i) {
        std::string name = spec.channelnames[i];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (channel_names.count(name)) {
            return i;
        }
    }

    return default_value;
}

/**
 * @brief Get the index of the red channel in the image specification.
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @return The index of the red channel, or -1 if not found.
 */
inline int get_red_channel_index(const OIIO::ImageSpec &spec) {
    return get_channel_index_by_names(spec, RED_CHANNEL_NAMES);
}

/**
 * @brief Get the index of the green channel in the image specification.
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @return The index of the green channel, or -1 if not found.
 */
inline int get_green_channel_index(const OIIO::ImageSpec &spec) {
    return get_channel_index_by_names(spec, GREEN_CHANNEL_NAMES);
}

/**
 * @brief Get the index of the blue channel in the image specification.
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @return The index of the blue channel, or -1 if not found.
 */
inline int get_blue_channel_index(const OIIO::ImageSpec &spec) {
    return get_channel_index_by_names(spec, BLUE_CHANNEL_NAMES);
}

/**
 * @brief Get the index of the alpha channel in the image specification.
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @return The index of the alpha channel, or spec.alpha_channel if not found.
 */
inline int get_alpha_channel_index(const OIIO::ImageSpec &spec) {
    return get_channel_index_by_names(spec, ALPHA_CHANNEL_NAMES, spec.alpha_channel);
}

/**
 * @brief Get the index of the luminance (grayscale) channel in the image specification.
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @return The index of the luminance channel, or -1 if not found.
 */
inline int get_luminance_channel_index(const OIIO::ImageSpec &spec) {
    return get_channel_index_by_names(spec, LUMINANCE_CHANNEL_NAMES);
}

/**
 * @brief Get the indices of the channels in the image specification.
 *
 * This function returns a map of channel names to their indices in the image specification.
 * The channel names are represented by a single character: 'R' for red, 'G' for green, 'B' for
 * blue, 'A' for alpha, 'Y' for luminance. The indices are the positions of the channels in the
 * image specification. If a channel is not found, the index is set to -1.
 *
 * @param spec The OpenImageIO ImageSpec describing the image.
 * @param image_type The type of the image.
 * @return A map of channel names to their indices.
 */
std::map<char, int>
get_channel_indices(const OIIO::ImageSpec &spec, const ImageType image_type) {
    if (image_type == IMAGE_RGBA) {
        int red_index   = get_red_channel_index(spec);
        int green_index = get_green_channel_index(spec);
        int blue_index  = get_blue_channel_index(spec);
        int alpha_index = get_alpha_channel_index(spec);

        return {
            {'R', red_index != -1 ? red_index : 0},
            {'G', green_index != -1 ? green_index : 1},
            {'B', blue_index != -1 ? blue_index : 2},
            {'A', alpha_index != -1 ? alpha_index : 3}};
    } else if (image_type == IMAGE_RGB) {
        int red_index   = get_red_channel_index(spec);
        int green_index = get_green_channel_index(spec);
        int blue_index  = get_blue_channel_index(spec);

        return {
            {'R', red_index != -1 ? red_index : 0},
            {'G', green_index != -1 ? green_index : 1},
            {'B', blue_index != -1 ? blue_index : 2}};
    } else if (image_type == IMAGE_GRAYSCALE) {
        int luminance_index = get_luminance_channel_index(spec);

        return {{'Y', luminance_index != -1 ? luminance_index : 0}};
    } else if (image_type == IMAGE_GRAYSCALE_ALPHA) {
        int luminance_index = get_luminance_channel_index(spec);
        int alpha_index     = get_alpha_channel_index(spec);

        return {
            {'Y', luminance_index != -1 ? luminance_index : 0},
            {'A', alpha_index != -1 ? alpha_index : 1}};
    } else {
        throw media_unreadable_error("Unsupported image type: " + std::to_string(image_type));
    }
}

/**
 * @brief Get the number of channels in the image.
 *
 * Returns the number of channels in the image based on the image type.
 * The number of channels is 1 for grayscale images, 2 for grayscale+alpha images, 3 for RGB
 * images, and 4 for RGBA images. If the image type is not supported, throws an error.
 *
 * @param image_type The type of the image.
 * @return The number of channels in the image.
 */
size_t get_rendered_image_channel_count(const ImageType image_type) {
    if (image_type == IMAGE_RGBA) {
        return 4;
    } else if (image_type == IMAGE_RGB) {
        return 3;
    } else if (image_type == IMAGE_GRAYSCALE_ALPHA) {
        return 2;
    } else if (image_type == IMAGE_GRAYSCALE) {
        return 1;
    } else {
        throw media_unreadable_error("Unsupported image type: " + std::to_string(image_type));
    }
}

/**
 * @brief Get the number of bytes per channel in the image.
 *
 * Returns the number of bytes per channel in the image based on the format.
 * The number of bytes per channel is 1 for UINT8, 2 for UINT16, 4 for FLOAT, and 2 for HALF.
 * If the format is not supported, throws an error.
 *
 * @param format The format of the image.
 * @return The number of bytes per channel in the image.
 */
int get_rendered_image_bytes_per_channel(const OIIO::TypeDesc format) {
    if (format == OIIO::TypeDesc::UINT8) {
        return 1;
    } else if (format == OIIO::TypeDesc::UINT16) {
        return 2;
    } else if (format == OIIO::TypeDesc::HALF) {
        return 2;
    } else if (format == OIIO::TypeDesc::FLOAT) {
        return 4;
    } else {
        throw media_unreadable_error("Unsupported format: " + std::string(format.c_str()));
    }
}

/**
 * @brief Get whether the image is half float.
 *
 * Returns whether the image is half float based on the format.
 * The image is half float if the format is HALF.
 * If the format is not supported, returns 0.
 *
 * @param format The format of the image.
 * @return Whether the image is half float.
 */
int get_is_half_float(const OIIO::TypeDesc format) {
    if (format == OIIO::TypeDesc::HALF) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * @brief Get the rendered format of the image.
 *
 * Returns the rendered format of the image based on the format.
 * The rendered format is the format of the image that will be used to render the image.
 * The rendered format is UINT8 for UINT8, UINT16 for UINT16, HALF for HALF, and FLOAT for
 * FLOAT. If the format is not supported, returns UINT8.
 *
 * @param format The format of the image.
 * @return The rendered format of the image.
 */
OIIO::TypeDesc get_rendered_image_format(const OIIO::TypeDesc format) {
    if (format == OIIO::TypeDesc::UINT8) {
        return OIIO::TypeDesc::UINT8;
    } else if (format == OIIO::TypeDesc::UINT16) {
        return OIIO::TypeDesc::UINT16;
    } else if (format == OIIO::TypeDesc::HALF) {
        return OIIO::TypeDesc::HALF;
    } else if (format == OIIO::TypeDesc::FLOAT) {
        return OIIO::TypeDesc::FLOAT;
    } else {
        return OIIO::TypeDesc::UINT8;
    }
}

/**
 * @brief Generic function to get the start offset of a channel in a planar image buffer.
 *
 * For RGB/RGBA images, the layout is: [R plane][G plane][B plane][A plane]
 * For grayscale images, R/G/B all point to plane 0 (Y), and A points to plane 1.
 *
 * @param channel The channel to get the offset for.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param bytes_per_channel Bytes per channel (1, 2, or 4).
 * @param image_type The type of the image.
 * @return The byte offset from the start of the buffer for this channel.
 */
size_t get_rendered_image_channel_start(
    Channel channel,
    const size_t width,
    const size_t height,
    const int bytes_per_channel,
    const ImageType image_type) {

    const size_t plane_size = width * height * bytes_per_channel;

    // For grayscale images, R/G/B all map to Y (plane 0)
    if (image_type == IMAGE_GRAYSCALE || image_type == IMAGE_GRAYSCALE_ALPHA) {
        if (channel == Channel::ALPHA) {
            return plane_size; // Alpha is in plane 1 for grayscale+alpha
        }
        return 0; // R, G, B all point to Y channel (plane 0)
    }

    // For RGB/RGBA images
    if (image_type == IMAGE_RGBA || image_type == IMAGE_RGB) {
        switch (channel) {
        case Channel::RED:
            return 0;
        case Channel::GREEN:
            return plane_size;
        case Channel::BLUE:
            return plane_size * 2;
        case Channel::ALPHA:
            return plane_size * 3;
        }
    }

    throw media_unreadable_error("Unsupported image type: " + std::to_string(image_type));
}

/**
 * @brief Returns the starting byte offset in the planar image buffer for the Red channel.
 *
 * This is a convenience wrapper around get_rendered_image_channel_start for improved
 * readability and backward compatibility.
 *
 * @param width               Width of the image in pixels.
 * @param height              Height of the image in pixels.
 * @param bytes_per_channel   Number of bytes used to represent each channel (e.g., 1 for uint8,
 * 2 for uint16, 4 for float).
 * @param image_type          The detected type of the image (e.g., IMAGE_RGB, IMAGE_RGBA,
 * IMAGE_GRAYSCALE, etc.).
 * @return                    Byte offset for the start of the Red channel plane in the packed
 * buffer.
 */
inline size_t get_rendered_image_channel_red_start(
    const size_t width,
    const size_t height,
    const int bytes_per_channel,
    const ImageType image_type) {
    return get_rendered_image_channel_start(
        Channel::RED, width, height, bytes_per_channel, image_type);
}

/**
 * @brief Returns the starting byte offset in the planar image buffer for the Green channel.
 *
 * This is a convenience wrapper around get_rendered_image_channel_start for improved
 * readability and backward compatibility.
 *
 * @param width               Width of the image in pixels.
 * @param height              Height of the image in pixels.
 * @param bytes_per_channel   Number of bytes used to represent each channel (e.g., 1 for uint8,
 * 2 for uint16, 4 for float).
 * @param image_type          The detected type of the image (e.g., IMAGE_RGB, IMAGE_RGBA,
 * IMAGE_GRAYSCALE, etc.).
 * @return                    Byte offset for the start of the Green channel plane in the packed
 * buffer.
 */
inline size_t get_rendered_image_channel_green_start(
    const size_t width,
    const size_t height,
    const int bytes_per_channel,
    const ImageType image_type) {
    return get_rendered_image_channel_start(
        Channel::GREEN, width, height, bytes_per_channel, image_type);
}

/**
 * @brief Returns the starting byte offset in the planar image buffer for the Blue channel.
 *
 * This is a convenience wrapper around get_rendered_image_channel_start for improved
 * readability and backward compatibility.
 *
 * @param width               Width of the image in pixels.
 * @param height              Height of the image in pixels.
 * @param bytes_per_channel   Number of bytes used to represent each channel (e.g., 1 for uint8,
 * 2 for uint16, 4 for float).
 * @param image_type          The detected type of the image (e.g., IMAGE_RGB, IMAGE_RGBA,
 * IMAGE_GRAYSCALE, etc.).
 * @return                    Byte offset for the start of the Blue channel plane in the packed
 * buffer.
 */
inline size_t get_rendered_image_channel_blue_start(
    const size_t width,
    const size_t height,
    const int bytes_per_channel,
    const ImageType image_type) {
    return get_rendered_image_channel_start(
        Channel::BLUE, width, height, bytes_per_channel, image_type);
}

/**
 * @brief Returns the starting byte offset in the planar image buffer for the Alpha channel.
 *
 * This is a convenience wrapper around get_rendered_image_channel_start for improved
 * readability and backward compatibility.
 *
 * @param width               Width of the image in pixels.
 * @param height              Height of the image in pixels.
 * @param bytes_per_channel   Number of bytes used to represent each channel (e.g., 1 for uint8,
 * 2 for uint16, 4 for float).
 * @param image_type          The detected type of the image (e.g., IMAGE_RGB, IMAGE_RGBA,
 * IMAGE_GRAYSCALE, etc.).
 * @return                    Byte offset for the start of the Alpha channel plane in the packed
 * buffer.
 */
inline size_t get_rendered_image_channel_alpha_start(
    const size_t width,
    const size_t height,
    const int bytes_per_channel,
    const ImageType image_type) {
    return get_rendered_image_channel_start(
        Channel::ALPHA, width, height, bytes_per_channel, image_type);
}

/**
 * @brief Reads image channels from the given OIIO::ImageInput and fills the output pixel
 * buffer.
 *
 * This function copies image channel data in planar format into the target buffer. It supports
 * images of type RGBA, RGB, GRAYSCALE, and GRAYSCALE+ALPHA. The function uses the provided
 * channel indices map to determine the correct input channel numbers for each color component.
 * Data is read in the provided rendered format and written into the output buffer, offset for
 * each channel as appropriate.
 *
 * @param image             Pointer to an open OIIO::ImageInput used to read channel data.
 * @param image_type        The detected type of the image (RGBA, RGB, GRAYSCALE, etc).
 * @param channel_indices   Map of channel letters ('R', 'G', 'B', 'A', 'Y') to their
 * corresponding channel indices.
 * @param rendered_format   The desired OIIO::TypeDesc of the output data (usually matches
 * buffer format).
 * @param buffer            Output pointer to the buffer to be filled with planar channel data.
 *                          Layout is assumed to be [R][G][B][A], channel-major.
 *                          Each channel plane is of size (spec.width * spec.height *
 * bytes_per_channel). For grayscale, the 'Y' channel is filled in the R component's plane. For
 * RGB and RGBA: interleaved in planar major order: R plane, then G, B, (then A).
 */
void fill_rendered_image(
    OIIO::ImageInput *image,
    const ImageType image_type,
    const std::map<char, int> &channel_indices,
    const OIIO::TypeDesc &rendered_format,
    byte *buffer,
    size_t width,
    size_t height,
    int bytes_per_channel) {
    size_t plane_size = width * height * bytes_per_channel;

    // RGB or RGBA (3 or 4-channel) images: fill R, G, B
    if (image_type == IMAGE_RGBA || image_type == IMAGE_RGB) {
        // Read R plane
        image->read_image(
            0,
            0,
            channel_indices.at('R'),
            channel_indices.at('R') + 1,
            rendered_format,
            &buffer[plane_size * 0]);
        // Read G plane
        image->read_image(
            0,
            0,
            channel_indices.at('G'),
            channel_indices.at('G') + 1,
            rendered_format,
            &buffer[plane_size * 1]);
        // Read B plane
        image->read_image(
            0,
            0,
            channel_indices.at('B'),
            channel_indices.at('B') + 1,
            rendered_format,
            &buffer[plane_size * 2]);
    }
    // Grayscale or Grayscale+Alpha images: fill Y
    else if (image_type == IMAGE_GRAYSCALE || image_type == IMAGE_GRAYSCALE_ALPHA) {
        image->read_image(
            0,
            0,
            channel_indices.at('Y'),
            channel_indices.at('Y') + 1,
            rendered_format,
            &buffer[plane_size * 0]);
    }

    // If Grayscale+Alpha, images: fill A
    if (image_type == IMAGE_GRAYSCALE_ALPHA) {
        image->read_image(
            0,
            0,
            channel_indices.at('A'),
            channel_indices.at('A') + 1,
            rendered_format,
            &buffer[plane_size * 1]);
    }
    // If RGBA, images: fill A
    else if (image_type == IMAGE_RGBA) {
        image->read_image(
            0,
            0,
            channel_indices.at('A'),
            channel_indices.at('A') + 1,
            rendered_format,
            &buffer[plane_size * 3]);
    }
}

ImageBufPtr OIIOMediaReader::image(const media::AVFrameID &mptr) {
    ImageBufPtr buf;

    try {
        // Step 1: Convert the URI to a POSIX path
        std::string path = uri_to_posix_path(mptr.uri());

        // Step 2: Open the image using OpenImageIO
        auto image = OIIO::ImageInput::open(path);
        if (!image) {
            throw media_corrupt_error("OIIO error: " + OIIO::geterror());
        }

        // Step 3: Retrieve the image specification
        const OIIO::ImageSpec &spec = image->spec();

        // Step 4: Extract image dimensions
        size_t width  = spec.width;
        size_t height = spec.height;

        // Step 5: Detect the image type (RGB, RGBA, Grayscale, Grayscale+Alpha)
        ImageType image_type = detect_image_type(spec);

        // Step 6: Get channel indices map (e.g., 'R', 'G', 'B', 'A', or 'Y')
        std::map<char, int> channel_indices = get_channel_indices(spec, image_type);

        // Step 7: Determine channel and pixel information for buffer allocation
        size_t num_channels            = get_rendered_image_channel_count(image_type);
        OIIO::TypeDesc rendered_format = get_rendered_image_format(spec.format);
        int bytes_per_channel          = get_rendered_image_bytes_per_channel(rendered_format);
        int is_half_float              = get_is_half_float(spec.format);

        // Step 8: Compute the channel offsets for the shader
        size_t channel_r_start =
            get_rendered_image_channel_red_start(width, height, bytes_per_channel, image_type);
        size_t channel_g_start = get_rendered_image_channel_green_start(
            width, height, bytes_per_channel, image_type);
        size_t channel_b_start =
            get_rendered_image_channel_blue_start(width, height, bytes_per_channel, image_type);
        size_t channel_a_start = get_rendered_image_channel_alpha_start(
            width, height, bytes_per_channel, image_type);

        // Step 9: Calculate total number of pixels and bytes per pixel
        size_t pixel_count  = spec.width * spec.height;
        int bytes_per_pixel = bytes_per_channel * num_channels;

        // Step 10: Prepare JSON input parameters for the shader program
        JsonStore jsn;
        jsn["width"]             = width;
        jsn["height"]            = height;
        jsn["bytes_per_channel"] = bytes_per_channel;
        jsn["is_half_float"]     = is_half_float;
        jsn["has_alpha"] = image_type == IMAGE_RGBA || image_type == IMAGE_GRAYSCALE_ALPHA;
        jsn["channel_r_start"] = channel_r_start;
        jsn["channel_g_start"] = channel_g_start;
        jsn["channel_b_start"] = channel_b_start;
        jsn["channel_a_start"] = channel_a_start;

        // Step 11: Allocate and configure the image buffer
        buf.reset(new ImageBuffer(myshader_uuid, jsn));
        buf->allocate(pixel_count * bytes_per_pixel);
        buf->set_shader(oiio_shader);
        buf->set_image_dimensions(Imath::V2i(width, height));

        // Step 12: Read the image and fill the buffer in planar format
        fill_rendered_image(
            image.get(),
            image_type,
            channel_indices,
            rendered_format,
            buf->buffer(),
            width,
            height,
            bytes_per_channel);

        // Step 13: Close the image file
        image->close();

    } catch (const std::exception &e) {
        throw media_unreadable_error(
            "Unable to read with OIIO: " + std::string(e.what()) + " (" +
            to_string(mptr.uri()) + ")");
    }

    return buf;
}

thumbnail::ThumbnailBufferPtr
OIIOMediaReader::thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) {

    try {

        // Step 1: Convert uri to POSIX file path.
        std::string path = uri_to_posix_path(mpr.uri());

        // Step 2: Open the image buffer.
        OIIO::ImageBuf imagebuf(path);
        if (imagebuf.has_error()) {
            throw media_corrupt_error("OIIO error: " + imagebuf.geterror());
        }

        // Step 3: Query image spec and dimensions.
        const OIIO::ImageSpec &spec = imagebuf.spec();
        int width                   = spec.width;
        int height                  = spec.height;

        // Step 4: Compute target dimensions for thumbnail, preserving aspect ratio.
        float aspect     = static_cast<float>(width) / static_cast<float>(height);
        int thumb_width  = thumb_size;
        int thumb_height = thumb_size;

        if (aspect > 1.0f) {
            // Wider than tall, constrain height.
            thumb_height = static_cast<int>(thumb_size / aspect);
        } else {
            // Taller than wide, constrain width.
            thumb_width = static_cast<int>(thumb_size * aspect);
        }

        // Step 5: Resize the image to thumbnail dimensions.
        OIIO::ImageBuf resized = OIIO::ImageBufAlgo::resize(
            imagebuf, "", 0, OIIO::ROI(0, thumb_width, 0, thumb_height));
        if (resized.has_error()) {
            throw std::runtime_error("OIIO resize error: " + resized.geterror());
        }

        // Step 6: Detect image type and get channel indices.
        ImageType image_type                = detect_image_type(spec);
        std::map<char, int> channel_indices = get_channel_indices(spec, image_type);

        // Step 7: Convert the image to a 3-channel RGB image buffer.
        OIIO::ImageBuf rgb_buf;

        if (image_type == IMAGE_RGBA || image_type == IMAGE_RGB) {
            int red_channel_index =
                get_red_channel_index(spec) != -1 ? get_red_channel_index(spec) : 0;
            int green_channel_index =
                get_green_channel_index(spec) != -1 ? get_green_channel_index(spec) : 1;
            int blue_channel_index =
                get_blue_channel_index(spec) != -1 ? get_blue_channel_index(spec) : 2;

            rgb_buf = OIIO::ImageBufAlgo::channels(
                resized, 3, {red_channel_index, green_channel_index, blue_channel_index});
        } else if (image_type == IMAGE_GRAYSCALE || image_type == IMAGE_GRAYSCALE_ALPHA) {
            int luminance_channel_index =
                get_luminance_channel_index(spec) != -1 ? get_luminance_channel_index(spec) : 0;

            rgb_buf = OIIO::ImageBufAlgo::channels(
                resized,
                3,
                {luminance_channel_index, luminance_channel_index, luminance_channel_index});
        } else {
            throw media_unreadable_error(
                "Unsupported image type: " + std::to_string(image_type));
        }

        // Step 8: Allocate the thumbnail buffer.
        auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
            thumb_width, thumb_height, thumbnail::TF_RGB24);

        // Step 9: Copy RGB data from buffer to the thumbnail.
        OIIO::ROI roi(0, thumb_width, 0, thumb_height, 0, 1, 0, 3);
        rgb_buf.get_pixels(roi, OIIO::TypeDesc::UINT8, &(thumb->data()[0]));
        if (rgb_buf.has_error()) {
            throw std::runtime_error("OIIO pixel copy error: " + rgb_buf.geterror());
        }

        // Step 10: Return the requested thumbnail.
        return thumb;

    } catch (const std::exception &e) {
        // Return a black thumbnail on error
        auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
            thumb_size, thumb_size, thumbnail::TF_RGB24);
        std::memset(&(thumb->data()[0]), 0, thumb->size());
        return thumb;
    }
}

media::MediaDetail OIIOMediaReader::detail(const caf::uri &uri) const {
    media::MediaDetail detail;
    detail.reader_ = name();

    try {
        // Step 1: Convert the URI to a POSIX path string
        std::string path = uri_to_posix_path(uri);

        // Step 2: Open the image input using OpenImageIO
        auto in = OIIO::ImageInput::open(path);
        if (!in) {
            throw std::runtime_error("Cannot open: " + OIIO::geterror());
        }

        // Step 3: Retrieve the image specification from the opened image
        const OIIO::ImageSpec &spec = in->spec();

        // Step 4: Populate a StreamDetail structure with metadata
        media::StreamDetail stream;
        stream.name_         = "image";
        stream.media_type_   = media::MediaType::MT_IMAGE;
        stream.resolution_   = Imath::V2i(spec.width, spec.height);
        stream.pixel_aspect_ = spec.get_float_attribute("PixelAspectRatio", 1.0f);

        // Step 5: Add the stream details to the MediaDetail object
        detail.streams_.push_back(stream);

        // Step 6: Close the image input
        in->close();

    } catch (const std::exception &e) {
        spdlog::warn("OIIO detail error: {}", e.what());
    }

    return detail;
}

MRCertainty
OIIOMediaReader::supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) {

    // Step 1: List of supported extensions by OIIO
    static const std::set<std::string> supported_extensions = {
        "JPG",
        "JPEG",
        "PNG",
        "TIF",
        "TIFF",
        "TGA",
        "BMP",
        "PSD",
        "HDR",
        "DPX",
        "ACES",
        "JP2",
        "J2K",
        "WEBP",
        "EXR",
    };

    // Step 2: Convert the URI to a POSIX path string and fs::path
    std::string path = uri_to_posix_path(uri);
    fs::path p(path);

    // Step 3: Check if the file exists and is a regular file
    // Return not supported if the file does not exist or is not a regular file
    if (!fs::exists(p) || !fs::is_regular_file(p)) {
        return MRC_NO;
    }

    // Step 4: Get the upper-case extension (handling platform differences)
#ifdef _WIN32
    std::string ext = ltrim_char(to_upper_path(p.extension()), '.');
#else
    std::string ext = ltrim_char(to_upper(p.extension().string()), '.');
#endif

    // Step 5: Check if the extension is in the supported list
    // Return fully supported if the extension is in the supported list
    if (supported_extensions.count(ext)) {
        return MRC_FULLY;
    }

    // Step 6: Try to detect via OIIO if the extension is supported
    // Return maybe supported if the extension is supported by OIIO
    auto in = OIIO::ImageInput::open(path);
    if (in) {
        in->close();
        return MRC_MAYBE;
    }

    // Step 7: Return not supported if all checks fail
    return MRC_NO;
}

// Plugin entry point
extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaReaderPlugin<MediaReaderActor<OIIOMediaReader>>>(
                s_plugin_uuid,
                "OpenImageIO",
                "xStudio",
                "OpenImageIO Media Reader",
                semver::version("1.0.0"))}));
}
}
