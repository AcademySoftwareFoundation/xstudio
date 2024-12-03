// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace media_reader {
    namespace ffmpeg {
        class FFMpegDecoder;
    }
    class FFMpegMediaReader : public MediaReader {
      public:
        FFMpegMediaReader(const utility::JsonStore &prefs = utility::JsonStore());
        virtual ~FFMpegMediaReader() = default;

        ImageBufPtr image(const media::AVFrameID &mptr) override;

        AudioBufPtr audio(const media::AVFrameID &mptr) override;

        void update_preferences(const utility::JsonStore &) override;
        MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;
        media::MediaDetail detail(const caf::uri &uri) const override;
        uint8_t maximum_readers(const caf::uri &) const override { return readers_per_source_; }
        bool prefer_sequential_access(const caf::uri &) const override { return true; }
        bool can_decode_audio() const override { return true; }
        std::shared_ptr<thumbnail::ThumbnailBuffer>
        thumbnail(const media::AVFrameID &mptr, const size_t thumb_size) override;

        [[nodiscard]] utility::Uuid plugin_uuid() const override;

        [[nodiscard]] ImageBuffer::PixelPickerFunc pixel_picker_func() const override {
            return &FFMpegMediaReader::ffmpeg_buffer_pixel_picker;
        }

      private:
        static PixelInfo
        ffmpeg_buffer_pixel_picker(const ImageBuffer &buf, const Imath::V2i &pixel_location);

        std::shared_ptr<ffmpeg::FFMpegDecoder> decoder;
        std::shared_ptr<ffmpeg::FFMpegDecoder> audio_decoder;
        std::shared_ptr<ffmpeg::FFMpegDecoder> thumbnail_decoder;

        int readers_per_source_;
        int soundcard_sample_rate_       = {4000};
        int channels_                    = 2;
        utility::FrameRate default_rate_ = {utility::FrameRate(timebase::k_flicks_24fps)};

        ImageBufPtr last_decoded_image_;
    };
} // namespace media_reader
} // namespace xstudio
