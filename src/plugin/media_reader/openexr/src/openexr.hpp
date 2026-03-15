// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"
#include <ImfChannelList.h>
#include <ImfHeader.h> // staticInitialize
#include <ImfMultiPartInputFile.h>

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
        static PixelInfo exr_buffer_pixel_picker(
            const ImageBuffer &buf,
            const Imath::V2i &pixel_location,
            const std::vector<Imath::V2i> &extra_pixel_locationss);

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
        int exr_thread_count_;

        // Cache of JSON headers per EXR part index. Metadata is identical across
        // all frames in a sequence, so we compute it once and reuse it.
        std::unordered_map<int, utility::JsonStore> cached_headers_;

        // Cache the last opened MultiPartInputFile to avoid reopening the same
        // file on consecutive reads (e.g. multiple streams from one file, or
        // scrubbing back to an already-read frame). Each OpenEXRMediaReader
        // instance is used by a single worker actor, so no locking is needed.
        std::string cached_file_path_;
        std::shared_ptr<Imf::MultiPartInputFile> cached_input_;
    };
} // namespace media_reader
} // namespace xstudio
