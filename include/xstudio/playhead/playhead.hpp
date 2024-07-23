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
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {

    class PlayheadBase : public utility::Container, public module::Module {
      public:
        typedef std::optional<utility::time_point> OptionalTimePoint;

        PlayheadBase(
            const std::string &name  = "PlayheadBase",
            const utility::Uuid uuid = utility::Uuid::generate());
        PlayheadBase(const utility::JsonStore &jsn);

        ~PlayheadBase() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        OptionalTimePoint play_step();

        [[nodiscard]] timebase::flicks position() const { return position_; }
        void set_position(const timebase::flicks p);

        [[nodiscard]] bool playing() const { return playing_->value(); }
        [[nodiscard]] bool forward() const { return forward_->value(); }
        [[nodiscard]] AutoAlignMode auto_align_mode() const;
        [[nodiscard]] int loop() const { return loop_mode_->value(); }
        [[nodiscard]] CompareMode compare_mode() const;
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
        [[nodiscard]] bool use_loop_range() const { return do_looping_->value(); }
        [[nodiscard]] timebase::flicks duration() const { return duration_; }
        [[nodiscard]] timebase::flicks effective_frame_period() const;
        timebase::flicks clamp_timepoint_to_loop_range(const timebase::flicks pos) const;

        void set_forward(const bool forward = true) { forward_->set_value(forward); }
        void set_loop(const LoopMode loop = LM_LOOP) { loop_mode_->set_value(loop); }
        void set_playing(const bool play = true);
        timebase::flicks adjusted_position() const;
        void set_play_rate_mode(const utility::TimeSourceMode play_rate_mode) {
            play_rate_mode_ = play_rate_mode;
        }

        void set_velocity_multiplier(const float velocity_multiplier = 1.0f) {
            velocity_multiplier_->set_value(velocity_multiplier);
        }
        void set_playhead_rate(const utility::FrameRate &rate) { playhead_rate_ = rate; }
        void set_duration(const timebase::flicks duration);
        void set_compare_mode(const CompareMode mode);

        bool set_use_loop_range(const bool use_loop_range);
        bool set_loop_start(const timebase::flicks loop_start);
        bool set_loop_end(const timebase::flicks loop_end);

        void throttle();
        void revert_throttle();

        bool pointer_event(const ui::PointerEvent &) override;
        void
        hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context) override;

        void connect_to_viewport(
            const std::string &viewport_name,
            const std::string &viewport_toolbar_name,
            bool connect) override;

        inline static const std::chrono::milliseconds playback_step_increment =
            std::chrono::milliseconds(5);

        inline static const std::vector<std::tuple<CompareMode, std::string, std::string, bool>>
            compare_mode_names = {
                {CM_STRING, "String", "Str", true},
                {CM_AB, "A/B", "A/B", true},
                {CM_VERTICAL, "Vertical", "Vert", false},
                {CM_HORIZONTAL, "Horizontal", "Horiz", false},
                {CM_GRID, "Grid", "Grid", false},
                {CM_OFF, "Off", "Off", true}};

      private:
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
        utility::Uuid reset_hotkey_;

        float drag_start_x_;
        timebase::flicks drag_start_playhead_position_;

      protected:
        void add_attributes();

        utility::time_point last_step_;
        float throttle_{1.0f};

        module::FloatAttribute *velocity_;
        module::StringChoiceAttribute *compare_mode_;
        module::QmlCodeAttribute *source_;
        module::StringChoiceAttribute *image_source_;
        module::StringChoiceAttribute *audio_source_;
        module::FloatAttribute *velocity_multiplier_;
        module::BooleanAttribute *playing_;
        module::BooleanAttribute *forward_;
        module::StringChoiceAttribute *auto_align_mode_;

        module::IntegerAttribute *max_compare_sources_;
        module::BooleanAttribute *restore_play_state_after_scrub_;
        module::IntegerAttribute *viewport_scrub_sensitivity_;

        module::IntegerAttribute *loop_mode_;
        module::IntegerAttribute *loop_start_frame_;
        module::IntegerAttribute *loop_end_frame_;
        module::IntegerAttribute *playhead_logical_frame_;
        module::IntegerAttribute *playhead_media_logical_frame_;
        module::IntegerAttribute *playhead_media_frame_;
        module::IntegerAttribute *duration_frames_;
        module::StringAttribute *current_source_frame_timecode_;
        module::StringAttribute *current_media_uuid_;
        module::StringAttribute *current_media_source_uuid_;
        module::BooleanAttribute *do_looping_;
        module::IntegerAttribute *audio_delay_millisecs_;

        bool was_playing_when_scrub_started_ = {false};
    };
} // namespace playhead
} // namespace xstudio