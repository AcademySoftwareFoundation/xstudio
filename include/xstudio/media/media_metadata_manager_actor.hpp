// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_addr.hpp>
#include <caf/actor_config.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/group.hpp>
#include <map>

#include "xstudio/utility/tree.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace media {
    class GlobalMetadataManager : public caf::event_based_actor {
      public:
        GlobalMetadataManager(caf::actor_config &cfg);
        ~GlobalMetadataManager() override = default;

        caf::behavior make_behavior() override { return behavior_; }

      private:
        void config_updated();

        caf::actor event_group_;
        utility::JsonTree metadata_config_;
        utility::JsonStore metadata_extraction_config_;
        caf::behavior behavior_;
    };
} // namespace media
} // namespace xstudio
