// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <set>

namespace xstudio {
namespace broadcast {
    class BroadcastActor : public caf::event_based_actor {
      public:
        BroadcastActor(caf::actor_config &cfg, caf::actor owner = caf::actor());
        ~BroadcastActor() override = default;

        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;
        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "BroadcastActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

        caf::skippable_result broadcast_message(caf::scheduled_actor *, caf::message &);

      private:
        caf::behavior behavior_;
        caf::actor_addr owner_;
        std::set<caf::actor_addr> subscribers_;
    };

} // namespace broadcast
} // namespace xstudio
