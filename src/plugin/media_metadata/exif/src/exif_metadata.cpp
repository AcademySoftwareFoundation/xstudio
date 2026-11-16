#include "exif_metadata.hpp"

#include <filesystem>
#include <unordered_set>
#include <iostream>
#include <regex>

#include <exiv2/exiv2.hpp>

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/helpers.hpp"

namespace fs = std::filesystem;

using namespace xstudio::media_metadata;
using namespace xstudio::utility;
using namespace xstudio;

EXIFMediaMetadata::EXIFMediaMetadata(const utility::JsonStore &prefs)
    : MediaMetadata("EXIF", prefs) {
    update_preferences(prefs);
}

void EXIFMediaMetadata::update_preferences(const utility::JsonStore &prefs) {
    try {
        supported_ = global_store::preference_value<JsonStore>(
            prefs, "/plugin/media_metadata/EXIF/supported");
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

MMCertainty EXIFMediaMetadata::supported(
    const caf::uri &uri, const std::array<uint8_t, 16> &signature) {
    auto result = MMC_NO;

    // Step 2: Convert the URI to a POSIX path string and fs::path
    std::string path = uri_to_posix_path(uri);
    fs::path p(path);

    // Step 3: Check if the file exists and is a regular file
    // Return not supported if the file does not exist or is not a regular file
    if (fs::exists(p) and fs::is_regular_file(p)) {

        // Step 4: Get the upper-case extension (handling platform differences)
#ifdef _WIN32
        std::string ext = ltrim_char(to_upper_path(p.extension()), '.');
#else
        std::string ext = ltrim_char(to_upper(p.extension().string()), '.');
#endif
        // Step 5: Check if the extension is in the supported list
        // Return fully supported if the extension is in the supported list
        try {
            auto value = supported_.value(ext, "");
            if (value.empty()) {
                if (MMCertainty possible = from_string(supported_.value("", "MMC_NO"));
                    possible != MMC_NO) {
                    // Step 6: Try to detect via OIIO if the extension is supported
                    // Return maybe supported if the extension is supported by OIIO
                    try {
                        auto image = Exiv2::ImageFactory::open(path);
                        image->readMetadata();
                        result = possible;
                    } catch(...) {}
                }
            } else
                result = from_string(value);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    // Step 7: Return not supported if all checks fail
    return result;
}


nlohmann::json EXIFMediaMetadata::read_metadata(const caf::uri &uri) {
    std::string path = uri_to_posix_path(uri);

    try {
        // open file to read..
        auto image = Exiv2::ImageFactory::open(path);
        image->readMetadata();

        auto jsn = R"({})"_json;

        jsn["mimeType"] = image->mimeType ();
        jsn["pixelWidth"] = image->pixelWidth();
        jsn["pixelHeight"] = image->pixelHeight();
        jsn["comment"] = image->comment();

        Exiv2::ExifData& exifData = image->exifData();
        if (exifData.empty()) {
            spdlog::debug("{} : No Exif data found in file", __PRETTY_FUNCTION__);
        } else {
            auto end = exifData.end();
            for (auto i = exifData.begin(); i != end; ++i) {
              // if (!bLint) {
                const auto parts = utility::split(i->key(), '.');
                auto ptr = json::json_pointer();
                for(const auto &p: parts) {
                    ptr /= p;
                    if(not jsn.contains(ptr))
                        jsn[ptr] = R"({})"_json;
                }

                const char* tn = i->typeName();

                jsn[ptr]["value"] = i->toString();
                jsn[ptr]["count"] = i->count();
                jsn[ptr]["type"] = (tn ? tn : "Unknown");
            }
        }

        Exiv2::IptcData &iptcData = image->iptcData();
        if (iptcData.empty()) {
            spdlog::debug("{} : No IPTC data found in file", __PRETTY_FUNCTION__);
        } else {
            Exiv2::IptcData::iterator end = iptcData.end();
            for (Exiv2::IptcData::iterator i = iptcData.begin(); i != end; ++i) {
                const auto parts = utility::split(i->key(), '.');
                auto ptr = json::json_pointer();
                for(const auto &p: parts) {
                    ptr /= p;
                    if(not jsn.contains(ptr))
                        jsn[ptr] = R"({})"_json;
                }
                const char* tn = i->typeName();

                jsn[ptr]["value"] = i->toString();
                jsn[ptr]["count"] = i->count();
                jsn[ptr]["type"] = (tn ? tn : "Unknown");
            }
        }

        return jsn;
    } catch (const std::exception &e) {
        spdlog::error("Failed to read metadata from {}: {}", path, e.what());
        return nlohmann::json::object();
    }
}

  // "Exif": {
  //   "Image": {
  //     "FrameRate": {
  //       "count": 1,
  //       "type": "SRational",
  //       "value": "30/1000"
  //     }
  //   }
  // },
  // "comment": "",
  // "mimeType": "image/jpeg",
  // "pixelHeight": 1080,
  // "pixelWidth": 1920


std::optional<MediaMetadata::StandardFields>
EXIFMediaMetadata::fill_standard_fields(const nlohmann::json &metadata) {
    // Step 1: Initialize StandardFields and set default format
    StandardFields fields;

    // spdlog::info("{}", metadata.dump(2));

    fields.format_ = "EXIF";

    // Step 2: Fill in the resolution if present in metadata
    if (metadata.contains("pixelWidth") and metadata.contains("pixelHeight")) {
        fields.resolution_ = std::to_string(metadata.at("pixelWidth").get<int>()) + " x " + std::to_string(metadata.at("pixelHeight").get<int>());
    }

    // Step 3: Fill in the image extension (format) if present in metadata
    if (metadata.contains("mimeType")) {
        fields.format_ = metadata.at("mimeType").get<std::string>();
    }

    fields.bit_depth_ = "8 bits";
    fields.pixel_aspect_ = 1.0;

    return std::make_optional(fields);
}

// Point d'entrée du plugin
extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>({std::make_shared<
            MediaMetadataPlugin<MediaMetadataActor<EXIFMediaMetadata>>>(
            Uuid("9e741b99-99b5-4ec5-9df5-37d027df8f49"),
            "EXIF",
            "xStudio",
            "EXIF Media Metadata Reader",
            semver::version("1.0.0"))}));
}
}
