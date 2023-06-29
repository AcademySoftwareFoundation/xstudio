// SPDX-License-Identifier: Apache-2.0

#include "xstudio/playhead/playhead.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/atoms.hpp"

#include <limits>
#include <cmath>

using namespace xstudio;
using namespace xstudio::playhead;
using namespace xstudio::utility;

PlayheadBase::PlayheadBase(const std::string &name, const utility::Uuid uuid)
    : Container(name, "PlayheadBase", std::move(uuid)),
      Module(name),

      playhead_rate_(timebase::k_flicks_24fps),
      position_(0),
      loop_start_(timebase::k_flicks_low),
      loop_end_(timebase::k_flicks_max)

{
    add_attributes();
}

PlayheadBase::PlayheadBase(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])),
      Module("PlayheadBase"),
      loop_(jsn["loop"]),
      play_rate_mode_(jsn["play_rate_mode"]),
      playhead_rate_(timebase::k_flicks_24fps),
      position_(jsn["position"]),
      loop_start_(jsn["loop_start"]),
      loop_end_(jsn["loop_end"])
// use_loop_range_(false) // use_loop_range_(jsn["use_loop_range"]) - forcing looprange off on
// load, unwanted behaviour
{

    add_attributes();
    if (jsn.find("module") != jsn.end()) {
        Module::deserialise(jsn["module"]);
    }
}

void PlayheadBase::add_attributes() {

    compare_mode_ = add_string_choice_attribute("Compare", "Cpm", compare_mode_names);
    compare_mode_->set_value("Off");
    compare_mode_->set_role_data(
        module::Attribute::InitOnlyPreferencePath, "/ui/qml/default_playhead_compare_mode");

    image_source_ = add_string_choice_attribute("Source", "Src", "", {}, {});

    audio_source_ = add_string_choice_attribute("Audio Source", "Aud Src", "", {}, {});

    velocity_ = add_float_attribute("Rate", "Rate", 1.0f, 0.1f, 16.0f, 0.05f);

    /* velocity_->set_role_data(
         module::Attribute::ToolTip,
         "Set playback speed. Double-click to toggle between last set value and default
       (1.0)");*/


    velocity_multiplier_ =
        add_float_attribute("Velocity Multiplier", "FFWD", 1.0f, 1.0f, 16.0f, 1.0f);

    forward_ = add_boolean_attribute("forward", "fwd", true);

    playing_ = add_boolean_attribute("playing", "playing", false);

    auto_align_mode_ =
        add_string_choice_attribute("Auto Align", "Auto Align", auto_align_mode_names);

    auto_align_mode_->set_value("Alignment Off");
    auto_align_mode_->set_role_data(
        module::Attribute::PreferencePath, "/core/playhead/align_mode");

    max_compare_sources_ =
        add_integer_attribute("Max Compare Sources", "Max Compare Sources", 9, 2, 32);
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

    compare_mode_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"any_toolbar", "playhead"});
    velocity_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"any_toolbar", "playhead"});

    source_ = add_qml_code_attribute(
        "Src",
        R"(
        import xStudio 1.0
        XsSourceToolbarButton {
            anchors.fill: parent
        }
    )");

    source_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"any_toolbar", "playhead"});

    image_source_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"image_source", "playhead"});
    audio_source_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"audio_source", "playhead"});

    playing_->set_role_data(module::Attribute::Groups, nlohmann::json{"playhead"});
    forward_->set_role_data(module::Attribute::Groups, nlohmann::json{"playhead"});
    auto_align_mode_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"playhead_align_mode"});

    velocity_->set_role_data(module::Attribute::ToolbarPosition, 3.0f);
    compare_mode_->set_role_data(module::Attribute::ToolbarPosition, 9.0f);
    source_->set_role_data(module::Attribute::ToolbarPosition, 12.0f);

    velocity_->set_role_data(module::Attribute::DefaultValue, 1.0f);

    compare_mode_->set_role_data(
        module::Attribute::ToolTip,
        "Select viewer Compare Mode. Shift-select multiple Media items to compare them.");

    play_hotkey_ = register_hotkey(
        int(' '), ui::NoModifier, "Start/Stop Playback", "This hotkey will toggle playback");

    play_forwards_hotkey_ = register_hotkey(
        int('L'),
        ui::NoModifier,
        "Play Forwards, Accellerate",
        "This hotkey starts forward playback, repeated presses thereon increase the playback "
        "speed");

    play_backwards_hotkey_ = register_hotkey(
        int('J'),
        ui::NoModifier,
        "Play Backwards, Accellerate",
        "This hotkey starts backward playback, repeated presses thereon increase the playback "
        "speed");

    stop_play_hotkey_ =
        register_hotkey(int('K'), ui::NoModifier, "Stop Playback", "Stops Playback");

    reset_hotkey_ = register_hotkey(
        int('R'),
        ui::ControlModifier,
        "Reset PlayheadBase",
        "Resets the playhead properties, to normal playback speed and forwards playing");
}


JsonStore PlayheadBase::serialise() const {
    JsonStore jsn;

    jsn["container"]      = Container::serialise();
    jsn["position"]       = position_.count();
    jsn["loop"]           = loop_;
    jsn["play_rate_mode"] = play_rate_mode_;
    jsn["loop_start"]     = loop_start_.count();
    jsn["loop_end"]       = loop_end_.count();
    jsn["use_loop_range"] = use_loop_range_;
    jsn["module"]         = Module::serialise();

    return jsn;
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

    const timebase::flicks in = use_loop_range_ and loop_start_ != timebase::k_flicks_low
                                    ? loop_start_
                                    : timebase::flicks(0);
    const timebase::flicks out =
        use_loop_range_ and loop_end_ != timebase::k_flicks_max ? loop_end_ : duration_;

    if (loop_ == LM_LOOP) {

        if (forward()) {
            if (position_ > out || position_ < in) {
                set_position(in);
            }
        } else {
            if (position_ < in || position_ > out) {
                set_position(out);
            }
        }

    } else if (loop_ == LM_PING_PONG) {

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

void PlayheadBase::set_playing(const bool play) {

    if (play != playing()) {

        // in play once mode, if the user wants to play again we set the
        // position back to the start to play through again
        if (play && loop_ == LM_PLAY_ONCE) {

            const timebase::flicks in =
                use_loop_range_ and loop_start_ != timebase::k_flicks_low ? loop_start_
                                                                          : timebase::flicks(0);
            const timebase::flicks out =
                use_loop_range_ and loop_end_ != timebase::k_flicks_max ? loop_end_ : duration_;

            if (forward()) {
                if (position_ == out)
                    position_ = in;
            } else {
                if (position_ == in)
                    position_ = out;
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
    if (loop_ == LM_LOOP) {

        if (forward()) {
            if (pos > out || pos < in) {
                rt = in;
            }
        } else {
            if (rt < in || rt > out) {
                rt = out;
            }
        }

    } else if (loop_ == LM_PING_PONG) {

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

void PlayheadBase::set_position(const timebase::flicks p) { position_ = p; }

bool PlayheadBase::set_use_loop_range(const bool use_loop_range) {

    bool position_changed = false;
    if (use_loop_range_ != use_loop_range) {
        use_loop_range_ = use_loop_range;
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

    if (use_loop_range_ && position_ < loop_start_) {
        set_position(loop_start_);
        position_changed = true;
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

    if (use_loop_range_ && position_ > loop_end_) {
        set_position(loop_end_);
        position_changed = true;
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

CompareMode PlayheadBase::compare_mode() const {

    CompareMode rt         = CM_OFF;
    const std::string cmpm = compare_mode_->value();

    for (auto opt : compare_mode_names) {
        if (std::get<1>(opt) == cmpm)
            rt = std::get<0>(opt);
    }
    return rt;
}

void PlayheadBase::set_compare_mode(const CompareMode mode) {
    std::string compare_str;

    for (auto opt : compare_mode_names) {
        if (std::get<0>(opt) == mode)
            compare_str = std::get<1>(opt);
    }

    compare_mode_->set_value(compare_str);
}

bool PlayheadBase::pointer_event(const ui::PointerEvent &e) {

    bool used = false;

    if (e.type() == ui::Signature::EventType::ButtonDown &&
        e.buttons() == ui::Signature::Button::Left) {

        drag_start_x_                   = e.x();
        drag_start_playhead_position_   = position_;
        used                            = true;
        was_playing_when_scrub_started_ = playing();
        set_playing(false);

    } else if (
        e.type() == ui::Signature::EventType::Drag &&
        e.buttons() == ui::Signature::Button::Left) {

        if (e.x() < e.width() and e.x() >= 0) {

            float delta_x     = e.x() - drag_start_x_;
            auto new_position = drag_start_playhead_position_;
            int nb_frames     = abs(round(delta_x * viewport_scrub_sensitivity_->value() / 20));

            if (delta_x < 0.0 and drag_start_playhead_position_.count() and drag_start_x_) {
                new_position -= nb_frames * playhead_rate_;
            } else if (delta_x > 0.0 and drag_start_x_ != e.width() - 1) {
                new_position += nb_frames * playhead_rate_;
            }

            // clamp to duration.
            new_position =
                max(timebase::k_flicks_zero_seconds,
                    min(new_position, duration_ - playhead_rate_.to_flicks()));
            if (self())
                anon_send(self(), scrub_frame_atom_v, new_position);
        }

        used = true;

    } else if (e.type() == ui::Signature::EventType::ButtonRelease) {

        if (was_playing_when_scrub_started_ && restore_play_state_after_scrub_->value()) {
            set_playing(true);
            was_playing_when_scrub_started_ = false;
        }
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
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {

    if (hotkey_uuid == play_hotkey_) {
        forward_->set_value(true);
        set_playing(!playing());
    } else if (hotkey_uuid == play_forwards_hotkey_) {
        play_faster(true);
    } else if (hotkey_uuid == play_backwards_hotkey_) {
        play_faster(false);
    } else if (hotkey_uuid == stop_play_hotkey_) {
        set_playing(false);
    } else if (hotkey_uuid == reset_hotkey_) {
        velocity_multiplier_->set_value(1.0f);
        velocity_->set_value(1.0f);
        forward_->set_value(true);
    }
}

void PlayheadBase::set_duration(const timebase::flicks duration) { duration_ = duration; }