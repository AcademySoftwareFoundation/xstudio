// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace media_reader {
    class PPMMediaReader : public MediaReader {
      public:
        PPMMediaReader(const utility::JsonStore &prefs = utility::JsonStore())
            : MediaReader("PPM", prefs) {}
        virtual ~PPMMediaReader() = default;

        ImageBufPtr image(const media::AVFrameID &mptr) override;
        MRCertainty supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;
        // media::MediaDetail detail(const caf::uri &uri) const override;
        [[nodiscard]] utility::Uuid plugin_uuid() const override;
    };
} // namespace media_reader
} // namespace xstudio
