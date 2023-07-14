// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playhead/playhead_global_events_actor.hpp"

#include <iostream>

using namespace xstudio::utility;
using namespace xstudio::module;
using namespace xstudio;

Module::Module(const std::string name) : name_(std::move(name)) {}

Module::~Module() {
    disconnect_from_ui();
    global_module_events_actor_ = caf::actor();
    keypress_monitor_actor_     = caf::actor();
    module_events_group_        = caf::actor();
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
        // self()->link_to(module_events_group_);
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

            scoped_actor sys{self()->home_system()};
            // send state of all attrs to 'other_module' so it can update its copies as required
            for (auto &attribute : attributes_) {
                if (link_all_attrs ||
                    linked_attrs_.find(attribute->uuid()) != linked_attrs_.end()) {
                    try {
                        utility::request_receive<bool>(
                            *sys,
                            other_module,
                            change_attribute_value_atom_v,
                            attribute->get_role_data<std::string>(Attribute::Title),
                            utility::JsonStore(attribute->role_data_as_json(Attribute::Value)),
                            true);
                    } catch (std::exception &e) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                    }
                }
            }
        }

        if (both_ways)
            anon_send(
                other_module, module::link_module_atom_v, self(), link_all_attrs, false, false);
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

    attributes_.emplace_back(static_cast<Attribute *>(rt));
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
    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}

JsonAttribute *Module::add_json_attribute(
    const std::string &title, const std::string &abbr_title, const nlohmann::json &value) {
    auto *rt(new JsonAttribute(title, abbr_title, value));
    rt->set_owner(this);
    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}

BooleanAttribute *Module::add_boolean_attribute(
    const std::string &title, const std::string &abbr_title, const bool value) {
    auto *rt(new BooleanAttribute(title, abbr_title, value));
    rt->set_owner(this);

    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}

StringAttribute *Module::add_string_attribute(
    const std::string &title, const std::string &abbr_title, const std::string &value) {
    auto rt = new StringAttribute(title, abbr_title, value);
    rt->set_owner(this);

    attributes_.emplace_back(static_cast<Attribute *>(rt));
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
    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}


QmlCodeAttribute *
Module::add_qml_code_attribute(const std::string &name, const std::string &qml_code) {
    auto rt = new QmlCodeAttribute(name, qml_code);
    rt->set_owner(this);
    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}

ColourAttribute *Module::add_colour_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const utility::ColourTriplet &value) {
    auto rt = new ColourAttribute(title, abbr_title, value);
    rt->set_owner(this);

    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}


ActionAttribute *
Module::add_action_attribute(const std::string &title, const std::string &abbr_title) {
    auto rt = new ActionAttribute(title, abbr_title);
    rt->set_owner(this);

    attributes_.emplace_back(static_cast<Attribute *>(rt));
    return rt;
}

bool Module::remove_attribute(const utility::Uuid &attribute_uuid) {
    bool rt = false;
    for (auto p = attributes_.begin(); p != attributes_.end(); p++) {
        if ((*p)->uuid() == attribute_uuid) {
            attributes_.erase(p);
            anon_send(module_events_group_, attribute_deleted_event_atom_v, attribute_uuid);
            rt = true;
            break;
        }
    }
    return rt;
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
             if (!pointer_event(e)) {
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

             if (activated && connected_to_ui_)
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
         [=](utility::name_atom) -> std::string { return name(); },
         [=](utility::event_atom, playhead::show_atom, const media_reader::ImageBufPtr &buf) {
             on_screen_image(buf);
         },
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
         }

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

        attribute_changed(attr_uuid, role, self_notify);
    }


    if (attr && attr->has_role_data(Attribute::PreferencePath) && self()) {
        if (!attrs_waiting_to_update_prefs_.size()) {
            // if we haven't already queued up attrs to update in the prefs,
            // order an update for 10 seconds time
            delayed_anon_send(
                self(), std::chrono::seconds(2), update_attribute_in_preferences_atom_v);
        }
        attrs_waiting_to_update_prefs_.insert(attr_uuid);
    }
}

void Module::attribute_changed(const utility::Uuid &attr_uuid, const int role_id, bool notify) {

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
    if (!global_module_events_actor_ && self()) {

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

    if (!connected_to_ui_) {
        connected_to_ui_ = true;
        connected_to_ui_changed();
    }

    register_hotkeys();

    anon_send(
        global_module_events_actor_, join_module_attr_events_atom_v, module_events_group_);
    anon_send(global_module_events_actor_, full_attributes_description_atom_v, full_module());
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

                attr->set_role_data(Attribute::Value, pref_value, false);

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
