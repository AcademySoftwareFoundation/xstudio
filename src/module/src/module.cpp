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
#include <stdlib.h>

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
    keypress_monitor_actor_ = caf::actor();
}

void Module::parent_actor_exiting() {

    if (self()) {

        disconnect_from_ui();

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        if (central_models_data_actor) {

            // make sure attribute data is removed from UI data models that expose
            // attrs in the UI layer
            for (auto &attribute : attributes_) {

                if (attribute->has_role_data(Attribute::UIDataModels)) {

                    try {

                        auto groups = attribute->get_role_data<std::vector<std::string>>(
                            Attribute::UIDataModels);

                        for (const auto &group : groups) {
                            anon_send(
                                central_models_data_actor,
                                ui::model_data::deregister_model_data_atom_v,
                                group,
                                attribute->uuid(),
                                caf::actor());
                        }
                    } catch (...) {
                    }
                }
            }
        }
    }
}

void Module::set_parent_actor_addr(caf::actor_addr addr) {


    parent_actor_addr_ = addr;
    if (!attribute_events_group_) {
        attribute_events_group_ =
            self()->home_system().spawn<broadcast::BroadcastActor>(self());
        try {
            auto prefs = global_store::GlobalStoreHelper(self()->home_system());
            // only on init..
            utility::JsonStore j;
            auto a = caf::actor_cast<caf::event_based_actor *>(self());
            if (a) {
                join_broadcast(a, prefs.get_group(j));
            } else {
                auto b = caf::actor_cast<caf::blocking_actor *>(self());
                if (b) {
                    join_broadcast(b, prefs.get_group(j));
                }
            }
            update_attrs_from_preferences(j);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    // we can't add hotkeys until the parent actor has been set. Subclasses of
    // Module should define hotkeys in the virtual register_hotkeys() function

    auto a = caf::actor_cast<caf::event_based_actor *>(self());
    if (a) {
        a->set_down_handler([=](down_msg &msg) {
            a->demonitor(msg.source);
            auto p = connected_viewports_.find(caf::actor_cast<caf::actor>(msg.source));
            if (p != connected_viewports_.end()) {
                connected_viewports_.erase(p);
            }
        });
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

            scoped_actor sys{self()->home_system()};
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

void Module::unlink_module(caf::actor other_module) {
    auto addr = caf::actor_cast<caf::actor_addr>(other_module);
    auto p    = std::find(fully_linked_modules_.begin(), fully_linked_modules_.end(), addr);
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
        anon_send(other_module, module::link_module_atom_v, self(), false);
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
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

IntegerVecAttribute *
Module::add_int_vec_attribute(const std::string &title, const std::string &abbr_title) {

    auto *rt(new IntegerVecAttribute(title, abbr_title, std::vector<int>()));
    // rt->set_role_data(module::Attribute::StringChoicesEnabled, std::vector<bool>{}, false);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

JsonAttribute *Module::add_json_attribute(
    const std::string &title, const std::string &abbr_title, const nlohmann::json &value) {
    auto *rt(new JsonAttribute(title, abbr_title, value));
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

BooleanAttribute *Module::add_boolean_attribute(
    const std::string &title, const std::string &abbr_title, const bool value) {
    auto *rt(new BooleanAttribute(title, abbr_title, value));
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

StringAttribute *Module::add_string_attribute(
    const std::string &title, const std::string &abbr_title, const std::string &value) {
    auto rt = new StringAttribute(title, abbr_title, value);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

IntegerAttribute *Module::add_integer_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const int64_t value,
    const int64_t int_min,
    const int64_t int_max) {

    auto rt = new IntegerAttribute(title, abbr_title, value, int_min, int_max);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}


QmlCodeAttribute *
Module::add_qml_code_attribute(const std::string &name, const std::string &qml_code) {
    auto rt = new QmlCodeAttribute(name, qml_code);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

ColourAttribute *Module::add_colour_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const utility::ColourTriplet &value) {
    auto rt = new ColourAttribute(title, abbr_title, value);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

Vec4fAttribute *Module::add_vec4f_attribute(
    const std::string &title, const std::string &abbr_title, const Imath::V4f &value) {
    auto rt = new Vec4fAttribute(title, abbr_title, value);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

Vec4fAttribute *Module::add_vec4f_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const Imath::V4f &value,
    const Imath::V4f &min,
    const Imath::V4f &max,
    const Imath::V4f &step) {
    auto rt = new Vec4fAttribute(title, abbr_title, value);
    add_attribute(static_cast<Attribute *>(rt));
    rt->set_role_data(Attribute::DefaultValue, value);
    rt->set_role_data(Attribute::FloatScrubMin, min);
    rt->set_role_data(Attribute::FloatScrubMax, max);
    rt->set_role_data(Attribute::FloatScrubStep, step);
    return rt;
}

FloatVectorAttribute *Module::add_float_vector_attribute(
    const std::string &title,
    const std::string &abbr_title,
    const std::vector<float> &value,
    const std::vector<float> &min,
    const std::vector<float> &max,
    const std::vector<float> &step) {
    auto rt = new FloatVectorAttribute(title, abbr_title, value);
    add_attribute(static_cast<Attribute *>(rt));
    rt->set_role_data(Attribute::DefaultValue, value);
    rt->set_role_data(Attribute::FloatScrubMin, min);
    rt->set_role_data(Attribute::FloatScrubMax, max);
    rt->set_role_data(Attribute::FloatScrubStep, step);
    return rt;
}


ActionAttribute *
Module::add_action_attribute(const std::string &title, const std::string &abbr_title) {
    auto rt = new ActionAttribute(title, abbr_title);
    add_attribute(static_cast<Attribute *>(rt));
    return rt;
}

void Module::remove_attribute(const utility::Uuid &attribute_uuid) {

    auto attr = get_attribute(attribute_uuid);
    if (attr) {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        if (attr->has_role_data(Attribute::UIDataModels)) {
            auto groups =
                attr->get_role_data<std::vector<std::string>>(Attribute::UIDataModels);
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
                "{}: No attribute with id {}", __PRETTY_FUNCTION__, to_string(attribute_uuid))
                .c_str());
    }
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
             const utility::Uuid &attr_uuid,
             const std::string &role_name) -> result<utility::JsonStore> {
             try {
                 int role = Attribute::role_index(role_name);
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
                 const std::string err = std::string("Request for attribute on module \"") +
                                         name_ +
                                         std::string("\" using unknown attribute uuid.");
                 return caf::make_error(xstudio_error::error, err);
             } catch (std::exception &e) {
                 return caf::make_error(xstudio_error::error, e.what());
             }
         },

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

         [=](utility::uuid_atom) -> utility::Uuid { return uuid(); },

         [=](utility::get_event_group_atom, bool) -> caf::actor {
             return attribute_events_group_;
         },

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

         [=](attribute_value_atom, const utility::Uuid &attr_uuid, bool full)
             -> result<utility::JsonStore> {
             auto attr = get_attribute(attr_uuid);
             if (attr) {
                 return attr->as_json();
             }
             const std::string err = std::string("No such attribute \"") +
                                     to_string(attr_uuid) + std::string(" on ") + name_;
             return caf::make_error(xstudio_error::error, err);
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

         [=](remove_attribute_atom, const utility::Uuid &uuid) -> result<bool> {
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

         [=](link_module_atom,
             caf::actor linkwith,
             bool all_attrs,
             bool both_ways,
             bool intial_push_sync) {
             link_to_module(linkwith, all_attrs, both_ways, intial_push_sync);
         },

         [=](link_module_atom, caf::actor linkwith, bool unlink) {
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
             if (connected_viewport_names_.find(e.context()) !=
                     connected_viewport_names_.end() &&
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
             const std::string &component) -> result<utility::Uuid> {
             try {
                 return register_hotkey(
                     default_keycode,
                     default_modifier,
                     hotkey_name,
                     description,
                     auto_repeat,
                     component);
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
             const std::string &component) -> result<utility::Uuid> {
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
                     component);
             } catch (std::exception &e) {
                 return make_error(xstudio_error::error, e.what());
             }
         },

         [=](ui::keypress_monitor::hotkey_event_atom,
             const utility::Uuid uuid,
             bool activated,
             const std::string &context,
             const std::string &window) {
             anon_send(
                 attribute_events_group_,
                 ui::keypress_monitor::hotkey_event_atom_v,
                 uuid,
                 activated,
                 context,
                 window);

             if (activated && connected_to_ui_ /*&&
                 connected_viewport_names_.find(context) != connected_viewport_names_.end()*/) {
                 hotkey_pressed(uuid, context, window);

                 for (auto attr_uuid : dock_widget_attributes_) {
                     module::Attribute *attr = get_attribute(attr_uuid);
                     if (attr->has_role_data(Attribute::HotkeyUuid)) {
                         if (uuid ==
                             attr->get_role_data<utility::Uuid>(Attribute::HotkeyUuid)) {
                             // user has hit a hotkey that toggles a docking toolbox on/off. To
                             // get notification in the UI (QML) layer, we set the 'UserData'
                             // role data on the attr that holds data about the dock widget.
                             // There is a QML item in the UI layer that is watching for changes
                             // to this data - it will get an update and can then toggle the
                             // toolbar shown/hidden
                             nlohmann::json event;
                             event["context"] = context;
                             // random num ensures the data changes every time
                             // so notification mechanism is triggered
                             event["id"] = (double)rand() / RAND_MAX;                             
                             attr->set_role_data(Attribute::UserData, event);
                         }
                     }
                 }

             } else if (!activated)
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
         [=](module::current_viewport_playhead_atom, const std::string &name, caf::actor_addr)
             -> bool { return true; },
         [=](ui::viewport::connect_to_viewport_toolbar_atom,
             const std::string &viewport_name,
             const std::string &viewport_toolbar_name,
             caf::actor viewport,
             bool connect) {
             connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);
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
             ui::viewport::viewport_atom,
             const std::string viewport_name,
             caf::actor viewport) {
             // this event message happens when a new viewport is created
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
             // this update is incoming from the UI layer when attributes
             // that are exposed in JsonTree model data get changed by
             // QML. It could be general data model or menu specifi one.
             // For menu specific one we need to fiddle a bit to map the
             // role data.
             try {


                 int role_index;
                 if (role == "is_checked" || role == "current_choice")
                     role_index = Attribute::Value;
                 else if (role == "choices")
                     role_index = Attribute::Value;
                 else
                     role_index = Attribute::role_index(role);
                 Attribute *attr = get_attribute(uuid_role_data);
                 if (attr) {
                     attr->set_role_data(role_index, data);
                 }

             } catch (std::exception &e) {
                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
             }
         },
         [=](utility::event_atom,
             xstudio::ui::model_data::model_data_atom,
             const std::string &model_name,
             const utility::JsonStore &data) -> bool { return true; },

         [=](utility::event_atom,
             ui::model_data::menu_node_activated_atom,
             const std::string path,
             const utility::JsonStore &menu_item_data,
             const std::string &user_data,
             const bool from_hotkey) {
             if (from_hotkey) {
                 // N.B. hotkeys can trigger menu events. This is useful in
                 // the QML layer where we might define a menu item with an
                 // associated hotkey. However, for Modules we create hotkeys
                 // and get events separet to the menu system. The only thing
                 // we might use is tag a hotkey on a menu item so the hotkey
                 // sequence is made visible on the menu item but the
                 // mechanism for handling it is throug the hotkey_event_atom
                 // messages.
                 return;
             }

             try {
                 if (menu_item_data.contains("uuid")) {

                     bool event_used = false;
                     utility::Uuid uuid(menu_item_data["uuid"]);
                     // is the menu item representing a toggle attribute?
                     auto *attr = get_attribute(uuid);
                     if (attr) {
                         try {
                             if (attr->get_role_data<std::string>(Attribute::Type) ==
                                 "OnOffToggle") {
                                 attr->set_role_data(
                                     Attribute::Value,
                                     !attr->get_role_data<bool>(Attribute::Value));
                                 event_used = true;
                             }
                         } catch (...) {
                             // not a bool attr
                         }
                     }

                     // in the case where the menu item isn't linked to a toggle
                     // attr we run our callback that subclasses use to implement
                     // their own response to a menu item action
                     menu_item_activated(menu_item_data, user_data);
                     anon_send(
                         attribute_events_group_,
                         ui::model_data::menu_node_activated_atom_v,
                         menu_item_data,
                         user_data);
                 }
             } catch (std::exception &e) {
                 std::cerr << "EE " << e.what() << "\n";
             }
         },

         [=](utility::event_atom,
             xstudio::ui::model_data::set_node_data_atom,
             const std::string model_name,
             const std::string path,
             const std::string role,
             const utility::JsonStore &data,
             const utility::Uuid &menu_item_uuid) {
             try {
                 Attribute *attr = get_attribute(menu_item_uuid);
                 if (attr) {
                     if (role == "is_checked" && data.is_boolean()) {
                         attr->set_role_data(Attribute::Value, data.get<bool>());
                     } else if (role == "current_choice" && data.is_string()) {
                         attr->set_role_data(Attribute::Value, data.get<std::string>());
                     } else {
                         throw std::runtime_error("bad data");
                     }
                 }

             } catch (std::exception &e) {
                 spdlog::warn(
                     "Module set_node_data_atom handler - {} {} {} {} {}",
                     e.what(),
                     model_name,
                     path,
                     role,
                     data.dump());
             }
         },
         [=](module_add_menu_item_atom,
             const std::string &menu_model_name,
             const std::string &menu_path,
             const float menu_item_position,
             const utility::Uuid &hotkey) -> result<utility::Uuid> {
             try {
                 return insert_hotkey_into_menu(
                     menu_model_name, menu_path, menu_item_position, hotkey);
             } catch (std::exception &e) {
                 return caf::make_error(xstudio_error::error, e.what());
             }
         },
         [=](module_add_menu_item_atom,
             const std::string &menu_model_name,
             const std::string &menu_text,
             const std::string &menu_path,
             const float menu_item_position,
             const utility::Uuid &attr_id,
             const bool divider,
             const utility::Uuid &hotkey,
             const std::string &user_data) -> result<utility::Uuid> {
             try {
                 return insert_menu_item(
                     menu_model_name,
                     menu_text,
                     menu_path,
                     menu_item_position,
                     attr_id.is_null() ? nullptr : get_attribute(attr_id),
                     divider,
                     hotkey,
                     user_data);
             } catch (std::exception &e) {
                 return caf::make_error(xstudio_error::error, e.what());
             }
         },
         [=](module_remove_menu_item_atom, const utility::Uuid &menu_id) {
             for (const auto &p : menu_items_) {

                 for (const auto uuid : p.second) {
                     if (uuid == menu_id) {
                         auto central_models_data_actor =
                             self()->home_system().registry().template get<caf::actor>(
                                 global_ui_model_data_registry);
                         anon_send(
                             central_models_data_actor,
                             ui::model_data::remove_node_atom_v,
                             p.first,
                             menu_id);
                         return;
                     }
                 }
             }
         },
         [=](xstudio::ui::model_data::set_node_data_atom,
             const std::string &menu_model_name,
             const std::string &submenu,
             const float submenu_position) {
             set_submenu_position_in_parent(menu_model_name, submenu, submenu_position);
         },

         [=](reset_module_atom) { reset(); }});

    return h.or_else(playhead::PlayheadGlobalEventsActor::default_event_handler());
}

void Module::notify_change(
    utility::Uuid attr_uuid,
    const int role,
    const utility::JsonStore &value,
    const bool redraw_viewport,
    const bool self_notify) {

    Attribute *attr = get_attribute(attr_uuid);

    if (attribute_events_group_) {

        anon_send(
            attribute_events_group_,
            change_attribute_event_atom_v,
            module_uuid_,
            attr_uuid,
            role,
            value);

        if (attr->has_role_data(Attribute::UIDataModels)) {

            auto central_models_data_actor =
                self()->home_system().registry().template get<caf::actor>(
                    global_ui_model_data_registry);

            auto groups =
                attr->get_role_data<std::vector<std::string>>(Attribute::UIDataModels);
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
            xstudio::utility::ActorSystemSingleton::actor_system_ref());

        std::string pref_path;
        try {

            pref_path = attr->get_role_data<std::string>(Attribute::PreferencePath);
            attr->set_role_data(
                Attribute::Value,
                prefs.get_existing_or_create_new_preference(
                    pref_path, attr->role_data_as_json(Attribute::Value), true, false));

        } catch (std::exception &e) {

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
    if (attr && attr->has_role_data(Attribute::MenuPaths)) {
        update_attribute_menu_item_data(attr);
    }

    if (role_id == Attribute::UIDataModels && attr) {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        auto groups = attr->get_role_data<std::vector<std::string>>(Attribute::UIDataModels);

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

    if (dock_widget_attributes_.find(attr_uuid) != dock_widget_attributes_.end() &&
        role_id == module::Attribute::Activated) {

        // this happens when the user has clicked on the button that shows/hides
        // a dockable toolbar (e.g. annotation tools) OR when a toolbar goes
        // offscreen
        module::Attribute *attr = get_attribute(attr_uuid);
        int activated           = 0;
        std::string name;
        try {
            activated = attr->get_role_data<int64_t>(module::Attribute::Activated);
            name      = attr->get_role_data<std::string>(module::Attribute::Title);
        } catch (...) {
        }

        if (activated == 1) {
            viewport_dockable_widget_activated(name);
        } else if (activated == 0) {
            viewport_dockable_widget_deactivated(name);
        }

        // silently set the 'Activated' role data to -1 so the UI can set it
        // to 0 or 1 (again) and we still get notification
        attr->set_role_data(module::Attribute::Activated, -1, false);
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
    const std::string sequence,
    const std::string &hotkey_name,
    const std::string &description,
    const bool auto_repeat,
    const std::string &component) {
    try {

        int key, modifier;
        ui::Hotkey::sequence_to_key_and_modifier(sequence, key, modifier);
        return register_hotkey(key, modifier, hotkey_name, description, auto_repeat, component);

    } catch (std::exception &e) {
        spdlog::warn("{} : {} {}", name(), __PRETTY_FUNCTION__, e.what());
    }

    return utility::Uuid();
}

utility::Uuid Module::register_hotkey(
    int default_keycode,
    int default_modifier,
    const std::string &hotkey_name,
    const std::string &description,
    const bool auto_repeat,
    const std::string &component) {
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
            "any", // window
            auto_repeat,
            caf::actor_cast<caf::actor_addr>(self()));

        anon_send(keypress_monitor_actor_, ui::keypress_monitor::register_hotkey_atom_v, hk);

        return hk.uuid();

    } else {
        spdlog::warn(
            "{} Attempt to register hotkey {} in module {} before module has been initialised "
            "with a parent actor",
            __PRETTY_FUNCTION__,
            hotkey_name,
            name());
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

void Module::connect_to_ui() {

    // if necessary, get the global module events actor and the associated events groups
    if ((!keypress_monitor_actor_ || !keyboard_and_mouse_group_) && self()) {

        try {

            scoped_actor sys{self()->home_system()};

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
        } else {
            auto b = caf::actor_cast<caf::blocking_actor *>(self());
            if (b) {
                join_broadcast(b, keyboard_and_mouse_group_);
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

        if (a->has_role_data(Attribute::UIDataModels)) {
            auto groups = a->get_role_data<std::vector<std::string>>(Attribute::UIDataModels);
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

    if (keyboard_and_mouse_group_ && self()) {
        // got to be a better way of doing this?
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a) {
            leave_broadcast(a, keyboard_and_mouse_group_);
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

        // Note: Ted, May 2024 - commenting out the below. If a module (like the
        // playhead) is disconnected from the UI (because it's not being viewed
        // in a viewport) we still want it's attributes to be live in the front
        // end because things like a timeline that might not be in the viewport
        // but active in the UI needs to know where the playhead is etc.

        // tell the UI middleman to remove our attributes from its data models
        /*auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        for (auto &a : attributes_) {

            if (a->has_role_data(Attribute::UIDataModels)) {
                auto groups =
        a->get_role_data<std::vector<std::string>>(Attribute::UIDataModels); for (const auto
        &group_name : groups) { anon_send( central_models_data_actor,
                        ui::model_data::deregister_model_data_atom_v,
                        group_name,
                        a->uuid(),
                        self());
                }
            }
        }*/
    }

    keyboard_and_mouse_group_ = caf::actor();

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

                attr->set_role_data(Attribute::Value, change, true);

            } catch (std::exception &e) {
                spdlog::warn("{} failed to set preference {}", __PRETTY_FUNCTION__, e.what());
            }
        } else if (
            attr->has_role_data(Attribute::InitOnlyPreferencePath) &&
            (attr->get_role_data<std::string>(Attribute::InitOnlyPreferencePath) + "/value") ==
                path) {

            try {

                attr->set_role_data(Attribute::Value, change, true);
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

                auto pref_path = attr->get_role_data<std::string>(Attribute::PreferencePath);

                try {

                    if (entire_prefs_dict.get(pref_path + "/value").is_null()) {

                        // if get "/default_value" throws an exception, we silence
                        // the warning as it means we have a preference that isn't
                        // forward declared in the default prefs files, and that
                        // is ok for module preferences as these can have a
                        // preference path for saving their value without the
                        // pref being declared in the .json pref files that
                        // are part of the codebase
                        std::ignore = entire_prefs_dict.get(pref_path + "/default_value");
                    }

                } catch (...) {
                    continue;
                }

                auto pref_value = global_store::preference_value<nlohmann::json>(
                    entire_prefs_dict, pref_path);
                attr->set_role_data(Attribute::Value, pref_value, true);

            } catch (std::exception &e) {
                spdlog::warn(
                    "{} failed to set preference {} : attr - {}",
                    __PRETTY_FUNCTION__,
                    e.what(),
                    attr->role_data_as_json(Attribute::PreferencePath).dump());
            }
        } else if (attr->has_role_data(Attribute::InitOnlyPreferencePath)) {

            try {
                auto pref_path =
                    attr->get_role_data<std::string>(Attribute::InitOnlyPreferencePath);

                if (!entire_prefs_dict.get(pref_path + "/value").is_null() ||
                    !entire_prefs_dict.get(pref_path + "/default_value").is_null()) {

                    auto pref_value = global_store::preference_value<nlohmann::json>(
                        entire_prefs_dict, pref_path);

                    attr->set_role_data(Attribute::Value, pref_value, false);

                    // wipe the 'InitOnlyPreferencePath' data so we never update again
                    // when preferences are updated
                    attr->delete_role_data(Attribute::InitOnlyPreferencePath);
                }

            } catch (std::exception &e) {
                spdlog::warn(
                    "{} failed to set preference {} : attr2 - {}",
                    __PRETTY_FUNCTION__,
                    e.what(),
                    attr->role_data_as_json(Attribute::PreferencePath).dump());
            }
        }
    }
}

void Module::add_multichoice_attr_to_menu(
    StringChoiceAttribute *attr,
    const std::string top_level_menu,
    const std::string menu_path,
    const std::string before) {
    // RESKIN DEPRECATE
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
    // RESKIN DEPRECATE
    std::vector<std::string> menu_paths;
    if (attr->has_role_data(module::Attribute::MenuPaths)) {
        menu_paths =
            attr->get_role_data<std::vector<std::string>>(module::Attribute::MenuPaths);
    }
    menu_paths.push_back(top_level_menu);
    attr->set_role_data(module::Attribute::MenuPaths, nlohmann::json(menu_paths));
}

utility::JsonStore Module::attribute_menu_item_data(Attribute *attr) {
    // for an attribute that is exposed in a UI menu, build a json dict that
    // describes that attribute data required for the menu model

    utility::JsonStore menu_item_data;
    if (attr->get_role_data<std::string>(Attribute::Type) == "ComboBox") {
        menu_item_data["current_choice"] = attr->get_role_data<std::string>(Attribute::Value);
        /*auto choices = nlohmann::json::parse("[]");
        for (const auto &c : multi_choice->options()) {
            choices.insert(choices.begin() + choices.size(), 1, c);
        }*/
        menu_item_data["choices"] =
            attr->get_role_data<std::vector<std::string>>(Attribute::StringChoices);
        menu_item_data["menu_item_type"] = "multichoice";

        if (attr->has_role_data(Attribute::StringChoicesIds)) {
            menu_item_data["choices_ids"] =
                attr->get_role_data<std::vector<utility::Uuid>>(Attribute::StringChoicesIds);
        }

    } else if (attr->get_role_data<std::string>(Attribute::Type) == "RadioGroup") {
        menu_item_data["current_choice"] = attr->get_role_data<std::string>(Attribute::Value);
        /*auto choices = nlohmann::json::parse("[]");
        for (const auto &c : multi_choice->options()) {
            choices.insert(choices.begin() + choices.size(), 1, c);
        }*/
        menu_item_data["choices"] =
            attr->get_role_data<std::vector<std::string>>(Attribute::StringChoices);
        menu_item_data["menu_item_type"] = "radiogroup";

        if (attr->has_role_data(Attribute::StringChoicesIds)) {
            menu_item_data["choices_ids"] =
                attr->get_role_data<std::vector<utility::Uuid>>(Attribute::StringChoicesIds);
        }

    } else if (attr->get_role_data<std::string>(Attribute::Type) == "OnOffToggle") {
        menu_item_data["is_checked"]     = attr->get_role_data<bool>(Attribute::Value);
        menu_item_data["menu_item_type"] = "toggle";
    } else {
        throw std::runtime_error("Attribute type not suitable for menu control.");
    }
    menu_item_data["uuid"]              = attr->uuid();
    menu_item_data["menu_item_enabled"] = attr->has_role_data(Attribute::Enabled)
                                              ? attr->get_role_data<bool>(Attribute::Enabled)
                                              : true;
    return menu_item_data;
}

utility::Uuid Module::insert_menu_item(
    const std::string &menu_model_name,
    const std::string &menu_text,
    const std::string &menu_path,
    const float menu_item_position,
    Attribute *attr,
    const bool divider,
    const utility::Uuid &hotkey,
    const std::string &user_data) {
    try {
        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        utility::JsonStore menu_item_data;
        if (attr) {
            menu_item_data = attribute_menu_item_data(attr);
            // For now using this 'RESKIN' dummy token to stop menus from 'old' skin
            // messing things up for the reskin. Alternatively (hack alert) if we
            // want to use the value of the attribute itself to set the menu item
            // name we start the menu path with USE_ATTR_VALUE
            std::string new_menu_path;
            if (menu_text == "USE_ATTR_VALUE") {
                new_menu_path = "USE_ATTR_VALUE|" + menu_model_name + "|" + menu_path + "|" +
                                attr->get_role_data<std::string>(Attribute::Value);
            } else {
                new_menu_path = "RESKIN|" + menu_model_name + "|" + menu_path + "|" + menu_text;
            }
            if (attr->has_role_data(Attribute::MenuPaths)) {
                auto paths =
                    attr->get_role_data<std::vector<std::string>>(Attribute::MenuPaths);
                paths.push_back(new_menu_path);
                attr->set_role_data(Attribute::MenuPaths, paths);
            } else {
                attr->set_role_data(
                    Attribute::MenuPaths, std::vector<std::string>({new_menu_path}));
            }
        } else {
            // a menu item that is not linked by to an attribute - it simply
            // has a uuid which, when the user clicks on the menu item, we run
            // a callback to menu_item_activated virtual method
            menu_item_data["menu_item_type"] = divider ? "divider" : "button";
            menu_item_data["uuid"]           = utility::Uuid::generate();
        }

        if (attr && menu_text == "USE_ATTR_VALUE") {
            menu_item_data["name"] = attr->get_role_data<std::string>(Attribute::Value);
        } else if (menu_text != "") {
            menu_item_data["name"] = menu_text;
        }

        menu_item_data["menu_item_position"] = menu_item_position;
        if (!hotkey.is_null()) {
            menu_item_data["hotkey_uuid"] = hotkey;
        }
        if (!user_data.empty()) {
            menu_item_data["user_data"] = user_data;
        }

        if (!menu_item_data.contains("menu_item_enabled")) {
            menu_item_data["menu_item_enabled"] = true;
        }
        menu_item_data["menu_item_context"] = std::string();

        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a) {
            a->send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                menu_model_name,
                menu_path,
                menu_item_data,
                self());
        } else {
            anon_send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                menu_model_name,
                menu_path,
                menu_item_data,
                self());
        }

        menu_items_[menu_model_name].push_back(menu_item_data["uuid"]);
        return menu_item_data["uuid"];

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return utility::Uuid();
}

utility::Uuid Module::insert_hotkey_into_menu(
    const std::string &menu_model_name,
    const std::string &menu_path,
    const float menu_item_position,
    const utility::Uuid &hotkey) {
    try {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        utility::JsonStore menu_item_data;
        menu_item_data["menu_item_type"]     = "button";
        auto menu_item_id                    = utility::Uuid::generate();
        menu_item_data["uuid"]               = menu_item_id;
        menu_item_data["hotkey_uuid"]        = hotkey;
        menu_item_data["menu_item_position"] = menu_item_position;

        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a) {
            a->send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                menu_model_name,
                menu_path,
                menu_item_data,
                self());
        } else {
            anon_send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                menu_model_name,
                menu_path,
                menu_item_data,
                self());
        }

        menu_items_[menu_model_name].push_back(menu_item_id);
        return menu_item_id;

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return utility::Uuid();
}

void Module::remove_all_menu_items(const std::string &menu_model_name) {
    if (menu_items_.find(menu_model_name) != menu_items_.end()) {
        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        for (const auto &uuid : menu_items_[menu_model_name]) {
            anon_send(
                central_models_data_actor,
                ui::model_data::remove_node_atom_v,
                menu_model_name,
                uuid);
        }
    }
}

void Module::update_attribute_menu_item_data(Attribute *attr) {

    try {
        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        utility::JsonStore menu_item_data = attribute_menu_item_data(attr);

        const auto paths = attr->get_role_data<std::vector<std::string>>(Attribute::MenuPaths);
        for (const auto &path : paths) {
            auto sections = utility::split(path, '|');
            if (sections.size() >= 2 &&
                (sections.front() == "RESKIN" || sections.front() == "USE_ATTR_VALUE")) {

                bool use_val = sections.front() == "USE_ATTR_VALUE";
                sections.erase(sections.begin());

                // when an attribute is first exposed in a menu, we add a string to the
                // MenuPaths attribute data ... at the front of the string is
                // the menu model name (i.e. the name of the set of data that
                // is driving a particular pop-up menu or menu bar), at the
                // back is the menu item name (i.e. what is displayed in the
                // menu) and the middle is the 'menu path' which is this list
                // of sub-menu names under which the menu item is inserted.

                if (use_val) {
                    menu_item_data["name"] = attr->get_role_data<std::string>(Attribute::Value);
                } else {
                    menu_item_data["name"] = sections.back();
                }
                std::string menu_path;
                for (size_t i = 1; i < (sections.size() - 1); ++i) {
                    menu_path = sections[i] + (i == (sections.size() - 1) ? "" : "|");
                }

                anon_send(
                    central_models_data_actor,
                    ui::model_data::insert_or_update_menu_node_atom_v,
                    sections.front(), // menu model name
                    menu_path,
                    menu_item_data,
                    self());
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, e.what(), attr->as_json().dump());
    }
}

void Module::remove_menu_item(const std::string &menu_model_name, const utility::Uuid item_id) {
    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    anon_send(
        central_models_data_actor,
        ui::model_data::remove_node_atom_v,
        menu_model_name,
        item_id);
}

utility::Uuid Module::insert_menu_divider(
    const std::string &menu_model_name,
    const std::string &menu_path,
    const float menu_item_position) {
    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    utility::JsonStore menu_item_data;

    menu_item_data["menu_item_position"] = menu_item_position;
    menu_item_data["menu_item_type"]     = "divider";
    menu_item_data["uuid"]               = utility::Uuid::generate();

    anon_send(
        central_models_data_actor,
        ui::model_data::insert_or_update_menu_node_atom_v,
        menu_model_name,
        menu_path,
        menu_item_data,
        self());

    return menu_item_data["uuid"];
}

void Module::set_submenu_position_in_parent(
    const std::string &menu_model_name,
    const std::string &submenu,
    const float submenu_position) {

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    anon_send(
        central_models_data_actor,
        ui::model_data::insert_or_update_menu_node_atom_v,
        menu_model_name,
        submenu,
        submenu_position);
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

            for (const auto &viewport_name : connected_viewport_names_) {

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
            for (const auto &viewport_name : connected_viewport_names_) {

                std::string toolbar_name = viewport_name + "_toolbar";
                attr->expose_in_ui_attrs_group(toolbar_name, false);

                anon_send(
                    central_models_data_actor,
                    ui::model_data::deregister_model_data_atom_v,
                    toolbar_name,
                    attr->uuid(),
                    caf::actor());
            }
        }
    }
}

void Module::redraw_viewport() {
    for (auto &vp : connected_viewports_) {
        anon_send(vp, playhead::redraw_viewport_atom_v);
    }
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

    attr->expose_in_ui_attrs_group(model_name, expose);
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
                ui::model_data::deregister_model_data_atom_v,
                model_name,
                attr->uuid(),
                self());
        }
    } catch (std::exception &) {
    }
}

void Module::connect_to_viewport(
    const std::string &viewport_name,
    const std::string &viewport_toolbar_name,
    bool connect,
    caf::actor viewport) {

    if (connect) {
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a)
            a->monitor(viewport);
        connected_viewport_names_.insert(viewport_name);
        connected_viewports_.insert(viewport);
    } else if (
        connected_viewport_names_.find(viewport_name) != connected_viewport_names_.end()) {
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        if (a)
            a->demonitor(viewport);
        connected_viewport_names_.erase(connected_viewport_names_.find(viewport_name));
        connected_viewports_.erase(connected_viewports_.find(viewport));
    }

    for (const auto &toolbar_attr_id : attrs_in_toolbar_) {
        Attribute *attr = get_attribute(toolbar_attr_id);
        if (attr) {
            expose_attribute_in_model_data(attr, viewport_toolbar_name, connect);
        }
    }
}

void Module::add_attribute(Attribute *attr) {
    attributes_.emplace_back(attr);
    attr->set_owner(this);
}

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

void Module::register_ui_panel_qml(
    const std::string &panel_name,
    const std::string &qml_code,
    const float position_in_menu,
    const std::string &viewport_popout_button_icon,
    const float &viewport_popout_button_position,
    const utility::Uuid toggle_hotkey_id) {

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    utility::JsonStore data;
    data["view_name"]       = panel_name;
    data["view_qml_source"] = qml_code;
    data["position"]        = position_in_menu;
    /*anon_send(
        central_models_data_actor,
        ui::model_data::insert_rows_atom_v,
        "views model", // the model called 'views model' is what's used to build the panels menu
        "/", // add to root
        data,
        0, // row
        1, // count
        caf::actor());*/

    scoped_actor sys{self()->home_system()};
    try {

        anon_send(
            central_models_data_actor,
            ui::model_data::insert_rows_atom_v,
            "view widgets", // the model called 'view widgets' is what's used to build the
                            // panels menu
            "",             // (path) add to root
            0,              // row
            1,              // count
            data);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (!viewport_popout_button_icon.empty() && viewport_popout_button_position != -1.0f) {

        utility::JsonStore data;
        data["view_name"]         = panel_name;
        data["icon_path"]         = viewport_popout_button_icon;
        data["view_qml_source"]   = qml_code;
        data["button_position"]   = viewport_popout_button_position;
        data["window_is_visible"] = false;
        data["hotkey_uuid"]       = toggle_hotkey_id;

        try {

            anon_send(
                central_models_data_actor,
                ui::model_data::insert_rows_atom_v,
                "popout windows", // the model called 'view widgets' is what's used to build the
                                  // panels menu
                "",               // (path) add to root
                0,                // row
                1,                // count
                data);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

Attribute *Module::register_viewport_dockable_widget(
    const std::string &widget_name,
    const std::string &button_icon_qrc_path,
    const std::string &button_tooltip,
    const float button_position,
    const bool enabled,
    const std::string &left_right_dockable_widget_qml,
    const std::string &top_bottom_dockable_widget_qml,
    const utility::Uuid toggle_widget_visible_hotkey) {

    auto attr = new QmlCodeAttribute(widget_name, left_right_dockable_widget_qml);
    attr->set_role_data(Attribute::IconPath, button_icon_qrc_path);
    attr->set_role_data(Attribute::ToolbarPosition, button_position);
    attr->set_role_data(Attribute::ToolTip, button_tooltip);
    attr->set_role_data(Attribute::Activated, -1);
    attr->set_role_data(Attribute::Enabled, enabled);

    if (!left_right_dockable_widget_qml.empty()) {
        attr->set_role_data(
            Attribute::LeftRightDockWidgetQmlCode, left_right_dockable_widget_qml);
    }
    if (!top_bottom_dockable_widget_qml.empty()) {
        attr->set_role_data(
            Attribute::TopBottomDockWidgetQmlCode, top_bottom_dockable_widget_qml);
    }

    if (!toggle_widget_visible_hotkey.is_null()) {
        attr->set_role_data(Attribute::HotkeyUuid, toggle_widget_visible_hotkey);
    }
    add_attribute(static_cast<Attribute *>(attr));
    expose_attribute_in_model_data(attr, "dockable viewport toolboxes", true);
    dock_widget_attributes_.insert(attr->uuid());
    return attr;
}

void Module::register_singleton_qml(const std::string &qml_code) {

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    utility::JsonStore data;
    data["source"] = qml_code;

    scoped_actor sys{self()->home_system()};
    try {

        anon_send(
            central_models_data_actor,
            ui::model_data::insert_rows_atom_v,
            "singleton items", // the model called 'singleton items' is what's used to build the
                               // singleton
            "",                // (path) add to root
            0,                 // row
            1,                 // count
            data);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}