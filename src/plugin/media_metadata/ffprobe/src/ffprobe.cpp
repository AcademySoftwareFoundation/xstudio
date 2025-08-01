// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <chrono>

#include "xstudio/media_metadata/media_metadata.hpp"
#include "xstudio/utility/helpers.hpp"
#include "ffprobe_lib.hpp"

namespace fs = std::filesystem;


using namespace xstudio::media_metadata;
using namespace xstudio::utility;
using namespace xstudio;
using namespace xstudio::ffprobe;

class FFProbeMediaMetadata : public MediaMetadata {
  public:
    FFProbeMediaMetadata() : MediaMetadata("FFProbe") {}
    ~FFProbeMediaMetadata() override = default;
    MMCertainty
    supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

  protected:
    nlohmann::json read_metadata(const caf::uri &uri) override;
    std::optional<StandardFields> fill_standard_fields(const nlohmann::json &metadata) override;

  private:
    FFProbe probe_;
};

nlohmann::json FFProbeMediaMetadata::read_metadata(const caf::uri &uri) {
    return probe_.probe_file(uri);
}

MMCertainty
FFProbeMediaMetadata::supported(const caf::uri &uri, const std::array<uint8_t, 16> &) {
    // we ignore the signature..
    // we cover so MANY...
    // but we're pretty good at movs..
    if (fs::path(uri.path().data()).extension() == ".mov")
        return MMC_YES;

    return MMC_MAYBE;
}

std::optional<MediaMetadata::StandardFields>
FFProbeMediaMetadata::fill_standard_fields(const nlohmann::json &metadata) {

    if (metadata.contains("streams") && metadata["streams"].is_array()) {

        StandardFields fields;

        auto p = metadata["streams"].begin();

        while (p != metadata["streams"].end()) {
            auto h = (*p);
            if (h.contains("codec_type") && h["codec_type"].get<std::string>() == "video") {
                break;
            }
            p++;
        }
        if (p == metadata["streams"].end())
            return {};

        auto h = (*p);

        if (h.contains("codec_name")) {
            fields.format_ = h["codec_name"].get<std::string>();
        }
        if (h.contains("width") && h.contains("height")) {
            const int x        = h["width"].get<int>();
            const int y        = h["height"].get<int>();
            fields.resolution_ = fmt::format("{} x {}", x, y);
        }
        if (h.contains("pix_fmt")) {

            std::cmatch m;
            const std::regex bitdepth("(f?)([0-9]+)(le|be)");
            auto pix_fmt = h["pix_fmt"].get<std::string>();
            if (std::regex_search(pix_fmt.c_str(), m, bitdepth)) {
                if (m[1].str() == "f") {
                    fields.bit_depth_ = m[2].str() + " bit float";
                } else {
                    fields.bit_depth_ = m[2].str() + " bits";
                }
            } else {
                fields.bit_depth_ = "8 bits";
            }
        }

        if (h.contains("sample_aspect_ratio") && h["sample_aspect_ratio"].is_string()) {

            std::cmatch m;
            const std::regex aspect("([0-9]+)\\:([0-9]+)");

            auto aspect_ratio = h["sample_aspect_ratio"].get<std::string>();
            if (std::regex_search(aspect_ratio.c_str(), m, aspect)) {
                try {
                    double num           = std::stod(m[1].str());
                    double den           = std::stod(m[2].str());
                    fields.pixel_aspect_ = num / den;
                } catch (...) {
                }
            }
        } else {
            fields.pixel_aspect_ = 1.0f;
        }
        return fields;
    }

    return {};
}


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaMetadataPlugin<MediaMetadataActor<FFProbeMediaMetadata>>>(
                Uuid("f5e270f5-2fa7-4122-8ed4-90ea391894b7"),
                "FFProbe",
                "xStudio",
                "FFProbe Media Metadata",
                semver::version("1.0.0"))}));
}
}
