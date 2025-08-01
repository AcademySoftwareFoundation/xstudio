// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
// #include <chrono>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {

    class SubPlayhead : public caf::event_based_actor {
      public:
        SubPlayhead(
            caf::actor_config &cfg,
            const std::string &name,
            caf::actor source,
            caf::actor parent,
            const timebase::flicks loop_in_point_,
            const timebase::flicks loop_out_point_,
            const utility::TimeSourceMode time_source_mode_,
            const utility::FrameRate override_frame_rate_,
            const media::MediaType media_type);
        ~SubPlayhead() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "SubPlayhead";

        void set_position(
            const timebase::flicks time,
            const bool forwards,
            const bool playing            = false,
            const float velocity          = 1.0f,
            const bool force_updates      = false,
            const bool timeline_scrubbing = false);

        void init();

        caf::behavior make_behavior() override { return behavior_; }

        void broadcast_image_frame(
            const utility::time_point when_to_show_frame,
            std::shared_ptr<const media::AVFrameID> frame,
            const bool is_future_frame,
            const timebase::flicks timeline_pts);

        void broadcast_audio_frame(
            const utility::time_point when_to_show_frame,
            std::shared_ptr<const media::AVFrameID> frame,
            const bool is_future_frame);

        std::vector<timebase::flicks> get_lookahead_frame_pointers(
            media::AVFrameIDsAndTimePoints &result, const int max_num_frames);

        void request_future_frames();

        void update_playback_precache_requests(caf::typed_response_promise<bool> &rp);

        void make_static_precache_request(
            caf::typed_response_promise<bool> &rp, const bool start_precache);

        void make_prefetch_requests_for_colour_pipeline(
            const media::AVFrameIDsAndTimePoints &lookahead_frames);

        void receive_image_from_cache(
            media_reader::ImageBufPtr image_buffer,
            const media::AVFrameID mptr,
            const utility::time_point tp);

        void get_full_timeline_frame_list(caf::typed_response_promise<caf::actor> rp);

        std::shared_ptr<const media::AVFrameID> get_frame(
            const timebase::flicks &time,
            timebase::flicks &frame_period,
            timebase::flicks &timeline_pts);

        void get_position_after_step_by_frames(
            const timebase::flicks start_position,
            caf::typed_response_promise<timebase::flicks> &rp,
            int step_frames,
            const bool loop);

        void set_in_and_out_frames();

        typedef std::vector<std::tuple<utility::Uuid, std::string, int, int>> BookmarkRanges;

        void extend_bookmark_frame(
            const bookmark::BookmarkDetail &detail,
            const int logical_playhead_frame,
            BookmarkRanges &bookmark_ranges);

        void full_bookmarks_update();

        void fetch_bookmark_annotations(BookmarkRanges bookmark_ranges);

        void add_annotations_data_to_frame(media_reader::ImageBufPtr &frame);

        void bookmark_deleted(const utility::Uuid &bookmark_uuid);

        void bookmark_changed(const utility::UuidActor bookmark);

      protected:
        int logical_frame_                = {0};
        timebase::flicks position_flicks_ = timebase::k_flicks_zero_seconds;

        bool playing_forwards_    = {true};
        float playback_velocity_  = {1.0f};
        int read_ahead_frames_    = {0};
        int precache_start_frame_ = {std::numeric_limits<int>::lowest()};

        int pre_cache_read_ahead_frames_                           = {32};
        std::chrono::milliseconds static_cache_delay_milliseconds_ = {
            std::chrono::milliseconds(500)};
        caf::behavior behavior_;
        utility::Container base_;
        caf::actor pre_reader_;
        caf::actor source_;
        caf::actor parent_;
        caf::actor event_group_;
        caf::actor current_media_actor_;

        utility::Uuid current_media_source_uuid_;
        utility::time_point last_image_timepoint_;
        bool waiting_for_next_frame_ = {false};
        timebase::flicks loop_in_point_;
        timebase::flicks loop_out_point_;
        utility::TimeSourceMode time_source_mode_;
        utility::FrameRate override_frame_rate_;
        const media::MediaType media_type_;
        std::shared_ptr<const media::AVFrameID> previous_frame_;
        utility::UuidSet all_media_uuids_;

        media::FrameTimeMap full_timeline_frames_;
        media::FrameTimeMap::iterator in_frame_, out_frame_, first_frame_, last_frame_;
        xstudio::bookmark::BookmarkAndAnnotations bookmarks_;
        BookmarkRanges bookmark_ranges_;

        typedef std::pair<media_reader::ImageBufPtr, colour_pipeline::ColourPipelineDataPtr>
            ImageAndLut;
        bool content_changed_{false};
        bool up_to_date_{false};
    };
} // namespace playhead
} // namespace xstudio