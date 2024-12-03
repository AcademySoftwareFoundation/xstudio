// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/ui/qml/module_data_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/module/attribute.hpp"

using namespace xstudio;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;

ModulesModelData::ModulesModelData(QObject *parent) : UIModelData(parent) {

    setRoleNames(utility::map_value_to_vec(module::Attribute::role_names));
}

void ModulesModelData::setSingleAttributeName(const QString single_attr_name) {

    if (single_attr_name_ != single_attr_name && !model_name_.empty()) {
        single_attr_name_ = single_attr_name;
        emit singleAttributeNameChanged();

        try {

            caf::scoped_actor sys{self()->home_system()};
            auto data = utility::request_receive<utility::JsonStore>(
                *sys,
                central_models_data_actor_,
                ui::model_data::register_model_data_atom_v,
                model_name_,
                utility::JsonStore(nlohmann::json::parse("{}")),
                as_actor());

            // process app/user..
            setModelData(data);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void ModulesModelData::setModelData(const nlohmann::json &data) {

    if (!single_attr_name_.isEmpty()) {

        // if we have single_attr_name_ set, then we
        nlohmann::json filtered_data       = nlohmann::json::parse(R"({ "children": [] })");
        const std::string filter_attr_name = StdFromQString(single_attr_name_);
        if (data.contains("children")) {
            nlohmann::json children = data["children"];
            if (children.is_array()) {
                int filtered_child_idx = 0;
                filtered_child_idx_    = -1;
                for (auto &child : children) {
                    if (child.contains("title") && child["title"].is_string() &&
                        child["title"].get<std::string>() == filter_attr_name) {
                        filtered_data["children"].push_back(child);
                        filtered_child_idx_ = filtered_child_idx;
                        break;
                    }
                    filtered_child_idx++;
                }
            }
        }
        if (filtered_child_idx_ != -1) {
            UIModelData::setModelData(filtered_data);
        } else {
            spdlog::debug(
                "{} Attribute named \"{}\" not found in model data \"{}\"",
                __PRETTY_FUNCTION__,
                filter_attr_name,
                model_name_);
            UIModelData::setModelData(data);
        }

    } else {
        UIModelData::setModelData(data);
    }
}

QVariant ModulesModelData::attributeRoleData(const QString attr_name, const QString role_name) {
    QModelIndex idx = searchRecursive(attr_name, "title");
    if (idx.isValid()) {
        return get(idx, role_name);
    }
    return QVariant();
}
