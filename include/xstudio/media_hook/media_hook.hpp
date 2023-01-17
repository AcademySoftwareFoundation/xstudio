// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <caf/all.hpp>
#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/media/media_actor.hpp"

namespace xstudio {
namespace media_hook {

    using namespace caf;

    class MediaHook {
      public:
        [[nodiscard]] std::string name() const { return name_; }
        virtual ~MediaHook() = default;

        virtual utility::JsonStore
        modify_metadata(const utility::MediaReference &, const utility::JsonStore &) {
            return utility::JsonStore{};
        }

        virtual std::optional<utility::MediaReference>
        modify_media_reference(const utility::MediaReference &, const utility::JsonStore &) {
            return {};
        }

        virtual std::vector<std::tuple<std::string, caf::uri, xstudio::utility::FrameList>>
        gather_media_sources(
            const std::vector<utility::MediaReference>
                &, // source media refs - extend and/or reorder this in your plugin
            const std::vector<std::string>
                &, // media_source names - change, extend and/or reorder this in your plugin
            std::string & // preferred_source - indicate the name of the source you want to
                          // set on the media item
        ) {
            return std::vector<
                std::tuple<std::string, caf::uri, xstudio::utility::FrameList>>();
        }


      protected:
        MediaHook(std::string name) : name_(std::move(name)) {}
        std::string default_path() const { return "/metadata/external/" + name_; }

      private:
        const std::string name_;
    };

    template <typename T>
    class MediaHookPlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        MediaHookPlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginType::PT_MEDIA_HOOK,
                  false,
                  author,
                  description,
                  version) {}
        ~MediaHookPlugin() override = default;
    };

    template <typename T> class MediaHookActor : public caf::event_based_actor {

      public:
        MediaHookActor(
            caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore())
            : caf::event_based_actor(cfg) {

            spdlog::debug("Created MediaHookActor");
            // print_on_exit(this, "MediaHookActor");

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
                [=](utility::name_atom) -> std::string { return media_hook_.name(); },


                [=](media_hook::gather_media_sources_atom,
                    caf::actor media_actor,
                    const std::vector<utility::MediaReference> &existing_source_refs,
                    const std::vector<std::string> &existing_source_names)
                    -> std::vector<
                        std::tuple<std::string, caf::uri, xstudio::utility::FrameList>> {
                    std::string preferred_source;
                    return media_hook_.gather_media_sources(
                        existing_source_refs, existing_source_names, preferred_source);
                },

                // [=](media_hook::gather_media_sources_atom,
                //     caf::actor media_actor,
                //     const std::vector<utility::MediaReference> &existing_source_refs,
                //     const std::vector<std::string> &existing_source_names
                //     ) ->bool {

                //     std::vector <std::string> media_source_names = existing_source_names;
                //     std::vector <utility::MediaReference> media_source_references =
                //     existing_source_refs; std::string preferred_source;

                //     // media hook plugin can re-order and expand the list of
                //     // MediaReference objects and also change the names
                //     if (media_hook_.gather_media_sources(
                //         media_source_references,
                //         media_source_names,
                //         preferred_source
                //         )) {

                //         if (media_source_references.size() != media_source_names.size())
                //         {

                //         } else {
                //             anon_send(
                //                 media_actor,
                //                 media::add_media_source_atom_v,
                //                 media_source_references,
                //                 media_source_names,
                //                 preferred_source);
                //         }
                //     }
                //     return true;

                // },

                [=](media_hook::get_media_hook_atom,
                    const utility::UuidActor &ua,
                    utility::MediaReference mr,
                    const utility::JsonStore &jsn) -> utility::JsonStore {
                    auto ref = media_hook_.modify_media_reference(mr, jsn);
                    if (ref) {
                        mr = *ref;
                        anon_send(ua.actor(), media::media_reference_atom_v, mr);
                    }

                    return media_hook_.modify_metadata(mr, jsn);
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T media_hook_;
    };

} // namespace media_hook
} // namespace xstudio
