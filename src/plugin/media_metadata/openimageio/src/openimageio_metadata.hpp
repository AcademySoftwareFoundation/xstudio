#pragma once

#include <array>

#include <nlohmann/json.hpp>

#include "xstudio/media_metadata/media_metadata.hpp"

namespace xstudio {
namespace media_metadata {

    class OpenImageIOMediaMetadata : public MediaMetadata {
      public:
        OpenImageIOMediaMetadata();
        ~OpenImageIOMediaMetadata() override = default;

        MMCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

      protected:
        nlohmann::json read_metadata(const caf::uri &uri) override;
        std::optional<StandardFields>
        fill_standard_fields(const nlohmann::json &metadata) override;
    };

} // namespace media_metadata
} // namespace xstudio
