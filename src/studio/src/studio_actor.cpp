// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/studio/studio_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/global_store/global_store.hpp"

using namespace caf;
using namespace xstudio::studio;
using namespace xstudio::utility;
using namespace xstudio::broadcast;
using namespace xstudio;

StudioActor::StudioActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {
    init();
}

StudioActor::StudioActor(caf::actor_config &cfg, const std::string &name)
    : caf::event_based_actor(cfg), base_(name) {
    init();
}

caf::message_handler StudioActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        make_get_version_handler(),

        [=](session::session_atom) -> caf::actor { return session_; },

        [=](bookmark::get_bookmark_atom atom) { delegate(session_, atom); },

        [=](session::session_atom, caf::actor session) -> bool {
            unlink_from(session_);
            send_exit(session_, caf::exit_reason::user_shutdown);
            session_ = session;
            link_to(session_);
            send(base_.event_group(), utility::event_atom_v, session::session_atom_v, session_);
            return true;
        },

        [=](session::session_request_atom,
            const std::string &path,
            const JsonStore &js) -> bool {
            // need to chat to UI ?
            send(
                base_.event_group(),
                utility::event_atom_v,
                session::session_request_atom_v,
                path,
                js);
            return true;
        },

        [=](ui::show_message_box_atom,
            const std::string &message_title,
            const std::string &message_body,
            const bool close_button,
            const int timeout_seconds) {
            caf::actor studio_ui_actor =
                system().registry().template get<caf::actor>(studio_ui_registry);

            // Request (from somewhere) to open light viewers for list of media items.
            // Forward to UI via event group so UI can handle it.
            if (studio_ui_actor) {
                anon_send(
                    studio_ui_actor,
                    utility::event_atom_v,
                    ui::show_message_box_atom_v,
                    message_title,
                    message_body,
                    close_button,
                    timeout_seconds);
            }
        },

        // [&](session::create_player_atom atom, const std::string &name) {// delegate(session_,
        // atom, name);// },
        [=](utility::serialise_atom) -> result<JsonStore> {
            if (session_) {
                auto rp = make_response_promise<JsonStore>();
                request(session_, caf::infinite, utility::serialise_atom_v)
                    .then(
                        [=](const JsonStore &_jsn) mutable {
                            JsonStore jsn;
                            jsn["base"]    = base_.serialise();
                            jsn["session"] = _jsn;
                            rp.deliver(jsn);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }

            JsonStore jsn;
            jsn["base"]    = base_.serialise();
            jsn["session"] = nullptr;
            return result<JsonStore>(jsn);
        },
        [=](ui::open_quickview_window_atom atom,
            const utility::UuidActorVector &media_items,
            std::string compare_mode) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                media_items,
                compare_mode,
                utility::JsonStore(),
                utility::JsonStore());
        },
        [=](ui::open_quickview_window_atom,
            const utility::UuidActorVector &media_items,
            std::string compare_mode,
            const utility::JsonStore in_point,
            const utility::JsonStore out_point) {
            caf::actor studio_ui_actor =
                system().registry().template get<caf::actor>(studio_ui_registry);

            if (studio_ui_actor) {
                // forward to StudioUI instance
                anon_send(
                    studio_ui_actor,
                    utility::event_atom_v,
                    ui::open_quickview_window_atom_v,
                    media_items,
                    compare_mode,
                    in_point,
                    out_point);
            } else {
                // UI hasn't started up yet, store the request
                QuickviewRequest request;
                request.media_actors = media_items;
                request.compare_mode = compare_mode;
                request.in_point     = in_point;
                request.out_point    = out_point;
                quickview_requests_.push_back(request);
            }
        },
        [=](ui::open_quickview_window_atom, caf::actor studio_ui_actor) {
            // the StudioUI instance has started up and pinged us with itself
            // so we can send it any pending requests for quickviewers

            for (const auto &r : quickview_requests_) {
                anon_send(
                    studio_ui_actor,
                    utility::event_atom_v,
                    ui::open_quickview_window_atom_v,
                    r.media_actors,
                    r.compare_mode,
                    r.in_point,
                    r.out_point);
            }
            quickview_requests_.clear();
        }};
}


void StudioActor::init() {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created StudioActor {}", base_.name());

    session_ = spawn<session::SessionActor>("New Session");
    link_to(session_);

    system().registry().put(studio_registry, this);
}

void StudioActor::on_exit() {
    // tell globals to quit... ?
    // destroy(session_);
    system().registry().erase(studio_registry);
}
