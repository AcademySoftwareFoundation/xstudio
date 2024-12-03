// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/gap.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/frame_rate_and_duration.hpp"

namespace xstudio {
namespace timeline {
    class GapActor : public caf::event_based_actor {
      public:
        GapActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        GapActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &item);
        GapActor(
            caf::actor_config &cfg,
            const std::string &name                    = "Gap",
            const utility::FrameRateDuration &duration = utility::FrameRateDuration(),
            const utility::Uuid &uuid                  = utility::Uuid::generate());

        GapActor(caf::actor_config &cfg, const Item &item);
        GapActor(caf::actor_config &cfg, const Item &item, Item &nitem);

        ~GapActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "GapActor";
        void init();

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

      private:
        Gap base_;
    };
} // namespace timeline
} // namespace xstudio
