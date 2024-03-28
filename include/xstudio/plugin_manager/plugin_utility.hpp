// SPDX-License-Identifier: Apache-2.0
#pragma once

// #include <array>
// #include <caf/all.hpp>
// #include <cstddef>
// #include <map>
// #include <nlohmann/json.hpp>
// #include <string>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/global_store/global_store.hpp"

namespace xstudio {
namespace plugin_manager {

    using namespace caf;

    class Utility {
      public:
        [[nodiscard]] std::string name() const { return name_; }
        virtual ~Utility() = default;

        virtual void preference_update(const utility::JsonStore &) {}

      protected:
        Utility(std::string name, const utility::JsonStore &jsn = utility::JsonStore())
            : name_(std::move(name)) {
            preference_update(jsn);
        }


      private:
        const std::string name_;
    };

    template <typename T>
    class UtilityPlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        UtilityPlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginFlags::PF_UTILITY,
                  true,
                  author,
                  description,
                  version) {}
        ~UtilityPlugin() override = default;
    };

    template <typename T> class UtilityPluginActor : public caf::event_based_actor {

      public:
        UtilityPluginActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn = utility::JsonStore())
            : caf::event_based_actor(cfg), utility_(jsn) {

            spdlog::debug("Created UtilityPlugin");

            // watch for updates to global preferences

            try {
                auto prefs = global_store::GlobalStoreHelper(system());
                utility::JsonStore js;
                join_broadcast(this, prefs.get_group(js));
                utility_.preference_update(js);
            } catch (...) {
            }

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
                [=](utility::name_atom) -> std::string { return utility_.name(); },

                [=](json_store::update_atom,
                    const utility::JsonStore & /*change*/,
                    const std::string & /*path*/,
                    const utility::JsonStore &full) {
                    delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
                },

                [=](json_store::update_atom, const utility::JsonStore &js) {
                    utility_.preference_update(js);
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T utility_;
    };

} // namespace plugin_manager
} // namespace xstudio
