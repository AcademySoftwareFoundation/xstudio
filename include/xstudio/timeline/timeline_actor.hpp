// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/timeline.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {
    class TimelineActor : public caf::event_based_actor {
      public:
        TimelineActor(
            caf::actor_config &cfg,
            const utility::JsonStore &jsn,
            const caf::actor &playlist = caf::actor());
        TimelineActor(
            caf::actor_config &cfg,
            const std::string &name    = "Timeline",
            const utility::Uuid &uuid  = utility::Uuid::generate(),
            const caf::actor &playlist = caf::actor());
        ~TimelineActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "TimelineActor";
        void init();

        caf::behavior make_behavior() override { return behavior_; }

        void deliver_media_pointer(
            const int logical_frame,
            const media::MediaType media_type,
            caf::typed_response_promise<media::AVFrameID> rp);

        void add_item(const utility::UuidActor &ua);

        void add_media(
            const utility::UuidActor &ua,
            const utility::Uuid &uuid_before,
            caf::typed_response_promise<utility::UuidActor> rp);
        void add_media(
            caf::actor actor,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());
        bool remove_media(caf::actor actor, const utility::Uuid &uuid);

        void insert_items(
            const int index,
            const utility::UuidActorVector &uav,
            caf::typed_response_promise<utility::JsonStore> rp);

        void remove_items(
            const int index,
            const int count,
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp);

        void erase_items(
            const int index,
            const int count,
            caf::typed_response_promise<utility::JsonStore> rp);

        void sort_alphabetically();

        void on_exit() override;

        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);

        void item_event_callback(const utility::JsonStore &event, Item &item);

      private:
        caf::behavior behavior_;
        Timeline base_;
        caf::actor event_group_;

        utility::UuidActorMap actors_;
        utility::UuidActorMap media_actors_;

        caf::actor_addr playlist_;
        bool content_changed_{false};
        utility::UuidActor playhead_;
        caf::actor history_;
        // bool update_edit_list_;
        // utility::EditList edit_list_;
    };

} // namespace timeline
} // namespace xstudio
