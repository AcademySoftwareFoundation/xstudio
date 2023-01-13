// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_addr.hpp>
#include <caf/actor_config.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/group.hpp>
#include <map>

#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace json_store {
    class JsonStoreActor : public caf::event_based_actor {
      public:
        JsonStoreActor(
            caf::actor_config &cfg,
            const utility::Uuid &uuid              = utility::Uuid::generate(),
            const utility::JsonStore &json         = utility::JsonStore(),
            const std::chrono::milliseconds &delay = std::chrono::milliseconds());
        ~JsonStoreActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        utility::JsonStore json_store_;
        static caf::message_handler default_event_handler();

      private:
        void broadcast_change(
            const utility::JsonStore &json = utility::JsonStore(),
            const std::string &path        = std::string(),
            const bool async               = true);

        caf::behavior behavior_;
        utility::Uuid uuid_;
        bool update_pending_;
        std::chrono::milliseconds broadcast_delay_;
        caf::actor broadcast_;
        std::map<caf::actor_addr, caf::actor> actor_group_;
        std::map<caf::actor, std::string> group_path_;
    };
} // namespace json_store
} // namespace xstudio
