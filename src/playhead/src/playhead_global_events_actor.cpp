// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/playhead/playhead_global_events_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::playhead;
using namespace xstudio::media;
using namespace caf;


PlayheadGlobalEventsActor::PlayheadGlobalEventsActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    init();
}

void PlayheadGlobalEventsActor::on_exit() {
    global_active_playhead_ = caf::actor();
    viewports_.clear();
    system().registry().erase(global_playhead_events_actor);

}

void PlayheadGlobalEventsActor::init() {

    spdlog::debug("Created PlayheadGlobalEventsActor {}", name());
    print_on_exit(this, "PlayheadGlobalEventsActor");

    system().registry().put(global_playhead_events_actor, this);

    event_group_             = spawn<broadcast::BroadcastActor>(this);
    fine_grain_events_group_ = spawn<broadcast::BroadcastActor>(this);

    link_to(event_group_);

    set_default_handler(
        [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
            //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

            spdlog::warn(
                "Got broadcast from {} {}", to_string(current_sender()), to_string(msg));

            return message{};
        });

    set_down_handler([=](down_msg &msg) {
        // a playhead OR a viewport has gone offline
        auto q = viewports_.begin();
        if (msg.source == global_active_playhead_) {
            global_active_playhead_ = caf::actor();
        }
        while (q != viewports_.end()) {
            if (msg.source == q->second.viewport) {
                demonitor(q->second.viewport);
                q = viewports_.erase(q);
            } else {
                if (msg.source == q->second.playhead) {
                    anon_send(
                        q->second.viewport,
                        ui::viewport::viewport_playhead_atom_v,
                        caf::actor());
                    demonitor(q->second.playhead);
                    q->second.playhead = caf::actor();
                }
                if (msg.source == global_active_playhead_) {
                    global_active_playhead_ = caf::actor();
                }
                q++;
            }
        }
    });

    behavior_.assign(

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},
        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },
        [=](broadcast::join_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::join_broadcast_atom_v, joiner);
        },
        [=](broadcast::leave_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::leave_broadcast_atom_v, joiner);
        },
        [=](ui::viewport::viewport_playhead_atom) -> caf::actor {
            return global_active_playhead_;
        },
        [=](ui::viewport::viewport_playhead_atom,
            caf::actor playhead,
            bool request) -> std::vector<caf::actor> {
            std::vector<caf::actor> r;
            // return viewports that are connected to the given playhead
            for (auto &p : viewports_) {
                if (p.second.playhead == playhead) {
                    r.push_back(p.second.viewport);
                }
            }
            return r;
        },
        [=](ui::viewport::viewport_cursor_atom, const std::string &cursor_name) {
            for (auto &p : viewports_) {
                anon_send(p.second.viewport, ui::viewport::viewport_cursor_atom_v, cursor_name);
            }
        },
        [=](ui::viewport::viewport_playhead_atom, caf::actor playhead) {
            // something can send us this message in order to set the 'global'
            // playhead - i.e. the playhead that is being viewed by non-quickview
            // viewports. SessionModel::setCurrentPlayheadFromPlaylist does this for example.
            for (auto &p : viewports_) {
                anon_send(p.second.viewport, ui::viewport::viewport_playhead_atom_v, playhead);
            }
            global_active_playhead_ = playhead;
            send(
                event_group_,
                utility::event_atom_v,
                ui::viewport::viewport_playhead_atom_v,
                global_active_playhead_);
        },
        [=](ui::viewport::viewport_playhead_atom,
            const std::string viewport_name,
            caf::actor playhead) {
            if (viewports_[viewport_name].playhead == playhead)
                return;

            // a viewport named 'viewport_name' is connecting to a playhead
            send(
                event_group_,
                utility::event_atom_v,
                ui::viewport::viewport_playhead_atom_v,
                viewport_name,
                playhead);

            // what's the playhead that is currently attached to the viewport
            // (if any)
            auto playhead_to_be_disconnected = viewports_[viewport_name].playhead;

            viewports_[viewport_name].playhead = playhead;

            // is this old playhead connected to another viewport?
            for (auto &p : viewports_) {
                if (p.second.playhead == playhead_to_be_disconnected) {
                    playhead_to_be_disconnected = caf::actor();
                    break;
                }
            }

            if (playhead_to_be_disconnected) {

                // No, no other viewports are using the playhead that is to
                // be disconnected from the viewport. Therefore we tell the
                // playhead to stop playing (if it is playing).
                anon_send(playhead_to_be_disconnected, playhead::play_atom_v, false);
                anon_send(playhead_to_be_disconnected, module::disconnect_from_ui_atom_v);
                // we can stop monitoring it as we don't care if it exits or
                // not - we're only keeping track of playheads that are
                // connected to viewports
                demonitor(playhead_to_be_disconnected);
            }

            if (playhead) {
                monitor(playhead);
                // since the playhead has changed we want to tell subscribers
                // the new media/media_source
                request(playhead, infinite, playhead::media_atom_v)
                    .then(
                        [=](caf::actor media) {
                            request(playhead, infinite, playhead::media_source_atom_v)
                                .then(
                                    [=](caf::actor media_source) {
                                        send(
                                            event_group_,
                                            utility::event_atom_v,
                                            show_atom_v,
                                            media,
                                            media_source,
                                            viewport_name);
                                    },
                                    [=](caf::error &err) {

                                    });
                        },
                        [=](caf::error &err) {

                        });
            }
        },

        [=](show_atom, const media_reader::ImageBufPtr &buf) {
            // a playhead is telling us a new frame is being shown.

            // Forward the info to our 'fine grain' message group with details
            // of which viewport the frame is being shown on
            auto playhead = caf::actor_cast<caf::actor>(current_sender());
            for (auto &p : viewports_) {
                if (p.second.playhead == playhead) {
                }
            }
        },
        [=](show_atom, caf::actor media, caf::actor media_source) {
            // a playhead is telling us the on-screen media has changed
            auto playhead = caf::actor_cast<caf::actor>(current_sender());
            for (auto &p : viewports_) {
                if (p.second.playhead == playhead) {
                    // forward the event, including the name of the viewport(s)
                    // that are attached to the playhead
                    send(
                        event_group_,
                        utility::event_atom_v,
                        show_atom_v,
                        media,
                        media_source,
                        p.first);
                }
            }
        },
        [=](ui::viewport::viewport_playhead_atom,
            const std::string viewport_name) -> result<caf::actor> {
            if (viewport_name.empty())
                return global_active_playhead_;
            if (viewports_.find(viewport_name) != viewports_.end()) {
                return viewports_[viewport_name].playhead;
            }
            return make_error(
                xstudio_error::error, fmt::format("No viewport named {}", viewport_name));
        },
        [=](ui::viewport::viewport_atom) -> std::vector<caf::actor> {
            std::vector<caf::actor> result;
            for (const auto &p : viewports_) {
                result.push_back(p.second.viewport);
            }
            return result;
        },
        [=](ui::viewport::viewport_atom, const std::string viewport_name, caf::actor viewport) {
            monitor(viewport);
            // viewports register themselves by sending us this message
            viewports_[viewport_name] = ViewportAndPlayhead({viewport, caf::actor()});
            send(
                event_group_,
                utility::event_atom_v,
                ui::viewport::viewport_atom_v,
                viewport_name,
                viewport);
        },
        [=](playhead::redraw_viewport_atom) { 
            
            // force all viewport to do a redraw
            for (const auto &p: viewports_) {
                anon_send(p.second.viewport, playhead::redraw_viewport_atom_v);
            }        
        },
        [=](ui::viewport::viewport_atom,
            const std::string viewport_name) -> result<caf::actor> {
            // Here we can request a named viewport
            caf::actor r;
            auto p = viewports_.find(viewport_name);
            if (p != viewports_.end()) {
                r = p->second.viewport;
            }
            if (!r)
                return make_error(
                    xstudio_error::error, fmt::format("No viewport named {}", viewport_name));
            return r;
        },
        [=](ui::viewport::fit_mode_atom atom,
            const ui::viewport::FitMode mode,
            const ui::viewport::MirrorMode mirror_mode,
            const float scale,
            const Imath::V2f pan,
            const std::string &viewport_name,
            const std::string &window_id) {
            // viewports tell us when they are zooming/panning, setting fit or mirror mode.
            // We broadcast to all other viewports so that they can track (if they want to)
            for (const auto &p : viewports_) {
                if (p.first != viewport_name) {
                    send(
                        p.second.viewport,
                        atom,
                        mode,
                        mirror_mode,
                        scale,
                        pan,
                        viewport_name,
                        window_id);
                }
            }
        },
        [=](ui::viewport::active_viewport_atom) -> caf::actor {
            // find a viewport that lives in the xstudio_main_window
            // and is visible
            caf::scoped_actor sys(system());
            try {

                for (const auto &p : viewports_) {
                    auto window_name = utility::request_receive<std::string>(
                        *sys, p.second.viewport, utility::name_atom_v, true);
                    if (window_name == "xstudio_main_window") {
                        if (utility::request_receive<bool>(
                                *sys,
                                p.second.viewport,
                                ui::viewport::viewport_visibility_atom_v)) {
                            return p.second.viewport;
                        }
                    }
                }
            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
            return caf::actor();
        });
}