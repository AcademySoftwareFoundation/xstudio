// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <memory>
#include <string>

#include "xstudio/plugin_manager/plugin_manager.hpp"

namespace xstudio {
namespace plugin_manager {

    class PluginManagerActor : public caf::event_based_actor {
      public:
        PluginManagerActor(caf::actor_config &cfg);
        ~PluginManagerActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "PluginManagerActor";

        void enable_resident(
            const utility::Uuid &uuid,
            const bool enable,
            const utility::JsonStore &json = utility::JsonStore());

        void update_from_preferences(const utility::JsonStore &json);


        caf::behavior behavior_;

        PluginManager manager_;
        utility::UuidActorMap resident_;
        std::map<std::string, caf::actor> base_plugins_;
    };

} // namespace plugin_manager
} // namespace xstudio
