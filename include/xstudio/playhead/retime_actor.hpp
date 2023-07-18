// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/media/media.hpp"

namespace xstudio {
namespace playhead {
    class RetimeActor : public caf::event_based_actor {
      public:
        RetimeActor(
            caf::actor_config &cfg,
            const std::string &name,
            caf::actor &source,
            const media::MediaType);
        ~RetimeActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "RetimeActor";

        caf::behavior make_behavior() override { return behavior_; }

      private:
        enum RetimeFrameResult { FAIL, OUT_OF_RANGE, HELD_FRAME, LOOPED_RANGE, IN_RANGE };

        void set_duration(const timebase::flicks &new_duration);
        int apply_retime(const int logical_frame, RetimeFrameResult &retime_result);
        void recursive_deliver_all_media_pointers(
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate,
            const media::MediaType media_type,
            const int clip_index,
            const timebase::flicks clip_start_time_point,
            std::shared_ptr<media::FrameTimeMap> result,
            caf::typed_response_promise<media::FrameTimeMap> rp);
        void get_source_edit_list(const media::MediaType mt);

        caf::behavior behavior_;
        caf::actor event_group_;
        caf::actor source_;
        int frames_offset_;
        utility::Uuid uuid_;
        utility::EditList source_edit_list_, retime_edit_list_;
        OverflowMode overflow_mode_       = {OM_HOLD};
        timebase::flicks forced_duration_ = {timebase::k_flicks_zero_seconds};
    };
} // namespace playhead
} // namespace xstudio
