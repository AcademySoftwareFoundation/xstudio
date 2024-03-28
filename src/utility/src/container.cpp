// SPDX-License-Identifier: Apache-2.0

#include <caf/scoped_actor.hpp>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

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
