// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/module/module.hpp"
#include <caf/all.hpp>

namespace xstudio {

namespace module {

    class GlobalModuleAttrEventsActor : public caf::event_based_actor {

      public:
        GlobalModuleAttrEventsActor(caf::actor_config &cfg);
        ~GlobalModuleAttrEventsActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }
        const char *name() const override { return NAME.c_str(); }

        void on_exit() override;

      private:
        inline static const std::string NAME = "GlobalModuleAttrEventsActor";
        caf::behavior behavior_;
        caf::actor module_backend_events_group_;
        caf::actor playhead_group_;
        caf::actor module_ui_events_group_;

        std::vector<utility::UuidActor> attr_owners_;
    };

} // namespace module
} // namespace xstudio