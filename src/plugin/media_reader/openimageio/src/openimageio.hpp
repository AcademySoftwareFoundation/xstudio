#pragma once

#include <string>
#include <OpenImageIO/imageio.h>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

namespace xstudio {
namespace media_reader {
    class OIIOMediaReader : public MediaReader {
      public:
        OIIOMediaReader(const utility::JsonStore &prefs = utility::JsonStore());
        ~OIIOMediaReader() override = default;

        void update_preferences(const utility::JsonStore &prefs) override;

        ImageBufPtr image(const media::AVFrameID &mptr) override;

        MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;
        std::vector<std::string> supported_extensions() const override;

        thumbnail::ThumbnailBufferPtr
        thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) override;

        media::MediaDetail detail(const caf::uri &uri) const override;

        [[nodiscard]] utility::Uuid plugin_uuid() const override;

      private:
        utility::JsonStore supported_;
    };
} // namespace media_reader
} // namespace xstudio
