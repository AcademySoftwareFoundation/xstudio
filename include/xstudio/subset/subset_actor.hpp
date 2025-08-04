// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/subset/subset.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/json_store/json_store_handler.hpp"

namespace xstudio {
namespace subset {
    class SubsetActor : public caf::event_based_actor {
      public:
        SubsetActor(caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn);
        SubsetActor(
            caf::actor_config &cfg,
            caf::actor playlist,
            const std::string &name,
            const utility::Uuid &uuid        = utility::Uuid::generate(),
            const std::string &override_type = "Subset");
        ~SubsetActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        caf::behavior make_behavior() override {
            return message_handler()
                .or_else(base_.container_message_handler(this))
                .or_else(jsn_handler_.message_handler());
        }

      private:
        inline static const std::string NAME = "SubsetActor";
        void init();

        caf::message_handler message_handler();


        void deliver_media_pointer(
            const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp);

        void sort_by_media_display_info(const int sort_column_index, const bool ascending);

        void add_media(
            const utility::UuidActor &ua,
            const utility::Uuid &uuid_before,
            caf::typed_response_promise<utility::UuidActor> rp);
        void add_media(
            caf::actor actor,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());
        bool remove_media(caf::actor actor, const utility::Uuid &uuid);

      protected:
        utility::JsonStore serialise() const { return base_.serialise(); }
        void monitor_media(const caf::actor &actor);

        std::map<caf::actor_addr, caf::disposable> monitor_;

        utility::JsonStore playhead_serialisation_;

        caf::behavior behavior_;
        caf::actor_addr playlist_;
        caf::actor change_event_group_;
        caf::actor selection_actor_;
        Subset base_;
        utility::UuidActorMap actors_;
        utility::UuidActor playhead_;

        json_store::JsonStoreHandler jsn_handler_;
    };
} // namespace subset
} // namespace xstudio
