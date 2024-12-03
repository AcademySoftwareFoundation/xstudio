// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/stack.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {
    class StackActor : public caf::event_based_actor {
      public:
        StackActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        StackActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &item);
        StackActor(
            caf::actor_config &cfg,
            const std::string &name        = "Stack",
            const utility::FrameRate &rate = utility::FrameRate(),
            const utility::Uuid &uuid      = utility::Uuid::generate());

        StackActor(caf::actor_config &cfg, const Item &item);
        StackActor(caf::actor_config &cfg, const Item &item, Item &nitem);
        ~StackActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "StackActor";
        void init();
        void on_exit() override;
        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

        void add_item(const utility::UuidActor &ua);
        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);
        void deserialise();
        void item_event_callback(const utility::JsonStore &event, Item &item);

        std::pair<utility::JsonStore, std::vector<timeline::Item>>
        remove_items(const int index, const int count = 1);

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

        void move_items(
            caf::typed_response_promise<utility::JsonStore> rp,
            const int src_index,
            const int count,
            const int dst_index);

      private:
        Stack base_;
        std::map<utility::Uuid, caf::actor> actors_;
        // might need to prune.. ?
        std::set<utility::Uuid> events_processed_;
    };
} // namespace timeline
} // namespace xstudio
