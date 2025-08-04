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
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/module/module.hpp"

namespace xstudio {
namespace media_hook {

    using namespace caf;

    class MediaHook : public module::Module {

      public:
        [[nodiscard]] std::string name() const { return name_; }
        ~MediaHook() override = default;

        virtual utility::JsonStore
        modify_metadata(const utility::MediaReference &, const utility::JsonStore &) {
            return utility::JsonStore{};
        }

        virtual utility::JsonStore modify_clip_metadata(
            const utility::JsonStore &clip_metadata, const utility::JsonStore &media_metadata) {
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

        virtual void update_prefs(const utility::JsonStore &full_prefs_dict) {}

        /* Allow custom display selection logic to be defined in MediaHook plugins.
           This can be used to assign a specific OCIO display to the detected
           monitor where the xStudio viewport is being shown. */
        virtual std::string detect_display(
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber,
            const utility::JsonStore &colour_params) {
            return {};
        }

      protected:
        MediaHook(std::string name) : module::Module(name), name_(std::move(name)) {}
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
                  plugin_manager::PluginFlags::PF_MEDIA_HOOK,
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
            : caf::event_based_actor(cfg), media_hook_() {

            spdlog::debug("Created MediaHookActor");
            // print_on_exit(this, "MediaHookActor");

            try {
                auto prefs = global_store::GlobalStoreHelper(system());
                utility::JsonStore j;
                utility::join_broadcast(this, prefs.get_group(j));
                media_hook_.update_prefs(j);
            } catch (...) {
            }

            media_hook_.set_parent_actor_addr(actor_cast<caf::actor_addr>(this));

            link_hook_to_other_instances();

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
                //             anon_mail(//                 media::add_media_source_atom_v,
                //                 media_source_references,
                //                 media_source_names,
                //                 preferred_source).send(//                 media_actor);
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
                        anon_mail(media::media_reference_atom_v, mr).send(ua.actor());
                    }

                    return media_hook_.modify_metadata(mr, jsn);
                },

                [=](media_hook::get_clip_hook_atom,
                    const utility::JsonStore &clip,
                    const utility::JsonStore &media) -> utility::JsonStore {
                    return media_hook_.modify_clip_metadata(clip, media);
                },

                [=](media_hook::detect_display_atom,
                    const std::string &name,
                    const std::string &model,
                    const std::string &manufacturer,
                    const std::string &serialNumber,
                    const utility::JsonStore &jsn) -> std::string {
                    return media_hook_.detect_display(
                        name, model, manufacturer, serialNumber, jsn);
                },

                [=](json_store::update_atom,
                    const utility::JsonStore & /*change*/,
                    const std::string & /*path*/,
                    const utility::JsonStore &full) { media_hook_.update_prefs(full); },

                [=](json_store::update_atom, const utility::JsonStore &j) mutable {
                    media_hook_.update_prefs(j);
                });
        }

        caf::behavior make_behavior() override {
            return media_hook_.message_handler().or_else(behavior_);
        }

        void link_hook_to_other_instances() {

            // this is a bit of hack for now. Multiple 'worker' media hook actors are
            // created by the GlobalMediaHookActor - we only want one instance
            // to connect to the UI. The other instances should be cloned and
            // driven by changes made to the instance that is connected to the
            // UI. Otherwise any attributes that the media hook plugin creates
            // and exposes in the UI will appear multiple times (due to multiple
            // worker instances).

            const std::string hook_type_id = typeid(T).name();
            static std::mutex m;
            static std::map<std::string, caf::actor_addr> main_instances;
            m.lock();
            auto p = main_instances.find(hook_type_id);
            if (p != main_instances.end()) {
                auto main_instance = caf::actor_cast<caf::actor>(p->second);
                if (main_instance) {
                    anon_mail(
                        module::link_module_atom_v,
                        actor_cast<caf::actor>(this),
                        true,
                        false,
                        true)
                        .send(main_instance);
                }
            } else {
                main_instances[hook_type_id] = actor_cast<caf::actor_addr>(this);
                anon_mail(module::connect_to_ui_atom_v)
                    .delay(std::chrono::milliseconds(250))
                    .send(actor_cast<caf::actor>(this));
            }
            m.unlock();
        }


      private:
        caf::behavior behavior_;
        T media_hook_;
    };

} // namespace media_hook
} // namespace xstudio
