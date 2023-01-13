// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <queue>

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace shotgun_client {
    class ShotgunClientActor : public caf::event_based_actor {
      public:
        ShotgunClientActor(caf::actor_config &cfg);
        ~ShotgunClientActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ShotgunClientActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

        void acquire_token(caf::typed_response_promise<std::pair<std::string, std::string>> rp);

      private:
        std::queue<caf::typed_response_promise<std::pair<std::string, std::string>>>
            request_refresh_queue_;
        std::queue<caf::typed_response_promise<std::pair<std::string, std::string>>>
            request_acquire_queue_;

        caf::behavior behavior_;
        ShotgunClient base_;
        caf::actor_addr secret_source_;
        caf::actor http_;
        caf::actor event_group_;
    };
} // namespace shotgun_client
} // namespace xstudio
