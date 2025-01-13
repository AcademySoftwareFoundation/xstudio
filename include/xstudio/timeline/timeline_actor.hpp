// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/timeline.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/notification_handler.hpp"

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
            const std::string &name        = "Timeline",
            const utility::FrameRate &rate = utility::FrameRate(),
            const utility::Uuid &uuid      = utility::Uuid::generate(),
            const caf::actor &playlist     = caf::actor(),
            const bool with_tracks         = false);
        ~TimelineActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "TimelineActor";
        void init();

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler()
                .or_else(base_.container_message_handler(this))
                .or_else(notification_.message_handler(this, base_.event_group()));
        }

        void add_item(const utility::UuidActor &ua);

        void add_media(
            caf::actor actor,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());

        void deliver_media_pointer(
            caf::typed_response_promise<media::AVFrameID> rp,
            const int logical_frame,
            const media::MediaType media_type);

        void add_media(
            caf::typed_response_promise<utility::UuidActor> rp,
            const utility::UuidActor &ua,
            const utility::Uuid &uuid_before);
        bool remove_media(caf::actor actor, const utility::Uuid &uuid);

        void insert_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int index,
            const utility::UuidActorVector &uav);

        void remove_items(
            caf::typed_response_promise<
                std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
            const int index,
            const int count = 1);

        void erase_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int index,
            const int count = 1);

        std::pair<utility::JsonStore, std::vector<timeline::Item>>
        remove_items(const int index, const int count = 1);

        void sort_by_media_display_info(const int sort_column_index, const bool ascending);

        void
        bake(caf::typed_response_promise<utility::UuidActor> rp, const utility::UuidSet &uuids);

        void on_exit() override;

        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);

        void item_pre_event_callback(const utility::JsonStore &event, Item &item);
        void item_post_event_callback(const utility::JsonStore &event, Item &item);

        void export_otio(
            caf::typed_response_promise<bool> rp,
            const std::string &otio_str,
            const caf::uri &path,
            const std::string &type);

        void export_otio_as_string(caf::typed_response_promise<std::string> rp);

      private:
        Timeline base_;
        caf::actor change_event_group_;

        utility::Uuid history_uuid_;
        caf::actor history_;

        caf::actor selection_actor_;

        utility::UuidActorMap actors_;
        utility::UuidActorMap media_actors_;

        utility::JsonStore playhead_serialisation_;

        caf::actor_addr playlist_;
        bool content_changed_{false};
        utility::UuidActor playhead_;
        // might need to prune.. ?
        std::set<utility::Uuid> events_processed_;
        utility::UuidActorVector video_tracks_;

        // current selection
        utility::UuidActorVector selection_;

        utility::NotificationHandler notification_;
    };

} // namespace timeline
} // namespace xstudio
