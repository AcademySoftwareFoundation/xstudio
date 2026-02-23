#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace demo_plugin {

    /*
    Demonstration media reader.

    This example does not read any data from the filesystem but rather shows
    how procedural image data can be generated on both the CPU at 'load' time
    or on the GPU when the image is actually displayed. This allows us to
    make visual data for our demonstration integration plugin.

    */
    class ProceduralImageGenReader : public media_reader::MediaReader {

      public:
        ProceduralImageGenReader(const utility::JsonStore &prefs = utility::JsonStore());
        virtual ~ProceduralImageGenReader() = default;

        // Our plugin requires its own static UUID.
        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("66022caa-3540-4007-875f-293f80b6125f");

        bool prefer_sequential_access(const caf::uri &) const override { return false; }
        media_reader::MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

        media_reader::ImageBufPtr image(const media::AVFrameID &mptr) override;
        media::MediaDetail detail(const caf::uri &uri) const override;
        thumbnail::ThumbnailBufferPtr
        thumbnail(const media::AVFrameID &mpr, const size_t thumb_size) override;

        [[nodiscard]] utility::Uuid plugin_uuid() const override { return PLUGIN_UUID; }
    };
} // namespace demo_plugin
} // namespace xstudio
