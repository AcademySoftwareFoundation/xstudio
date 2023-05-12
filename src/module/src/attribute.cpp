// SPDX-License-Identifier: Apache-2.0
#include "xstudio/atoms.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/module/attribute.hpp"
#include "xstudio/module/module.hpp"

#include <iostream>

using namespace xstudio::module;
using namespace xstudio;

bool AttributeData::set(const nlohmann::json &data) {

    bool rt = false;
    if (data.is_string()) {

        if (typeid(utility::Uuid) == data_.type()) {
            rt = set(utility::Uuid(data.get<std::string>()));
        } else {
            rt = set(data.get<std::string>());
        }

    } else if (
        data.is_array() && data.size() == 5 && data.begin().value().is_string() &&
        data.begin().value().get<std::string>() == "vec3") {

        rt = set(data.get<Imath::V3f>());

    } else if (
        data.is_array() && data.size() == 5 && data.begin().value().is_string() &&
        data.begin().value().get<std::string>() == "colour") {

        rt = set(data.get<utility::ColourTriplet>());

    } else if (data.is_array() && data.size() && data.begin().value().is_string()) {

        std::vector<std::string> v;
        for (auto p = data.begin(); p != data.end(); p++) {
            if (p.value().is_string()) {
                v.push_back(p.value().get<std::string>());
            }
        }
        rt = set(v);

    } else if (data.is_array() && data.size() && data.begin().value().is_boolean()) {

        std::vector<bool> v;
        for (auto p = data.begin(); p != data.end(); p++) {
            if (p.value().is_boolean()) {
                v.push_back(p.value().get<bool>());
            }
        }
        rt = set(v);

    } else if (data.is_array() && data.size() && data.begin().value().is_number_float()) {

        std::vector<float> v;
        for (auto p = data.begin(); p != data.end(); p++) {
            if (p.value().is_number_float()) {
                v.push_back(p.value().get<float>());
            }
        }
        rt = set(v);

    } else if (data.is_boolean()) {
        rt = set(data.get<bool>());
    } else if (data.is_number_integer()) {
        rt = set(data.get<int>());
    } else if (data.is_number_float()) {
        rt = set(data.get<float>());
    } else if (data_.type() == typeid(nlohmann::json) && get<nlohmann::json>() != data) {
        data_ = data;
        rt    = true;
    } else if (!data_.has_value()) {
        data_ = data;
        rt    = true;
    } else if (data.is_null()) {
        rt = true;
    }
    return rt;
}


Attribute::Attribute(
    const std::string &title, const std::string &abbr_title, const std::string &type_name)
    : owner_(nullptr), redraw_viewport_on_change_(false) {
    role_data_[Enabled].set(true);
    role_data_[Type].set(type_name);
    role_data_[Title].set(title);
    role_data_[AbbrTitle].set(abbr_title);
    role_data_[UuidRole].set(utility::Uuid::generate());
    role_data_[Groups].set(std::vector<std::string>());
}

void Attribute::set_owner(Module *owner) { owner_ = owner; }

void Attribute::set_redraw_viewport_on_change(const bool redraw_viewport_on_change) {
    redraw_viewport_on_change_ = redraw_viewport_on_change;
}

nlohmann::json Attribute::role_data_as_json(const int role) const {

    const auto p = role_data_.find(role);
    if (p == role_data_.end()) {
        std::stringstream msg;
        msg << "Attribute::role_data_as_json " << get_role_data<std::string>(Title)
            << " of type " << get_role_data<std::string>(Type)
            << " does not have role data for " << role_name(role) << "\n";
        throw std::runtime_error(msg.str().c_str());
    }
    return p->second.to_json();
}

void Attribute::notify_change(
    const int role, const nlohmann::json &data, const bool self_notify) {
    if (owner_) {
        owner_->notify_change(
            uuid(), role, role_data_as_json(role), redraw_viewport_on_change_, self_notify);
    }
}

nlohmann::json Attribute::as_json() const {

    nlohmann::json result;
    for (auto p : role_data_) {
        result[role_name(p.first)] = p.second.to_json();
    }

    return result;
}

bool Attribute::belongs_to_group(const std::string group_name) const {

    bool rt          = false;
    auto group_names = get_role_data<std::vector<std::string>>(Groups);
    for (const auto &p : group_names) {
        if (p == group_name) {
            rt = true;
            break;
        }
    }
    return rt;
}

std::string Attribute::role_name(const int role) {
    const auto &p = role_names.find(role);
    if (p != role_names.end()) {
        return p->second;
    }
    return std::string("BadRoleID");
}

int Attribute::role_index(const std::string &role_name) {
    int rt = -1;
    for (auto p : role_names) {
        if (p.second == role_name) {
            rt = p.first;
            break;
        }
    }
    return rt;
}

std::string Attribute::type_name(const int tp) {
    const auto &p = type_names.find(tp);
    if (p != type_names.end()) {
        return p->second;
    }
    return std::string("BadRoleID");
}

void Attribute::set_preference_path(const std::string &preference_path) {
    set_role_data(PreferencePath, preference_path);
}

void Attribute::expose_in_ui_attrs_group(const std::string &group_name) {
    auto n = role_data_[Groups].get<std::vector<std::string>>();
    n.push_back(group_name);
    set_role_data(Groups, n);
}

void Attribute::set_tool_tip(const std::string &tool_tip) { set_role_data(ToolTip, tool_tip); }
