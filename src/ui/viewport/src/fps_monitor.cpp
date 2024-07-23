// SPDX-License-Identifier: Apache-2.0
#include "fps_monitor.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

#define MAX_FPS_MEASURE_EVENTS 48
#define MIN_FPS_MEASURE_EVENTS 8

using namespace xstudio::ui::fps_monitor;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace xstudio::utility;
using namespace xstudio::colour_pipeline;

namespace {

std::string make_fps_display_string(
    const FrameRate &actual, const FrameRate &target, const float velocity) {

    // if the FPS is 24.0 we want the display to show '24.0' i.e. one decimal place. If the
    // FPS is 23.976 then we ideally need 3 decimal places, but to fit the values into the
    // box in the UI we're truncating to 2 decimal places

    if (!target) {
        return std::string("--/--");
    }

    const float target_fps = velocity * target.to_fps();

    // We round the displayed fps to increments of 4% of the target
    const float rounded_actual =
        round(actual.to_fps() * 25.0f / target_fps) * target_fps / 25.0f;

    std::array<char, 1024> rbuf;

    if (actual) {
        if (round(target_fps) == target_fps) {
            // one decimal place
            sprintf(rbuf.data(), "%.1f/%.1f", rounded_actual, target_fps);
        } else {
            sprintf(rbuf.data(), "%.2f/%.2f", rounded_actual, target_fps);
        }
    } else {
        // not playing_
        if (round(target_fps) == target_fps) {
            // one decimal place
            sprintf(rbuf.data(), "--.-/%.1f", target_fps);

        } else {
            sprintf(rbuf.data(), "--.-/%.2f", target_fps);
        }
    }
    return std::string(rbuf.data());
}
} // namespace

FpsMonitor::FpsMonitor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    set_default_handler(caf::drop);

    // connect to view port.
    fps_event_grp_ = spawn<broadcast::BroadcastActor>(this);
    link_to(fps_event_grp_);

    behavior_.assign(

        [=](utility::get_event_group_atom) -> caf::actor { return fps_event_grp_; },
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](ui::fps_monitor::connect_to_playhead_atom, caf::actor playhead) {
            connect_to_playhead(playhead);
        },
        [=](ui::fps_monitor::framebuffer_swapped_atom,
            const utility::time_point &tp,
            const int frame) {
            framebuffer_swapped(tp, frame);
            frame_queued_for_display_ = false;
        },
        [=](update_actual_fps_atom) {
            std::string expr;
            if (!playing_) {
                expr = make_fps_display_string(FrameRate(), target_playhead_rate_, velocity_);
            } else if (velocity_multiplier_ == 1.0f) {
                const FrameRate r = caluculate_actual_fps_measure();
                expr = make_fps_display_string(r, target_playhead_rate_, velocity_);
                caf::scoped_actor sys(this->system());
                sys->delayed_send(
                    this, std::chrono::milliseconds(500), update_actual_fps_atom_v);
            } else {
                std::array<char, 128> buf;
                if (forward_) {
                    sprintf(buf.data(), "FF x %.0f", velocity_multiplier_);
                } else {
                    sprintf(buf.data(), "RW x %.0f", velocity_multiplier_);
                }
                expr = std::string(buf.data());
            }
            send(fps_event_grp_, utility::event_atom_v, fps_meter_update_atom_v, expr);
        },
        [=](utility::event_atom, playhead::play_atom, const bool is_playing) {
            playing_ = is_playing;
            if (playing_) {
                reset_actual_fps_measure();
            } else {
                velocity_multiplier_ = 1.0;
            }
            anon_send(this, update_actual_fps_atom_v);
        },
        [=](playhead::show_atom) { frame_queued_for_display_ = true; },
        [=](utility::event_atom, velocity_multiplier_atom, const float multiplier) {
            velocity_multiplier_ = multiplier;
            anon_send(this, update_actual_fps_atom_v);
        },
        [=](utility::event_atom, velocity_atom, const float v) {
            velocity_ = v;
            anon_send(this, update_actual_fps_atom_v);
        },
        [=](utility::event_atom, playhead::play_forward_atom, const bool fwd) {
            forward_ = fwd;
            anon_send(this, update_actual_fps_atom_v);
        },
        [=](utility::event_atom,
            playhead::actual_playback_rate_atom,
            const utility::FrameRate &rate) {
            target_playhead_rate_ = rate;
            anon_send(this, update_actual_fps_atom_v);
        },
        [=](const error &err) mutable { aout(this) << err << std::endl; });
}

void FpsMonitor::reset_actual_fps_measure() { viewport_frame_update_timepoints_.clear(); }

void FpsMonitor::framebuffer_swapped(const utility::time_point &tp, const int frame) {

    // we don't have enough samples, so fill in some phony samples
    if (viewport_frame_update_timepoints_.size() < MIN_FPS_MEASURE_EVENTS) {

        const int n         = MIN_FPS_MEASURE_EVENTS - viewport_frame_update_timepoints_.size();
        const auto duration = target_playhead_rate_.to_microseconds();
        auto time           = tp - duration;
        for (int i = 0; i < n; ++i) {
            viewport_frame_update_timepoints_.push_front(std::make_pair(time, frame - n + i));
            time -= duration;
        }
    }

    viewport_frame_update_timepoints_.push_back(std::make_pair(tp, frame));
    if (viewport_frame_update_timepoints_.size() > MAX_FPS_MEASURE_EVENTS) {
        viewport_frame_update_timepoints_.pop_front();
    }
}

FrameRate FpsMonitor::caluculate_actual_fps_measure() const {

    if (viewport_frame_update_timepoints_.size() >= MIN_FPS_MEASURE_EVENTS) {

        const int ms_interval = std::chrono::duration_cast<std::chrono::microseconds>(
                                    viewport_frame_update_timepoints_.back().first -
                                    viewport_frame_update_timepoints_.front().first)
                                    .count();

        return FrameRate(
            double(ms_interval) / (1000000.0 * (viewport_frame_update_timepoints_.size() - 1)));
    }

    // not enough samples
    return target_playhead_rate_;
}

void FpsMonitor::connect_to_playhead(caf::actor &playhead) {

    caf::scoped_actor sys(this->system());

    target_playhead_rate_ = utility::FrameRate(24.0);
    velocity_             = 1.0f;
    velocity_multiplier_  = 1.0f;
    forward_              = true;
    playing_              = false;
    try {

        auto playhead_fps_events = utility::request_receive<caf::actor>(
            *sys, playhead, playhead::fps_event_group_atom_v);

        if (playhead_grp_) {
            try {
                request_receive<bool>(
                    *sys, playhead_grp_, broadcast::leave_broadcast_atom_v, this);
            } catch (...) {
            }
        }
        playhead_grp_ = playhead_fps_events;

        request_receive<bool>(*sys, playhead_grp_, broadcast::join_broadcast_atom_v, this);

        playing_ = utility::request_receive<bool>(*sys, playhead, playhead::play_atom_v);

        forward_ =
            utility::request_receive<bool>(*sys, playhead, playhead::play_forward_atom_v);

        velocity_ = utility::request_receive<float>(*sys, playhead, playhead::velocity_atom_v);

        velocity_multiplier_ = utility::request_receive<float>(
            *sys, playhead, playhead::velocity_multiplier_atom_v);

        // this will often throw an error if the playlist is empty or out of
        // range etc., so we ignore
        try {
            target_playhead_rate_ = utility::request_receive<utility::FrameRate>(
                *sys, playhead, playhead::actual_playback_rate_atom_v);
        } catch (...) {
        }

    } catch ([[maybe_unused]] std::exception &e) {
    }
    anon_send(this, update_actual_fps_atom_v);
}