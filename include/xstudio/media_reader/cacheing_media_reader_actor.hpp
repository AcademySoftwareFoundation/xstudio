// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

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
        struct ImmediateImageWaiter {
            ImmediateImageWaiter(
                const utility::Uuid &playhead_uuid,
                caf::typed_response_promise<ImageBufPtr> &rp)
                : playhead_uuid_(playhead_uuid), response_promise_(rp) {}

            utility::Uuid playhead_uuid_;
            caf::typed_response_promise<ImageBufPtr> response_promise_;
        };

        struct PendingImmediateImageRequest {
            PendingImmediateImageRequest() = default;

            explicit PendingImmediateImageRequest(const media::AVFrameID &mptr) : mptr_(mptr) {}

            media::AVFrameID mptr_;
            std::vector<ImmediateImageWaiter> waiters_;
            bool queued_{false};
            bool active_{false};
        };

        struct WorkerPoolState {
            std::vector<caf::actor> workers_;
            std::vector<bool> busy_;
            size_t next_dispatch_index_{0};
            size_t next_round_robin_index_{0};
        };

        struct WorkerPreferences {
            size_t urgent_worker_count_{1};
            size_t precache_worker_count_{1};
            size_t audio_worker_count_{1};
        };

        WorkerPreferences worker_preferences(const utility::JsonStore &js) const;
        void spawn_worker_pool(
            WorkerPoolState &pool,
            const utility::Uuid &media_reader_plugin_uuid,
            const utility::JsonStore &js,
            size_t worker_count);

        caf::actor next_worker(WorkerPoolState &pool);
        std::optional<size_t> acquire_idle_worker(WorkerPoolState &pool);
        void release_worker(WorkerPoolState &pool, size_t worker_index);

        void cancel_superseded_request(const utility::Uuid &playhead_uuid);
        void enqueue_urgent_image_request(
            const media::AVFrameID &mptr,
            const utility::Uuid &playhead_uuid,
            caf::typed_response_promise<ImageBufPtr> &rp);
        void dispatch_pending_urgent_image_requests();
        void dispatch_urgent_image_request(
            const media::MediaKey &key, PendingImmediateImageRequest &request, size_t worker_index);
        void finish_urgent_image_request(
            const media::MediaKey &key,
            size_t worker_index,
            const ImageBufPtr &buf,
            const caf::error *err = nullptr);

        caf::typed_response_promise<ImageBufPtr> receive_image_buffer_request(
            const media::AVFrameID &mptr, const utility::Uuid playhead_uuid);

        ImageBufPtr make_error_buffer(const caf::error &err, const media::AVFrameID &mptr);

        inline static const std::string NAME = "CachingMediaReaderActor";

        caf::behavior behavior_;
        caf::actor image_cache_;
        caf::actor audio_cache_;

        WorkerPoolState urgent_workers_;
        WorkerPoolState precache_workers_;
        WorkerPoolState audio_workers_;

        std::unordered_map<utility::Uuid, media::MediaKey> playhead_pending_image_requests_;
        std::unordered_map<media::MediaKey, PendingImmediateImageRequest>
            pending_get_image_requests_;
        std::deque<media::MediaKey> pending_get_image_order_;

        ImageBufPtr blank_image_;
    };
} // namespace media_reader
} // namespace xstudio
