// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace media_reader {
    class OpenEXRMediaReader : public MediaReader {
      public:
        OpenEXRMediaReader(const utility::JsonStore &prefs = utility::JsonStore());
        virtual ~OpenEXRMediaReader() = default;

        void update_preferences(const utility::JsonStore &) override;

        uint8_t maximum_readers(const caf::uri &) const override { return readers_per_source_; }
        bool prefer_sequential_access(const caf::uri &) const override { return false; }
        MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

        ImageBufPtr image(const media::AVFrameID &mptr) override;
        media::MediaDetail detail(const caf::uri &uri) const override;
        thumbnail::ThumbnailBufferPtr
        thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) override;

        [[nodiscard]] utility::Uuid plugin_uuid() const override;

      private:
        float max_exr_overscan_percent_;
        int readers_per_source_;
    };
} // namespace media_reader
} // namespace xstudio
