// SPDX-License-Identifier: Apache-2.0
#include <fmt/format.h>
#include <caf/policy/select_all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media_metadata_manager_actor.hpp"
#include "xstudio/utility/helpers.hpp"

#include <iostream>

using namespace xstudio;
using namespace xstudio::media;

namespace {
inline void dump_tree2(const utility::JsonTree &node, const int depth = 0) {
    for (const auto &i : node.base())
        dump_tree2(i, depth + 1);
}

static const std::vector<std::string> relevant_keys = {
    "metadata_path", "data_type", "object", "info_key", "regex_match", "regex_format"};

utility::JsonStore make_metadata_extraction_info_dict(const utility::JsonTree &node) {

    utility::JsonStore result;
    const auto data = node.data();
    for (const auto &k : relevant_keys) {
        if (data.contains(k))
            result[k] = data[k];
    }
    result["children"] = R"([])"_json;

    for (const auto &i : node.base()) {
        result["children"].push_back(make_metadata_extraction_info_dict(i));
    }
    return result;
}
} // namespace

GlobalMetadataManager::GlobalMetadataManager(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    utility::print_on_create(this, "GlobalMetadataManager");
    utility::print_on_exit(this, "GlobalMetadataManager");

    system().registry().put(global_media_metadata_manager_registry, this);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    // We have a 'model' managed by the  GlobalUIModelData called
    // "media metata exposure model". This model gives us info about the media
    // metadata values that we are interested in showing in the UI and also
    // data about HOW we want to display that info. For example, the columns of
    // information in the MediaList panels in the xSTUDIO UI are driven by this
    // model.
    // On the backend, we need all MediaActor instances to be kept up-to-date
    // about what metadata we want to display in the UI. That's what this here
    // class is here to do. It inspects the items in the
    // "media metata exposure model" and boils this down into a list of metadata
    // that MediaActors should broadcast.
    // This class broadcasts this list of interesting metadata to its event
    // group (to which MediaActors will subscribe) - the MediaActors will use
    // the list to build their dictionary of metadata to broadcast to the UI.

    auto central_models_data_actor =
        home_system().registry().template get<caf::actor>(global_ui_model_data_registry);

    request(
        central_models_data_actor,
        infinite,
        ui::model_data::register_model_data_atom_v,
        "media metata exposure model",
        "/ui/qml/media_list_columns_config",
        caf::actor_cast<caf::actor>(this))
        .then(
            [=](const utility::JsonStore &data) {
                try {
                    metadata_config_ = utility::json_to_tree(data, "children");
                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            },
            [=](caf::error &e) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e)); });

    behavior_.assign(

        [=](utility::get_event_group_atom atom) -> caf::actor { return event_group_; },

        [=](utility::event_atom,
            xstudio::ui::model_data::set_node_data_atom,
            const std::string &model_name,
            const std::string &path,
            const utility::JsonStore &data) {
            try {

                nlohmann::json::json_pointer ptr(path);
                auto *node = pointer_to_tree(metadata_config_, "children", ptr);
                if (!node) {
                    std::stringstream ss;
                    ss << "failed to set data for path " << path << " in " << model_name
                       << "\n";
                    throw std::runtime_error(ss.str().c_str());
                }
                auto &j                    = node->data();
                utility::JsonTree new_node = json_to_tree(data, "children");
                *node                      = new_node;
                config_updated();

            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        },
        [=](utility::event_atom,
            xstudio::ui::model_data::set_node_data_atom,
            const std::string &model_name,
            const std::string path,
            const utility::JsonStore &data,
            const std::string role,
            const utility::Uuid &uuid_role_data) {
            nlohmann::json::json_pointer ptr(path);

            try {
                nlohmann::json::json_pointer ptr(path);
                auto *node = pointer_to_tree(metadata_config_, "children", ptr);
                if (!node) {
                    std::stringstream ss;
                    ss << "failed to set data for path " << path << " in " << model_name
                       << "\n";
                    throw std::runtime_error(ss.str().c_str());
                }
                auto &j = node->data();

                bool changed = false;
                utility::Uuid uuid_role_data;
                if (j.is_object() && !role.empty()) {
                    if (!j.contains(role) || j[role] != data) {
                        changed = true;
                        j[role] = data;
                    }
                } else if (role.empty()) {
                    *node   = utility::json_to_tree(data, "children");
                    changed = true;
                }

                if (changed) {
                    config_updated();
                }

            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        },
        [=](utility::event_atom,
            xstudio::ui::model_data::model_data_atom,
            const std::string &model_name,
            const utility::JsonStore &data) -> bool {
            try {

                metadata_config_ = utility::json_to_tree(data, "children");
                config_updated();

            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
            return true;
        },
        [=](utility::event_atom,
            ui::model_data::menu_node_activated_atom,
            const std::string path,
            const utility::JsonStore &menu_item_data,
            const std::string &user_data,
            const bool from_hotkey) {
            // ignore, menu data events not relevant
        },
        [=](utility::event_atom,
            xstudio::ui::model_data::set_node_data_atom,
            const std::string model_name,
            const std::string path,
            const std::string role,
            const utility::JsonStore &data,
            const utility::Uuid &menu_item_uuid) {
            // ignore, menu data events not relevant
        },
        [=](xstudio::ui::model_data::set_node_data_atom,
            const std::string &menu_model_name,
            const std::string &submenu,
            const float submenu_position) {
            // ignore, menu data events not relevant
        },
        [=](media::media_display_info_atom) -> utility::JsonStore {
            return metadata_extraction_config_;
        },
        [=](media::media_display_info_atom, bool es_event) {
            send(
                caf::actor_cast<caf::actor>(current_sender()),
                utility::event_atom_v,
                media::media_display_info_atom_v,
                metadata_extraction_config_);
        },

        [=](const caf::error &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        },
        [=](caf::message &msg) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(msg)); });
}

void GlobalMetadataManager::config_updated() {
    utility::JsonStore extraction_dict = make_metadata_extraction_info_dict(metadata_config_);

    if (extraction_dict != metadata_extraction_config_) {
        metadata_extraction_config_ = extraction_dict;
        send(
            event_group_,
            utility::event_atom_v,
            media::media_display_info_atom_v,
            metadata_extraction_config_);
    }
}