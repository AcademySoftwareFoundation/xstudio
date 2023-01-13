// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace media_reader {
    class BlankMediaReader : public MediaReader {
      public:
        BlankMediaReader(const utility::JsonStore &prefs = utility::JsonStore())
            : MediaReader("Blank", prefs) {}
        virtual ~BlankMediaReader() = default;

        ImageBufPtr image(const media::AVFrameID &mptr) override;
        MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

        thumbnail::ThumbnailBufferPtr
        thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) override;

        [[nodiscard]] utility::Uuid plugin_uuid() const override;

      private:
        std::array<std::byte, 3> get_colour(const std::string &colour) const;
    };
} // namespace media_reader
} // namespace xstudio
