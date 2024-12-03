// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace media_reader {

    class CachingMediaReaderActor : public caf::event_based_actor {
      public:
        CachingMediaReaderActor(
            caf::actor_config &cfg,
            const utility::Uuid &plugin_uuid,
            caf::actor image_cache = caf::actor(),
            caf::actor audio_cache = caf::actor());
        ~CachingMediaReaderActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        const char *name() const override { return NAME.c_str(); }

      private:
        struct ImmediateImageReqest {

            ImmediateImageReqest(
                const media::AVFrameID mptr,
                caf::actor &playhead,
                const utility::time_point &time,
                timebase::flicks playhead_position)
                : mptr_(mptr), playhead_(playhead), time_point_(time), playhead_position_(playhead_position) {}

            ImmediateImageReqest(const ImmediateImageReqest &) = default;
            ImmediateImageReqest()                             = default;

            ImmediateImageReqest &operator=(const ImmediateImageReqest &o) = default;

            media::AVFrameID mptr_;
            caf::actor playhead_;
            utility::time_point time_point_;
            timebase::flicks playhead_position_;
        };

        void do_urgent_get_image();
        void receive_image_buffer_request(
            const media::AVFrameID &mptr,
            caf::actor playhead,
            const utility::Uuid playhead_uuid,
            const utility::time_point &tp,
            const timebase::flicks playhead_position);

        std::map<const utility::Uuid, ImmediateImageReqest> pending_get_image_requests_;

        inline static const std::string NAME = "CachingMediaReaderActor";

        caf::behavior behavior_;
        // bool sequential_access_;
        caf::actor image_cache_;
        caf::actor audio_cache_;

        bool urgent_worker_busy_ = {false};

        caf::actor urgent_worker_;
        caf::actor precache_worker_;
        caf::actor audio_worker_;
    };
} // namespace media_reader
} // namespace xstudio
