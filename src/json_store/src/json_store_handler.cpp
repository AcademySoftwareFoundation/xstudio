// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_handler.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace caf;
using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace xstudio;

caf::message_handler JsonStoreHandler::default_event_handler() {
    return JsonStoreActor::default_event_handler();
}

JsonStoreHandler::JsonStoreHandler(
    caf::event_based_actor *act,
    caf::actor event_group,
    const utility::Uuid &uuid,
    const utility::JsonStore &json,
    const std::chrono::milliseconds &delay) {

    actor_       = act;
    event_group_ = event_group;

    json_store_ = actor_->spawn<json_store::JsonStoreActor>(uuid, json, delay);
    actor_->link_to(json_store_);
    utility::join_event_group(act, json_store_);
}


caf::message_handler JsonStoreHandler::message_handler() {
    return caf::message_handler({
        [=](json_store::get_json_atom _get_atom) {
            return actor_->mail(_get_atom).delegate(json_store_);
        },

        [=](json_store::get_json_atom _get_atom, const std::string &path, bool) {
            return actor_->mail(_get_atom, path).delegate(json_store_);
        },

        [=](json_store::get_json_atom _get_atom, const std::string &path) {
            return actor_->mail(_get_atom, path).delegate(json_store_);
        },

        [=](utility::get_group_atom _get_group_atom) {
            return actor_->mail(_get_group_atom).delegate(json_store_);
        },

        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            if (actor_->current_sender() == json_store_)
                actor_->mail(json_store::update_atom_v, change, path, full).send(event_group_);
        },

        [=](json_store::update_atom, const JsonStore &full) mutable {
            if (actor_->current_sender() == json_store_)
                actor_->mail(json_store::update_atom_v, full).send(event_group_);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json) {
            return actor_->mail(atom, json).delegate(json_store_);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            return actor_->mail(atom, json, path).delegate(json_store_);
        },

    });
}
