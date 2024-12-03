// SPDX-License-Identifier: Apache-2.0

#include <caf/scoped_actor.hpp>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"

using namespace xstudio;
using namespace xstudio::utility;

void Container::set_file_version(const std::string &version, const bool warn) {
    semver::version nv(version);
    if (nv > file_version_ and warn)
        spdlog::debug(
            "Deserialising old format {} -> {} {}",
            to_string(file_version_),
            to_string(nv),
            type_);
    file_version_ = std::move(nv);
}

Container::Container(const JsonStore &jsn) : Container(jsn["name"], jsn["type"], jsn["uuid"]) {
    if (jsn.count("version"))
        version_ = semver::from_string(static_cast<std::string>(jsn["version"]));
    if (jsn.count("file_version"))
        file_version_ = semver::from_string(static_cast<std::string>(jsn["file_version"]));
}

void Container::deserialise(const utility::JsonStore &jsn) {
    name_    = jsn.at("name");
    type_    = jsn.at("type");
    uuid_    = jsn.at("uuid");
    version_ = semver::from_string(PROJECT_VERSION);

    if (jsn.count("version"))
        version_ = semver::from_string(jsn.at("version").get<std::string>());

    if (jsn.count("file_version"))
        file_version_ = semver::from_string(jsn.at("file_version").get<std::string>());

    last_changed_ = utility::clock::now();
}

Container Container::duplicate() const {
    Container result = *this;

    result.set_uuid(Uuid::generate());

    return result;
}

caf::message_handler Container::default_event_handler() {
    return caf::message_handler(
        {[=](utility::event_atom, utility::name_atom, const std::string &) {},
         [=](utility::event_atom, utility::last_changed_atom, const time_point &) {}});
}

caf::message_handler Container::container_message_handler(caf::event_based_actor *act) {
    actor_       = act;
    event_group_ = actor_->spawn<broadcast::BroadcastActor>(actor_);
    actor_->link_to(event_group_);

    return caf::message_handler({
        [=](name_atom, const std::string &name) { // make_set_name_handler
            name_ = name;
            actor_->send(event_group_, event_atom_v, name_atom_v, name);
            send_changed();
        },
        [=](name_atom) -> std::string { return name_; }, // make_get_name_handler
        [=](last_changed_atom) -> time_point {
            return last_changed_;
        },                                                       // make_last_changed_getter
        [=](last_changed_atom, const time_point &last_changed) { // make_last_changed_setter
            if (last_changed > last_changed_ or last_changed == time_point()) {
                last_changed_ = last_changed;
                actor_->send(
                    event_group_, utility::event_atom_v, last_changed_atom_v, last_changed_);
            }
        },
        [=](utility::event_atom,
            last_changed_atom,
            const time_point &last_changed) { // make_last_changed_event_handler
            if (last_changed > last_changed_) {
                last_changed_ = last_changed;
                actor_->send(
                    event_group_, utility::event_atom_v, last_changed_atom_v, last_changed_);
            }
        },
        [=](uuid_atom) -> Uuid { return uuid_; },        // make_get_uuid_handler
        [=](type_atom) -> std::string { return type_; }, // make_get_type_handler
        [=](get_event_group_atom) -> caf::actor {
            return event_group_;
        }, // make_get_event_group_handler
        [=](detail_atom) -> ContainerDetail {
            return detail(actor_, event_group_);
        } // make_get_detail_handler
    });
}


JsonStore Container::serialise() const {
    utility::JsonStore jsn;
    jsn["name"]         = name_;
    jsn["type"]         = type_;
    jsn["uuid"]         = uuid_;
    jsn["version"]      = to_string(version_);
    jsn["file_version"] = to_string(file_version_);
    return jsn;
}


PlaylistItem::PlaylistItem(const JsonStore &jsn)
    : PlaylistItem(jsn["name"], jsn["type"], jsn["uuid"], jsn["flag"]) {}

JsonStore PlaylistItem::serialise() const {
    utility::JsonStore jsn;
    jsn["name"] = name_;
    jsn["type"] = type_;
    jsn["flag"] = flag_;
    jsn["uuid"] = uuid_;

    return jsn;
}


bool PlaylistTree::rename(
    const std::string &name, const utility::Uuid &uuid, UuidTree<PlaylistItem> &child) {
    bool result = false;
    if (child.uuid_ == uuid) {
        child.value().set_name(name);
        result = true;
    } else {
        for (auto &i : child.children_ref()) {
            result = rename(name, uuid, i);
            if (result)
                break;
        }
    }

    return result;
}

bool PlaylistTree::reflag(
    const std::string &flag, const utility::Uuid &uuid, UuidTree<PlaylistItem> &child) {
    bool result = false;
    if (child.uuid_ == uuid) {
        child.value().set_flag(flag);
        result = true;
    } else {
        for (auto &i : child.children_ref()) {
            result = reflag(flag, uuid, i);
            if (result)
                break;
        }
    }

    return result;
}

utility::UuidVector
PlaylistTree::children_uuid(const UuidTree<PlaylistItem> &child, const bool recursive) const {
    utility::UuidVector result;

    for (const auto &i : child.children_ref()) {
        if (recursive) {
            auto tmp = children_uuid(i, recursive);
            result.insert(result.end(), tmp.begin(), tmp.end());
        }
        result.push_back(i.value().uuid());
    }

    return result;
}


std::optional<UuidTree<PlaylistItem> *> PlaylistTree::find_value(const utility::Uuid &uuid) {
    // find item by playlistitem uuid
    if (uuid == value_uuid())
        return this;

    auto i = std::begin(children_);
    for (; i != std::end(children_); ++i) {
        if (i->value().uuid() == uuid) {
            return &(*i);
        }
        auto tmp = dynamic_cast<PlaylistTree *>(&(*i))->find_value(uuid_);
        if (tmp)
            return *tmp;
    }
    return {};
}

std::optional<const UuidTree<PlaylistItem> *>
PlaylistTree::cfind_value(const utility::Uuid &uuid) const {
    // find item by playlistitem uuid
    if (uuid == value_uuid())
        return this;

    auto i = std::cbegin(children_);
    for (; i != std::cend(children_); ++i) {
        if (i->value().uuid() == uuid) {
            return &(*i);
        }
        auto plt = dynamic_cast<const PlaylistTree *>(&(*i));
        if (plt) {
            auto tmp = plt->cfind_value(uuid_);
            if (tmp)
                return *tmp;
        }
    }
    return {};
}

std::optional<UuidTree<PlaylistItem> *> PlaylistTree::find_any(const utility::Uuid &uuid) {
    auto c = find(uuid);
    if (c)
        return *c;

    auto v = find_value(uuid);
    if (v)
        return *v;

    return {};
}

std::optional<const UuidTree<PlaylistItem> *>
PlaylistTree::cfind_any(const utility::Uuid &uuid) const {
    auto c = cfind(uuid);
    if (c)
        return *c;

    auto v = cfind_value(uuid);
    if (v)
        return *v;

    return {};
}

std::optional<Uuid>
PlaylistTree::find_next_value_at_same_depth(const utility::Uuid &uuid) const {

    auto i = std::cbegin(children_);
    for (; i != std::cend(children_); ++i) {
        if (i->value().uuid() == uuid) {
            i++;
            if (i != std::cend(children_)) {
                return i->value().uuid();
            }
            // found item is at end of children_ list, so try
            // using the previous item in children
            i--;
            if (i != std::cbegin(children_)) {
                return i->value().uuid();
            }
            // fail, as there is only one chid
        }

        auto child = dynamic_cast<const PlaylistTree *>(&(*i));
        if (child) {
            auto tmp = child->find_next_value_at_same_depth(uuid);
            if (tmp)
                return *tmp;
        }
    }
    return {};
}
