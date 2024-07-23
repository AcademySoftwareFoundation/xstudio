// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/studio/studio.hpp"

namespace xstudio {
namespace studio {
    class StudioActor : public caf::event_based_actor {
      public:
        StudioActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        StudioActor(caf::actor_config &cfg, const std::string &name);
        ~StudioActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }
        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "StudioActor";
        void init();

        caf::behavior behavior_;
        Studio base_;
        caf::actor session_;

        struct QuickviewRequest {
            utility::UuidActorVector media_actors;
            std::string compare_mode;
        };
        std::vector<QuickviewRequest> quickview_requests_;
    };
} // namespace studio
} // namespace xstudio
