// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <QPdfDocument>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

namespace xstudio {
namespace media_reader {
    class PDFMediaReader : public MediaReader {
      public:
        PDFMediaReader(const utility::JsonStore &prefs = utility::JsonStore())
            : MediaReader("PDF", prefs) {
            update_preferences(prefs);
        }
        virtual ~PDFMediaReader() = default;
        void update_preferences(const utility::JsonStore &) override;

        media::MediaDetail detail(const caf::uri &uri) const override;

        ImageBufPtr image(const media::AVFrameID &mptr) override;
        MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;
        std::vector<std::string> supported_extensions() const override;
        [[nodiscard]] utility::Uuid plugin_uuid() const override;

        std::shared_ptr<thumbnail::ThumbnailBuffer>
        thumbnail(const media::AVFrameID &mptr, const size_t thumb_size) override;

      private:
        uint64_t points_to_pixels(const double points) const;
        uint32_t dpi_{100};
        utility::JsonStore supported_;

        mutable std::map<caf::uri, std::shared_ptr<QPdfDocument>> cache_;
    };
} // namespace media_reader
} // namespace xstudio
