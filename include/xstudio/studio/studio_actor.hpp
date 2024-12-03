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

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }
        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "StudioActor";
        void init();

        Studio base_;
        caf::actor session_;

        struct QuickviewRequest {
            utility::UuidActorVector media_actors;
            std::string compare_mode;
            utility::JsonStore in_point;
            utility::JsonStore out_point;
        };
        std::vector<QuickviewRequest> quickview_requests_;
    };
} // namespace studio
} // namespace xstudio
