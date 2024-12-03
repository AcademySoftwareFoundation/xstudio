// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "xstudio/plugin_manager/hud_plugin.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::plugin;
using namespace xstudio;

HUDPluginBase::HUDPluginBase(
    caf::actor_config &cfg,
    std::string name,
    const utility::JsonStore &init_settings,
    const float toolbar_position)
    : plugin::StandardPlugin(cfg, name, init_settings) {

    hud_data_ = add_boolean_attribute(name, name, true);
    hud_data_->expose_in_ui_attrs_group("hud_element_toggles");

    // add a preference path using the plugin name so that the status of the
    // HUD plugin (on or off) is preserved between sessions
    static std::regex whitespace(R"(\s)");
    static std::regex specials(R"([^_a-zA-Z0-9])");
    plugin_underscore_name_ = std::regex_replace(utility::to_lower(name), whitespace, "_");
    plugin_underscore_name_ = std::regex_replace(plugin_underscore_name_, specials, "");

    hud_data_->set_preference_path(fmt::format("/plugin/{}/enabled", plugin_underscore_name_));

    // toolbar position is used to sort the items in the HUD menu pop-up in
    // the viewport toolbar
    hud_data_->set_role_data(
        module::Attribute::ToolbarPosition,
        init_settings.is_null()
            ? toolbar_position
            : init_settings.value("hud_menu_item_position", toolbar_position));

    // this means the 'settings' button will not be visible in the HUD pop-up menu against this
    // HUD tool. (See XsHudToolbarButton .. visibility on the settings button is hooked to
    // 'disabled_value' property that comes through via the model data)
    hud_data_->set_role_data(module::Attribute::DisabledValue, true);
    add_hud_settings_attribute(hud_data_);

    message_handler_ = {
        [=](ui::viewport::hud_settings_atom, bool hud_enabled) {
            globally_enabled_ = hud_enabled;
            redraw_viewport();
        },
        [=](ui::viewport::hud_settings_atom,
            const std::string qml_code,
            HUDElementPosition position) -> bool {
            hud_element_qml(qml_code, position);
            return true;
        }};
}

void HUDPluginBase::add_hud_settings_attribute(module::Attribute *attr) {
    attr->expose_in_ui_attrs_group(Module::name() + " Settings");
    // this means the 'settings' button WILL be visible!
    hud_data_->set_role_data(module::Attribute::DisabledValue, false);
}

void HUDPluginBase::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {
    if (hud_item_position_ && hud_item_position_->uuid() == attribute_uuid &&
        role == module::Attribute::Value) {

        std::vector<std::string> groups;
        if (hud_data_->has_role_data(module::Attribute::UIDataModels)) {

            auto central_models_data_actor =
                system().registry().template get<caf::actor>(global_ui_model_data_registry);

            // a bit ugly, but if the hud is in 'HUD Bottom Left', say, then
            // and user wants it 'HUD Top Right' then we have to erase 'Hud Bottom Left'
            // from the UIDataModels attribute role data but preserve other groups
            // that it might belong to
            static const auto positions = utility::map_value_to_vec(position_names_);
            groups                      = hud_data_->get_role_data<std::vector<std::string>>(
                module::Attribute::UIDataModels);
            auto p = groups.begin();
            while (p != groups.end()) {
                if (std::find(positions.begin(), positions.end(), *p) != positions.end()) {
                    // We need to tell the central models actor that the attribute
                    // is being taken out of the group (e.g. 'HUD Bottom Left')
                    anon_send(
                        central_models_data_actor,
                        ui::model_data::deregister_model_data_atom_v,
                        *p,
                        hud_data_->uuid(),
                        self());
                    p = groups.erase(p);
                } else {
                    p++;
                }
            }
        }
        groups.push_back(hud_item_position_->value());

        hud_data_->set_role_data(module::Attribute::UIDataModels, groups);
    }

    plugin::StandardPlugin::attribute_changed(attribute_uuid, role);
}

void HUDPluginBase::hud_element_qml(
    const std::string qml_code, const HUDElementPosition position) {

    hud_data_->set_role_data(module::Attribute::QmlCode, qml_code);

    if (position == FullScreen) {
        hud_data_->expose_in_ui_attrs_group("hud_elements_fullscreen");
        return;
    } else if (!hud_item_position_) {
        // add settings attribute for the position of the HUD element
        hud_item_position_ = add_string_choice_attribute(
            "Position On Screen",
            "Position On Screen",
            position_names_[position],
            utility::map_value_to_vec(position_names_));
        add_hud_settings_attribute(hud_item_position_);
        hud_item_position_->set_preference_path(
            fmt::format("/plugin/{}/hud_item_position", plugin_underscore_name_));
    }

    if (hud_item_position_) {
        attribute_changed(hud_item_position_->uuid(), module::Attribute::Value);
    }
}