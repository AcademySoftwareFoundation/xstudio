// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {

    class PlayheadActor : public caf::event_based_actor, public PlayheadBase {
      public:
        PlayheadActor(
            caf::actor_config &cfg,
            const utility::JsonStore &jsn,
            caf::actor playlist_selection = caf::actor());
        PlayheadActor(
            caf::actor_config &cfg,
            const std::string &name,
            caf::actor playlist_selection = caf::actor(),
            const utility::Uuid uuid      = utility::Uuid::generate());
        ~PlayheadActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "PlayheadActor";

        void init();

        void play();

        caf::behavior make_behavior() override {
            return behavior_.or_else(PlayheadBase::message_handler());
        }

        void clear_all_precache_requests(caf::typed_response_promise<bool> rp);
        void clear_child_playheads();
        caf::actor make_child_playhead(caf::actor source);
        void make_audio_child_playhead(const int source_index);
        void rebuild();
        void connect_to_playlist_selection_actor(caf::actor playlist_selection);
        void new_source_list(const std::vector<caf::actor> &sl);
        void switch_key_playhead(int idx);
        void calculate_duration();
        void update_child_playhead_positions(
            const bool force_broadcast = false, const bool playhead_scrubbing = false);
        void notify_loop_end_changed();
        void notify_loop_start_changed();
        void update_duration(caf::typed_response_promise<timebase::flicks> rp);
        void notify_offset_changed();
        void update_audio_source(caf::actor media_actor);
        void skip_through_sources(const int skip_by);
        void select_media(const utility::Uuid media_uuid, const bool clear_selection);
        void move_playhead_to_media_source(const utility::Uuid &uuid = utility::Uuid());
        void jump_to_source(const utility::Uuid &media_uuid);
        void update_playback_rate();
        void update_cached_frames_status(
            const media::MediaKeyVector &new_keys    = media::MediaKeyVector(),
            const media::MediaKeyVector &remove_keys = media::MediaKeyVector());
        void rebuild_cached_frames_status();
        void
        select_media(const utility::UuidList &selection, caf::typed_response_promise<bool> &rp);
        void align_clip_frame_numbers();
        void move_playhead_to_last_viewed_frame_of_current_source();
        void
        move_playhead_to_last_viewed_frame_of_given_source(const utility::Uuid &source_uuid);
        void current_media_changed(caf::actor media_actor, const bool force = false);
        void update_source_multichoice(
            module::StringChoiceAttribute *attr, const media::MediaType mt);
        void
        restart_readahead_cacheing(const bool all_child_playheads, const bool force = false);
        void switch_media_source(const std::string new_source_name, const media::MediaType mt);
        bool has_selection_changed();
        int previous_selected_sources_count_ = {-1};

        void manage_playback_video_refresh_sync(
            const utility::time_point &when_video_framebuffer_was_swapped_to_screen,
            const timebase::flicks video_refresh_rate_hint,
            const int viewer_index);

      protected:
        void attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) override;
        void connected_to_ui_changed() override;
        void check_if_loop_range_makes_sense();

        caf::message_handler behavior_;
        caf::actor playlist_selection_;
        caf::actor empty_clip_;
        caf::actor broadcast_;
        caf::actor event_group_;
        caf::actor fps_moniotor_group_;
        caf::actor viewport_events_group_;
        caf::actor playhead_media_events_group_;
        std::vector<caf::actor> sub_playheads_;
        std::vector<caf::actor> source_wrappers_;
        std::vector<caf::actor> source_actors_;
        caf::actor key_playhead_;
        caf::actor audio_output_actor_;
        caf::actor playhead_events_actor_;
        caf::actor audio_playhead_;
        caf::actor audio_playhead_retimer_;
        caf::actor image_cache_;
        caf::actor pre_reader_;
        caf::actor_addr playlist_selection_addr_;
        utility::Uuid previous_source_uuid_;
        utility::Uuid current_source_uuid_;
        utility::Uuid key_playhead_uuid_;
        std::map<utility::Uuid, int> media_frame_per_media_uuid_;
        // std::map<utility::Uuid, timebase::flicks> media_frame_per_media_uuid_;
        std::vector<std::pair<int, int>> cached_frames_ranges_;
        std::vector<std::tuple<utility::Uuid, std::string, int, int>> bookmark_frames_ranges_;

        std::set<media::MediaKey> frames_cached_;

        media::MediaKeyVector all_frames_keys_;
        bool updating_source_list_                      = {false};
        bool child_playhead_changed_                    = {false};
        timebase::flicks vid_refresh_sync_phase_adjust_ = timebase::flicks{0};
        int media_logical_frame_                        = {0};
    };
} // namespace playhead
} // namespace xstudio
