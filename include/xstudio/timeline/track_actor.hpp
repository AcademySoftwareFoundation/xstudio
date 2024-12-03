// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/notification_handler.hpp"

namespace xstudio {
namespace timeline {
    class TrackActor : public caf::event_based_actor {
      public:
        TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &item);
        TrackActor(
            caf::actor_config &cfg,
            const std::string &name           = "Track",
            const utility::FrameRate &rate    = utility::FrameRate(),
            const media::MediaType media_type = media::MediaType::MT_IMAGE,
            const utility::Uuid &uuid         = utility::Uuid::generate());
        TrackActor(caf::actor_config &cfg, const Item &item);
        TrackActor(caf::actor_config &cfg, const Item &item, Item &nitem);

        ~TrackActor() override = default;

        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "TrackActor";
        void init();

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler()
                .or_else(base_.container_message_handler(this))
                .or_else(notification_.message_handler(this, base_.event_group()));
        }

        // void deliver_media_pointer(
        //     const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp);
        void add_item(const utility::UuidActor &ua);
        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);
        void deserialise();
        void item_event_callback(const utility::JsonStore &event, Item &item);

        void split_item(
            caf::typed_response_promise<utility::JsonStore> rp,
            const Items::const_iterator &item,
            const int frame);

        void insert_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int index,

            const utility::UuidActorVector &uav);

        void insert_items_at_frame(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int frame,
            const utility::UuidActorVector &uav);

        void remove_items_at_frame(
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
            const int frame,
            const int duration,
            const bool add_gap,
            const bool collapse_gaps);

        void remove_items(
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
            const int index,
            const int count,
            const bool add_gap,
            const bool collapse_gaps);

        void erase_items_at_frame(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int frame,
            const int duration,
            const bool add_gap,
            const bool collapse_gaps);

        void erase_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int index,
            const int count,
            const bool add_gap,
            const bool collapse_gaps);

        void move_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int src_index,
            const int count,
            const int dst_index,
            const bool add_gap,
            const bool replace_with_gap = false);

        void move_items_at_frame(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int frame,
            const int duration,
            const int dest_frame,
            const bool insert,
            const bool add_gap,
            const bool replace_with_gap = false);

        void merge_gaps(caf::typed_response_promise<utility::JsonStore> rp);
        utility::JsonStore merge_gaps();

        std::pair<utility::JsonStore, std::vector<timeline::Item>> remove_items(
            const int index, const int count, const bool add_gap, const bool collapse_gaps);

        // void merge_gaps(caf::typed_response_promise<utility::JsonStore> rp);

      private:
        Track base_;
        std::map<utility::Uuid, caf::actor> actors_;
        // might need to prune.. ?
        std::set<utility::Uuid> events_processed_;
        utility::NotificationHandler notification_;

    };
} // namespace timeline
} // namespace xstudio
