// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <string>

#include "xstudio/module/module.hpp"
#include "xstudio/enums.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {

    class PlayheadBase : public module::Module {
      public:
        typedef std::optional<utility::time_point> OptionalTimePoint;

        PlayheadBase(
            const std::string &name  = "PlayheadBase",
            const utility::Uuid uuid = utility::Uuid::generate());

        virtual ~PlayheadBase();

        [[nodiscard]] utility::JsonStore serialise() const override;

        void deserialise(const utility::JsonStore &jsn);

        OptionalTimePoint play_step();

        [[nodiscard]] timebase::flicks position() const { return position_; }

        // adjusts the playhead position during playback by audio_delay_millisecs_
        // to allow for timing difference between audio and video playback
        [[nodiscard]] timebase::flicks adjusted_position() const;

        void set_position(const timebase::flicks p);

        inline static const std::map<std::string, playhead::LoopMode> loop_modes_ = {
            {"Play Once", playhead::LoopMode::LM_PLAY_ONCE},
            {"Loop", playhead::LoopMode::LM_LOOP},
            {"Ping Pong", playhead::LoopMode::LM_PING_PONG}};

        [[nodiscard]] bool playing() const { return playing_->value(); }
        [[nodiscard]] bool forward() const { return forward_->value(); }
        [[nodiscard]] AutoAlignMode auto_align_mode() const;
        [[nodiscard]] playhead::LoopMode loop() const {
            const auto p = loop_modes_.find(loop_mode_->value());
            if (p != loop_modes_.end()) {
                return p->second;
            }
            return playhead::LoopMode::LM_LOOP;
        }
        [[nodiscard]] AssemblyMode assemply_mode() const;
        [[nodiscard]] float velocity() const { return velocity_->value(); }
        [[nodiscard]] float velocity_multiplier() const {
            return velocity_multiplier_->value();
        }
        [[nodiscard]] utility::TimeSourceMode play_rate_mode() const { return play_rate_mode_; }
        [[nodiscard]] utility::FrameRate playhead_rate() const { return playhead_rate_; }
        [[nodiscard]] timebase::flicks loop_start() const {
            return use_loop_range()
                       ? loop_start_
                       : timebase::flicks(std::numeric_limits<timebase::flicks::rep>::lowest());
        }
        [[nodiscard]] timebase::flicks loop_end() const {
            return use_loop_range()
                       ? loop_end_
                       : timebase::flicks(std::numeric_limits<timebase::flicks::rep>::max());
        }
        [[nodiscard]] bool use_loop_range() const { return loop_range_enabled_->value(); }
        [[nodiscard]] timebase::flicks duration() const { return duration_; }
        [[nodiscard]] timebase::flicks effective_frame_period() const;
        [[nodiscard]] AssemblyMode assembly_mode() const { return assembly_mode_; }
        [[nodiscard]] const utility::time_point & last_playhead_set_timepoint() const { return position_set_tp_; }

        timebase::flicks clamp_timepoint_to_loop_range(const timebase::flicks pos) const;

        void set_forward(const bool forward = true) { forward_->set_value(forward); }
        void set_loop(const LoopMode loop = LM_LOOP) {
            for (const auto &p : loop_modes_) {
                if (p.second == loop) {
                    loop_mode_->set_value(p.first);
                }
            }
        }
        void set_playing(const bool play = true);
        void set_play_rate_mode(const utility::TimeSourceMode play_rate_mode) {
            play_rate_mode_ = play_rate_mode;
        }

        void set_velocity_multiplier(const float velocity_multiplier = 1.0f) {
            velocity_multiplier_->set_value(velocity_multiplier);
        }
        void set_playhead_rate(const utility::FrameRate &rate) {
            playhead_rate_ = rate;
            playhead_rate_attr_->set_value(rate.to_seconds());
        }
        void set_duration(const timebase::flicks duration);
        void set_assembly_mode(const AssemblyMode mode);
        void set_auto_align_mode(const AutoAlignMode mode);

        bool set_use_loop_range(const bool use_loop_range);
        bool set_loop_start(const timebase::flicks loop_start);
        bool set_loop_end(const timebase::flicks loop_end);

        void throttle();
        void revert_throttle();

        void disconnect_from_ui() override;

        bool pointer_event(const ui::PointerEvent &) override;
        void hotkey_pressed(
            const utility::Uuid &hotkey_uuid,
            const std::string &context,
            const std::string &window) override;

        void connect_to_viewport(
            const std::string &viewport_name,
            const std::string &viewport_toolbar_name,
            bool connect,
            caf::actor viewport) override;

        void menu_item_activated(
            const utility::JsonStore &menu_item_data, const std::string &user_data) override;

        inline static const std::chrono::milliseconds playback_step_increment =
            std::chrono::milliseconds(5);

        void reset() override;

        void register_hotkeys() override;

      protected:
        void play_faster(const bool forwards);

        inline static const std::vector<
            std::tuple<AutoAlignMode, std::string, std::string, bool>>
            auto_align_mode_names = {
                {AAM_ALIGN_OFF, "Off", "Off", true},
                {AAM_ALIGN_FRAMES, "On", "On", true},
                {AAM_ALIGN_TRIM, "On (Trim)", "Trim", true}};

        utility::TimeSourceMode play_rate_mode_{utility::TimeSourceMode::DYNAMIC};
        utility::FrameRate playhead_rate_;

        timebase::flicks position_;
        timebase::flicks duration_;
        timebase::flicks loop_start_;
        timebase::flicks loop_end_;

        utility::Uuid play_hotkey_;
        utility::Uuid play_forwards_hotkey_;
        utility::Uuid play_backwards_hotkey_;
        utility::Uuid stop_play_hotkey_;
        utility::Uuid toggle_loop_range_;
        utility::Uuid set_loop_in_;
        utility::Uuid set_loop_out_;
        utility::Uuid step_forward_;
        utility::Uuid step_backward_;
        utility::Uuid jump_to_first_frame_;
        utility::Uuid jump_to_last_frame_;
        utility::Uuid jump_to_next_clip_;
        utility::Uuid jump_to_previous_clip_;

        float drag_start_x_;
        timebase::flicks drag_start_playhead_position_;
        utility::time_point click_timepoint_;

        void add_attributes();

        utility::time_point last_step_, position_set_tp_;
        float throttle_{1.0f};
        AssemblyMode assembly_mode_ = {AssemblyMode::AM_ONE};

        module::FloatAttribute *velocity_;
        module::QmlCodeAttribute *source_;
        module::StringAttribute *image_source_name_;
        module::StringChoiceAttribute *image_source_;
        module::StringChoiceAttribute *image_stream_;
        module::StringChoiceAttribute *audio_source_;
        module::IntegerAttribute *velocity_multiplier_;
        module::BooleanAttribute *playing_;
        module::BooleanAttribute *forward_;
        module::StringChoiceAttribute *auto_align_mode_;
        module::IntegerAttribute *audio_delay_millisecs_;
        module::IntegerVecAttribute *cached_frames_;
        module::IntegerVecAttribute *bookmarked_frames_;
        module::IntegerVecAttribute *media_transition_frames_;

        module::IntegerAttribute *max_compare_sources_;
        module::BooleanAttribute *restore_play_state_after_scrub_;
        module::BooleanAttribute *click_to_toggle_play_;
        module::BooleanAttribute *timeline_mode_;
        module::IntegerAttribute *viewport_scrub_sensitivity_;
        module::IntegerAttribute *source_offset_frames_;

        module::StringChoiceAttribute *loop_mode_;
        module::IntegerAttribute *loop_start_frame_;
        module::IntegerAttribute *loop_end_frame_;
        module::IntegerAttribute *playhead_logical_frame_;
        module::FloatAttribute *playhead_position_seconds_;
        module::IntegerAttribute *playhead_position_flicks_;
        module::FloatAttribute *playhead_rate_attr_;
        module::IntegerAttribute *playhead_media_logical_frame_;
        module::IntegerAttribute *playhead_media_frame_;
        module::IntegerAttribute *duration_frames_;
        module::IntegerAttribute *key_playhead_index_;
        module::IntegerAttribute *num_sub_playheads_;
        module::FloatAttribute *duration_seconds_;
        module::StringAttribute *current_frame_timecode_;
        module::IntegerAttribute *current_frame_timecode_as_frame_;
        module::StringAttribute *current_media_uuid_;
        module::StringAttribute *current_media_source_uuid_;
        module::BooleanAttribute *loop_range_enabled_;
        module::BooleanAttribute *user_is_frame_scrubbing_;
        module::BooleanAttribute *pinned_source_mode_;
        module::StringAttribute *compare_mode_;
        module::IntegerVecAttribute *source_alignment_values_;

        bool was_playing_when_scrub_started_ = {false};
        std::set<std::string> active_viewports_;
    };
} // namespace playhead
} // namespace xstudio