// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <utility>

#include "xstudio/media/media.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/playhead/playhead_selection.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {
    class PlayheadSelectionActor : public caf::event_based_actor {
      public:
        PlayheadSelectionActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn, caf::actor playlist);
        PlayheadSelectionActor(
            caf::actor_config &cfg, const std::string &name, caf::actor playlist);
        ~PlayheadSelectionActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "PlayheadSelectionActor";
        void init();

        caf::behavior make_behavior() override { return behavior_; }

        void select_media(const utility::UuidList &media_uuids);

        void insert_actor(
            caf::actor actor, const utility::Uuid media_uuid, const utility::Uuid &before_uuid);

        void remove_dead_actor(caf::actor_addr actor);

        void select_all();

        void select_one();

        void select_next_media_item(const int skip_by);

      private:
        PlayheadSelection base_;
        caf::behavior behavior_;
        caf::actor event_group_;
        caf::actor playlist_;
        std::map<utility::Uuid, caf::actor> source_actors_;
    };
} // namespace playhead
} // namespace xstudio
