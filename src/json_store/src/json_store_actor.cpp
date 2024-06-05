// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <chrono>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/types.hpp"

using namespace std::chrono_literals;
using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace xstudio;
using namespace caf;

JsonStoreActor::JsonStoreActor(
    caf::actor_config &cfg,
    const Uuid &uuid,
    const JsonStore &json,
    const std::chrono::milliseconds &delay)
    : caf::event_based_actor(cfg),
      json_store_(json),
      uuid_(uuid),
      update_pending_(false),
      broadcast_delay_(delay) {
    if (uuid_.is_null())
        uuid_.generate_in_place();

    broadcast_ = spawn<broadcast::BroadcastActor>(this);
    link_to(broadcast_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](get_json_atom) -> JsonStore { return json_store_; },

        [=](get_json_atom, const std::string &path) -> caf::result<JsonStore> {
            try {
                std::string np = path;
                return JsonStore(json_store_.get(np));
            } catch (const std::exception &e) {
                return make_error(
                    xstudio_error::error, std::string("get_json_atom ") + e.what());
            }
        },

        [=](jsonstore_change_atom) {
            send(broadcast_, update_atom_v, json_store_);
            update_pending_ = false;
        },

        [=](erase_json_atom, const std::string &path) -> bool {
            std::string p = path;
            auto result = json_store_.remove(path);
            if (result)
                broadcast_change();
            return result;
        },

        [=](patch_atom, const JsonStore &json) -> bool {
            const JsonStore j = json;
            json_store_ = json_store_.patch(j);
            broadcast_change();
            return true;
        },

        [=](merge_json_atom, const JsonStore &json) -> bool {
            const JsonStore j = json;
            json_store_.merge(j);
            broadcast_change();
            return true;
        },

        [=](utility::serialise_atom) -> JsonStore { return json_store_; },

        [=](set_json_atom atom, const JsonStore &json, const std::string &path) {
            std::string p = path;
            delegate(caf::actor_cast<actor>(this), atom, json, p, false);
        },

        [=](set_json_atom, const JsonStore &json) -> bool {
            // replace all
            const JsonStore j = json;
            json_store_.set(j);
            broadcast_change();
            return true;
        },

        [=](set_json_atom, const JsonStore &json, const std::string &path, const bool async)
            -> bool {
            // is it a subset
            std::string p = path;
            const JsonStore j = json;
            try {
                json_store_.set(j, p);
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                return false;
            }
            broadcast_change(j, p, async);
            return true;
        },

        [=](set_json_atom,
            const JsonStore &json,
            const std::string &path,
            const bool async,
            const bool _broadcast_change) -> bool {
            // is it a subset
            std::string p     = path;
            const JsonStore j = json;
            try {
                json_store_.set(j, p);
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                return false;
            }
            if (_broadcast_change)
                broadcast_change(j, p, async);
            return true;
        },

        [=](subscribe_atom, const std::string &path, caf::actor _actor) -> caf::result<bool> {
            // delegate to reader, return promise ?
            std::string p     = path;
            auto rp = make_response_promise<bool>();
            this->request(_actor, caf::infinite, utility::get_group_atom_v)
                .then(
                    [&, p, _actor, rp](
                        const std::pair<caf::actor, JsonStore> &data) mutable {
                        const auto [grp, json]                       = data;
                        actor_group_[actor_cast<actor_addr>(_actor)] = grp;
                        group_path_[grp]                             = p;

                        this->request(grp, caf::infinite, broadcast::join_broadcast_atom_v)
                            .then(
                                [=](const bool) mutable {},
                                [=](const error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                        json_store_.set(json, p);
                        broadcast_change();
                        rp.deliver(true);
                    },
                    [&](const caf::error &) { rp.deliver(false); });
            return rp;
        },

        [=](unsubscribe_atom, caf::actor _actor) {
            auto grp = actor_group_[actor_cast<actor_addr>(_actor)];
            leave_broadcast(this, grp);
            actor_group_.erase(actor_cast<actor_addr>(_actor));
            group_path_.erase(grp);
        },

        [=](update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
        },

        [=](update_atom, const JsonStore &json) {
            std::string path =
                group_path_[actor_group_[caf::actor_cast<actor_addr>(current_sender())]];
            nlohmann::json::json_pointer p(path);
            // submerge ?
            try {
                (*(dynamic_cast<nlohmann::json *>(&json_store_)))[p] = json;
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            broadcast_change();
        },

        [=](utility::get_group_atom) -> std::pair<caf::actor, JsonStore> {
            return std::make_pair(broadcast_, json_store_);
        },

        [=](utility::uuid_atom) -> Uuid { return uuid_; });
}


caf::message_handler JsonStoreActor::default_event_handler() {
    return {
        [=](update_atom, const JsonStore &, const std::string &, const JsonStore &) {},
        [=](update_atom, const JsonStore &) {}};
}

void JsonStoreActor::broadcast_change(
    const JsonStore &change, const std::string &path, const bool async) {
    std::string p     = path;
    if (broadcast_delay_.count() and async) {
        if (not update_pending_) {
            delayed_anon_send(this, broadcast_delay_, jsonstore_change_atom_v);
            update_pending_ = true;
        }
    } else {
        // minor change, send now (DANGER MAYBE CAUSE ASYNC ISSUES)
        send(broadcast_, update_atom_v, change, p, json_store_);
    }
}
