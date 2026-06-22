// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/allowed_unsafe_message_type.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/uuid.hpp"

#include <map>
#include <vector>

namespace xstudio {
namespace media_reader {

    /**
     *  @brief FrameRequest struct.
     *
     *  @details
     *   A simple struct to store details about a request for a media (video)
     *   frame that might come from a playhead wishing to broadcast the video
     *   frame to the xstudio viewport. These requests are processed by the
     *   CacheingMediaReaderActor(s).
     */
    struct FrameRequest {

        FrameRequest() = default;

        FrameRequest(
            std::shared_ptr<const media::AVFrameID> fi,
            const utility::time_point &rb,
            const utility::Uuid &ph)
            : requested_frame_(std::move(fi)),
              required_by_(rb),
              requesting_playhead_uuid_(ph) {}

        FrameRequest(const FrameRequest &o) = default;

        FrameRequest &operator=(const FrameRequest &o) = default;

        std::shared_ptr<const media::AVFrameID> requested_frame_;
        utility::time_point required_by_;
        utility::Uuid requesting_playhead_uuid_;
    };


    /**
     *  @brief FrameRequestQueue class.
     *
     *  @details
     *   This class implements the logic of managing requests for frames made by
     *   playheads to the media readers. As playheads change their position during
     *   playback, user scrubbing in the timeline or any other means they make
     *   requests for frames to be read and cached ready for display. Requests for
     *   frames to be pre-read ready for playback have to be removed whenever the
     *   playhead changes position. If the reader can't keep up with the playhead
     *   then 'out of date' unfulfilled requests that were needed in the past need
     *   to be pruned and so-on
     */
    class FrameRequestQueue {

      public:
        /**
         *  @brief Constructor (default)
         */
        FrameRequestQueue() = default;

        /**
         *  @brief Destructor (default)
         */
        ~FrameRequestQueue() = default;

        /**
         *  @brief Take 'stale' frame requests out of the queue
         *
         *  @details For each playhead that has made a request for a frame to be loaded,
         *  we remove any requests that were needed for a timepoint that is now in the
         *  past, except for the most recent - this still allows the reader to deliver the
         *  most recently needed frame even if it's late.
         */
        void prune_stale_frame_requests();

        /**
         *   @brief Get the next ordered frame request
         *
         */
        std::optional<FrameRequest>
        pop_request(const std::map<utility::Uuid, int> &exclude_playheads);

        /**
         *   @brief Add a request to the queue
         *
         */
        void add_frame_request(
            const media::AVFrameID &frame_info,
            const utility::time_point &required_by,
            const utility::Uuid &requesting_playhead_uuid);

        /**
         *   @brief Add a request to the queue
         *
         */
        void add_frame_requests(
            const media::AVFrameIDsAndTimePoints &frames_info,
            const utility::Uuid &requesting_playhead_uuid);

        /**
         *   @brief Remove all frame requests in the queue originating from
         *   the indicated playhead
         */
        void clear_pending_requests(const utility::Uuid &playhead_uuid);

      private:
        std::vector<std::shared_ptr<FrameRequest>> queue_;
    };

} // namespace media_reader
} // namespace xstudio

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::optional<xstudio::media_reader::FrameRequest>)
