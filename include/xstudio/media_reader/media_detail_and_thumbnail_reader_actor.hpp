// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include <queue>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/uuid.hpp"

// MediaDetail is crucial to the set-up of a media source so that it is playable.
// As such, we process all pending requests for media detail before addressing
// requests for thumbnails. The assumption is that thumbnail generation is
// somewhat slower and also heavier on IO as we are often decoding whole frames
// of video data so we don't want to hold up requests for MediaDetail. However,
// the global reader actor creates a pool of these and as such the requests are
// distributed across instances and it is therefore possible that some thumbnails
// will start arriving before some MediaDetail requests because we don't have
// a single queue

namespace xstudio {
namespace media_reader {

    class MediaDetailAndThumbnailReaderActor : public caf::event_based_actor {

      public:
        MediaDetailAndThumbnailReaderActor(
            caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());
        ~MediaDetailAndThumbnailReaderActor() override = default;
        caf::behavior make_behavior() override { return behavior_; }
        const char *name() const override { return NAME.c_str(); }

      private:
        void send_error_to_source(const caf::actor_addr &addr, const caf::error &err);
        void process_get_media_detail_queue();
        void process_get_thumbnail_queue();
        bool queues_empty() const {
            return media_detail_request_queue_.empty() && thumbnail_request_queue_.empty();
        }
        void get_thumbnail_from_reader_plugin(
            caf::actor &,
            const media::AVFrameID,
            const size_t,
            caf::typed_response_promise<thumbnail::ThumbnailBufferPtr>);
        void continue_processing_queue();

      private:
        struct MediaDetailRequest {

            MediaDetailRequest(const MediaDetailRequest &o) = default;
            MediaDetailRequest()                            = default;
            MediaDetailRequest(
                const caf::uri uri,
                const caf::actor_addr key,
                caf::typed_response_promise<media::MediaDetail> rp)
                : uri_(std::move(uri)), key_(std::move(key)), rp_(std::move(rp)) {}

            caf::uri uri_;
            caf::actor_addr key_;
            caf::typed_response_promise<media::MediaDetail> rp_;
        };

        struct ThumbnailRequest {

            ThumbnailRequest(const ThumbnailRequest &o) = default;
            ThumbnailRequest()                          = default;
            ThumbnailRequest(
                const media::AVFrameID media_pointer,
                const size_t size,
                caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp)
                : media_pointer_(std::move(media_pointer)), size_(size), rp_(std::move(rp)) {}

            media::AVFrameID media_pointer_;
            size_t size_;
            caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp_;
        };

      private:
        inline static const std::string NAME = "MediaDetailAndThumbnailReaderActor";
        caf::behavior behavior_;

        int num_detail_requests_since_thumbnail_request_ = {0};

        std::queue<MediaDetailRequest> media_detail_request_queue_;
        std::queue<ThumbnailRequest> thumbnail_request_queue_;

        std::map<caf::uri, media::MediaDetail> media_detail_cache_;
        std::map<caf::uri, utility::time_point> media_detail_cache_age_;

        utility::Uuid uuid_;
        std::vector<caf::actor> plugins_;
        std::map<utility::Uuid, caf::actor> plugins_map_;
    };
} // namespace media_reader
} // namespace xstudio