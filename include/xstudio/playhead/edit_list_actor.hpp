// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {
    class EditListActor : public caf::event_based_actor {
      public:
        // SelectionStringActor(
        // 	caf::actor_config& cfg,
        // 	const utility::JsonStore &jsn
        // );
        EditListActor(
            caf::actor_config &cfg,
            const std::string &name,
            const std::vector<caf::actor> &media_clip_list,
            const media::MediaType mt);
        ~EditListActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "EditListActor";

        caf::behavior make_behavior() override { return behavior_; }

        void full_media_key_list(caf::typed_response_promise<media::MediaKeyVector> rp);

        caf::actor media_source_actor(const utility::Uuid &source_uuid);

        void recursive_deliver_all_media_pointers(
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate,
            const media::MediaType media_type,
            const int clip_index,
            const timebase::flicks clip_start_time_point,
            std::shared_ptr<media::FrameTimeMap> result,
            caf::typed_response_promise<media::FrameTimeMap> rp);

      private:
        caf::behavior behavior_;
        caf::actor event_group_;
        std::map<utility::Uuid, caf::actor> source_actors_per_uuid_;
        std::vector<caf::actor> source_actors_;
        utility::EditList edit_list_;
        int frames_offset_;
    };
} // namespace playhead
} // namespace xstudio
