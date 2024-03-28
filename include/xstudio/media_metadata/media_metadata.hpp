// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <caf/uri.hpp>
#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"

namespace xstudio {
namespace media_metadata {

    using namespace caf;

    // type should be std::byte... need newer gcc..
    class MediaMetadata {
      public:
        [[nodiscard]] std::string name() const;

        nlohmann::json metadata(const caf::uri &uri);

        virtual MMCertainty
        supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) = 0;

        virtual ~MediaMetadata() = default;

      protected:
        struct StandardFields {
            std::string resolution_;
            std::string format_;
            std::string bit_depth_;
            float pixel_aspect_ = {1.0f};
        };

        /**
         *  @brief Pure virtual method for filling in 'standard' metadata fields
         *
         *  @details The metadata reader implementation should parse the metadata dictionary
         * and fill in the 'standard' fields, resolution, format and bit_depth where
         * possible
         */
        virtual std::optional<StandardFields>
        fill_standard_fields(const nlohmann::json &metadata) = 0;

        virtual nlohmann::json read_metadata(const caf::uri &uri) = 0;

        MediaMetadata(std::string name);

      private:
        const std::string name_;
    };

    template <typename T>
    class MediaMetadataPlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        MediaMetadataPlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginFlags::PF_MEDIA_METADATA,
                  false,
                  author,
                  description,
                  version) {}
        ~MediaMetadataPlugin() override = default;
    };

    template <typename T> class MediaMetadataActor : public caf::event_based_actor {

      public:
        MediaMetadataActor(
            caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore())
            : caf::event_based_actor(cfg) {

            spdlog::debug("Created MediaMetadataActor");
            // print_on_exit(this, "MediaHookActor");

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
                [=](utility::name_atom) -> std::string { return media_metadata_.name(); },

                [=](get_metadata_atom,
                    const caf::uri &_uri,
                    const int frame) -> result<std::pair<utility::JsonStore, int>> {
                    nlohmann::json json;
                    try {
                        json = media_metadata_.metadata(_uri);
                    } catch (const std::exception &e) {
                        return make_error(xstudio_error::error, e.what());
                    }
                    return std::make_pair(utility::JsonStore(json), frame);
                },

                [=](media_reader::supported_atom,
                    const caf::uri &_uri,
                    const std::array<uint8_t, 16> &signature)
                    -> std::pair<std::string, MMCertainty> {
                    return std::make_pair(
                        media_metadata_.name(), media_metadata_.supported(_uri, signature));
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T media_metadata_;
    };
} // namespace media_metadata
} // namespace xstudio
