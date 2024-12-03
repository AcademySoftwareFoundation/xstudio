// SPDX-License-Identifier: Apache-2.0

#include "xstudio/playhead/playhead.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/atoms.hpp"

#include <limits>
#include <cmath>

using namespace xstudio;
using namespace xstudio::playhead;
using namespace xstudio::utility;

PlayheadBase::PlayheadBase(const std::string &name, const utility::Uuid uuid)
    : Module(name, uuid),
      playhead_rate_(timebase::k_flicks_24fps),
      position_(0),
      loop_start_(timebase::k_flicks_low),
      loop_end_(timebase::k_flicks_max)

{
    add_attributes();
}

PlayheadBase::~PlayheadBase() {}

void PlayheadBase::add_attributes() {

    image_source_ = add_string_choice_attribute("Source", "Src", "", {}, {});

    image_source_name_ = add_string_attribute("Source Name", "Src", "");

    image_stream_ = add_string_choice_attribute("Stream", "Strm", "", {}, {});

    audio_source_ = add_string_choice_attribute("Audio Source", "Aud Src", "", {}, {});

    velocity_ = add_float_attribute("Rate", "Rate", 1.0f, 0.1f, 16.0f, 0.05f);

    /* velocity_->set_role_data(
         module::Attribute::ToolTip,
         "Set playback speed. Double-click to toggle between last set value and default
       (1.0)");*/

    velocity_multiplier_ = add_integer_attribute("Velocity Multiplier", "FFWD", 1, 1, 16);

    forward_ = add_boolean_attribute("forward", "fwd", true);

    playing_ = add_boolean_attribute("playing", "playing", false);

    auto_align_mode_ =
        add_string_choice_attribute("Auto Align", "Algn.", auto_align_mode_names);

    auto_align_mode_->set_value("Off");

    max_compare_sources_ =
        add_integer_attribute("Max Compare Sources", "Max Compare Sources", 16, 4, 32);
    max_compare_sources_->set_role_data(
        module::Attribute::PreferencePath, "/core/playhead/max_compare_sources");

    restore_play_state_after_scrub_ =
        add_boolean_attribute("Restore Play After Scrub", "Restore Play After Scrub", true);
    restore_play_state_after_scrub_->set_role_data(
        module::Attribute::PreferencePath, "/ui/qml/restore_play_state_after_scrub");

    viewport_scrub_sensitivity_ = add_integer_attribute(
        "Viewport Scrub Sensitivity", "Viewport Scrub Sensitivity", 5, 1, 100);

    viewport_scrub_sensitivity_->set_role_data(
        module::Attribute::PreferencePath, "/ui/viewport/viewport_scrub_sensitivity");

    auto_align_mode_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"playhead_align_mode"});

    velocity_->set_role_data(module::Attribute::ToolbarPosition, 3.0f);

    velocity_->set_role_data(module::Attribute::DefaultValue, 1.0f);

    loop_mode_ = add_string_choice_attribute(
        "Loop Mode", "Loop Mode", "Loop", utility::map_key_to_vec(loop_modes_));

    loop_start_frame_       = add_integer_attribute("Loop Start Frame", "Loop Start Frame", 0);
    loop_end_frame_         = add_integer_attribute("Loop End Frame", "Loop End Frame", 0);
    playhead_logical_frame_ = add_integer_attribute("Logical Frame", "Logical Frame", 0);
    playhead_position_seconds_ =
        add_float_attribute("Position Seconds", "Position Seconds", 0.0);
    playhead_position_flicks_ = add_integer_attribute("Position Flicks", "Position Flicks", 0);

    playhead_rate_attr_ = add_float_attribute("Frame Rate", "Frame Rate", 24.0);
    playhead_media_logical_frame_ =
        add_integer_attribute("Media Logical Frame", "Media Logical Frame", 0);
    playhead_media_frame_ = add_integer_attribute("Media Frame", "Media Frame", 0);
    duration_frames_      = add_integer_attribute("Duration Frames", "Duration Frames", 0);
    duration_seconds_     = add_float_attribute("Duration Seconds", "Duration Seconds", 0.0);
    cached_frames_        = add_int_vec_attribute("Cached Frames");
    bookmarked_frames_    = add_int_vec_attribute("Bookmarked Frames");

    media_transition_frames_ = add_int_vec_attribute("Media Transition Frames");

    source_alignment_values_ = add_int_vec_attribute("Source Alignment Frames");

    current_frame_timecode_ = add_string_attribute("Timecode", "Timecode", "");

    current_frame_timecode_as_frame_ =
        add_integer_attribute("Timecode As Frame", "Timecode As Frame", 0);

    current_media_uuid_ = add_string_attribute("Current Media Uuid", "Current Media Uuid", "");
    current_media_source_uuid_ =
        add_string_attribute("Current Media Source Uuid", "Current Media Source Uuid", "");
    loop_range_enabled_ =
        add_boolean_attribute("Enable Loop Range", "Enable Loop Range", false);

    user_is_frame_scrubbing_ =
        add_boolean_attribute("User Is Frame Scrubbing", "User Is Frame Scrubbing", false);

    pinned_source_mode_ =
        add_boolean_attribute("Pinned Source Mode", "Pinned Source Mode", true);

    // this attr tracks the global 'Audio Delay Millisecs' preference
    audio_delay_millisecs_ =
        add_integer_attribute("Audio Delay Millisecs", "Aud. Delay", 0, -1000, 1000);
    audio_delay_millisecs_->set_role_data(
        module::Attribute::PreferencePath, "/core/audio/audio_latency_millisecs");
    audio_delay_millisecs_->set_role_data(module::Attribute::ToolbarPosition, 20.0f);
    audio_delay_millisecs_->set_role_data(module::Attribute::DefaultValue, 0);
    key_playhead_index_ = add_integer_attribute("Key Playhead Index", "Key Playhead Index", 0);
    num_sub_playheads_  = add_integer_attribute("Num Sub Playheads", "Num Sub Playheads", 0);
    click_to_toggle_play_ =
        add_boolean_attribute("Click to Toggle Play", "Play on Click", false);
    click_to_toggle_play_->set_role_data(
        module::Attribute::PreferencePath, "/ui/qml/click_to_toggle_play");

    source_offset_frames_ =
        add_integer_attribute("Source Offset Frames", "Source Offset Frames", 0);

    timeline_mode_ =
        add_boolean_attribute("Timeline Mode", "Timeline Mode", false);

    // Compare mode needs custom QML code for instatiation into the toolbar as
    // the choices are determined through viewport layout plugins
    compare_mode_ = add_string_attribute(
        "Compare",
        "Compare",
        "Off");
    compare_mode_->set_tool_tip("Access compare mode controls");
    compare_mode_->set_role_data(module::Attribute::Type, "QmlCode");
    compare_mode_->set_role_data(
        module::Attribute::QmlCode,
        R"(import xStudio 1.0
            XsViewerCompareModeButton {})");
    compare_mode_->set_role_data(module::Attribute::ToolbarPosition, 9.0f);

}


JsonStore PlayheadBase::serialise() const {

    JsonStore jsn;
    jsn["name"] = Module::name();
    jsn["velocity"] = velocity_->value();
    jsn["position"] = position_.count();
    jsn["compare_mode"] = compare_mode_->value();
    jsn["auto_align_mode"] = auto_align_mode_->value();
    jsn["source_alignment_values"] = source_alignment_values_->value();    
    return jsn;
}

void PlayheadBase::deserialise(const JsonStore &jsn) {

    if (jsn.is_null()) return;
    velocity_->set_value(jsn.value("velocity", velocity_->value()));
    compare_mode_->set_value(jsn.value("compare_mode", compare_mode_->value()));
    auto_align_mode_->set_value(jsn.value("auto_align_mode", auto_align_mode_->value()));
    source_alignment_values_->set_value(jsn.value("source_alignment_values", source_alignment_values_->value()));
    position_ = timebase::flicks(jsn.value("position", position_.count()));
}

PlayheadBase::OptionalTimePoint PlayheadBase::play_step() {

    const auto now = utility::clock::now();
    const timebase::flicks delta(timebase::flicks::rep(
        throttle_ * velocity_->value() * velocity_multiplier_->value() *
        float(std::chrono::duration_cast<timebase::flicks>(now - last_step_).count())));
    last_step_ = now;

    if (forward()) {
        set_position(position_ + delta);
    } else {
        set_position(position_ - delta);
    }

    const timebase::flicks in = use_loop_range() and loop_start_ != timebase::k_flicks_low
                                    ? loop_start_
                                    : timebase::flicks(0);
    const timebase::flicks out =
        use_loop_range() and loop_end_ != timebase::k_flicks_max ? loop_end_ : duration_;

    if (loop() == LM_LOOP) {

        if (forward()) {
            if (position_ > out || position_ < in) {
                set_position(in);
            }
        } else {
            if (position_ < in || position_ > out) {
                set_position(out);
            }
        }

    } else if (loop() == LM_PING_PONG) {

        if (forward()) {
            if (position_ > out) {
                set_position(out - delta);
                forward_->set_value(false);
            } else if (position_ < in) {
                set_position(in);
            }
        } else {
            if (position_ < in) {
                set_position(in + delta);
                forward_->set_value(true);
            } else if (position_ > out) {
                set_position(out);
            }
        }

    } else {

        if (forward()) {

            if (position_ > out) {
                set_position(out);
                playing_->set_value(false);
            }

        } else if (position_ < in) {

            set_position(in);
            playing_->set_value(false);
        }
    }

    if (playing())
        return now + playback_step_increment;
    return {};
}

void PlayheadBase::register_hotkeys() {

    play_hotkey_ = register_hotkey(
        int(' '),
        ui::NoModifier,
        "Start/Stop Playback",
        "This hotkey will toggle playback",
        false,
        "Playback");

    play_forwards_hotkey_ = register_hotkey(
        int('L'),
        ui::NoModifier,
        "Play Forwards, Accellerate",
        "This hotkey starts forward playback, repeated presses thereon increase the playback "
        "speed",
        false,
        "Playback");

    play_backwards_hotkey_ = register_hotkey(
        int('J'),
        ui::NoModifier,
        "Play Backwards, Accellerate",
        "This hotkey starts backward playback, repeated presses thereon increase the playback "
        "speed",
        false,
        "Playback");

    stop_play_hotkey_ = register_hotkey(
        int('K'), ui::NoModifier, "Stop Playback", "Stops Playback", false, "Playback");

    toggle_loop_range_ = register_hotkey(
        int('P'),
        ui::NoModifier,
        "Toggle Loop Range",
        "Use this hotkey to turn on/off the loop range in/out points that let you loop on a "
        "specified range in the timeline.",
        false,
        "Playback");

    set_loop_in_ = register_hotkey(
        int('I'),
        ui::NoModifier,
        "Set Loop to In Frame",
        "Hit this hotkey to set the 'in' frame of the loop range.",
        false,
        "Playback");

    set_loop_out_ = register_hotkey(
        int('O'),
        ui::NoModifier,
        "Set Loop to Out Frame",
        "Hit this hotkey to set the 'out' frame of the loop range.",
        false,
        "Playback");

    step_forward_ = register_hotkey(
        "Right", "Step Forward", "Steps the playhead forward by one frame.", false, "Playback");

    step_backward_ = register_hotkey(
        "Left",
        "Step Backward",
        "Steps the playhead backward by one frame.",
        false,
        "Playback");

    jump_to_first_frame_ = register_hotkey(
        "Home",
        "Jump to First Frame",
        "Jump the playhead to the first in the playhead range.",
        false,
        "Playback");

    jump_to_last_frame_ = register_hotkey(
        "End",
        "Jump to Last Frame",
        "Jump the playhead to the very last frame in the playhead range.",
        false,
        "Playback");

    jump_to_next_clip_ = register_hotkey(
        "Shift+Right",
        "Jump to Next Clip",
        "Jump the playhead to the start of the next clip in a timeline.",
        false,
        "Playback");

    jump_to_previous_clip_ = register_hotkey(
        "Shift+Left",
        "Jump to Previous Clip",
        "Jump the playhead to the start of the current clip, or the start of the previous "
        "clip.",
        false,
        "Playback");
}


timebase::flicks PlayheadBase::adjusted_position() const {
    if (!playing())
        return position_;

    const timebase::flicks delta = std::chrono::duration_cast<timebase::flicks>(
        std::chrono::milliseconds(audio_delay_millisecs_->value()));

    const timebase::flicks in = use_loop_range() and loop_start_ != timebase::k_flicks_low
                                    ? loop_start_
                                    : timebase::flicks(0);
    const timebase::flicks out =
        use_loop_range() and loop_end_ != timebase::k_flicks_max ? loop_end_ : duration_;

    if (out <= in) {
        return in;
    }

    // somewhat fiddly - we are advancing the position by 'delta' but what if
    // this wraps through the in/out points ... and what if it wraps more than
    // the whole duration of the loop in/out region?
    if (forward() && (position_ + delta) > out) {

        auto remainder = (position_ + delta) - out;
        if (loop() == LM_LOOP) {

            while (remainder > (out - in)) {
                remainder -= (out - in);
            }
            return in + remainder;

        } else if (loop() == LM_PING_PONG) {

            bool fwd = 0;
            while (remainder > (out - in)) {
                remainder -= (out - in);
                fwd = !fwd;
            }
            if (fwd) {
                return in + remainder;
            } else {
                return out - remainder;
            }

        } else {
            return out;
        }

    } else if (forward() && (position_ + delta) < in) {

        auto remainder = in - (position_ + delta);
        if (loop() == LM_LOOP) {

            while (remainder > (out - in)) {
                remainder -= (out - in);
            }
            return out - remainder;

        } else if (loop() == LM_PING_PONG) {

            bool fwd = 0;
            while (remainder > (out - in)) {
                remainder -= (out - in);
                fwd = !fwd;
            }
            if (fwd) {
                return in + remainder;
            } else {
                return out - remainder;
            }

        } else {
            return out;
        }

    } else if (!forward() && (position_ - delta) < in) {

        auto remainder = in - (position_ - delta);
        if (loop() == LM_LOOP) {

            while (remainder > (out - in)) {
                remainder -= (out - in);
            }
            return out - remainder;

        } else if (loop() == LM_PING_PONG) {

            bool fwd = true;
            while (remainder > (out - in)) {
                remainder -= (out - in);
                fwd = !fwd;
            }
            if (fwd) {
                return in + remainder;
            } else {
                return out - remainder;
            }

        } else {
            return out;
        }

    } else if (!forward() && (position_ + delta) > out) {

        auto remainder = (position_ + delta) - out;
        if (loop() == LM_LOOP) {

            while (remainder > (out - in)) {
                remainder -= (out - in);
            }
            return in + remainder;

        } else if (loop() == LM_PING_PONG) {

            bool fwd = false;
            while (remainder > (out - in)) {
                remainder -= (out - in);
                fwd = !fwd;
            }
            if (fwd) {
                return in + remainder;
            } else {
                return out - remainder;
            }

        } else {
            return out;
        }

    } else if (!forward()) {
        return position_ - delta;
    }

    return position_ + delta;
}


void PlayheadBase::set_playing(const bool play) {

    if (play != playing()) {

        // in play once mode, if the user wants to play again we set the
        // position back to the start to play through again
        if (play && loop() == LM_PLAY_ONCE) {

            const timebase::flicks in =
                use_loop_range() and loop_start_ != timebase::k_flicks_low
                    ? loop_start_
                    : timebase::flicks(0);
            const timebase::flicks out =
                use_loop_range() and loop_end_ != timebase::k_flicks_max ? loop_end_
                                                                         : duration_;

            if (forward()) {
                if (position_ == out)
                    set_position(in);
            } else {
                if (position_ == in)
                    set_position(out);
            }
        }

        playing_->set_value(play);
    }
}


timebase::flicks PlayheadBase::effective_frame_period() const {

    return throttle_ == 1.0f ? playhead_rate_.to_flicks()
                             : timebase::to_flicks(static_cast<double>(
                                   playhead_rate_.to_seconds() * (1.0f / throttle_)));
}

timebase::flicks PlayheadBase::clamp_timepoint_to_loop_range(const timebase::flicks pos) const {

    const timebase::flicks in  = loop_start();
    const timebase::flicks out = loop_end();

    auto rt = pos;
    if (loop() == LM_LOOP) {

        if (forward()) {
            if (pos > out || pos < in) {
                rt = in;
            }
        } else {
            if (rt < in || rt > out) {
                rt = out;
            }
        }

    } else if (loop() == LM_PING_PONG) {

        if (forward()) {
            if (pos > out) {
                rt = out;
            } else if (pos < in) {
                rt = in;
            }
        } else {
            if (pos < in) {
                rt = in;
            } else if (pos > out) {
                rt = out;
            }
        }

    } else {

        if (forward()) {

            if (pos > out) {
                rt = out;
            }

        } else if (pos < in) {

            rt = in;
        }
    }
    return rt;
}

void PlayheadBase::set_position(const timebase::flicks p) { 
    position_ = p; 
    position_set_tp_ = utility::clock::now();
}

bool PlayheadBase::set_use_loop_range(const bool use_loop_range) {

    bool position_changed = false;
    if (this->use_loop_range() != use_loop_range) {
        loop_range_enabled_->set_value(use_loop_range);
        if (use_loop_range) {
            if (position_ < loop_start_) {
                set_position(loop_start_);
                position_changed = true;
            } else if (position_ > loop_end_) {
                set_position(loop_end_);
                position_changed = true;
            }
        }
    }
    return position_changed;
}

bool PlayheadBase::set_loop_start(const timebase::flicks loop_start) {

    loop_start_           = loop_start;
    bool position_changed = false;
    if (loop_end_ < loop_start_) {
        loop_end_        = timebase::k_flicks_max;
        position_changed = true;
    }

    if (use_loop_range() && position_ < loop_start_) {
        set_position(loop_start_);
        position_changed = true;
    }

    if (loop_start_ != timebase::k_flicks_low) {
        set_use_loop_range(true);
    }

    return position_changed;
}

bool PlayheadBase::set_loop_end(const timebase::flicks loop_end) {

    loop_end_             = loop_end;
    bool position_changed = false;
    if (loop_start_ > loop_end_) {
        loop_start_      = timebase::k_flicks_low;
        position_changed = true;
    }

    if (use_loop_range() && position_ > loop_end_) {
        set_position(loop_end_);
        position_changed = true;
    }

    if (loop_end_ != timebase::k_flicks_max) {
        set_use_loop_range(true);
    }

    return position_changed;
}

void PlayheadBase::throttle() {

    if (throttle_ > 0.1f)
        throttle_ *= 0.8f;
}

void PlayheadBase::revert_throttle() {

    if (throttle_ < 0.9f)
        throttle_ *= 1.2f;
    else
        throttle_ = 1.0f;
}

AutoAlignMode PlayheadBase::auto_align_mode() const {

    AutoAlignMode rt      = AAM_ALIGN_OFF;
    const std::string aam = auto_align_mode_->value();

    for (auto opt : auto_align_mode_names) {
        if (std::get<1>(opt) == aam)
            rt = std::get<0>(opt);
    }

    return rt;
}


void PlayheadBase::set_assembly_mode(const AssemblyMode mode) {
    assembly_mode_ = mode;
}

void PlayheadBase::set_auto_align_mode(const AutoAlignMode mode) {
    for (auto &a: auto_align_mode_names) {
        if (std::get<0>(a) == mode) {
            auto_align_mode_->set_value(std::get<1>(a));
            break;
        }
    }
}

void PlayheadBase::disconnect_from_ui() {
    set_playing(false);
    Module::disconnect_from_ui();
}

bool PlayheadBase::pointer_event(const ui::PointerEvent &e) {

    bool used = false;
    if (e.type() == ui::Signature::EventType::ButtonDown &&
        e.buttons() == ui::Signature::Button::Left) {

        click_timepoint_                = utility::clock::now();
        drag_start_x_                   = e.x();
        drag_start_playhead_position_   = position_;
        used                            = true;
        was_playing_when_scrub_started_ = playing();
        set_playing(false);
        user_is_frame_scrubbing_->set_value(true);

    } else if (
        e.type() == ui::Signature::EventType::Drag &&
        e.buttons() == ui::Signature::Button::Left) {

        float delta_x     = e.x() - drag_start_x_;
        auto new_position = drag_start_playhead_position_;
        int nb_frames     = abs(round(delta_x * viewport_scrub_sensitivity_->value() / 20));

        if (delta_x < 0.0 and drag_start_playhead_position_.count() and drag_start_x_) {
            new_position -= nb_frames * playhead_rate_.to_flicks();
        } else if (delta_x > 0.0 and drag_start_x_ != e.width() - 1) {
            new_position += nb_frames * playhead_rate_.to_flicks();
        }

        // clamp to duration.
        new_position =
            max(timebase::k_flicks_zero_seconds,
                min(new_position, duration_ - playhead_rate_.to_flicks()));
        if (self())
            anon_send(self(), jump_atom_v, new_position);

        used = true;

    } else if (e.type() == ui::Signature::EventType::ButtonRelease) {

        const auto milliseconds_since_press =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                utility::clock::now() - click_timepoint_)
                .count();

        if (milliseconds_since_press < 200 && click_to_toggle_play_->value()) {
            // it was a quick click, and 'click_to_toggle_play_' is on ...
            set_playing(!was_playing_when_scrub_started_);

        } else if (
            was_playing_when_scrub_started_ && restore_play_state_after_scrub_->value()) {
            set_playing(true);
            was_playing_when_scrub_started_ = false;
        }

        user_is_frame_scrubbing_->set_value(false);
    }

    return used;
}

void PlayheadBase::play_faster(const bool forwards) {

    if (!playing()) {
        set_forward(forwards);
        velocity_multiplier_->set_value(1.0f);
        if (self())
            anon_send(self(), play_atom_v, true);
    } else if (forwards != forward_->value()) {
        forward_->set_value(forwards);
        velocity_multiplier_->set_value(1.0f);
    } else if (velocity_multiplier_->value() < 16) {
        velocity_multiplier_->set_value(velocity_multiplier_->value() * 2.0f);
    }
}

void PlayheadBase::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {

    // If the context starts with 'viewport' the hotkey was hit while a viewport
    // had mouse focus. If that viewport is NOT attached to this playhead we
    // ignore the hotkey
    if (context.find("viewport") == 0 &&
        active_viewports_.find(context) == active_viewports_.end()) {
        return;
    }

    if (hotkey_uuid == play_hotkey_) {
        forward_->set_value(true);
        set_playing(!playing());
    } else if (hotkey_uuid == play_forwards_hotkey_) {
        play_faster(true);
    } else if (hotkey_uuid == play_backwards_hotkey_) {
        play_faster(false);
    } else if (hotkey_uuid == stop_play_hotkey_) {
        set_playing(false);
    } else if (hotkey_uuid == toggle_loop_range_) {
        loop_range_enabled_->set_value(!loop_range_enabled_->value());
    }
}

void PlayheadBase::reset() {
    velocity_multiplier_->set_value(1.0f);
    velocity_->set_value(1.0f);
    forward_->set_value(true);
}

void PlayheadBase::set_duration(const timebase::flicks duration) { duration_ = duration; }

void PlayheadBase::connect_to_viewport(
    const std::string &viewport_name,
    const std::string &viewport_toolbar_name,
    bool connect,
    caf::actor viewport) {

    // this playhead needs to be connected (exposed) in a given toolbar
    // attributes group, so that the compare, source and velocity attrs
    // are visible in a particular viewport toolbar
    // .. or, disconnected
    expose_attribute_in_model_data(
        image_source_, viewport_toolbar_name + "_image_source", connect);
    expose_attribute_in_model_data(
        audio_source_, viewport_toolbar_name + "_audio_source", connect);

    expose_attribute_in_model_data(velocity_, viewport_toolbar_name, connect);

    expose_attribute_in_model_data(compare_mode_, viewport_toolbar_name, connect);

    // Here we can add attrs to show up in the viewer context menu (right click)

    std::string viewport_context_menu_model_name = viewport_name + "_context_menu";

    if (connect) {

        // add menu items to the viewport pop-up menu - since there can be
        // multiple viewports, there can be multiple menu items active in the UI.
        // We get a callback

        // Divider
        insert_menu_item(viewport_context_menu_model_name, "", "", 4.5f, nullptr, true);

        insert_menu_item(
            viewport_context_menu_model_name,
            "In Frame",
            "Set Loop",
            1.0f,
            nullptr,
            false,
            set_loop_in_);

        insert_menu_item(
            viewport_context_menu_model_name,
            "Out Frame",
            "Set Loop",
            2.0f,
            nullptr,
            false,
            set_loop_out_);


        // Divider
        insert_menu_item(viewport_context_menu_model_name, "", "Set Loop", 2.5f, nullptr, true);

        set_submenu_position_in_parent(viewport_context_menu_model_name, "Set Loop", 5.0f);


        insert_menu_item(
            viewport_context_menu_model_name,
            "Enable Loop",
            "",
            7.0f,
            loop_range_enabled_,
            false,
            toggle_loop_range_);


    } else {

        remove_all_menu_items(viewport_context_menu_model_name);
    }

    Module::connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);

    if (connect) {
        active_viewports_.insert(viewport_name);
    } else {
        auto p = active_viewports_.find(viewport_name);
        if (p != active_viewports_.end()) {
            active_viewports_.erase(p);
        }
    }
}

void PlayheadBase::menu_item_activated(
    const utility::JsonStore &menu_item_data, const std::string &user_data) {
    // use the hotkey handling that's set-up in PlayheadActor

    if (menu_item_data["name"] == "In Frame") {

        hotkey_pressed(set_loop_in_, "", "");

    } else if (menu_item_data["name"] == "Out Frame") {

        hotkey_pressed(set_loop_out_, "", "");
    }
}
