// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"
#include <ImfChannelList.h>
#include <ImfHeader.h> // staticInitialize

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
        [[nodiscard]] ImageBuffer::PixelPickerFunc pixel_picker_func() const override {
            return &OpenEXRMediaReader::exr_buffer_pixel_picker;
        }

      private:
        static PixelInfo
        exr_buffer_pixel_picker(const ImageBuffer &buf, const Imath::V2i &pixel_location);

        void get_channel_names_by_layer(
            const Imf::Header &header,
            std::map<std::string, std::vector<std::string>> &channel_names_by_layer) const;

        void stream_ids_from_exr_part(
            const Imf::Header &header, std::vector<std::string> &stream_ids) const;

        std::array<Imf::PixelType, 4> pick_exr_channels_from_stream_id(
            const Imf::Header &header,
            const std::string &stream_id,
            std::vector<std::string> &exr_channels_to_load) const;

        float max_exr_overscan_percent_;
        int readers_per_source_;
    };
} // namespace media_reader
} // namespace xstudio
