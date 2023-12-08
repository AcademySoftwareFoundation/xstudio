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

void StudioActor::init() {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created StudioActor {}", base_.name());

    session_ = spawn<session::SessionActor>("New Session");
    link_to(session_);

    system().registry().put(studio_registry, this);

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        make_get_version_handler(),

        [=](session::session_atom) -> caf::actor { return session_; },

        [=](session::session_atom, caf::actor session) -> bool {
            unlink_from(session_);
            send_exit(session_, caf::exit_reason::user_shutdown);
            session_ = session;
            link_to(session_);
            send(event_group_, utility::event_atom_v, session::session_atom_v, session_);
            return true;
        },

        [=](session::session_request_atom,
            const std::string &path,
            const JsonStore &js) -> bool {
            // need to chat to UI ?
            send(
                event_group_, utility::event_atom_v, session::session_request_atom_v, path, js);
            return true;
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
        });
}

void StudioActor::on_exit() {
    // tell globals to quit... ?
    // destroy(session_);
    system().registry().erase(studio_registry);
}
