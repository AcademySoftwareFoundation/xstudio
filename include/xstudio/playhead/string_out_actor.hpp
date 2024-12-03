// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
// #include <chrono>

#include "xstudio/utility/uuid.hpp"
#include "xstudio/media/media.hpp"

namespace xstudio {
namespace playhead {

    /* This simple actor allows a number of sources to be 'strung' together
    so that they can be played in a sequence. This supports the 'String'
    compare mode. It provides the one message handler required to be 
    playable by a SubPlayhead, plug it handles event messages emitted by
    any of its sources. 
    
    This handles mixed frame rate sources.
    
    */
    class StringOutActor : public caf::event_based_actor {
      public:

        StringOutActor(
            caf::actor_config &cfg,
            const utility::UuidActorVector &sources);

        virtual ~StringOutActor() = default;

        const char *name() const override { return NAME.c_str(); }

      private:

      void build_frame_map(
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate,
            caf::typed_response_promise<media::FrameTimeMapPtr> rp);


        void finalise_frame_map(caf::typed_response_promise<media::FrameTimeMapPtr> rp);

        inline static const std::string NAME = "StringOutActor";        

        caf::behavior make_behavior() override {
            return behavior_;
        }

        utility::UuidActorVector source_actors_;
        caf::actor event_group_;
        std::map< utility::Uuid, media::FrameTimeMapPtr > source_frames_;
        caf::behavior behavior_;

    };
} // namespace playhead
} // namespace xstudio