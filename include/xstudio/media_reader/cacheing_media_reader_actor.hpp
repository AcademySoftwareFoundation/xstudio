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
                const media::AVFrameID mptr, caf::typed_response_promise<ImageBufPtr> &rp)
                : mptr_(mptr), response_promise_(rp) {}

            ImmediateImageReqest(const ImmediateImageReqest &) = default;
            ImmediateImageReqest()                             = default;

            ImmediateImageReqest &operator=(const ImmediateImageReqest &o) = default;

            media::AVFrameID mptr_;
            caf::typed_response_promise<ImageBufPtr> response_promise_;
        };

        void do_urgent_get_image();
        caf::typed_response_promise<ImageBufPtr> receive_image_buffer_request(
            const media::AVFrameID &mptr, const utility::Uuid playhead_uuid);

        ImageBufPtr make_error_buffer(const caf::error &err, const media::AVFrameID &mptr);

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

        ImageBufPtr blank_image_;
    };
} // namespace media_reader
} // namespace xstudio
