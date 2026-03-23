// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/subset/subset_actor.hpp"

namespace xstudio {
namespace contact_sheet {

    class ContactSheetActor : public subset::SubsetActor {

      public:
        ContactSheetActor(
            caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn);
        ContactSheetActor(caf::actor_config &cfg, caf::actor playlist, const std::string &name);

        caf::behavior make_behavior() override {
            return override_behaviour_.or_else(subset::SubsetActor::make_behavior());
        }

      private:
        void init();
        inline static const std::string NAME = "ContactSheetActor";

        caf::message_handler override_behaviour_;
    };
} // namespace contact_sheet
} // namespace xstudio
