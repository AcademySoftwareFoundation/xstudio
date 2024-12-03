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
        utility::JsonStore playhead_serialisation_;
    };

    /*class ContactSheetActor : public caf::event_based_actor {
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
        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }
        void sort_by_media_display_info(const int sort_column_index, const bool ascending);

      private:
        caf::actor_addr playlist_;
        ContactSheet base_;

        caf::actor change_event_group_;
        caf::actor selection_actor_;

        utility::UuidActorMap actors_;
        utility::UuidActor playhead_;
    };*/
} // namespace contact_sheet
} // namespace xstudio
