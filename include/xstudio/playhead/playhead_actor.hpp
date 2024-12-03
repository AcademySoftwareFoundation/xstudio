// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/playhead/playhead.hpp"
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
            caf::actor playlist_selection   = caf::actor(),
            const utility::Uuid uuid        = utility::Uuid::generate(),
            caf::actor_addr parent_playlist = caf::actor_addr());

        PlayheadActor(
            caf::actor_config &cfg,
            const std::string &name,
            caf::actor playlist_selection   = caf::actor(),
            const utility::Uuid uuid        = utility::Uuid::generate(),
            caf::actor_addr parent_playlist = caf::actor_addr());

        virtual ~PlayheadActor();

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
        caf::actor make_child_playhead(utility::UuidActor source);
        void make_audio_child_playhead(const int source_index);
        void rebuild_from_timeline_sources();
        void rebuild_from_dynamic_sources();

        void connect_to_playlist_selection_actor(caf::actor playlist_selection);
        void new_source_list();
        void switch_key_playhead(int idx);
        void calculate_duration();
        void update_child_playhead_positions(const bool force_broadcast);
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
        void match_video_track_durations();
        void align_clip_frame_numbers();
        void move_playhead_to_last_viewed_frame_of_current_source();
        void
        move_playhead_to_last_viewed_frame_of_given_source(const utility::Uuid &source_uuid);
        void current_media_changed(caf::actor media_actor, const bool force = false);
        void update_source_multichoice(
            module::StringChoiceAttribute *attr, const media::MediaType mt);
        void update_stream_multichoice(
            module::StringChoiceAttribute *streams_attr, const media::MediaType mt);

        void
        restart_readahead_cacheing(const bool all_child_playheads, const bool force = false);
        void switch_media_source(const std::string new_source_name, const media::MediaType mt);
        void switch_media_stream(
            caf::actor media_actor,
            const std::string new_stream_name,
            const media::MediaType mt,
            bool apply_to_selected);
        bool has_selection_changed();
        int previous_selected_sources_count_ = {-1};

        void on_exit() override;

      protected:

        void attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) override;
        void hotkey_pressed(
            const utility::Uuid &hotkey_uuid,
            const std::string &context,
            const std::string &window) override;
        void
        hotkey_released(const utility::Uuid &hotkey_uuid, const std::string &context) override;
        bool timeline_mode() const { return pinned_source_mode_->value() && timeline_actor_; }
        bool contact_sheet_mode() const { return contact_sheet_mode_; }
        void connected_to_ui_changed() override;
        void check_if_loop_range_makes_sense();
        void make_source_menu_model();

        caf::message_handler behavior_;

        caf::actor playlist_selection_;
        utility::UuidActor empty_clip_;
        caf::actor broadcast_;
        caf::actor fps_moniotor_group_;
        caf::actor viewport_events_group_;
        caf::actor playhead_media_events_group_;
        caf::actor event_group_;

        utility::UuidActor hero_sub_playhead_;
        utility::UuidActorVector sub_playheads_;

        utility::UuidActor video_string_out_actor_;
        utility::UuidActor timeline_actor_;
        utility::UuidActorVector source_actors_;
        utility::UuidActorVector timeline_track_actors_;
        utility::UuidActorVector dynamic_source_actors_;

        utility::UuidActorVector string_audio_sources_;
        utility::UuidActor audio_src_;
        caf::actor audio_playhead_;
        caf::actor audio_playhead_retimer_;

        caf::actor audio_output_actor_;
        caf::actor playhead_events_actor_;
        caf::actor image_cache_;
        caf::actor pre_reader_;
        caf::actor_addr playlist_selection_addr_;
        caf::actor_addr parent_playlist_;

        utility::Uuid previous_source_uuid_;
        utility::Uuid current_source_uuid_;
        std::map<utility::Uuid, int> media_frame_per_media_uuid_;
        std::map<utility::Uuid, int> switch_key_playhead_hotkeys_;        
        utility::Uuid move_selection_up_hotkey_;
        utility::Uuid move_selection_down_hotkey_;
        utility::Uuid jump_to_previous_note_hotkey_;
        utility::Uuid jump_to_next_note_hotkey_;

        std::map<utility::Uuid, int64_t> source_offsets_;

        // std::map<utility::Uuid, timebase::flicks> media_frame_per_media_uuid_;
        std::vector<std::tuple<utility::Uuid, std::string, int, int>> bookmark_frames_ranges_;

        std::set<media::MediaKey> frames_cached_;

        media::MediaKeyVector all_frames_keys_;
        bool updating_source_list_                      = {false};
        bool child_playhead_changed_                    = {false};
        timebase::flicks vid_refresh_sync_phase_adjust_ = timebase::flicks{0};
        int media_logical_frame_                        = {0};
        float step_keypress_event_id_                   = {0};
        bool precacheing_enabled_                       = {true};
        bool wrap_sources_                              = {false};
        bool contact_sheet_mode_                        = {false};
        int sub_playhead_precache_idx_                  = {0};

        utility::UuidActorVector to_uuid_actor_vec(const std::vector<caf::actor> &actors);
    };
} // namespace playhead
} // namespace xstudio
