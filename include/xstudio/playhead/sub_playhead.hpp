// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
// #include <chrono>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
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
            utility::UuidActor source,
            caf::actor parent,
            const bool source_is_timeline,
            const timebase::flicks loop_in_point_,
            const timebase::flicks loop_out_point_,
            const utility::TimeSourceMode time_source_mode_,
            const utility::FrameRate override_frame_rate_,
            const media::MediaType media_type,
            const utility::Uuid & uuid = utility::Uuid::generate());
        ~SubPlayhead();

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "SubPlayhead";

        void set_position(
            const timebase::flicks time,
            const bool forwards,
            const bool playing,
            const float velocity,
            const bool force_updates,
            const bool active_in_ui,
            const bool scrubbing);

        void on_exit() override;

        void init();

        caf::behavior make_behavior() override {
            return behavior_;
        }

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
            const utility::time_point tp,
            const timebase::flicks timeline_pts);

        void get_full_timeline_frame_list(
            caf::typed_response_promise<caf::actor> rp, const bool retry = false);

        std::shared_ptr<const media::AVFrameID> get_frame(
            const timebase::flicks &time,
            timebase::flicks &frame_period,
            timebase::flicks &timeline_pts);

        void get_position_after_step_by_frames(
            const timebase::flicks start_position,
            caf::typed_response_promise<timebase::flicks> &rp,
            int step_frames,
            const bool loop);

        timebase::flicks get_next_or_previous_clip_start_position(
            const timebase::flicks start_position, const bool next_clip);

        void store_media_frame_ranges();

        void update_retiming();

        void set_in_and_out_frames();

        typedef std::vector<std::tuple<utility::Uuid, std::string, int, int>> BookmarkRanges;

        void extend_bookmark_frame(
            const bookmark::BookmarkDetail &detail,
            const int logical_playhead_frame,
            BookmarkRanges &bookmark_ranges);

        void full_bookmarks_update(caf::typed_response_promise<bool> done);

        void fetch_bookmark_annotations(
            BookmarkRanges bookmark_ranges, caf::typed_response_promise<bool> done);

        void add_annotations_data_to_frame(media_reader::ImageBufPtr &frame);

        void bookmark_deleted(const utility::Uuid &bookmark_uuid);

        void bookmark_changed(const utility::UuidActor bookmark);

      protected:

        media::FrameTimeMap::iterator current_frame_iterator();
        media::FrameTimeMap::iterator current_frame_iterator(const timebase::flicks t);

        inline int logical_frame_from_pts(const timebase::flicks t) const {
            // we use logical_frames_ as a lookup to get the corresponding logical
            // frame at time t - this is much more efficient than looking up from
            // an iterator from retimed_frames_ and using std::distance to get
            // the position.
            auto p = logical_frames_.lower_bound(t);
            if (p == logical_frames_.end()) {
                return logical_frames_.size() ? logical_frames_.rbegin()->second : 0;
            }
            return p->second;
        }

        const std::string name_;
        const utility::Uuid uuid_;

        int logical_frame_                = {0};
        timebase::flicks position_flicks_ = timebase::k_flicks_zero_seconds;
        bool playing_forwards_    = {true};
        float playback_velocity_  = {1.0f};
        int read_ahead_frames_    = {0};
        int precache_start_frame_ = {std::numeric_limits<int>::lowest()};
        int64_t frame_offset_         = {0};
        timebase::flicks forced_duration_ = timebase::k_flicks_zero_seconds;
        utility::FrameRate default_rate_ = utility::FrameRate(timebase::k_flicks_24fps);
        const bool source_is_timeline_;

        int pre_cache_read_ahead_frames_                           = {32};
        std::chrono::milliseconds static_cache_delay_milliseconds_ = {
            std::chrono::milliseconds(500)};
        caf::behavior behavior_;
        caf::actor pre_reader_;
        utility::UuidActor source_;
        caf::actor parent_;
        caf::actor current_media_actor_;
        caf::actor global_prefs_actor_;
        caf::actor event_group_;

        utility::Uuid current_media_source_uuid_;
        utility::time_point last_image_timepoint_;
        bool waiting_for_next_frame_ = {false};
        timebase::flicks loop_in_point_;
        timebase::flicks loop_out_point_;
        utility::TimeSourceMode time_source_mode_;
        utility::FrameRate override_frame_rate_;
        media::MediaType media_type_;
        std::shared_ptr<const media::AVFrameID> previous_frame_;
        utility::UuidSet all_media_uuids_;

        std::map<timebase::flicks, int> logical_frames_;
        media::FrameTimeMap full_timeline_frames_;
        media::FrameTimeMap retimed_frames_;
        media::FrameTimeMap::iterator in_frame_, out_frame_, first_frame_, last_frame_;
        xstudio::bookmark::BookmarkAndAnnotations bookmarks_;
        BookmarkRanges bookmark_ranges_;
        std::vector<int> media_ranges_;

        typedef std::pair<media_reader::ImageBufPtr, colour_pipeline::ColourPipelineDataPtr>
            ImageAndLut;
        bool up_to_date_{false};
        bool full_precache_activated_{false};
        utility::time_point last_change_timepoint_;
        std::vector<caf::typed_response_promise<caf::actor>> inflight_update_requests_;
    };
} // namespace playhead
} // namespace xstudio