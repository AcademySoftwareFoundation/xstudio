#include "openimageio_metadata.hpp"

#include <filesystem>
#include <unordered_set>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>

#include "xstudio/utility/helpers.hpp"

namespace fs = std::filesystem;

using namespace xstudio::media_metadata;
using namespace xstudio::utility;
using namespace xstudio;

OpenImageIOMediaMetadata::OpenImageIOMediaMetadata() : MediaMetadata("OpenImageIO") {}

MMCertainty OpenImageIOMediaMetadata::supported(
    const caf::uri &uri, const std::array<uint8_t, 16> &signature) {
    // Step 1: List of supported extensions by OIIO
    static const std::unordered_set<std::string> supported_extensions = {
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
        return MMC_NO;
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
        return MMC_FULLY;
    }

    // Step 6: Try to detect via OIIO if the extension is supported
    // Return maybe supported if the extension is supported by OIIO
    auto in = OIIO::ImageInput::open(path);
    if (in) {
        in->close();
        return MMC_MAYBE;
    }

    // Step 7: Return not supported if all checks fail
    return MMC_NO;
}

nlohmann::json OpenImageIOMediaMetadata::read_metadata(const caf::uri &uri) {
    std::string path = uri_to_posix_path(uri);

    try {
        // Step 1: Open the image using OpenImageIO
        auto in = OIIO::ImageInput::open(path);
        if (!in) {
            throw std::runtime_error("Cannot open: " + OIIO::geterror());
        }

        // Step 2: Get the image specification
        const OIIO::ImageSpec &spec = in->spec();

        // Step 3: Initialize a JSON object for metadata
        nlohmann::json metadata;

        // Step 4: Populate basic image properties (width, height, resolution)
        metadata["width"] = spec.width;
        metadata["height"] = spec.height;
        metadata["resolution"] = fmt::format("{} x {}", spec.width, spec.height);

        // Step 5: Extract and format the file extension (format)
        fs::path p(path);
#ifdef _WIN32
        std::string ext = ltrim_char(to_upper_path(p.extension()), '.');
#else
        std::string ext = ltrim_char(to_upper(p.extension().string()), '.');
#endif
        metadata["format"] = ext;
        
        // Step 6: Determine bit depth and add to metadata
        if (spec.format == OIIO::TypeDesc::UINT8) {
            metadata["bit_depth"] = "8 bits";
        } else if (spec.format == OIIO::TypeDesc::UINT16) {
            metadata["bit_depth"] = "16 bits";
        } else if (spec.format == OIIO::TypeDesc::HALF) {
            metadata["bit_depth"] = "16 bits float";
        } else if (spec.format == OIIO::TypeDesc::FLOAT) {
            metadata["bit_depth"] = "32 bits float";
        } else {
            metadata["bit_depth"] = "unknown";
        }
        
        // Step 7: Read pixel aspect ratio and aspect ratio
        metadata["pixel_aspect"] = spec.get_float_attribute("PixelAspectRatio", 1.0f);
        metadata["aspect_ratio"]  = spec.get_float_attribute("XResolution", spec.width)
                                    / spec.get_float_attribute("YResolution", spec.height);

        // Step 8: Add extra attributes to metadata
        for (const auto &param : spec.extra_attribs) {
            metadata[param.name().string()] = param.get_string();
        }

        // Step 9: Return metadata
        return metadata;
    } catch (const std::exception &e) {
        spdlog::error("Failed to read metadata from {}: {}", path, e.what());
        return nlohmann::json::object();
    }
}

std::optional<MediaMetadata::StandardFields>
OpenImageIOMediaMetadata::fill_standard_fields(const nlohmann::json &metadata) {
    // Step 1: Initialize StandardFields and set default format
    StandardFields fields;
    fields.format_ = "OpenImageIO";

    // Step 2: Fill in the resolution if present in metadata
    if (metadata.contains("resolution")) {
        fields.resolution_ = metadata["resolution"].get<std::string>();
    }

    // Step 3: Fill in the image extension (format) if present in metadata
    if (metadata.contains("format")) {
        fields.format_ = metadata["format"].get<std::string>();
    }

    // Step 4: Fill in the bit depth if present in metadata
    if (metadata.contains("bit_depth")) {
        fields.bit_depth_ = metadata["bit_depth"].get<std::string>();
    }

    // Step 5: Fill in the pixel aspect ratio if present in metadata
    if (metadata.contains("pixel_aspect")) {
        fields.pixel_aspect_ = metadata["pixel_aspect"].get<float>();
    }

    return std::make_optional(fields);
}

// Point d'entrée du plugin
extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>({std::make_shared<
            MediaMetadataPlugin<MediaMetadataActor<OpenImageIOMediaMetadata>>>(
            Uuid("8f3c4a7e-9b2d-4e1f-a5c8-3d6f7e8a9b1c"),
            "OpenImageIO",
            "xStudio",
            "OpenImageIO Media Metadata Reader",
            semver::version("1.0.0"))}));
}
}
