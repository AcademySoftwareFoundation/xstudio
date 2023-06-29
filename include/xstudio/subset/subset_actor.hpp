// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/subset/subset.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace subset {
    class SubsetActor : public caf::event_based_actor {
      public:
        SubsetActor(caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn);
        SubsetActor(caf::actor_config &cfg, caf::actor playlist, const std::string &name);
        ~SubsetActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "SubsetActor";
        void init();

        caf::behavior make_behavior() override { return behavior_; }

        void deliver_media_pointer(
            const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp);

        void sort_alphabetically();

        void add_media(
            const utility::UuidActor &ua,
            const utility::Uuid &uuid_before,
            caf::typed_response_promise<utility::UuidActor> rp);
        void add_media(
            caf::actor actor,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());
        bool remove_media(caf::actor actor, const utility::Uuid &uuid);

      private:
        caf::behavior behavior_;
        caf::actor_addr playlist_;
        caf::actor event_group_, change_event_group_;
        Subset base_;
        utility::UuidActorMap actors_;
        utility::UuidActor playhead_;
    };
} // namespace subset
} // namespace xstudio
