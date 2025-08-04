// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

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

void PlayheadGlobalEventsActor::monitor_it(const caf::actor &actor) {
    auto act_addr = caf::actor_cast<caf::actor_addr>(actor);

    if (auto sit = monitor_.find(act_addr); sit == std::end(monitor_)) {
        monitor_[act_addr] = monitor(actor, [this, addr = actor.address()](const error &) {
            if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(addr));
                mit != std::end(monitor_))
                monitor_.erase(mit);

            auto q = viewports_.begin();
            if (addr == global_active_playhead_) {
                global_active_playhead_ = caf::actor();
            }
            while (q != viewports_.end()) {
                if (addr == q->second.viewport) {
                    q = viewports_.erase(q);
                } else {
                    if (addr == q->second.playhead) {
                        anon_mail(ui::viewport::viewport_playhead_atom_v, caf::actor())
                            .send(q->second.viewport);
                        q->second.playhead = caf::actor();
                    }
                    if (addr == global_active_playhead_) {
                        global_active_playhead_ = caf::actor();
                    }
                    q++;
                }
            }
        });
    }
}


void PlayheadGlobalEventsActor::init() {

    spdlog::debug("Created PlayheadGlobalEventsActor {}", name());
    print_on_exit(this, "PlayheadGlobalEventsActor");

    system().registry().put(global_playhead_events_actor, this);

    event_group_             = spawn<broadcast::BroadcastActor>(this);
    fine_grain_events_group_ = spawn<broadcast::BroadcastActor>(this);

    link_to(event_group_);

    // set_default_handler(
    //     [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
    //         //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

    //         spdlog::warn(
    //             "Got broadcast from {} {}", to_string(current_sender()), to_string(msg));

    //         return message{};
    //     });

    behavior_.assign(

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },
        [=](broadcast::join_broadcast_atom, caf::actor joiner) {
            return mail(broadcast::join_broadcast_atom_v, joiner).delegate(event_group_);
        },
        [=](broadcast::leave_broadcast_atom, caf::actor joiner) {
            return mail(broadcast::leave_broadcast_atom_v, joiner).delegate(event_group_);
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
        [=](ui::viewport::viewport_cursor_atom,
            const std::string &cursor_name,
            const int size,
            const int x_offset,
            const int y_offset) {
            for (auto &p : viewports_) {
                anon_mail(
                    ui::viewport::viewport_cursor_atom_v, cursor_name, size, x_offset, y_offset)
                    .send(p.second.viewport);
            }
        },
        [=](ui::viewport::viewport_playhead_atom, caf::actor playhead) {
            // something can send us this message in order to set the 'global'
            // playhead - i.e. the playhead that is being viewed by non-quickview
            // viewports. SessionModel::setCurrentPlayheadFromPlaylist does this for example.
            for (auto &p : viewports_) {
                anon_mail(ui::viewport::viewport_playhead_atom_v, playhead)
                    .send(p.second.viewport);
            }
            global_active_playhead_ = playhead;
            mail(
                utility::event_atom_v,
                ui::viewport::viewport_playhead_atom_v,
                global_active_playhead_)
                .send(event_group_);
        },
        [=](ui::viewport::viewport_playhead_atom,
            const std::string viewport_name,
            caf::actor playhead) {
            if (viewports_[viewport_name].playhead == playhead)
                return;

            // a viewport named 'viewport_name' is connecting to a playhead
            mail(
                utility::event_atom_v,
                ui::viewport::viewport_playhead_atom_v,
                viewport_name,
                playhead)
                .send(event_group_);

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
                anon_mail(playhead::play_atom_v, false).send(playhead_to_be_disconnected);
                anon_mail(module::disconnect_from_ui_atom_v).send(playhead_to_be_disconnected);
                // we can stop monitoring it as we don't care if it exits or
                // not - we're only keeping track of playheads that are
                // connected to viewports

                if (auto it = monitor_.find(
                        caf::actor_cast<caf::actor_addr>(playhead_to_be_disconnected));
                    it != std::end(monitor_)) {
                    it->second.dispose();
                    monitor_.erase(it);
                }
            }

            if (playhead) {
                monitor_it(playhead);
                // since the playhead has changed we want to tell subscribers
                // the new media/media_source
                mail(playhead::media_atom_v)
                    .request(playhead, infinite)
                    .then(
                        [=](caf::actor media) {
                            mail(playhead::media_source_atom_v)
                                .request(playhead, infinite)
                                .then(
                                    [=](caf::actor media_source) {
                                        mail(
                                            utility::event_atom_v,
                                            show_atom_v,
                                            media,
                                            media_source,
                                            viewport_name)
                                            .send(event_group_);
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
                    mail(utility::event_atom_v, show_atom_v, media, media_source, p.first)
                        .send(event_group_);
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
        [=](ui::viewport::viewport_atom, const std::string viewport_name, const std::string window_id, caf::actor viewport) {
            monitor_it(viewport);
            // viewports register themselves by sending us this message
            viewports_[viewport_name] = ViewportAndPlayhead({viewport, caf::actor(), window_id});
            mail(utility::event_atom_v, ui::viewport::viewport_atom_v, viewport_name, viewport)
                .send(event_group_);
        },
        [=](playhead::redraw_viewport_atom) {
            // force all viewport to do a redraw
            for (const auto &p : viewports_) {
                anon_mail(playhead::redraw_viewport_atom_v).send(p.second.viewport);
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
                    mail(atom, mode, mirror_mode, scale, pan, viewport_name, window_id)
                        .send(p.second.viewport);
                }
            }
        },
        [=](ui::viewport::active_viewport_atom) -> result<caf::actor> {
            // find a viewport that lives in the xstudio_main_window
            // and is visible
            auto rp = make_response_promise<caf::actor>();
            auto ct = std::make_shared<int>(0);
            for (const auto &p : viewports_) {
                if (p.second.window_id == "xstudio_main_window") {
                    (*ct)++;
                    auto vp = p.second.viewport;
                    mail(ui::viewport::viewport_visibility_atom_v).request(vp, infinite).then(
                        [=](bool visible) mutable {
                            if (visible && rp.pending()) {
                                rp.deliver(vp);
                            }
                            (*ct)--;
                            if (!(*ct) && rp.pending()) {
                                rp.deliver(caf::actor());
                            }
                        },
                        [=](caf::error &err) mutable {
                            (*ct)--;
                            if (!(*ct) && rp.pending()) {
                                rp.deliver(caf::actor());
                            }
                        });
                }
            }
            if (!(*ct)) rp.deliver(caf::actor());
            return rp;
        },
        [=](ui::viewport::active_viewport_atom, bool) -> caf::actor {
            // find ANY viewport that lives in the xstudio_main_window. This
            // is used when a new viewport is created - it asks an already
            // exsiting viewport for its fit mode/zoom/pan
            for (const auto &p : viewports_) {
                if (p.second.window_id == "xstudio_main_window") {
                    return p.second.viewport;
                }
            }
            return caf::actor();
        });
}