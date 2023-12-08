// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/contact_sheet/contact_sheet.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace contact_sheet {
    class ContactSheetActor : public caf::event_based_actor {
      public:
        ContactSheetActor(
            caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn);
        ContactSheetActor(caf::actor_config &cfg, caf::actor playlist, const std::string &name);
        ~ContactSheetActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ContactSheetActor";
        void init();

        void add_media(
            caf::actor actor,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());
        bool remove_media(caf::actor actor, const utility::Uuid &uuid);
        caf::behavior make_behavior() override { return behavior_; }
        void sort_alphabetically();

      private:
        caf::behavior behavior_;
        caf::actor_addr playlist_;
        ContactSheet base_;
        utility::UuidActorMap actors_;
        utility::UuidActor playhead_;
    };
} // namespace contact_sheet
} // namespace xstudio
