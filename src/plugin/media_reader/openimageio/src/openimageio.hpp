#pragma once

#include <string>
#include <OpenImageIO/imageio.h>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

namespace xstudio::media_reader {
class OIIOMediaReader : public MediaReader {
  public:
    OIIOMediaReader(const utility::JsonStore &prefs = utility::JsonStore());
    ~OIIOMediaReader() override = default;

    void update_preferences(const utility::JsonStore &prefs) override;

    ImageBufPtr image(const media::AVFrameID &mptr) override;

    [[nodiscard]] bool prefer_sequential_access() const override { return false; }

    MRCertainty
    supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;
    [[nodiscard]] std::vector<std::string> supported_extensions() const override;

    thumbnail::ThumbnailBufferPtr
    thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) override;

    [[nodiscard]] media::MediaDetail detail(const caf::uri &uri) const override;

    [[nodiscard]] utility::Uuid plugin_uuid() const override;

    [[nodiscard]] ImageBuffer::PixelPickerFunc pixel_picker_func() const override {
        return &OIIOMediaReader::oiio_buffer_pixel_picker;
    }

  private:
    static PixelInfo oiio_buffer_pixel_picker(
        const ImageBuffer &buf,
        const utility::JsonStore &pixel_unpack_uniforms,
        const Imath::V2i &pixel_location,
        const std::vector<Imath::V2i> &extra_pixel_locations);

    utility::JsonStore supported_;
};
} // namespace xstudio::media_reader
