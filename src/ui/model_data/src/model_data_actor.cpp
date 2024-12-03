// SPDX-License-Identifier: Apache-2.0
#include <fmt/format.h>
#include <caf/policy/select_all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/module/attribute.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/ui/model_data/model_data_actor.hpp"
#include "xstudio/ui/keyboard.hpp"

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
    std::cerr << fmt::format("{:>{}} {}", " ", depth * 4, node.data().dump()) << "\n";
    for (const auto &i : node.base())
        dump_tree2(i, depth + 1);
}

utility::JsonTree *find_node_matching_string_field(
    utility::JsonTree *data,
    const std::string &field_name,
    const std::string &field_value,
    int depth      = -1,
    int real_depth = 0) {
    if (data->data().value(field_name, std::string()) == field_value) {
        return data;
    }
    if (depth) {
        for (auto c = data->begin(); c != data->end(); c++) {
            try {
                utility::JsonTree *r = find_node_matching_string_field(
                    &(*c), field_name, field_value, depth - 1, real_depth + 1);
                if (r)
                    return r;
            } catch (...) {
            }
        }
    }
    if (!real_depth) {
        std::stringstream ss;
        ss << "Failed to find field \"" << field_name << "\" with value matching \""
           << field_value << "\"";
        throw std::runtime_error(ss.str().c_str());
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

    {
        // here we join the hotkeys manager events group to get update on
        // chnanges to hotkeys. If menu items in the menu models have links
        // to a hotkey then we need to update the info if the hotkey is
        // changed
        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);
        request(
            keyboard_manager,
            infinite,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v)
            .then(
                [=](caf::actor hotkeys_config_events_group) mutable {
                    anon_send(
                        hotkeys_config_events_group,
                        broadcast::join_broadcast_atom_v,
                        caf::actor_cast<caf::actor>(this));
                },
                [=](caf::error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }


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
            std::vector<std::pair<std::string, utility::Uuid>> dead_menu_nodes;
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
                if (w->second.empty()) {
                    dead_menu_nodes.emplace_back(p.first, w->first);
                }
                w++;
            }
            for (const auto &d : dead_menu_nodes) {
                remove_node(d.first, d.second);
            }
        }
    });

    behavior_.assign(
        [=](register_model_data_atom,
            const std::string &model_name,
            const bool deregister,
            caf::actor client) {
            if (deregister)
                stop_watching_model(model_name, client);
        },
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
            const utility::JsonStore &model_data,
            caf::actor client,
            bool force_resend_data_to_client) -> result<utility::JsonStore> {
            try {
                register_model(model_name, model_data, client, force_resend_data_to_client);
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
        [=](deregister_model_data_atom,
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
            const utility::JsonStore &model_data,
            caf::actor client) -> result<utility::JsonStore> {
            try {
                register_model(model_name, model_data, client, false, preferences_path);
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
                register_model(
                    model_name, utility::JsonStore(), client, false, preferences_path);
                return model_data_as_json(model_name);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](reset_model_atom,
            const std::string &model_name,
            const std::string &preferences_path) {
            try {
                reset_model(model_name, preferences_path);
            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        },
        [=](reset_model_atom,
            const std::string &model_name,
            const utility::JsonStore &data,
            const bool broadcast_back_to_sender) {
            reset_model(
                model_name,
                data,
                broadcast_back_to_sender ? caf::actor()
                                         : actor_cast<caf::actor>(current_sender()));
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
            const utility::Uuid &attribute_uuid) {
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

        [=](menu_node_activated_atom,
            const std::string &model_name,
            const std::string &path,
            const std::string &user_data) {
            node_activated(model_name, path, user_data, false);
        },

        [=](insert_or_update_menu_node_atom,
            const std::string &model_name,
            const std::string &menu_path,
            const utility::JsonStore &menu_data,
            caf::actor watcher) {
            insert_into_menu_model(model_name, menu_path, menu_data, watcher);
        },
        [=](insert_or_update_menu_node_atom,
            const std::string &model_name,
            const std::string &menu_path,
            const float position_in_menu) {
            // this message can be used to change the order of some submenu
            // within a menu model to set it's position amongst submenus at
            // the same level.
            set_menu_node_position(model_name, menu_path, position_in_menu);
        },
        [=](set_row_ordering_role_atom,
            const std::string &model_name,
            const std::string &role_name) {
            // not implemented yet .. idea is you can sort the model based off
            // a particular role.
            check_model_is_registered(model_name, true);
            models_[model_name]->sort_key_ = role_name;
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
                    anon_send(
                        client,
                        utility::event_atom_v,
                        model_data_atom_v,
                        model_name,
                        model_data);
                }
            }
            models_to_be_fully_broadcasted_.clear();
        },
        [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> & /*hotkeys*/) {
            // ignored event from hotkey manager
        },
        [=](keypress_monitor::hotkey_event_atom, Hotkey &hotkey) {
            update_hotkeys_model_data(hotkey);
        },
        [=](keypress_monitor::hotkey_event_atom,
            const utility::Uuid kotkey_uuid,
            const bool pressed,
            const std::string &context,
            const std::string &window) {
            if (pressed)
                hotkey_pressed(kotkey_uuid, context, window);
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

        if (changed) { //} && !role.empty()) {

            if (!role.empty()) {
                for (auto &client : models_[model_name]->clients_) {
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
            } else {
                for (auto &client : models_[model_name]->clients_) {
                    send(
                        client,
                        utility::event_atom_v,
                        set_node_data_atom_v,
                        model_name,
                        path,
                        data);
                }
            }

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
                            uuid_role_data.is_null() ? uuid : uuid_role_data);
                    }
                }
            }
        } else if (changed) {

            // this was a bigger JSon push which could go down any number
            // of levels in the tree, so do a full brute force update
            push_to_prefs(model_name);
            broadcast_whole_model_data(model_name);
        }

    } catch ([[maybe_unused]] std::exception &e) {
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
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
    const utility::JsonStore &attr_data,
    const std::string &sort_role,
    caf::actor client) {

    const utility::JsonStore attribute_data = attr_data;
    auto p                                  = models_.find(model_name);
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
        utility::JsonStore blank_model(nlohmann::json::parse(R"({})"));
        models_[model_name] = std::make_shared<ModelData>(model_name, blank_model, client);
        monitor(client);
    }

    utility::JsonTree *parent_node = &(models_[model_name]->data_);
    try {
        parent_node = find_node_matching_string_field(
            parent_node, attr_uuid_role_name, to_string(attribute_uuid));

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

void GlobalUIModelData::stop_watching_model(const std::string &model_name, caf::actor client) {
    auto p = models_.find(model_name);
    if (p != models_.end()) {

        // model with this name already exists. Simply add client and send the
        // full model state to the client
        auto c = p->second->clients_.begin();
        while (c != p->second->clients_.end()) {
            if (*c == client) {
                c = p->second->clients_.erase(c);
            } else {
                c++;
            }
        }
    }
}

void GlobalUIModelData::register_model(
    const std::string &model_name,
    const utility::JsonStore &model_data,
    caf::actor client,
    const bool force_resend,
    const std::string &preference_path) {

    auto p = models_.find(model_name);
    if (p != models_.end()) {

        if (p->second->data_.total_size() == 0 && p->second->data_.data().is_null()) {
            // model is already registered but it is empty. Use incoming model_data
            // to set-up the model data.
            p->second->data_ = utility::json_to_tree(model_data, "children");
        }

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

        if (force_resend) {

            caf::scoped_actor sys(system());
            utility::request_receive<bool>(
                *sys,
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
        anon_send(
            client, utility::event_atom_v, model_data_atom_v, model_name, data_from_prefs);
        monitor(client);

    } else if (!preference_path.empty()) {

        // don't load from preferennces, but make the model data a saved preference
        models_[model_name] =
            std::make_shared<ModelData>(model_name, model_data, client, preference_path);
        monitor(client);

    } else {

        auto new_model_data = std::make_shared<ModelData>(model_name, model_data, client);

        // If we are adding a new model - check if there is a model with a wildcard
        // If there is, we duplicate that wildcard model data into the new
        // model that we're adding as a starting point.
        //
        // This specifically means that if there is a C++ plugin that has created
        // menu items in a model named 'MyModel*' then any models created
        // in the UI layer called 'MyModel<something>' will include the menu
        // items created in the backend.
        for (auto &p : models_) {

            if (p.first.find("*") == p.first.size() - 1) {

                std::string match_name(p.first, 0, (p.first.size() - 1));
                if (model_name.find(match_name) == 0) {
                    new_model_data->data_ = p.second->data_;
                    new_model_data->clients_.insert(
                        new_model_data->clients_.begin(),
                        p.second->clients_.begin(),
                        p.second->clients_.end());
                    new_model_data->menu_watchers_ = p.second->menu_watchers_;
                }
            }
        }

        models_[model_name] = new_model_data;
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

            // is there a model here with a wildcard in its name that matches
            // the model we need to create?

            auto new_model_data =
                std::make_shared<ModelData>(model_name, nlohmann::json::parse("[]"));

            for (auto &p : models_) {
                if (p.first.find("*") == p.first.size() - 1) {
                    std::string match_name(p.first, 0, (p.first.size() - 1));
                    if (model_name.find(match_name) == 0) {
                        new_model_data->data_ = p.second->data_;
                    }
                }
            }

            models_[model_name] = new_model_data;
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

        bool blank = false;
        if (models_.find(model_name) == models_.end()) {
            // it's possible something tries to insert data to a model that
            // hasn't been set-up yet, so do the set-up now.
            utility::JsonStore blank_model(nlohmann::json::parse(R"({})"));
            models_[model_name] = std::make_shared<ModelData>(model_name, blank_model);
            blank               = true;
        }

        auto &model_data = models_[model_name]->data_;

        nlohmann::json::json_pointer ptr(path);
        auto j = pointer_to_tree(model_data, "children", ptr);

        if (j and row >= 0 and row <= static_cast<int>(j->size())) {
            while (count--) {
                j->insert(std::next(j->begin(), row), utility::json_to_tree(data, "children"));
            }

        } else if (j) {
            std::stringstream ss;
            ss << "Trying to insert row @ " << row << " where only " << j->size()
               << " rows exist already";
            throw std::runtime_error(ss.str().c_str());
        } else {
            std::stringstream ss;
            ss << "Trying to insert row @ " << row << " to node with path \"" << path
               << "\" - no node at this path!";
            throw std::runtime_error(ss.str().c_str());
        }

        if (!models_[model_name]->sort_key_.empty()) {
            do_ordering(&model_data, models_[model_name]->sort_key_);
        }

        auto model_data_json = model_data_as_json(model_name);

        if (data.contains("hotkey_uuid")) {

            utility::Uuid hotkey_uuid(data["hotkey_uuid"]);
            if (!hotkey_uuid.is_null()) {

                models_[model_name]->hotkeys_.insert(hotkey_uuid);
            }
        }

        // caf::scoped_actor sys(system());
        for (auto &client : models_[model_name]->clients_) {

            // if we know 'requester', then the requester does not want to
            // get the change event as it has already updated its local model
            if (client && client != requester) {
                anon_send(
                    //*sys,
                    client,
                    utility::event_atom_v,
                    model_data_atom_v,
                    model_name,
                    model_data_json);
            }
        }

        push_to_prefs(model_name);

    } catch (std::exception &e) {
        spdlog::warn(
            "{} {} {} {} {}", __PRETTY_FUNCTION__, e.what(), model_name, path, data.dump());
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

        caf::scoped_actor sys(system());
        for (auto &client : models_[model_name]->clients_) {
            // if we know 'requester', then the requester does not want to
            // get the change event as it has already updated its local model
            if (client != requester) {
                anon_send(
                    client,
                    utility::event_atom_v,
                    model_data_atom_v,
                    model_name,
                    model_data_json);
            }
        }
        push_to_prefs(model_name);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void GlobalUIModelData::remove_attribute_from_model(
    const std::string &model_name, const utility::Uuid &attr_uuid) {
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
                std::chrono::seconds(2),
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

void GlobalUIModelData::node_activated(
    const std::string &model_name,
    const std::string &path,
    const std::string &user_data,
    const bool from_hotkey) {

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
                        send(
                            watcher,
                            utility::event_atom_v,
                            menu_node_activated_atom_v,
                            path,
                            utility::JsonStore(j),
                            user_data,
                            from_hotkey);
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

        // store_wilcarded_menu_item(model_name, menu_path, menu_data, watcher);

        auto menu_models = get_menu_models(model_name);

        for (auto mmn : menu_models) {

            utility::JsonTree *menu_model_data = &(mmn->data_);
            const std::string &model_name      = mmn->name_;

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
                        menu_model_data, "name", parent_menus.front(), 1);

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
                menu_model_data = &(*(menu_model_data->insert(
                    std::next(menu_model_data->begin(), menu_model_data->size()), menu_data)));
            } else {
                // update - copy back in key/values that were there before but
                // not defined in 'menu_data'
                const auto old_data     = menu_model_data->data();
                menu_model_data->data() = menu_data;
                if (old_data.is_object() && menu_data.is_object()) {
                    for (auto it = old_data.begin(); it != old_data.end(); it++) {
                        if (!menu_data.contains(it.key())) {
                            menu_model_data->data()[it.key()] = it.value();
                        }
                    }
                }
            }

            if (menu_model_data->data().contains("hotkey_uuid")) {

                utility::Uuid hotkey_uuid(menu_model_data->data()["hotkey_uuid"]);
                if (!hotkey_uuid.is_null()) {

                    mmn->hotkeys_.insert(hotkey_uuid);
                    auto keyboard_manager =
                        system().registry().template get<caf::actor>(keyboard_events);
                    caf::scoped_actor sys(system());
                    try {
                        auto hotkey = utility::request_receive<Hotkey>(
                            *sys,
                            keyboard_manager,
                            ui::keypress_monitor::hotkey_atom_v,
                            hotkey_uuid);
                        menu_model_data->data()["hotkey_sequence"] = hotkey.hotkey_sequence();

                        if (!menu_model_data->data().contains("name")) {
                            // we've been asked to add a menu item based off a hotkey
                            // that's alread been defined
                            menu_model_data->data()["name"] = hotkey.hotkey_name();
                        }

                    } catch (std::exception &e) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                    }
                }
            }

            if (menu_model_data->parent())
                do_ordering(menu_model_data->parent());

            utility::Uuid uuid(menu_data["uuid"].get<std::string>());
            auto &watchers = models_[model_name]->menu_watchers_[uuid];
            if (std::find(watchers.begin(), watchers.end(), watcher) == watchers.end()) {
                watchers.push_back(watcher);
            }
            monitor(watcher);
            broadcast_whole_model_data(model_name);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

/*void GlobalUIModelData::store_wilcarded_menu_item(
    const std::string &model_name,
    const std::string &menu_path,
    const utility::JsonStore &menu_data,
    caf::actor watcher)
{
    if (model_name.find("*") != std::string::npos) {

        // someone wants to create a menu item that is added to all instances
        // of a menu model matching the wildcard. We need to keep a record
        // of these in case a matching menu model is created *after* the
        // wildcarded menu model item was instances


    }
}*/

void GlobalUIModelData::set_menu_node_position(
    const std::string &model_name, const std::string &menu_path, const float position_in_menu) {
    utility::JsonTree *menu_model_data = nullptr;
    try {

        check_model_is_registered(model_name, true);

        auto menu_models = get_menu_models(model_name);
        for (auto mmn : menu_models) {

            menu_model_data = &(mmn->data_);

            // see note in insert_into_menu_model for an explanation of this
            std::vector<std::string> parent_menus = utility::split(menu_path, '|');
            while (parent_menus.size()) {
                // this call will throw an exception if there isn't a menu
                // with a name matching the first element in parent_menus
                // which would mean that 'menu_path' does not match an existing
                // menu item in the menu model
                menu_model_data = find_node_matching_string_field(
                    menu_model_data, "name", parent_menus.front());
                parent_menus.erase(parent_menus.begin());
            }

            menu_model_data->data()["menu_item_position"] = position_in_menu;

            if (menu_model_data->parent()) {
                do_ordering(menu_model_data->parent());
            }

            broadcast_whole_model_data(model_name);
        }

    } catch (std::exception &e) {

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

GlobalUIModelData::ModelDataVec
GlobalUIModelData::get_menu_models(const std::string model_name) {

    // One awkward problem with how we create menus items in QML by declaring
    // XsMenuItem{} is that if the parent object is instanced multiple times
    // we have multiple declarations of the menu item trying to insert themselves
    // into the same backend menu model data. To get around this, we can
    // create a unique menu model for each instance of that parent object by
    // making a unique menu model name - so in other words the menu model data
    // itself has multiple instances managed by this class (GlobalUIModelData).
    //
    // This way given instances of an actual menu in the UI will be connected
    // to unique instances of the associated XsMenuItem. However, this presents
    // another problem: if we have a singleton backend C++ plugin that wants to
    // expose menu items in the UI, how does it connect to these multiple menu
    // models?
    // The solution is that the backend object uses a wildcard '*' at the end
    // of the menu model name so that the menu item it is creating gets inserted
    // to all matching menu models.
    //

    // So in the frontend we might have this:

    /*

    Item {
        id: the_parent

        // make a unique menu model name for every instance of this Item
        property string menuModelName: "MyPluginMenu" + the_parent

        // declare menu items:
        XsMenuItem {
            menu_model_name: menuModelName
        }
        ...
        XsMenuModel {
            id: the_model
            menu_model_name: menuModelName
        }
        ...
        XsPopupMenu {
            menu_mode: the_model
        }
    }

    // and it the backend plugin, to insert menu items into all instances of
    // this menu:

    void MyPlugin::setup_menus() {

        // here we can add a simple menu item - we get a callback with the uuid
        // in menu_item_activated when the user clicks on it
        my_menu_item_ = insert_menu_item(
                "MyPluginMenu*", // N.B. note the wildcard!
                "Backend Multichoice",
                "Backend Example|Submenu 1|Submenu 2",
                0.0f);
    }

    */


    ModelDataVec result;
    if (model_name.find("*") == (model_name.size() - 1)) {

        // we have a model with a wildcard in the name, so find all matching models
        std::string match_name(model_name, 0, (model_name.size() - 1));
        for (auto p : models_) {
            if (p.first.find(match_name) == 0 || p.first == model_name) {
                result.push_back(p.second);
            }
        }

    } else {
        auto p = models_.find(model_name);
        if (p != models_.end()) {
            result.push_back(p->second);
        } else {
            std::stringstream ss;
            ss << "No such model \"" << model_name << "\" registered with GlobalUIModelData";
            throw std::runtime_error(ss.str().c_str());
        }
    }

    return result;
}

void GlobalUIModelData::remove_node(
    const std::string &model_name, const utility::Uuid &model_item_id) {
    try {

        check_model_is_registered(model_name, true);
        auto menu_models = get_menu_models(model_name);
        for (auto mmn : menu_models) {

            utility::JsonTree *menu_model_data = &(mmn->data_);

            std::string uuid_string = to_string(model_item_id);
            std::string field("uuid");
            if (find_and_delete(menu_model_data, field, uuid_string)) {
                broadcast_whole_model_data(mmn->name_);
            }
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

void GlobalUIModelData::do_ordering(utility::JsonTree *node, const std::string &ordering_key) {

    auto comp = [&ordering_key](
                    const utility::JsonTree &branchA,
                    const utility::JsonTree &branchB) -> bool {
        return branchA.data().value(ordering_key, 0.0f) <
               branchB.data().value(ordering_key, 0.0f);
    };

    // order the children of the node by their "menu_item_position" (if there
    // is such data)
    node->do_sort(comp);
}

void GlobalUIModelData::update_hotkeys_model_data(const Hotkey &hotkey) {
    for (auto &p : models_) {
        if (p.second->hotkeys_.find(hotkey.uuid()) != p.second->hotkeys_.end()) {

            try {

                auto menu_model_data = find_node_matching_string_field(
                    &(p.second->data_), "hotkey_uuid", to_string(hotkey.uuid()));
                set_data(
                    p.second->name_,
                    path_from_node(menu_model_data),
                    utility::JsonStore(hotkey.hotkey_sequence()),
                    "hotkey_sequence");

            } catch (std::exception &e) {
                spdlog::debug("{} {} {}", __PRETTY_FUNCTION__, e.what(), hotkey.hotkey_name());
            }
        }
    }
}

void GlobalUIModelData::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {
    // a hotkey has been pressed and we've beein informed by the keypress_monitor
    // class

    // Are any of our models linked to this hotkey?
    for (auto &p : models_) {

        if (p.second->hotkeys_.find(hotkey_uuid) != p.second->hotkeys_.end()) {

            try {

                // find the menu item that is linked to the hotkey
                auto menu_model_data = find_node_matching_string_field(
                    &(p.second->data_), "hotkey_uuid", to_string(hotkey_uuid));

                if (p.second->name_ == "popout windows" && menu_model_data) {
                    // special case .. the "popout windows" model is the button tray
                    // in the top left of the viewport panels that shows/hides some
                    // registered ui elements in pop-out windows... we can use
                    // a hotkey to toggle them. That's what's happening now.
                    // We will toggle the "window_is_visible" role data
                    if (menu_model_data->data().contains("window_is_visible") &&
                        menu_model_data->data()["window_is_visible"].is_boolean()) {
                        auto v = nlohmann::json(
                            !menu_model_data->data()["window_is_visible"].get<bool>());
                        set_data(
                            p.second->name_,
                            path_from_node(menu_model_data),
                            v,
                            "window_is_visible");
                        return;
                    }
                }

                // run our 'activated' method which will trigger an 'activated'
                // signal on any XsMenuItem that are hooked to this node
                node_activated(p.second->name_, path_from_node(menu_model_data), context, true);

            } catch (std::exception &e) {
                spdlog::warn(
                    "{} {} for model {}", __PRETTY_FUNCTION__, e.what(), p.second->name_);
            }
        }
    }
}

void GlobalUIModelData::reset_model(
    const std::string &model_name, const std::string &preferences_path) {

    check_model_is_registered(model_name, true);

    // load data from preferennces
    auto prefs_helper          = global_store::GlobalStoreHelper(system());
    auto data_from_prefs       = prefs_helper.default_value<JsonStore>(preferences_path);
    models_[model_name]->data_ = utility::json_to_tree(data_from_prefs, "children");
    broadcast_whole_model_data(model_name);
}

void GlobalUIModelData::reset_model(
    const std::string &model_name, const utility::JsonStore &data, caf::actor excluded_client) {

    try {
        check_model_is_registered(model_name, true);
        models_[model_name]->data_ = utility::json_to_tree(data, "children");
        const auto model_data      = model_data_as_json(model_name);
        for (auto &client : models_[model_name]->clients_) {
            if (client == excluded_client)
                continue;
            anon_send(client, utility::event_atom_v, model_data_atom_v, model_name, model_data);
        }
        push_to_prefs(model_name, false);

    } catch (std::exception &e) {
        spdlog::warn("{} {} for model {}", __PRETTY_FUNCTION__, e.what(), model_name);
    }
}