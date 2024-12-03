// SPDX-License-Identifier: Apache-2.0
// reader needs to be very smart..
// handle contention on devices.
// separate IO from decompression?
// handle reverse movie reading ?
// handle audio streams
// handle eager readahead
// handle pre-emption
// handle eager readahead invalidation
// hold handles open if coedit_listy..

// should be able to store blind plugin data to allow movie reading without reopening
// only applies to container access. (not frame sequences).
// stick them LRU DB. how do we cope with concurrent access to same movie ? Store last frame ?

#pragma once

#include <array>
#include <caf/uri.hpp>
#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media/media_error.hpp"
#include "xstudio/media/caf_media_error.hpp"
#include "xstudio/media/media_error.hpp"
#include "xstudio/media_reader/audio_buffer.hpp"
#include "xstudio/media_reader/frame_request_queue.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/media_reader/pixel_info.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace thumbnail {
    class ThumbnailBuffer;
}
namespace media_reader {

    class MediaReader {
      public:
        MediaReader(std::string name, const utility::JsonStore &prefs = utility::JsonStore());
        virtual ~MediaReader() = default;

        [[nodiscard]] std::string name() const;

        virtual void update_preferences(const utility::JsonStore &prefs);

        virtual ImageBufPtr image(const media::AVFrameID &mptr);

        virtual ImageBufPtr partial_image(
            const media::AVFrameID &mptr,
            ImageBufPtr &current_loaded,
            const Imath::Box2f &viewport_visible_area,
            const Imath::V2i &viewport_size_screen_pixels,
            const Imath::M44f &image_transform) {
            return image(mptr);
        }
        virtual AudioBufPtr audio(const media::AVFrameID &mptr);

        virtual std::shared_ptr<thumbnail::ThumbnailBuffer>
        thumbnail(const media::AVFrameID &mptr, const size_t thumb_size);
        [[nodiscard]] virtual media::MediaDetail detail(const caf::uri &uri) const;
        [[nodiscard]] virtual uint8_t maximum_readers(const caf::uri &uri) const;
        [[nodiscard]] virtual bool prefer_sequential_access(const caf::uri &uri) const;
        [[nodiscard]] virtual bool can_decode_audio() const;
        [[nodiscard]] virtual bool can_do_partial_frames() const;
        [[nodiscard]] virtual utility::Uuid plugin_uuid() const = 0;
        [[nodiscard]] virtual ImageBuffer::PixelPickerFunc pixel_picker_func() const {
            return &MediaReader::default_pixel_picker;
        }

        virtual MRCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature);

      private:
        static PixelInfo
        default_pixel_picker(const ImageBuffer &buf, const Imath::V2i &pixel_location) {
            return PixelInfo(pixel_location);
        }
        const std::string name_;
    };

    template <typename T>
    class MediaReaderPlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        MediaReaderPlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginFlags::PF_MEDIA_READER,
                  false,
                  author,
                  description,
                  version) {}
        ~MediaReaderPlugin() override = default;
    };

    template <typename T> class MediaReaderActor : public caf::event_based_actor {

      public:
        MediaReaderActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn = utility::JsonStore())
            : caf::event_based_actor(cfg), media_reader_(jsn) {

            spdlog::debug("Created MediaReaderActor");
            utility::print_on_exit(this, "MediaReaderActor");

            {
                auto prefs = global_store::GlobalStoreHelper(system());
                utility::JsonStore js;
                utility::join_broadcast(this, prefs.get_group(js));
                media_reader_.update_preferences(js);
            }

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
                [=](utility::name_atom) -> std::string { return media_reader_.name(); },

                [=](get_audio_atom, const media::AVFrameID &mptr) -> result<AudioBufPtr> {
                    AudioBufPtr mb;
                    try {
                        mb = media_reader_.audio(mptr);
                        if (mb) {
                            mb->set_media_key(mptr.key());
                        }
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                    return mb;
                },

                [=](get_image_atom, const media::AVFrameID &mptr) -> result<ImageBufPtr> {
                    ImageBufPtr mb;
                    try {
                        std::string path = utility::uri_to_posix_path(mptr.uri());
                        mb               = media_reader_.image(mptr);
                        if (mb) {
                            mb->set_media_key(mptr.key());
                            mb->set_pixel_picker_func(media_reader_.pixel_picker_func());
                            mb->params()["path"]   = path;
                            mb->params()["frame"]  = mptr.frame();
                            mb->params()["reader"] = media_reader_.name();
                        }
                    } catch (const media_missing_error &e) {
                        return make_error(media::media_error::missing, e.what());
                    } catch (const media_corrupt_error &e) {
                        return make_error(media::media_error::corrupt, e.what());
                    } catch (const media_unsupported_error &e) {
                        return make_error(media::media_error::unsupported, e.what());
                    } catch (const media_unreadable_error &e) {
                        return make_error(media::media_error::unreadable, e.what());
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                    return mb;
                },

                [=](get_media_detail_atom, const caf::uri &_uri) -> result<media::MediaDetail> {
                    try {
                        auto wazoo = media_reader_.detail(_uri);
                        return result<media::MediaDetail>(wazoo);
                    } catch (const media_missing_error &e) {
                        return make_error(media::media_error::missing, e.what());
                    } catch (const media_corrupt_error &e) {
                        return make_error(media::media_error::corrupt, e.what());
                    } catch (const media_unsupported_error &e) {
                        return make_error(media::media_error::unsupported, e.what());
                    } catch (const media_unreadable_error &e) {
                        return make_error(media::media_error::unreadable, e.what());
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                    return make_error(
                        media::media_error::unsupported, "Media format not supported");
                },

                [=](media_reader::get_thumbnail_atom,
                    const media::AVFrameID &mptr,
                    const size_t thumb_size) -> result<thumbnail::ThumbnailBufferPtr> {
                    try {
                        return media_reader_.thumbnail(mptr, thumb_size);
                    } catch (const media_missing_error &e) {
                        return make_error(media::media_error::missing, e.what());
                    } catch (const media_corrupt_error &e) {
                        return make_error(media::media_error::corrupt, e.what());
                    } catch (const media_unsupported_error &e) {
                        return make_error(media::media_error::unsupported, e.what());
                    } catch (const media_unreadable_error &e) {
                        return make_error(media::media_error::unreadable, e.what());
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                    return thumbnail::ThumbnailBufferPtr();
                },

                [=](media_reader::supported_atom,
                    const caf::uri &_uri,
                    const std::array<uint8_t, 16> &signature)
                    -> result<std::pair<utility::Uuid, MRCertainty>> {
                    try {
                        return std::make_pair(
                            media_reader_.plugin_uuid(),
                            media_reader_.supported(_uri, signature));
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                },

                [=](json_store::update_atom,
                    const utility::JsonStore & /*change*/,
                    const std::string & /*path*/,
                    const utility::JsonStore &full) {
                    delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
                },

                [=](json_store::update_atom, const utility::JsonStore &js) {
                    try {
                        media_reader_.update_preferences(js);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T media_reader_;
    };

} // namespace media_reader
} // namespace xstudio
