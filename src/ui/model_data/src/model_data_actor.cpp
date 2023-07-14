// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/module/global_module_events_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/ui/model_data/model_data_actor.hpp"

#include <iostream>

using namespace xstudio;
using namespace xstudio::ui::model_data;
using namespace xstudio::utility;

GlobalUIModelData::GlobalUIModelData(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    print_on_create(this, "GlobalUIModelData");
    print_on_exit(this, "GlobalUIModelData");

    system().registry().put(global_ui_model_data_registry, this);

    set_down_handler([=](down_msg &msg) {
        // has a client exited?
        for (auto p : models_) {
            auto c = p.second->clients_.begin();
            while (c != p.second->clients_.end()) {
                if (caf::actor_cast<caf::actor_addr>(*c) == msg.source) {
                    c = p.second->clients_.erase(c);
                } else {
                    c++;
                }
            }
            auto w = p.second->menu_watchers_.begin();
            while (w != p.second->menu_watchers_.end()) {
                auto watcher = w->second.begin();
                while (watcher != w->second.end()) {
                    if (caf::actor_cast<caf::actor_addr>(*watcher) == msg.source) {
                        watcher = w->second.erase(watcher);
                    } else {
                        watcher++;
                    }
                }
                w++;
            }
        }
    });

    behavior_.assign(
        [=](register_model_data_atom,
            const std::string &model_name,
            const utility::JsonStore &model_data,
            caf::actor client) -> result<utility::JsonStore> {
            try {
                register_model(model_name, model_data, client);
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](register_model_data_atom,
            const std::string &model_name,
            const std::string &preferences_path,
            caf::actor client) -> result<utility::JsonStore> {
            try {
                register_model(model_name, utility::JsonStore(), client, preferences_path);
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](model_data_atom, const std::string &model_name) -> result<utility::JsonStore> {
            try {
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](set_node_data_atom,
            const std::string &model_name,
            const std::string path,
            const utility::JsonStore &data) { set_data(model_name, path, data); },
        [=](set_node_data_atom,
            const std::string &model_name,
            const std::string path,
            const utility::JsonStore &data,
            const std::string &role) { set_data(model_name, path, data, role); },
        [=](insert_rows_atom,
            const std::string &model_name,
            const std::string &path,
            const int row,
            const int count,
            const utility::JsonStore &data) {
            insert_rows(model_name, path, row, count, data);
        },
        [=](insert_rows_atom,
            const std::string &model_name,
            const std::string &path,
            const int row,
            const int count,
            const utility::JsonStore &data,
            const bool broadcast_back_to_sender) {
            if (broadcast_back_to_sender) {
                insert_rows(model_name, path, row, count, data);
            } else {
                auto sender = actor_cast<caf::actor>(current_sender());
                insert_rows(model_name, path, row, count, data, sender);
            }
        },
        [=](remove_rows_atom,
            const std::string &model_name,
            const std::string &path,
            const int row,
            const int count) { remove_rows(model_name, path, row, count); },
        [=](menu_node_activated_atom, const std::string &model_name, const std::string &path) {
            node_activated(model_name, path);
        },
        [=](insert_or_update_menu_node_atom,
            const std::string &model_name,
            const std::string &menu_path,
            const utility::JsonStore &menu_data,
            caf::actor watcher) {
            insert_into_menu_model(model_name, menu_path, menu_data, watcher);
        },
        [=](remove_node_atom,
            const std::string model_name,
            const utility::Uuid &model_item_id) { remove_node(model_name, model_item_id); },
        [=](json_store::update_atom, const std::string model_name) {
            push_to_prefs(model_name);
        });
}

void GlobalUIModelData::set_data(
    const std::string &model_name,
    const std::string path,
    const utility::JsonStore &data,
    const std::string &role) {

    try {

        check_model_is_registered(model_name);

        auto &model_data = models_[model_name]->data_;

        nlohmann::json::json_pointer ptr(path);

        auto *node = pointer_to_tree(model_data, "children", ptr);
        if (!node) {
            std::stringstream ss;
            ss << "failed to set data for path " << path << " in "
               << model_data_as_json(model_name).dump() << "\n";
            throw std::runtime_error(ss.str().c_str());
        }
        auto &j = node->data();

        bool changed = false;
        if (j.is_object() && !role.empty()) {
            if (!j.contains(role) || j[role] != data) {
                changed = true;
                j[role] = data;
            }
        } else if (role.empty()) {
            j = data;
        }

        if (changed) {
            for (auto &client : models_[model_name]->clients_)
                send(client, utility::event_atom_v, set_node_data_atom_v, path, data, role);
            push_to_prefs(model_name);

            if (j.contains("uuid")) {
                utility::Uuid uuid(j["uuid"].get<std::string>());
                auto p = models_[model_name]->menu_watchers_.find(uuid);
                if (p != models_[model_name]->menu_watchers_.end()) {
                    auto &watchers = p->second;
                    for (auto watcher : watchers) {
                        send(
                            watcher,
                            utility::event_atom_v,
                            set_node_data_atom_v,
                            path,
                            role,
                            data);
                    }
                }
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}


void GlobalUIModelData::register_model(
    const std::string &model_name,
    const utility::JsonStore &model_data,
    caf::actor client,
    const std::string &preference_path) {

    auto p = models_.find(model_name);
    if (p != models_.end()) {

        // model with this name already exists. Simply add client and send the
        // full model state to the client
        bool already_a_client = false;
        for (auto &c : p->second->clients_) {
            if (c == client) {
                already_a_client = true;
            }
        }
        if (!already_a_client) {
            p->second->clients_.push_back(client);
            monitor(client);
            send(
                client,
                utility::event_atom_v,
                model_data_atom_v,
                model_data_as_json(model_name));
        }

    } else if (!preference_path.empty() && model_data.is_null()) {

        // load data from preferennces
        auto prefs = global_store::GlobalStoreHelper(system());
        JsonStore j;
        prefs.get_group(j);
        auto data_from_prefs = global_store::preference_value<JsonStore>(j, preference_path);
        models_[model_name] =
            std::make_shared<ModelData>(model_name, data_from_prefs, client, preference_path);
        send(client, utility::event_atom_v, model_data_atom_v, data_from_prefs);
        monitor(client);

    } else {
        models_[model_name] = std::make_shared<ModelData>(model_name, model_data, client);
        monitor(client);
    }
}

utility::JsonStore GlobalUIModelData::model_data_as_json(const std::string &model_name) const {
    auto p = models_.find(model_name);
    if (models_.find(model_name) == models_.end()) {
        std::stringstream ss;
        ss << "No such model \"" << model_name << "\" registered with GlobalUIModelData";
        throw std::runtime_error(ss.str().c_str());
    }
    return utility::JsonStore(utility::tree_to_json(p->second->data_, "children"));
}

void GlobalUIModelData::check_model_is_registered(
    const std::string &model_name, bool auto_register) {
    if (models_.find(model_name) == models_.end()) {
        if (!auto_register) {
            std::stringstream ss;
            ss << "No such model \"" << model_name << "\" registered with GlobalUIModelData";
            throw std::runtime_error(ss.str().c_str());
        } else {
            models_[model_name] =
                std::make_shared<ModelData>(model_name, nlohmann::json::parse("[]"));
        }
    }
}

void GlobalUIModelData::insert_rows(
    const std::string &model_name,
    const std::string &path,
    const int row,
    int count,
    const utility::JsonStore &data,
    caf::actor requester) {

    try {

        check_model_is_registered(model_name);

        auto &model_data = models_[model_name]->data_;

        nlohmann::json::json_pointer ptr(path);
        auto j = pointer_to_tree(model_data, "children", ptr);

        if (j and row >= 0 and row <= static_cast<int>(j->size())) {
            while (count--) {
                j->insert(std::next(j->begin(), row), data);
            }
        } else {
            std::stringstream ss;
            ss << "Trying to insert row @ " << row << " where only " << j->size()
               << " rows exist already";
            throw std::runtime_error(ss.str().c_str());
        }

        for (auto &client : models_[model_name]->clients_) {
            // if we know 'requester', then the requester does not want to
            // get the change event as it has already updated its local model
            if (client != requester) {
                send(
                    client,
                    utility::event_atom_v,
                    model_data_atom_v,
                    model_data_as_json(model_name));
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::remove_rows(
    const std::string &model_name, const std::string &path, const int row, int count) {

    try {

        check_model_is_registered(model_name);

        nlohmann::json::json_pointer ptr(path);
        auto &model_data = models_[model_name]->data_;
        auto j           = pointer_to_tree(model_data, "children", ptr);

        while (row < j->size() && count--) {
            j->erase(std::next(j->begin(), row));
        }

        for (auto &client : models_[model_name]->clients_)
            send(
                client,
                utility::event_atom_v,
                model_data_atom_v,
                model_data_as_json(model_name));
        push_to_prefs(model_name);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::push_to_prefs(const std::string &model_name) {

    try {

        check_model_is_registered(model_name);
        if (models_[model_name]->preference_path_.empty())
            return;

        if (!models_[model_name]->pending_prefs_update_) {
            models_[model_name]->pending_prefs_update_ = true;
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::seconds(10),
                json_store::update_atom_v,
                model_name);
        } else {

            models_[model_name]->pending_prefs_update_ = false;
            auto prefs = global_store::GlobalStoreHelper(home_system());
            prefs.set_value(
                model_data_as_json(model_name), models_[model_name]->preference_path_);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::node_activated(const std::string &model_name, const std::string &path) {

    try {

        check_model_is_registered(model_name);

        auto &model_data = models_[model_name]->data_;

        if (!path.empty()) {
            nlohmann::json::json_pointer ptr(path);
            auto &j = pointer_to_tree(model_data, "children", ptr)->data();
            if (j.contains("uuid")) {
                utility::Uuid uuid(j["uuid"].get<std::string>());
                auto p = models_[model_name]->menu_watchers_.find(uuid);
                if (p != models_[model_name]->menu_watchers_.end()) {
                    auto &watchers = p->second;
                    for (auto watcher : watchers) {
                        send(watcher, utility::event_atom_v, menu_node_activated_atom_v, path);
                    }
                }
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

utility::JsonTree *add_node(utility::JsonTree *node, const nlohmann::json &new_entry_data) {

    auto p = node->insert(std::next(node->begin(), node->size()), new_entry_data);
    return &(*p);
}

utility::JsonTree *find_node_matching_string_field(
    utility::JsonTree *data, const std::string &field_name, const std::string &field_value) {
    if (data->data().value(field_name, std::string()) == field_value) {
        return data;
    }
    for (auto c = data->begin(); c != data->end(); c++) {
        try {
            utility::JsonTree *r =
                find_node_matching_string_field(&(*c), field_name, field_value);
            if (r)
                return r;
        } catch (...) {
        }
    }
    std::stringstream ss;
    ss << "Failed to find field \"" << field_name << "\" with value matching \"" << field_value
       << "\"";
    throw std::runtime_error(ss.str().c_str());
    return nullptr;
}

void GlobalUIModelData::insert_into_menu_model(
    const std::string &model_name,
    const std::string &menu_path,
    const utility::JsonStore &menu_data,
    caf::actor watcher) {
    try {

        check_model_is_registered(model_name, true);

        utility::JsonTree *menu_model_data = &(models_[model_name]->data_);

        // If menu path is, say "Publish|Annotations|Latest", we need to find
        // the top level node where "name"=="Publish". Then we search that node's
        // children for one where "name"=="Annotaitons". Then we search that
        // node's children for one where "name"=="Latest". If we do not find
        // the matching child, we break as this is where we need to start
        // creating children items to build the sub-menu heirachy
        std::vector<std::string> parent_menus = utility::split(menu_path, '|');

        while (parent_menus.size()) {
            try {
                menu_model_data = find_node_matching_string_field(
                    menu_model_data, "name", parent_menus.front());
                parent_menus.erase(parent_menus.begin());
            } catch (std::exception &e) {
                // exception is thrown if we fail to find a match
                break;
            }
        }

        while (parent_menus.size()) {

            nlohmann::json data;
            data["name"]           = parent_menus.front();
            data["menu_item_type"] = "menu";
            menu_model_data        = add_node(menu_model_data, data);
            parent_menus.erase(parent_menus.begin());
        }

        bool already_defined = false;
        for (auto p = menu_model_data->begin(); p != menu_model_data->end(); p++) {
            if ((*p).data().is_object() &&
                (*p).data().value("uuid", std::string()) == menu_data["uuid"]) {
                already_defined = true;
                menu_model_data = &(*p);
                break;
            }
        }

        if (!already_defined) {
            menu_model_data->insert(
                std::next(menu_model_data->begin(), menu_model_data->size()), menu_data);
        } else {
            // update
            menu_model_data->data() = menu_data;
        }

        utility::Uuid uuid(menu_data["uuid"].get<std::string>());
        auto &watchers = models_[model_name]->menu_watchers_[uuid];
        if (std::find(watchers.begin(), watchers.end(), watcher) == watchers.end()) {
            watchers.push_back(watcher);
        }
        monitor(watcher);

        for (auto &client : models_[model_name]->clients_) {
            send(
                client,
                utility::event_atom_v,
                model_data_atom_v,
                model_data_as_json(model_name));
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

bool find_and_delete(
    utility::JsonTree *data, const std::string &field, const std::string &field_value) {
    if (data->data().contains(field) && data->data()[field].get<std::string>() == field_value) {

        data->parent()->erase(std::next(data->parent()->begin(), data->index()));
        return true;

    } else {
        for (auto p = data->begin(); p != data->end(); ++p) {
            if (find_and_delete(&(*p), field, field_value)) {
                return true;
            }
        }
    }
    return false;
}

void GlobalUIModelData::remove_node(
    const std::string &model_name, const utility::Uuid &model_item_id) {
    try {

        check_model_is_registered(model_name, true);

        auto *menu_model_data = &(models_[model_name]->data_);

        std::string uuid_string = to_string(model_item_id);
        std::string field("uuid");
        if (find_and_delete(menu_model_data, field, uuid_string)) {
            for (auto &client : models_[model_name]->clients_) {
                send(
                    client,
                    utility::event_atom_v,
                    model_data_atom_v,
                    model_data_as_json(model_name));
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::on_exit() { system().registry().erase(global_ui_model_data_registry); }
