// SPDX-License-Identifier: Apache-2.0
#include <fmt/format.h>
#include <caf/policy/select_all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/module/global_module_events_actor.hpp"
#include "xstudio/module/attribute.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/ui/model_data/model_data_actor.hpp"

#include <iostream>

using namespace xstudio;
using namespace xstudio::ui::model_data;
using namespace xstudio::utility;

namespace {

utility::JsonTree *add_node(utility::JsonTree *node, const nlohmann::json &new_entry_data) {

    auto p = node->insert(std::next(node->begin(), node->size()), new_entry_data);
    return &(*p);
}

std::string path_from_node(utility::JsonTree *node) {
    if (!node->parent())
        return std::string();
    return path_from_node(node->parent()) +
           std::string(fmt::format("/children/{}", node->index()));
}

inline void dump_tree2(const JsonTree &node, const int depth = 0) {
    spdlog::warn("{:>{}} {}", " ", depth * 4, node.data().dump(2));
    for (const auto &i : node.base())
        dump_tree2(i, depth + 1);
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
    return nullptr;
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

static const std::string attr_uuid_role_name(
    module::Attribute::role_names.find(module::Attribute::UuidRole)->second);

} // namespace

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
            const utility::JsonStore &attribute_data,
            const utility::Uuid &attribute_uuid,
            const std::string &sort_role,
            caf::actor client) -> result<utility::JsonStore> {
            try {
                insert_attribute_data_into_model(
                    model_name, attribute_uuid, attribute_data, sort_role, client);
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](register_model_data_atom,
            const std::string &model_name,
            const utility::Uuid &attribute_uuid,
            caf::actor client) {
            try {
                remove_attribute_data_from_model(model_name, attribute_uuid, client);
            } catch (std::exception &e) {
                std::cerr << "E " << e.what() << "\n";
                // return caf::make_error(xstudio_error::error, e.what());
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
        [=](set_node_data_atom,
            const std::string &model_name,
            const utility::Uuid &attribute_uuid,
            const std::string &role,
            const utility::JsonStore &data,
            caf::actor setter) { set_data(model_name, attribute_uuid, role, data, setter); },
        [=](insert_rows_atom,
            const std::string &model_name,
            const std::string &path,
            const int row,
            const int count,
            const utility::JsonStore &data) -> result<utility::JsonStore> {
            insert_rows(model_name, path, row, count, data);
            try {
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
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

        [=](remove_rows_atom,
            const std::string &model_name,
            const utility::Uuid &attribute_uuid) 
        { 
            remove_attribute_from_model(model_name, attribute_uuid); 
        },

        [=](remove_rows_atom,
            const std::string &model_name,
            const std::string &path,
            const int row,
            const int count,
            const bool broadcast_back_to_sender) {
            if (broadcast_back_to_sender) {
                remove_rows(model_name, path, row, count);
            } else {
                auto sender = actor_cast<caf::actor>(current_sender());
                remove_rows(model_name, path, row, count, sender);
            }
        },

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
            push_to_prefs(model_name, true);
        },
        [=](model_data_atom) {
            for (const auto &model_name : models_to_be_fully_broadcasted_) {

                const auto model_data = model_data_as_json(model_name);

                for (auto &client : models_[model_name]->clients_) {
                    send(
                        client,
                        utility::event_atom_v,
                        model_data_atom_v,
                        model_name,
                        model_data);
                }
            }
            models_to_be_fully_broadcasted_.clear();
        },
        [=](const caf::error &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        },
        [=](caf::message &msg) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(msg)); });
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
        utility::Uuid uuid_role_data;
        if (j.is_object() && !role.empty()) {
            if (!j.contains(role) || j[role] != data) {
                changed = true;
                j[role] = data;
                if (j.contains(attr_uuid_role_name)) {
                    uuid_role_data = utility::Uuid(j[attr_uuid_role_name].get<std::string>());
                }
            }
        } else if (role.empty()) {

            JsonTree new_node = json_to_tree(data, "children");
            *node             = new_node;
            changed           = true;
        }

        if (changed && !role.empty()) {
            for (auto &client : models_[model_name]->clients_)
                send(
                    client,
                    utility::event_atom_v,
                    set_node_data_atom_v,
                    model_name,
                    path,
                    data,
                    role,
                    uuid_role_data);

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
                            model_name,
                            path,
                            role,
                            data,
                            uuid_role_data);
                    }
                }
            }
        } else if (changed) {

            // this was a bigger JSon push which could go down any number
            // of levels in the tree, so do a full brute force update
            push_to_prefs(model_name);
            broadcast_whole_model_data(model_name);
        }

    } catch ([[maybe_unused]]std::exception &e) {
        // spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::set_data(
    const std::string &model_name,
    const utility::Uuid &attribute_uuid,
    const std::string &role,
    const utility::JsonStore &data,
    caf::actor setter) {

    try {

        check_model_is_registered(model_name);

        utility::JsonTree *model_data = &(models_[model_name]->data_);

        utility::JsonTree *node = find_node_matching_string_field(
            &(models_[model_name]->data_), attr_uuid_role_name, to_string(attribute_uuid));
        if (!node) {
            throw std::runtime_error("Failed to find expected field");
        }
        auto &j = node->data();

        bool changed = false;
        utility::Uuid uuid_role_data;
        if (j.is_object() && !role.empty()) {
            if (!j.contains(role) || j[role] != data) {
                changed = true;
                j[role] = data;
                if (j.contains(attr_uuid_role_name)) {
                    uuid_role_data = utility::Uuid(j[attr_uuid_role_name].get<std::string>());
                }
            }
        } else if (role.empty()) {
            j = data;
        }

        if (changed) {

            std::string path = path_from_node(node);

            for (auto &client : models_[model_name]->clients_) {
                if (client != setter)
                    send(
                        client,
                        utility::event_atom_v,
                        set_node_data_atom_v,
                        model_name,
                        path,
                        data,
                        role,
                        uuid_role_data);
            }

            push_to_prefs(model_name);

            if (j.contains("uuid")) {
                utility::Uuid uuid(j["uuid"].get<std::string>());
                auto p = models_[model_name]->menu_watchers_.find(uuid);
                if (p != models_[model_name]->menu_watchers_.end()) {
                    auto &watchers = p->second;
                    for (auto watcher : watchers) {
                        // we don't notify the thing that is setting this data
                        // as it will update it's local data
                        if (watcher != setter)
                            send(
                                watcher,
                                utility::event_atom_v,
                                set_node_data_atom_v,
                                model_name,
                                path,
                                role,
                                data,
                                uuid_role_data);
                    }
                }
            }
        }

    } catch ([[maybe_unused]] std::exception &e) {
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::insert_attribute_data_into_model(
    const std::string &model_name,
    const utility::Uuid &attribute_uuid,
    const utility::JsonStore &attribute_data,
    const std::string &sort_role,
    caf::actor client) {

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
        }

    } else {
        utility::JsonStore blank_model(nlohmann::json::parse(R"({ "children": [] })"));
        models_[model_name] = std::make_shared<ModelData>(model_name, blank_model, client);
        monitor(client);
    }

    utility::JsonTree *parent_node = &(models_[model_name]->data_);
    try {
        auto found_node = find_node_matching_string_field(
            parent_node, attr_uuid_role_name, to_string(attribute_uuid));
        if (found_node) {
            parent_node = found_node;
        } else {
            throw std::runtime_error("Failed to find expected field");
        }

        const auto &d = parent_node->data();

        bool full_push = false;
        std::vector<std::string> changed;
        for (auto it = attribute_data.begin(); it != attribute_data.end(); it++) {
            if (d.contains(it.key()) && d[it.key()] != it.value()) {
                changed.push_back(it.key());
            } else if (not d.contains(it.key())) {
                full_push = true;
            }
        }

        if (full_push) {
            broadcast_whole_model_data(model_name);
        } else {
            for (const auto &c : changed) {
                set_data(model_name, attribute_uuid, c, attribute_data[c], client);
            }
        }

    } catch ([[maybe_unused]] std::exception &e) {
        // exception is thrown if we fail to find a match
        if (!sort_role.empty() && attribute_data.contains(sort_role)) {
            const auto &sort_v = attribute_data[sort_role];
            auto insert_pt     = parent_node->begin();
            while (insert_pt != parent_node->end()) {
                if (insert_pt->data().contains(sort_role)) {
                    if (insert_pt->data()[sort_role] >= sort_v) {
                        break;
                    }
                }
                insert_pt++;
            }
            parent_node->insert(insert_pt, attribute_data);
        } else {
            parent_node->insert(parent_node->end(), attribute_data);
        }
        broadcast_whole_model_data(model_name);
    }
}

void GlobalUIModelData::remove_attribute_data_from_model(
    const std::string &model_name, const utility::Uuid &attribute_uuid, caf::actor client) {

    try {

        check_model_is_registered(model_name);

        utility::JsonTree *model_data = &(models_[model_name]->data_);
        auto &clients                 = models_[model_name]->clients_;
        for (auto c = clients.begin(); c != clients.end(); ++c) {
            if (*c == client) {
                clients.erase(c);
                break;
            }
        }
        if (find_and_delete(model_data, attr_uuid_role_name, to_string(attribute_uuid))) {
            broadcast_whole_model_data(model_name);
        }

    } catch (...) {
        throw;
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
                model_name,
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
        send(client, utility::event_atom_v, model_data_atom_v, model_name, data_from_prefs);
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

        const auto model_data_json = model_data_as_json(model_name);

        for (auto &client : models_[model_name]->clients_) {
            // if we know 'requester', then the requester does not want to
            // get the change event as it has already updated its local model
            if (client != requester) {
                send(
                    client,
                    utility::event_atom_v,
                    model_data_atom_v,
                    model_name,
                    model_data_json);
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::remove_rows(
    const std::string &model_name,
    const std::string &path,
    const int row,
    int count,
    caf::actor requester) {

    try {

        check_model_is_registered(model_name);

        nlohmann::json::json_pointer ptr(path);
        auto &model_data = models_[model_name]->data_;
        auto j           = pointer_to_tree(model_data, "children", ptr);

        while (row < j->size() && count--) {
            j->erase(std::next(j->begin(), row));
        }

        const auto model_data_json = model_data_as_json(model_name);
        for (auto &client : models_[model_name]->clients_) {
            if (client == requester)
                continue;
            send(client, utility::event_atom_v, model_data_atom_v, model_name, model_data_json);
        }
        push_to_prefs(model_name);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::remove_attribute_from_model(const std::string &model_name, const utility::Uuid &attr_uuid)
{
    try {

        check_model_is_registered(model_name);
        utility::JsonTree *model_root = &(models_[model_name]->data_);

        if (find_and_delete(model_root, attr_uuid_role_name, to_string(attr_uuid))) {
            broadcast_whole_model_data(model_name);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

}

void GlobalUIModelData::push_to_prefs(const std::string &model_name, const bool actually_push) {

    try {

        check_model_is_registered(model_name);
        if (models_[model_name]->preference_path_.empty())
            return;

        // if we haven't sheduled and update, mark as pending and send ourselves
        // a delayed message to actually do the update.
        //
        // The reason is that we don't want to update the prefs store with
        // every single change as if the user is dragging something in the UI
        // that is stored in the prefs (like window/panel sizing) the prefs
        // system will get overloaded with update messages
        if (!models_[model_name]->pending_prefs_update_) {
            models_[model_name]->pending_prefs_update_ = true;
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::seconds(20),
                json_store::update_atom_v,
                model_name);
        } else if (actually_push) {

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
                if (!menu_model_data) {
                    throw std::runtime_error("Failed to find expected field");
                }
                parent_menus.erase(parent_menus.begin());
            } catch ([[maybe_unused]] std::exception &e) {
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
        broadcast_whole_model_data(model_name);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}


void GlobalUIModelData::remove_node(
    const std::string &model_name, const utility::Uuid &model_item_id) {
    try {

        check_model_is_registered(model_name, true);

        auto *menu_model_data = &(models_[model_name]->data_);

        std::string uuid_string = to_string(model_item_id);
        std::string field("uuid");
        if (find_and_delete(menu_model_data, field, uuid_string)) {
            broadcast_whole_model_data(model_name);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::on_exit() { system().registry().erase(global_ui_model_data_registry); }

void GlobalUIModelData::broadcast_whole_model_data(const std::string &model_name) {
    // sometimes when a model is being built by backend components like a Module
    // that is setting up attribute data to be exposed in a model we could get
    // many full broadcasts of the entire data in short succession (thanks to
    // GlobalUIModelData::insert_attribute_data_into_model). Instead we put
    // the model in a list waiting to be fully broadcasted and then do it once
    // in 50ms.
    if (models_to_be_fully_broadcasted_.find(model_name) ==
        models_to_be_fully_broadcasted_.end()) {
        if (models_to_be_fully_broadcasted_.empty())
            delayed_anon_send(this, std::chrono::milliseconds(50), model_data_atom_v);
        models_to_be_fully_broadcasted_.insert(model_name);
    }
}
