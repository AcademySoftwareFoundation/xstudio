// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {
    class TrackActor : public caf::event_based_actor {
      public:
        TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &item);
        TrackActor(
            caf::actor_config &cfg,
            const std::string &name           = "Track",
            const media::MediaType media_type = media::MediaType::MT_IMAGE,
            const utility::Uuid &uuid         = utility::Uuid::generate());
        ~TrackActor() override = default;

        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

      private:
        inline static const std::string NAME = "TrackActor";
        void init();

        caf::behavior make_behavior() override { return behavior_; }
        // void deliver_media_pointer(
        //     const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp);
        void add_item(const utility::UuidActor &ua);
        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);
        void item_event_callback(const utility::JsonStore &event, Item &item);

        void split_item(
            const Items::const_iterator &item,
            const int frame,
            caf::typed_response_promise<utility::JsonStore> rp);

        void insert_items(
            const int index,
            const utility::UuidActorVector &uav,
            caf::typed_response_promise<utility::JsonStore> rp);

        void insert_items_at_frame(
            const int frame,
            const utility::UuidActorVector &uav,
            caf::typed_response_promise<utility::JsonStore> rp);

        void remove_items_at_frame(
            const int frame,
            const int duration,
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp);

        void remove_items(
            const int index,
            const int count,
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp);

        void erase_items_at_frame(
            const int frame,
            const int duration,
            caf::typed_response_promise<utility::JsonStore> rp);

        void erase_items(
            const int index,
            const int count,
            caf::typed_response_promise<utility::JsonStore> rp);

        void move_items(
            const int src_index,
            const int count,
            const int dst_index,
            caf::typed_response_promise<utility::JsonStore> rp);

        void move_items_at_frame(
            const int frame,
            const int duration,
            const int dest_frame,
            const bool insert,
            caf::typed_response_promise<utility::JsonStore> rp);

      private:
        caf::behavior behavior_;
        Track base_;
        caf::actor event_group_;
        std::map<utility::Uuid, caf::actor> actors_;

        // bool update_edit_list_;
        // utility::EditList edit_list_;
    };
} // namespace timeline
} // namespace xstudio
