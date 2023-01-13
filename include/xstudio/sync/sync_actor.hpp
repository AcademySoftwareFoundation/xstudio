// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>

#include <caf/actor_config.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>

#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace sync {
    class SyncActor : public caf::event_based_actor {
      public:
        SyncActor(caf::actor_config &cfg);
        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      private:
        void init();
        inline static const std::string NAME = "SyncActor";
        caf::behavior behavior_;
    };

    class SyncGatewayActor : public caf::event_based_actor {
      public:
        SyncGatewayActor(caf::actor_config &cfg);
        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      private:
        void init();
        inline static const std::string NAME = "SyncGatewayActor";
        caf::behavior behavior_;
    };

    class SyncGatewayManagerActor : public caf::event_based_actor {
      public:
        SyncGatewayManagerActor(caf::actor_config &cfg);
        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      private:
        void init();
        inline static const std::string NAME = "SyncGatewayManagerActor";
        caf::behavior behavior_;
        std::map<utility::Uuid, std::string> lock_key_;
        std::map<utility::Uuid, caf::actor> clients_;
    };
} // namespace sync
} // namespace xstudio
