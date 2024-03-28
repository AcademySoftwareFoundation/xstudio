// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/tree.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playhead/playhead_global_events_actor.hpp"

#include <iostream>

using namespace xstudio::utility;
using namespace xstudio::module;
using namespace xstudio;

namespace {
caf::behavior delayed_resend(caf::event_based_actor *) {
    return {[](update_attribute_in_preferences_atom, caf::actor_addr module) {
        auto mod = caf::actor_cast<caf::actor>(module);
        if (mod) {
            anon_send(mod, update_attribute_in_preferences_atom_v);
        }
    }};
}
} // namespace

Module::Module(const std::string name, const utility::Uuid &uuid)
    : name_(std::move(name)), module_uuid_(uuid) {}

Module::~Module() {
    disconnect_from_ui();
    global_module_events_actor_ = caf::actor();
    keypress_monitor_actor_     = caf::actor();
}

void Module::set_parent_actor_addr(caf::actor_addr addr) {

    parent_actor_addr_ = addr;
    if (!module_events_group_) {
        module_events_group_ = self()->home_system().spawn<broadcast::BroadcastActor>(self());
        attribute_events_group_ =
            self()->home_system().spawn<broadcast::BroadcastActor>(self());
        try {
            auto prefs = global_store::GlobalStoreHelper(self()->home_system());
            // only on init..
            utility::JsonStore j;
            auto a = caf::actor_cast<caf::event_based_actor *>(self());
            if (a) {
                join_broadcast(a, prefs.get_group(j));
                a->link_to(module_events_group_);
            } else {
                auto b = caf::actor_cast<caf::blocking_actor *>(self());
                if (b) {
                    join_broadcast(b, prefs.get_group(j));
                    b->link_to(module_events_group_);
                }
            }
            update_attrs_from_preferences(j);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    // we can't add hotkeys until the parent actor has been set. Subclasses of
    // Module should define hotkeys in the virtual register_hotkeys() function


    // Some 'Modules' might try and register their hotkeys before they have been
    // hooked in to an actor - here we are able to register tham
    if (unregistered_hotkeys_.size()) {
        if (!keypress_monitor_actor_) {
            keypress_monitor_actor_ =
                self()->home_system().registry().template get<caf::actor>(keyboard_events);
        }

        for (auto &hk : unregistered_hotkeys_) {
            hk.add_watcher(addr);
            anon_send(
                keypress_monitor_actor_, ui::keypress_monitor::register_hotkey_atom_v, hk);
        }
        unregistered_hotkeys_.clear();
    }

    /*if (self()) {
        self()->attach_functor([=](const caf::error &reason) {
            spdlog::debug(
                "STANKSTONK {} exited: {}",
                name(),
                to_string(reason));
            cleanup();
            spdlog::debug(
                "STINKDONK {} exited: {}",
                name(),
                to_string(reason));
        });
    }*/
}

void Module::delete_attribute(const utility::Uuid &attribute_uuid) {
    for (auto p = attributes_.begin(); p != attributes_.end(); ++p) {
        if ((*p)->uuid() == attribute_uuid) {
            attributes_.erase(p);
            break;
        }
    }
}

void Module::link_to_module(
    caf::actor other_module,
    const bool link_all_attrs,
    const bool both_ways,
    const bool intial_push_sync) {

    auto addr = caf::actor_cast<caf::actor_addr>(other_module);
    if (addr) {

        if (link_all_attrs)
            fully_linked_modules_.insert(addr);
        else
            partially_linked_modules_.insert(addr);

        if (intial_push_sync) {

            // send state of all attrs to 'other_module' so it can update its copies as required
            for (auto &attribute : attributes_) {
                if (link_all_attrs ||
                    linked_attrs_.find(attribute->uuid()) != linked_attrs_.end()) {
                    anon_send(
                        other_module,
                        change_attribute_value_atom_v,
                        attribute->get_role_data<std::string>(Attribute::Title),
                        utility::JsonStore(attribute->role_data_as_json(Attribute::Value)),
                        true);
                }
            }
        }

        if (both_ways)
            anon_send(
                other_module, module::link_module_atom_v, self(), link_all_attrs, false, false);
    }
}

void Module::unlink_module(caf::actor other_module)
{
    auto addr = caf::actor_cast<caf::actor_addr>(other_module);
    auto p = std::find(fully_linked_modules_.begin(), fully_linked_modules_.end(), addr);
    bool found_link = false;
    if (p != fully_linked_modules_.end()) {
        fully_linked_modules_.erase(p);
        found_link = true;
    }
    p = std::find(partially_linked_modules_.begin(), partially_linked_modules_.end(), addr);
    if (p != partially_linked_modules_.end()) {
        partially_linked_modules_.erase(p);
        found_link = true;
    }

    if (found_link) {
        anon_send(
                other_module, module::link_module_atom_v, self(), false);
    }
}

FloatAttribute *Module::add_float_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const float value,
    const float float_min,
    const float float_max,
    const float float_step,
    const int float_display_decimals,
    const float fscrub_sensitivity) {

    auto *rt(new FloatAttribute(
        title,
        abbr_title,
        value,
        float_min,
        float_max,
        float_step,
        float_display_decimals,
        fscrub_sensitivity));
    rt->set_owner(this);

    add_attribute(static_cast<Attribute *>(rt));

    return rt;
}

StringChoiceAttribute *Module::add_string_choice_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const std::string &value,
    const std::vector<std::string> &options,
    const std::vector<std::string> &abbr_options) {
    auto *rt(new StringChoiceAttribute(
        title, abbr_title, value, options, abbr_options.empty() ? options : abbr_options));
    // rt->set_role_data(module::Attribute::StringChoicesEnabled, std::vector<bool>{}, false);
    rt->set_owner(this);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

JsonAttribute *Module::add_json_attribute(
    const std::string &title, const std::string &abbr_title, const nlohmann::json &value) {
    auto *rt(new JsonAttribute(title, abbr_title, value));
    rt->set_owner(this);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

BooleanAttribute *Module::add_boolean_attribute(
    const std::string &title, const std::string &abbr_title, const bool value) {
    auto *rt(new BooleanAttribute(title, abbr_title, value));
    rt->set_owner(this);

    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

StringAttribute *Module::add_string_attribute(
    const std::string &title, const std::string &abbr_title, const std::string &value) {
    auto rt = new StringAttribute(title, abbr_title, value);
    rt->set_owner(this);

    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

IntegerAttribute *Module::add_integer_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const int value,
    const int int_min,
    const int int_max) {

    auto rt = new IntegerAttribute(title, abbr_title, value, int_min, int_max);
    rt->set_owner(this);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}


QmlCodeAttribute *
Module::add_qml_code_attribute(const std::string &name, const std::string &qml_code) {
    auto rt = new QmlCodeAttribute(name, qml_code);
    rt->set_owner(this);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

ColourAttribute *Module::add_colour_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const utility::ColourTriplet &value) {
    auto rt = new ColourAttribute(title, abbr_title, value);
    rt->set_owner(this);

    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}


ActionAttribute *
Module::add_action_attribute(const std::string &title, const std::string &abbr_title) {
    auto rt = new ActionAttribute(title, abbr_title);
    rt->set_owner(this);

    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

bool Module::remove_attribute(const utility::Uuid &attribute_uuid) {

    auto attr = get_attribute(attribute_uuid);
    if (attr) {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        if (attr->has_role_data(Attribute::Groups)) {
            auto groups = attr->get_role_data<std::vector<std::string>>(Attribute::Groups);
            for (const auto &group_name : groups) {

                anon_send(
                    central_models_data_actor,
                    ui::model_data::remove_rows_atom_v,
                    group_name,
                    attribute_uuid);
            }
        }
        for (auto p = attributes_.begin(); p != attributes_.end(); p++) {
            if ((*p)->uuid() == attribute_uuid) {
                attributes_.erase(p);
                break;
            }
        }
    } else {
        throw std::runtime_error(
            fmt::format(
                "{}: No attribute with id {}",
                __PRETTY_FUNCTION__,
                to_string(attribute_uuid)).c_str()
                );
    }
    return true;

}

utility::JsonStore Module::serialise() const {
    utility::JsonStore result;
    for (const auto &attr : attributes_) {

        if (attr->has_role_data(Attribute::SerializeKey) &&
            attr->has_role_data(Attribute::Value)) {
            result[attr->get_role_data<std::string>(Attribute::SerializeKey)] =
                attr->role_data_as_json(Attribute::Value);
        } else if (
            attr->has_role_data(Attribute::Title) && attr->has_role_data(Attribute::Value)) {
            result[attr->get_role_data<std::string>(Attribute::Title)] =
                attr->role_data_as_json(Attribute::Value);
        }
    }
    return result;
}

void Module::deserialise(const nlohmann::json &json) {

    for (auto &attr : attributes_) {
        if (attr->has_role_data(Attribute::Title) ||
            attr->has_role_data(Attribute::SerializeKey)) {
            const auto attr_title =
                attr->has_role_data(Attribute::SerializeKey)
                    ? attr->get_role_data<std::string>(Attribute::SerializeKey)
                    : attr->get_role_data<std::string>(Attribute::Title);
            auto j = json.find(attr_title);
            if (j != json.end()) {
                attr->set_role_data(Attribute::Value, j.value());
            }
        }
    }
}

AttributeSet Module::full_module(const std::vector<std::string> &attr_groups) const {
    AttributeSet attrs;
    for (const auto &attr : attributes_) {
        if (attr->belongs_to_groups(attr_groups) || attr_groups.empty()) {
            attrs.emplace_back(new Attribute(*attr));
        }
    }

    return attrs;
}

AttributeSet Module::menu_attrs(const std::string &root_menu_name) const {
    AttributeSet attrs;
    for (const auto &attr : attributes_) {
        if (attr->has_role_data(Attribute::MenuPaths)) {
            try {
                auto menu_paths =
                    attr->get_role_data<std::vector<std::string>>(Attribute::MenuPaths);
                for (const auto &menu_path : menu_paths) {

                    if (menu_path.find(root_menu_name) == 0) {
                        attrs.emplace_back(new Attribute(*attr));
                        break;
                    }
                }
            } catch (std::exception &) {
            }
        }
    }

    return attrs;
}

caf::message_handler Module::message_handler() {
    caf::message_handler h(
        {[=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

         [=](attribute_role_data_atom,
             utility::Uuid attr_uuid,
             const std::string &role_name,
             const utility::JsonStore &value) -> result<bool> {
             try {

                 for (const auto &p : attributes_) {
                     if (p->uuid() == attr_uuid) {

                         try {
                             p->set_role_data(Attribute::role_index(role_name), value);
                             return true;
                         } catch (std::exception &e) {
                             return caf::make_error(xstudio_error::error, e.what());
                         }
                     }
                 }

                 std::stringstream msg;
                 msg << "Failed to set attr in \"" << name_ << "\" - invalid attr uuid passed.";
                 return caf::make_error(xstudio_error::error, msg.str());

             } catch (std::exception &e) {
                 return caf::make_error(xstudio_error::error, e.what());
             }
         },

         [=](utility::get_event_group_atom) -> caf::actor { return attribute_events_group_; },

         [=](broadcast::join_broadcast_atom, caf::actor subscriber) -> result<bool> {
             try {

                 scoped_actor sys{self()->home_system()};
                 bool r = utility::request_receive<bool>(
                     *sys,
                     attribute_events_group_,
                     broadcast::join_broadcast_atom_v,
                     subscriber);
                 return r;

             } catch (std::exception &e) {
                 return caf::make_error(xstudio_error::error, e.what());
             }
         },

         [=](json_store::update_atom,
             const JsonStore &change,
             const std::string &path,
             const JsonStore &full) {
             try {
                 update_attr_from_preference(path, change);
             } catch (const std::exception &err) {
                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
             }
         },

         [=](json_store::update_atom, const JsonStore &js) {
             try {
                 update_attrs_from_preferences(js);
             } catch (const std::exception &err) {
                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
             }
         },

         [=](change_attribute_request_atom,
             utility::Uuid attr_uuid,
             const int role,
             const utility::JsonStore &value) {
             for (const auto &p : attributes_) {
                 if (p->uuid() == attr_uuid) {

                     try {
                         p->set_role_data(role, value);
                     } catch (std::exception &e) {
                         spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                     }
                 }
             }
         },

         [=](change_attribute_request_atom,
             const std::string &attr_title,
             const int role,
             const utility::JsonStore &value) {
             auto attr = get_attribute(attr_title);
             if (attr) {
                 try {
                     attr->set_role_data(role, value);
                 } catch (std::exception &e) {
                     spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                 }
             }
         },

         [=](change_attribute_value_atom,
             const utility::JsonStore &full_attr_description,
             bool notify) -> bool {
             const std::string attr_title =
                 full_attr_description.value(Attribute::role_name(Attribute::Title), "");
             auto attr = get_attribute(attr_title);
             if (attr) {
                 try {
                     // preserve the attr uuid - we only match on the attr Title
                     attr->update_from_json(full_attr_description, notify);
                 } catch (std::exception &e) {
                     spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                 }
             }

             return true;
         },

         [=](change_attribute_value_atom,
             const std::string &attr_title,
             const utility::JsonStore &value,
             bool notify) -> bool {
             auto attr = get_attribute(attr_title);
             if (attr) {
                 try {
                     attr->set_role_data(Attribute::Value, value, notify);
                 } catch (std::exception &e) {
                     spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                 }
             }
             return true;
         },

         [=](change_attribute_value_atom,
             const std::string &attr_title,
             const int role,
             bool notify,
             const utility::JsonStore &value,
             caf::actor_addr attr_sync_source_adress) -> bool {
             attr_sync_source_adress_ = attr_sync_source_adress;
             auto attr                = get_attribute(attr_title);

             if (attr) {
                 try {
                     attr->set_role_data(role, value, notify);
                 } catch (std::exception &e) {
                     spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                 }
             }
             attr_sync_source_adress_ = caf::actor_addr();
             return true;
         },

         [=](change_attribute_value_atom,
             const utility::Uuid &attr_uuid,
             const utility::JsonStore &value,
             bool notify) -> bool {
             for (const auto &p : attributes_) {
                 if (p->uuid() == attr_uuid) {
                     try {
                         p->set_role_data(Attribute::Value, value, notify);
                     } catch (std::exception &e) {
                         spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                     }
                 }
             }
             return true;
         },

         [=](attribute_value_atom,
             const std::string &attr_title) -> result<utility::JsonStore> {
             auto attr = get_attribute(attr_title);
             if (attr) {
                 return attr->role_data_as_json(Attribute::Value);
             }
             const std::string err =
                 std::string("No such attribute \"") + attr_title + std::string(" on ") + name_;
             return caf::make_error(xstudio_error::error, err);
         },

         [=](attribute_value_atom,
             const utility::Uuid &attr_uuid) -> result<utility::JsonStore> {
             for (const auto &p : attributes_) {
                 if (p->uuid() == attr_uuid) {
                     return p->role_data_as_json(Attribute::Value);
                 }
             }
             const std::string err = std::string("Request for attribute on module \"") + name_ +
                                     std::string("\" using unknown attribute uuid.");
             return caf::make_error(xstudio_error::error, err);
         },

         [=](attribute_value_atom, const utility::Uuid &attr_uuid, const int role)
             -> result<utility::JsonStore> {
             for (const auto &p : attributes_) {
                 if (p->uuid() == attr_uuid) {
                     if (p->has_role_data(role)) {
                         return p->role_data_as_json(role);
                     } else {
                         std::stringstream ss;
                         ss << "Request for attribute role data \""
                            << Attribute::role_name(role) << "\" on attr \""
                            << p->get_role_data<std::string>(Attribute::Title)
                            << "\" on module \"" << name_
                            << "\" : attribute does not have this role data.";
                         return caf::make_error(xstudio_error::error, ss.str().c_str());
                     }
                 }
             }
             const std::string err = std::string("Request for attribute on module \"") + name_ +
                                     std::string("\" using unknown attribute uuid.");
             return caf::make_error(xstudio_error::error, err);
         },

         [=](attribute_value_atom,
             const utility::Uuid &attr_uuid,
             const utility::JsonStore &value) -> result<bool> {
             try {

                 for (const auto &p : attributes_) {
                     if (p->uuid() == attr_uuid) {
                         p->set_role_data(Attribute::Value, value);
                         return true;
                     }
                 }

                 const std::string err = std::string("Request for set attribute on module \"") +
                                         name_ +
                                         std::string("\" using unknown attribute uuid.");
                 throw std::runtime_error(err.c_str());

             } catch (std::exception &e) {

                 return caf::make_error(xstudio_error::error, e.what());
             }
         },

         [=](add_attribute_atom,
             const std::string attr_title,
             const utility::JsonStore &attr_value,
             const utility::JsonStore &attr_role_data) -> result<utility::Uuid> {
             try {

                 Attribute *attr = add_attribute(attr_title, attr_value, attr_role_data);

                 return attr->uuid();

             } catch (std::exception &e) {

                 std::stringstream ss;
                 ss << "Failed to add attribute \"" << attr_title << "\" with value \""
                    << attr_value.dump() << "\" and role data \"" << attr_role_data.dump()
                    << " with error: " << e.what();
                 return caf::make_error(xstudio_error::error, ss.str());
             }
         },

         [=](remove_attribute_atom,
            const utility::Uuid & uuid) -> result<bool> {

            try {
                remove_attribute(uuid);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
            return true;

        },


         [=](attribute_uuids_atom) -> std::vector<xstudio::utility::Uuid> {
             std::vector<xstudio::utility::Uuid> rt;
             for (auto &attr : attributes_) {
                 rt.push_back(attr->uuid());
             }
             return rt;
         },

         [=](request_full_attributes_description_atom) -> utility::JsonStore {
             utility::JsonStore r;
             for (auto &attr : attributes_) {
                 r[attr->get_role_data<std::string>(Attribute::Title)] = attr->as_json();
             }
             return r;
         },

         [=](request_full_attributes_description_atom,
             const std::vector<std::string> &attr_group,
             const utility::Uuid &requester_uuid) {
             anon_send(
                 module_events_group_,
                 full_attributes_description_atom_v,
                 full_module(attr_group),
                 requester_uuid);
         },

         [=](request_menu_attributes_description_atom,
             const std::string &root_menu_name,
             const utility::Uuid &requester_uuid) {
             anon_send(
                 module_events_group_,
                 full_attributes_description_atom_v,
                 menu_attrs(root_menu_name),
                 requester_uuid);
         },

         [=](link_module_atom,
             caf::actor linkwith,
             bool all_attrs,
             bool both_ways,
             bool intial_push_sync) {
             link_to_module(linkwith, all_attrs, both_ways, intial_push_sync);
         },

         [=](link_module_atom,
             caf::actor linkwith,
             bool unlink) {
            if (unlink) {
                unlink_module(linkwith);
            }
         },

         [=](connect_to_ui_atom) { connect_to_ui(); },

         [=](disconnect_from_ui_atom) { disconnect_from_ui(); },

         [=](ui::keypress_monitor::text_entry_atom,
             const std::string &text,
             const std::string &context) { text_entered(text, context); },
         [=](ui::keypress_monitor::key_down_atom,
             int key,
             const std::string &context,
             const bool auto_repeat) { key_pressed(key, context, auto_repeat); },

         [=](ui::keypress_monitor::mouse_event_atom, const ui::PointerEvent &e) {
             if (connected_viewports_.find(e.context()) != connected_viewports_.end() &&
                 !pointer_event(e)) {
                 // pointer event was not used
             }
         },

         [=](ui::keypress_monitor::register_hotkey_atom,
             int default_keycode,
             int default_modifier,
             const std::string &hotkey_name,
             const std::string &description,
             const bool auto_repeat,
             const std::string &component,
             const std::string &context) -> result<utility::Uuid> {
             try {
                 return register_hotkey(
                     default_keycode,
                     default_modifier,
                     hotkey_name,
                     description,
                     auto_repeat,
                     component,
                     context);
             } catch (std::exception &e) {
                 return make_error(xstudio_error::error, e.what());
             }
         },

         [=](ui::keypress_monitor::register_hotkey_atom,
             std::string key_name,
             int default_modifier,
             const std::string &hotkey_name,
             const std::string &description,
             const bool auto_repeat,
             const std::string &component,
             const std::string &context) -> result<utility::Uuid> {
             int default_keycode = -1;
             for (const auto &p : ui::Hotkey::key_names) {
                 if (p.second == key_name) {
                     default_keycode = p.first;
                     break;
                 }
             }

             if (default_keycode == -1) {
                 std::stringstream ss;
                 ss << "Unkown keyboard key name \"" << key_name << "\"";
                 return make_error(xstudio_error::error, ss.str().c_str());
             }

             try {
                 return register_hotkey(
                     default_keycode,
                     default_modifier,
                     hotkey_name,
                     description,
                     auto_repeat,
                     component,
                     context);
             } catch (std::exception &e) {
                 return make_error(xstudio_error::error, e.what());
             }
         },

         [=](ui::keypress_monitor::hotkey_event_atom,
             const utility::Uuid uuid,
             bool activated,
             const std::string &context) {
             anon_send(
                 attribute_events_group_,
                 ui::keypress_monitor::hotkey_event_atom_v,
                 uuid,
                 activated,
                 context);

             if (activated && connected_to_ui_ &&
                 connected_viewports_.find(context) != connected_viewports_.end())
                 hotkey_pressed(uuid, context);
             else
                 hotkey_released(uuid, context);
         },

         [=](deserialise_atom, const utility::JsonStore &json) { deserialise(json); },

         [=](serialise_atom) -> utility::JsonStore { return serialise(); },

         [=](update_attribute_in_preferences_atom) {
             auto prefs = global_store::GlobalStoreHelper(self()->home_system());
             for (const auto &attr_uuid : attrs_waiting_to_update_prefs_) {
                 Attribute *attr = get_attribute(attr_uuid);
                 if (attr) {
                     auto pref_path =
                         attr->get_role_data<std::string>(Attribute::PreferencePath);
                     prefs.set_value(
                         attr->role_data_as_json(Attribute::Value),
                         pref_path,
                         true,
                         false // do not broadcast the change *
                     );

                     // * if we set a pref and we don't suppress broadcast the
                     // GlobalStore actor that manages prefs re-broadcasts the
                     // new preference and it comes back up to us ... this can
                     // cause some nasty sync issues and anyway, we only want
                     // one-way setting of a preference here
                 }
             }
             attrs_waiting_to_update_prefs_.clear();
         },
         [=](module::current_viewport_playhead_atom, caf::actor_addr) {},
         [=](ui::viewport::connect_to_viewport_toolbar_atom,
             const std::string &viewport_name,
             const std::string &viewport_toolbar_name,
             bool connect) {
             connect_to_viewport(viewport_name, viewport_toolbar_name, connect);
         },
         [=](utility::name_atom) -> std::string { return name(); },
         [=](utility::event_atom,
             playhead::show_atom,
             caf::actor media,
             caf::actor media_source) {
             on_screen_media_changed(media, media_source);
             anon_send(
                 attribute_events_group_,
                 utility::event_atom_v,
                 playhead::show_atom_v,
                 media,
                 media_source);
         },
         [=](utility::event_atom,
             xstudio::ui::model_data::set_node_data_atom,
             const std::string &model_name,
             const std::string &path,
             const utility::JsonStore &data) {},
         [=](utility::event_atom,
             xstudio::ui::model_data::set_node_data_atom,
             const std::string &model_name,
             const std::string path,
             const utility::JsonStore &data,
             const std::string role,
             const utility::Uuid &uuid_role_data) {
             try {
                 Attribute *attr = get_attribute(uuid_role_data);
                 if (attr) {
                     attr->set_role_data(Attribute::role_index(role), data);
                 }
             } catch (std::exception &e) {
                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
             }
         },
         [=](utility::event_atom,
             xstudio::ui::model_data::model_data_atom,
             const std::string &model_name,
             const utility::JsonStore &data) {}

        });
    return h.or_else(playhead::PlayheadGlobalEventsActor::default_event_handler());
}

void Module::notify_change(
    utility::Uuid attr_uuid,
    const int role,
    const utility::JsonStore &value,
    const bool redraw_viewport,
    const bool self_notify) {

    Attribute *attr = get_attribute(attr_uuid);

    if (module_events_group_) {
        anon_send(
            module_events_group_,
            change_attribute_event_atom_v,
            module_uuid_,
            attr_uuid,
            role,
            value,
            redraw_viewport);

        anon_send(
            attribute_events_group_,
            change_attribute_event_atom_v,
            module_uuid_,
            attr_uuid,
            role,
            value);

        if (attr->has_role_data(Attribute::Groups)) {

            auto central_models_data_actor =
                self()->home_system().registry().template get<caf::actor>(
                    global_ui_model_data_registry);

            auto groups = attr->get_role_data<std::vector<std::string>>(Attribute::Groups);
            for (const auto &group_name : groups) {
                anon_send(
                    central_models_data_actor,
                    ui::model_data::set_node_data_atom_v,
                    group_name,
                    attr->uuid(),
                    Attribute::role_name(role),
                    value,
                    self());
            }
        }
        attribute_changed(attr_uuid, role, self_notify);
    }

    if (role == Attribute::PreferencePath) {
        
        // looks like the preference path is being set on the attribute. Note 
        // we might get here before ser_parent_actor_addr' has been called so
        // we don't have 'self()' which is why I use the ActorSystemSingleton
        // to get to the caf system to get a GlobalStoreHelper
        auto prefs = global_store::GlobalStoreHelper(
            xstudio::utility::ActorSystemSingleton::actor_system_ref()
            );

        std::string pref_path;
        try {

            pref_path =  attr->get_role_data<std::string>(Attribute::PreferencePath);
            attr->set_role_data(
                Attribute::Value,
                prefs.get_existing_or_create_new_preference(
                    pref_path,
                    attr->role_data_as_json(Attribute::Value),
                    true,
                    false
                    )
                );

        } catch (std::exception & e) {

            spdlog::warn("{} : {} {}", name(), __PRETTY_FUNCTION__, e.what());
        }
    }

    // if an attr has a PreferencePath this means its value will be stored and
    // retrieved from the preferences system so the attribute value persists
    // between sessions. So if you set Volume to level 8, next time you start
    // xSTUDIO it is already at 8 for example.
    if (attr && attr->has_role_data(Attribute::PreferencePath) && self()) {
        if (!attrs_waiting_to_update_prefs_.size()) {

            // In order to prevent rapid granular attr updates spamming the
            // preference store when a user grabs a slider (like volume adjust, say)
            // and interacts with it, we make a list of attrs that have changed
            // and then do a periodic update to push the value to the prefs.

            // 'delayed_anon_send' is causing big problems with and Modules that
            // have a parent actor that lives in the Qt layer (Viewport, for
            // example) because it will hang the actor system if the Viewport is
            // destroyed before the delayed message is received.

            /*delayed_anon_send(
                self(), std::chrono::seconds(2), update_attribute_in_preferences_atom_v);*/

            // To get around this problem we can do this shenannegans instead...
            auto resender = self()->home_system().spawn(delayed_resend);
            delayed_anon_send(
                resender,
                std::chrono::seconds(2),
                update_attribute_in_preferences_atom_v,
                parent_actor_addr_);
        }

        attrs_waiting_to_update_prefs_.insert(attr_uuid);
    }
}

void Module::attribute_changed(const utility::Uuid &attr_uuid, const int role_id, bool notify) {


    module::Attribute *attr = get_attribute(attr_uuid);

    if (role_id == Attribute::Groups && attr) {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        auto groups = attr->get_role_data<std::vector<std::string>>(Attribute::Groups);

        for (const auto &group_name : groups) {
            anon_send(
                central_models_data_actor,
                ui::model_data::register_model_data_atom_v,
                group_name,
                utility::JsonStore(attr->as_json()),
                attr_uuid,
                Attribute::role_name(Attribute::ToolbarPosition),
                self());
        }
    }

    // This is where the 'linking' mechanism is enacted. We send a change_attribute
    // message to linked modules.
    if (linking_disabled_ || role_id != Attribute::Value) {
        if (notify)
            attribute_changed(attr_uuid, role_id);
        return;
    }

    auto my_adress = caf::actor_cast<caf::actor_addr>(self());
    if (linked_attrs_.find(attr_uuid) != linked_attrs_.end()) {
        for (auto &linked_module_addr : partially_linked_modules_) {

            // If the attribute is changed because of a sync coming from another
            // linked module, we don't want to broadcast the change *back* to
            // that module. That's what this line does!
            if (linked_module_addr == attr_sync_source_adress_)
                continue;

            auto linked_module = caf::actor_cast<caf::actor>(linked_module_addr);
            if (!linked_module)
                continue;
            module::Attribute *attr = get_attribute(attr_uuid);
            auto attr_name      = attr->get_role_data<std::string>(module::Attribute::Title);
            auto attr_role_data = attr->role_data_as_json(role_id);
            anon_send(
                linked_module,
                module::change_attribute_value_atom_v,
                attr_name,
                role_id,
                notify,
                utility::JsonStore(attr_role_data),
                my_adress);
        }
    }
    for (auto &linked_module_addr : fully_linked_modules_) {
        if (linked_module_addr == attr_sync_source_adress_)
            continue;
        auto linked_module = caf::actor_cast<caf::actor>(linked_module_addr);
        if (!linked_module)
            continue;
        module::Attribute *attr = get_attribute(attr_uuid);
        auto attr_name          = attr->get_role_data<std::string>(module::Attribute::Title);
        auto attr_role_data     = attr->role_data_as_json(role_id);
        anon_send(
            linked_module,
            module::change_attribute_value_atom_v,
            attr_name,
            role_id,
            notify,
            utility::JsonStore(attr_role_data),
            my_adress);
    }

    if (notify)
        attribute_changed(attr_uuid, role_id);
}

Attribute *Module::get_attribute(const utility::Uuid &attr_uuid) {
    Attribute *r = nullptr;
    for (auto &attr : attributes_) {
        if (attr->uuid() == attr_uuid) {
            r = attr.get();
            break;
        }
    }
    return r;
}

Attribute *Module::get_attribute(const std::string &attr_title) {
    Attribute *r = nullptr;
    for (auto &attr : attributes_) {
        if (attr->has_role_data(Attribute::Title) &&
            attr_title == attr->get_role_data<std::string>(module::Attribute::Title)) {
            r = attr.get();
            break;
        }
    }
    return r;
}

utility::Uuid Module::register_hotkey(
    int default_keycode,
    int default_modifier,
    const std::string &hotkey_name,
    const std::string &description,
    const bool auto_repeat,
    const std::string &component,
    const std::string &context) {
    if (self()) {

        if (!keypress_monitor_actor_) {
            keypress_monitor_actor_ =
                self()->home_system().registry().template get<caf::actor>(keyboard_events);
        }

        ui::Hotkey hk(
            default_keycode,
            default_modifier,
            hotkey_name,
            component == "MODULE_NAME" ? name() : component,
            description,
            context,
            auto_repeat,
            caf::actor_cast<caf::actor_addr>(self()));

        anon_send(keypress_monitor_actor_, ui::keypress_monitor::register_hotkey_atom_v, hk);

        return hk.uuid();

    } else {

        unregistered_hotkeys_.emplace_back(
            default_keycode,
            default_modifier,
            hotkey_name,
            component == "MODULE_NAME" ? name() : component,
            description,
            context,
            auto_repeat,
            caf::actor_addr());

        return unregistered_hotkeys_.back().uuid();
    }

    return utility::Uuid();
}

void Module::remove_hotkey(const utility::Uuid & /*hotkey_uuid*/) {}

void Module::grab_mouse_focus() {

    if (keypress_monitor_actor_ && self()) {
        anon_send(keypress_monitor_actor_, grab_all_mouse_input_atom_v, self(), true);
    }
}

void Module::release_mouse_focus() {

    if (keypress_monitor_actor_ && self()) {
        anon_send(keypress_monitor_actor_, grab_all_mouse_input_atom_v, self(), false);
    }
}

void Module::grab_keyboard_focus() {

    if (keypress_monitor_actor_ && self()) {
        anon_send(keypress_monitor_actor_, grab_all_keyboard_input_atom_v, self(), true);
    }
}

void Module::release_keyboard_focus() {

    if (keypress_monitor_actor_ && self()) {
        anon_send(keypress_monitor_actor_, grab_all_keyboard_input_atom_v, self(), false);
    }
}

void Module::listen_to_playhead_events(const bool listen) {

    // join the global playhead events group - this tells us when the playhead that should
    // be on screen changes, among other things
    auto ph_events =
        self()->home_system().registry().template get<caf::actor>(global_playhead_events_actor);
    if (listen)
        anon_send(ph_events, broadcast::join_broadcast_atom_v, self());
    else
        anon_send(ph_events, broadcast::leave_broadcast_atom_v, self());
}

void Module::connect_to_ui() {

    // if necessary, get the global module events actor and the associated events groups
    if ((!global_module_events_actor_ || !keypress_monitor_actor_ ||
         !keyboard_and_mouse_group_) &&
        self()) {

        try {

            global_module_events_actor_ =
                self()->home_system().registry().template get<caf::actor>(
                    module_events_registry);

            scoped_actor sys{self()->home_system()};

            ui_attribute_events_group_ = utility::request_receive<caf::actor>(
                *sys, global_module_events_actor_, module_ui_events_group_atom_v);

            keypress_monitor_actor_ =
                self()->home_system().registry().template get<caf::actor>(keyboard_events);
            keyboard_and_mouse_group_ = utility::request_receive<caf::actor>(
                *sys, keypress_monitor_actor_, utility::get_event_group_atom_v);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            return;
        }
    }

    try {

        // Now join the events groups to 'connect' to events coming from the UI.
        // (got to be a better way of doing this than these casts!?)
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a) {
            join_broadcast(a, keyboard_and_mouse_group_);
            join_broadcast(a, ui_attribute_events_group_);
        } else {
            auto b = caf::actor_cast<caf::blocking_actor *>(self());
            if (b) {
                join_broadcast(b, keyboard_and_mouse_group_);
                join_broadcast(b, ui_attribute_events_group_);
            } else {
                spdlog::warn(
                    "{} {}",
                    __PRETTY_FUNCTION__,
                    "Unable to cast parent actor for hotkey registration");
            }
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (!connected_to_ui_) {
        connected_to_ui_ = true;
        connected_to_ui_changed();
    } else {
        return;
    }

    register_hotkeys();

    anon_send(
        global_module_events_actor_, join_module_attr_events_atom_v, module_events_group_);
    anon_send(global_module_events_actor_, full_attributes_description_atom_v, full_module());

    // here we set-up a tree model that holds the state of attributes that we want
    // to make visible in the UI (Qt/QML) layer using QAbstractItemModel.
    // The tree lives in a central actor that is a middleman between us and the
    // UI. When we update an attribute we notify the middleman about the change,
    // and this is forwarded to the UI. Likewise, if the UI changes an attribute
    // a message is sent to the middleman which is passed back to Module here so
    // we update the actual attribute.

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    for (auto &a : attributes_) {

        if (a->has_role_data(Attribute::Groups)) {
            auto groups = a->get_role_data<std::vector<std::string>>(Attribute::Groups);
            for (const auto &group_name : groups) {
                anon_send(
                    central_models_data_actor,
                    ui::model_data::register_model_data_atom_v,
                    group_name,
                    utility::JsonStore(a->as_json()),
                    a->uuid(),
                    Attribute::role_name(Attribute::ToolbarPosition),
                    self());
            }
        }
    }
}

void Module::disconnect_from_ui() {

    if (!connected_to_ui_)
        return;

    if (keyboard_and_mouse_group_ && ui_attribute_events_group_ && self()) {
        // got to be a better way of doing this?
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a) {
            leave_broadcast(a, keyboard_and_mouse_group_);
            leave_broadcast(a, ui_attribute_events_group_);
        } else {
            auto b = caf::actor_cast<caf::blocking_actor *>(self());
            if (b) {
                leave_broadcast(b, keyboard_and_mouse_group_);
            } else {
                spdlog::warn(
                    "{} {}",
                    __PRETTY_FUNCTION__,
                    "Unable to cast parent actor for hotkey registration");
            }
        }

        // tell the UI middleman to remove our attributes from its data models

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        for (auto &a : attributes_) {

            if (a->has_role_data(Attribute::Groups)) {
                auto groups = a->get_role_data<std::vector<std::string>>(Attribute::Groups);
                for (const auto &group_name : groups) {
                    anon_send(
                        central_models_data_actor,
                        ui::model_data::register_model_data_atom_v,
                        group_name,
                        a->uuid(),
                        self());
                }
            }
        }
    }

    if (global_module_events_actor_) {
        anon_send(
            global_module_events_actor_, leave_module_attr_events_atom_v, module_events_group_);
        std::vector<utility::Uuid> attr_uuids;
        for (const auto &p : attributes_) {
            attr_uuids.push_back(p->uuid());
        }
        anon_send(global_module_events_actor_, remove_attrs_from_ui_atom_v, attr_uuids);
    }

    keyboard_and_mouse_group_   = caf::actor();
    ui_attribute_events_group_  = caf::actor();
    global_module_events_actor_ = caf::actor();

    if (connected_to_ui_) {
        connected_to_ui_ = false;
        connected_to_ui_changed();
    }
}

void Module::update_attr_from_preference(const std::string &path, const JsonStore &change) {
    for (auto &attr : attributes_) {
        if (attr->has_role_data(Attribute::PreferencePath) &&
            (attr->get_role_data<std::string>(Attribute::PreferencePath) + "/value") == path) {

            try {

                attr->set_role_data(Attribute::Value, change, false);

            } catch (std::exception &e) {
                spdlog::warn("{} failed to set preference {}", __PRETTY_FUNCTION__, e.what());
            }
        } else if (
            attr->has_role_data(Attribute::InitOnlyPreferencePath) &&
            (attr->get_role_data<std::string>(Attribute::InitOnlyPreferencePath) + "/value") ==
                path) {

            try {

                attr->set_role_data(Attribute::Value, change, false);
                // wipe the 'InitOnlyPreferencePath' data so we never update again
                // when preferences are updated
                attr->delete_role_data(Attribute::InitOnlyPreferencePath);

            } catch (std::exception &e) {
                spdlog::warn("{} failed to set preference {}", __PRETTY_FUNCTION__, e.what());
            }
        }
    }
}


void Module::update_attrs_from_preferences(const utility::JsonStore &entire_prefs_dict) {

    for (auto &attr : attributes_) {
        if (attr->has_role_data(Attribute::PreferencePath)) {

            try {

                auto pref_path  = attr->get_role_data<std::string>(Attribute::PreferencePath);
                auto pref_value = global_store::preference_value<nlohmann::json>(
                    entire_prefs_dict, pref_path);

                attr->set_role_data(Attribute::Value, pref_value, true);

            } catch (std::exception &e) {
                spdlog::warn("{} failed to set preference {}", __PRETTY_FUNCTION__, e.what());
            }
        } else if (attr->has_role_data(Attribute::InitOnlyPreferencePath)) {

            try {

                auto pref_path =
                    attr->get_role_data<std::string>(Attribute::InitOnlyPreferencePath);
                auto pref_value = global_store::preference_value<nlohmann::json>(
                    entire_prefs_dict, pref_path);

                attr->set_role_data(Attribute::Value, pref_value, false);

                // wipe the 'InitOnlyPreferencePath' data so we never update again
                // when preferences are updated
                attr->delete_role_data(Attribute::InitOnlyPreferencePath);

            } catch (std::exception &e) {
                spdlog::warn("{} failed to set preference {}", __PRETTY_FUNCTION__, e.what());
            }
        }
    }
}

void Module::add_multichoice_attr_to_menu(
    StringChoiceAttribute *attr,
    const std::string top_level_menu,
    const std::string menu_path,
    const std::string before) {
    std::string full_path = top_level_menu + "|" + menu_path;
    std::vector<std::string> menu_paths;
    if (attr->has_role_data(module::Attribute::MenuPaths)) {
        menu_paths =
            attr->get_role_data<std::vector<std::string>>(module::Attribute::MenuPaths);
    }
    menu_paths.push_back(full_path);
    attr->set_role_data(module::Attribute::MenuPaths, nlohmann::json(menu_paths));
}

void Module::add_boolean_attr_to_menu(
    BooleanAttribute *attr, const std::string top_level_menu, const std::string before) {
    std::vector<std::string> menu_paths;
    if (attr->has_role_data(module::Attribute::MenuPaths)) {
        menu_paths =
            attr->get_role_data<std::vector<std::string>>(module::Attribute::MenuPaths);
    }
    menu_paths.push_back(top_level_menu);
    attr->set_role_data(module::Attribute::MenuPaths, nlohmann::json(menu_paths));
}

void Module::make_attribute_visible_in_viewport_toolbar(
    Attribute *attr, const bool make_visible) {
    if (make_visible) {

        attrs_in_toolbar_.insert(attr->uuid());

        if (!self())
            return;

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        if (central_models_data_actor) {

            for (const auto &viewport_name : connected_viewports_) {

                std::string toolbar_name = viewport_name + "_toolbar";
                attr->expose_in_ui_attrs_group(toolbar_name, true);

                anon_send(
                    central_models_data_actor,
                    ui::model_data::register_model_data_atom_v,
                    toolbar_name,
                    utility::JsonStore(attr->as_json()),
                    attr->uuid(),
                    Attribute::role_name(Attribute::ToolbarPosition),
                    self());
            }
        }

    } else {

        if (attrs_in_toolbar_.find(attr->uuid()) != attrs_in_toolbar_.end()) {
            attrs_in_toolbar_.erase(attrs_in_toolbar_.find(attr->uuid()));
        }

        if (!self())
            return;

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        if (central_models_data_actor) {
            for (const auto &viewport_name : connected_viewports_) {

                std::string toolbar_name = viewport_name + "_toolbar";
                attr->expose_in_ui_attrs_group(toolbar_name, false);

                anon_send(
                    central_models_data_actor,
                    ui::model_data::register_model_data_atom_v,
                    toolbar_name,
                    attr->uuid(),
                    caf::actor());
            }
        }
    }
}

void Module::redraw_viewport() {
    anon_send(module_events_group_, playhead::redraw_viewport_atom_v);
}


Attribute *Module::add_attribute(
    const std::string &title,
    const utility::JsonStore &value,
    const utility::JsonStore &role_data) {
    Attribute *attr = nullptr;
    if (role_data.contains("combo_box_options")) {

        // we need to do some specific type checking if the caller is trying
        // to create a combo box (string choice) attribute
        const auto &opts = role_data["combo_box_options"];
        if (!opts.is_array()) {
            throw std::runtime_error("combo_box_options must be a list of strings.");
        }

        if (!value.is_string()) {
            throw std::runtime_error(
                "Attr providing combo_box_optionsrole data must have string type value.");
        }

        std::vector<std::string> options;
        auto cs = opts.begin();
        while (cs != opts.end()) {
            if (!cs.value().is_string()) {
                throw std::runtime_error("combo_box_options must be a list of strings.");
            }
            options.push_back(cs.value().get<std::string>());
            cs++;
        }

        attr = static_cast<Attribute *>(
            add_string_choice_attribute(title, title, value.get<std::string>(), options));

    } else if (role_data.contains("qml_code")) {

        attr = static_cast<Attribute *>(
            add_qml_code_attribute(title, role_data["qml_code"].get<std::string>()));

    } else if (role_data.contains("menu_paths")) {

        attr = static_cast<Attribute *>(add_action_attribute(title, title));

    } else if (value.is_number_integer()) {

        throw std::runtime_error("Integer type attribute not supported.");

    } else if (value.is_number_float()) {

        attr = static_cast<Attribute *>(add_float_attribute(title, title, value.get<float>()));

    } else if (value.is_boolean()) {

        attr = static_cast<Attribute *>(add_boolean_attribute(title, title, value.get<bool>()));

    } else if (value.is_string()) {

        attr = static_cast<Attribute *>(
            add_string_attribute(title, title, value.get<std::string>()));

    } else if (value.is_object() || value.is_null()) {

        attr = static_cast<Attribute *>(add_json_attribute(title, nlohmann::json("{}")));
        attr->set_role_data(Attribute::Value, value);

    } else {
        throw std::runtime_error("Unrecognised attribute value type");
    }

    if (role_data.is_object()) {
        for (auto p = role_data.begin(); p != role_data.end(); ++p) {
            const auto key     = p.key();
            const int role_idx = Attribute::role_index(key);
            attr->set_role_data(role_idx, p.value());
        }
    }

    return attr;
}

void Module::expose_attribute_in_model_data(
    Attribute *attr, const std::string &model_name, const bool expose) {

    attr->expose_in_ui_attrs_group(model_name, connect);
    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    try {
        if (expose) {
            anon_send(
                central_models_data_actor,
                ui::model_data::register_model_data_atom_v,
                model_name,
                utility::JsonStore(attr->as_json()),
                attr->uuid(),
                Attribute::role_name(Attribute::ToolbarPosition),
                self());
        } else {
            // this removes the attribute from the model of name
            // 'model_name'
            anon_send(
                central_models_data_actor,
                ui::model_data::register_model_data_atom_v,
                model_name,
                attr->uuid(),
                self());
        }
    } catch (std::exception &) {
    }
}

void Module::connect_to_viewport(
    const std::string &viewport_name, const std::string &viewport_toolbar_name, bool connect) {

    if (connect) {
        connected_viewports_.insert(viewport_name);
    } else if (connected_viewports_.find(viewport_name) != connected_viewports_.end()) {
        connected_viewports_.erase(connected_viewports_.find(viewport_name));
    }

    for (const auto &toolbar_attr_id : attrs_in_toolbar_) {
        Attribute *attr = get_attribute(toolbar_attr_id);
        if (attr) {
            expose_attribute_in_model_data(attr, viewport_toolbar_name, connect);
        }
    }
}

void Module::add_attribute(Attribute *attr) { attributes_.emplace_back(attr); }

utility::JsonStore Module::public_state_data() {

    // This is not called. Maybe we need live JsonTree data for the whole
    // session at the backend to simplify frontend stuff for exposing data.
    utility::JsonStore data;
    data["name"]     = name();
    data["children"] = nlohmann::json::array();
    for (auto &attr : attributes_) {

        data["children"].push_back(attr->as_json());
    }
    // std::cerr << data.dump(2) << "\n";
    return data;
}
