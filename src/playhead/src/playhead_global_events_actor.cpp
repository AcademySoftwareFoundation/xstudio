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

void PlayheadGlobalEventsActor::init() {

    spdlog::debug("Created PlayheadGlobalEventsActor {}", name());
    print_on_exit(this, "PlayheadGlobalEventsActor");

    system().registry().put(global_playhead_events_actor, this);

    event_group_ = spawn<broadcast::BroadcastActor>(this);

    link_to(event_group_);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        if (msg.source == on_screen_playhead_) {
            demonitor(on_screen_playhead_);
            on_screen_playhead_ = caf::actor();
            send(
                event_group_,
                utility::event_atom_v,
                ui::viewport::viewport_playhead_atom_v,
                on_screen_playhead_);
        }
    });

    behavior_.assign(

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},
        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },
        [=](broadcast::join_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::join_broadcast_atom_v, joiner);
            if (on_screen_playhead_) {
                // re-send events so new joiner has current on-screen buffer etc.
                send(
                    event_group_,
                    utility::event_atom_v,
                    ui::viewport::viewport_playhead_atom_v,
                    on_screen_playhead_);
                request(on_screen_playhead_, infinite, buffer_atom_v)
                    .then(
                        [=](const media_reader::ImageBufPtr &buf) {
                            send(event_group_, utility::event_atom_v, show_atom_v, buf);
                        },
                        [=](caf::error &) {});
            }
        },
        [=](broadcast::leave_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::leave_broadcast_atom_v, joiner);
        },
        [=](ui::viewport::viewport_playhead_atom, caf::actor playhead) {
            if (on_screen_playhead_ != playhead) {
                send(
                    event_group_,
                    utility::event_atom_v,
                    ui::viewport::viewport_playhead_atom_v,
                    playhead);
                on_screen_playhead_ = playhead;
                monitor(playhead);
            }
        },
        [=](show_atom, const media_reader::ImageBufPtr &buf) {
            if (caf::actor_cast<caf::actor>(current_sender()) == on_screen_playhead_) {
                send(event_group_, utility::event_atom_v, show_atom_v, buf);
            }
        },
        [=](show_atom, caf::actor media, caf::actor media_source) {
            if (caf::actor_cast<caf::actor>(current_sender()) == on_screen_playhead_) {
                send(event_group_, utility::event_atom_v, show_atom_v, media, media_source);
            }
        },
        [=](ui::viewport::viewport_playhead_atom) -> caf::actor {
            return on_screen_playhead_;
        });
}