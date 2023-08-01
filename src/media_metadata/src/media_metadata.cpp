// SPDX-License-Identifier: Apache-2.0
// #include <filesystem>
#ifdef __linux__
#include <dlfcn.h>
#endif
#include <filesystem>

#include <fstream>
#include <iostream>

#include "xstudio/media_metadata/media_metadata.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

namespace fs = std::filesystem;


namespace xstudio::media_metadata {

MediaMetadata::MediaMetadata(std::string name) : name_(std::move(name)) {}

std::string MediaMetadata::name() const { return name_; }

nlohmann::json MediaMetadata::metadata(const caf::uri &uri) {
    nlohmann::json _metadata = read_metadata(uri);
    try {

        std::optional<StandardFields> fields = fill_standard_fields(_metadata);
        if (fields) {
            _metadata["standard_fields"]["resolution"]   = fields.value().resolution_;
            _metadata["standard_fields"]["format"]       = fields.value().format_;
            _metadata["standard_fields"]["bit_depth"]    = fields.value().bit_depth_;
            _metadata["standard_fields"]["pixel_aspect"] = fields.value().pixel_aspect_;
        }

    } catch (const std::exception &) {
        // stay silent if standard fields fail
    }
    return _metadata;
}

} // namespace xstudio::media_metadata
