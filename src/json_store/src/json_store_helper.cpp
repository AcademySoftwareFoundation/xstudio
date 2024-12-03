// SPDX-License-Identifier: Apache-2.0
#include "xstudio/json_store/json_store_helper.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/types.hpp"

using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace caf;

namespace xstudio::json_store {
JsonStoreHelper::JsonStoreHelper(caf::actor_system &sys, const caf::actor store_actor)
    : system_(sys), store_actor_(actor_cast<caf::actor_addr>(store_actor)) {}

JsonStore JsonStoreHelper::get(const std::string &path) const {
    JsonStore res;
    auto a = caf::actor_cast<caf::actor>(store_actor_);
    if (not a)
        throw std::runtime_error("JsonStoreHelper is dead");

    system_->request(a, infinite, get_json_atom_v, path)
        .receive(
            [&](const JsonStore &_json) { res = _json; },
            [&](const error &err) { throw std::runtime_error(to_string(err)); });
    return res;
}

void JsonStoreHelper::set(
    const JsonStore &V,
    const std::string &path,
    const bool async,
    const bool broadcast_change) {
    auto a = caf::actor_cast<caf::actor>(store_actor_);
    if (not a)
        throw std::runtime_error("JsonStoreHelper is dead");
    if (async) {
        system_->send(a, set_json_atom_v, V, path, broadcast_change);
    } else {
        system_->request(a, infinite, set_json_atom_v, V, path, broadcast_change)
            .receive(
                [&](const bool &) {},
                [&](const error &err) { throw std::runtime_error(to_string(err)); });
    }
}

caf::actor JsonStoreHelper::get_jsonactor() const {
    auto a = caf::actor_cast<caf::actor>(store_actor_);
    caf::actor result;

    if (not a)
        throw std::runtime_error("JsonStoreHelper is dead");

    system_->request(a, infinite, utility::get_group_atom_v)
        .receive(
            [&](const std::tuple<caf::actor, caf::actor, JsonStore> &data) {
                const auto [jsa, grpa, json] = data;
                result                       = jsa;
            },
            [&](const error &err) { throw std::runtime_error(to_string(err)); });

    return result;
};

caf::actor JsonStoreHelper::get_group(utility::JsonStore &V) const {
    auto a = caf::actor_cast<caf::actor>(store_actor_);
    caf::actor grp;

    if (not a)
        throw std::runtime_error("JsonStoreHelper is dead");

    system_->request(a, infinite, utility::get_group_atom_v)
        .receive(
            [&](const std::tuple<caf::actor, caf::actor, JsonStore> &data) {
                const auto [jsa, grpa, json] = data;

                grp = grpa;
                V   = json;
            },
            [&](const error &err) { throw std::runtime_error(to_string(err)); });

    return grp;
}
} // namespace xstudio::json_store
