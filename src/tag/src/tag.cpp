// SPDX-License-Identifier: Apache-2.0

#include "xstudio/tag/tag.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::tag;
using namespace xstudio::utility;

Tag::Tag(const utility::JsonStore &jsn) {
    id_         = jsn.at("id");
    link_       = jsn.at("link");
    type_       = jsn.at("type");
    data_       = jsn.at("data");
    unique_     = jsn.at("unique");
    persistent_ = true;
}

utility::JsonStore Tag::serialise() const {
    utility::JsonStore jsn;

    jsn["id"]     = id_;
    jsn["link"]   = link_;
    jsn["type"]   = type_;
    jsn["data"]   = data_;
    jsn["unique"] = unique_;

    return jsn;
}


TagBase::TagBase(const std::string &name) : Container(name, "TagBase") {}

TagBase::TagBase(const utility::JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])) {
    for (const auto &i : jsn["tags"])
        add_tag(Tag(i));
}

std::optional<Tag> TagBase::add_tag(const Tag tag) {
    // if unique keys exists we replace with new entry
    bool replaced = false;
    Tag replaced_tag;

    if (tag.unique()) {
        auto uni = tag.unique();
        for (const auto &i : tag_map_) {
            if (i.second.unique() == uni) {
                replaced     = true;
                replaced_tag = i.second;
                remove_tag(i.first);
                break;
            }
        }
    }

    tag_map_[tag.id()] = tag;
    if (not link_map_.count(tag.link()))
        link_map_[tag.link()] = utility::UuidVector();
    link_map_[tag.link()].push_back(tag.id());

    if (replaced)
        return replaced_tag;

    return {};
}

bool TagBase::remove_tag(const utility::Uuid &id) {
    if (tag_map_.count(id)) {
        std::remove(
            link_map_[tag_map_[id].link()].begin(), link_map_[tag_map_[id].link()].end(), id);
        tag_map_.erase(tag_map_.find(id));
        return true;
    }
    return false;
}

std::optional<Tag> TagBase::get_tag(const utility::Uuid &id) const {
    if (tag_map_.count(id))
        return tag_map_.at(id);
    return {};
}

std::vector<Tag> TagBase::get_tags(const utility::Uuid &link) const {
    if (link_map_.count(link)) {
        std::vector<Tag> tags;
        for (const auto &i : link_map_.at(link)) {
            tags.push_back(tag_map_.at(i));
        }

        return tags;
    }

    return std::vector<Tag>();
}


std::vector<Tag> TagBase::get_tags() const { return map_value_to_vec(tag_map_); }

JsonStore TagBase::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["tags"]      = nlohmann::json::array();

    for (const auto &i : tag_map_) {
        if (i.second.persistent())
            jsn["tags"].push_back(i.second.serialise());
    }

    return jsn;
}
