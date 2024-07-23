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
            const std::string &name   = "Stack",
            const utility::Uuid &uuid = utility::Uuid::generate());
        ~StackActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "StackActor";
        void init();
        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

        void add_item(const utility::UuidActor &ua);
        caf::actor
        deserialise(const utility::JsonStore &value, const bool replace_item = false);
        void item_event_callback(const utility::JsonStore &event, Item &item);
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

        void move_items(
            const int src_index,
            const int count,
            const int dst_index,
            caf::typed_response_promise<utility::JsonStore> rp);

      private:
        caf::behavior behavior_;
        Stack base_;
        caf::actor event_group_;
        std::map<utility::Uuid, caf::actor> actors_;
    };
} // namespace timeline
} // namespace xstudio
