// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "xstudio/ui/viewport/hud_plugin.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui;
using namespace xstudio;

HUDPluginBase::HUDPluginBase(
    caf::actor_config &cfg, std::string name, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, name, init_settings) {

    enabled_ = add_boolean_attribute(name, name, true);
    enabled_->expose_in_ui_attrs_group("hud_element_toggles");
    // this means the 'settings' button will not be visible in the HUD pop-up menu against this
    // HUD tool. (See XsHudToolbarButton .. visibility on the settings button is hooked to
    // 'disabled_value' property that comes through via the model data)
    enabled_->set_role_data(module::Attribute::DisabledValue, true);

    message_handler_ = {[=](enable_hud_atom, bool enabled) {
        globally_enabled_ = enabled;
        redraw_viewport();
    }};
}

void HUDPluginBase::add_hud_settings_attribute(module::Attribute *attr) {
    attr->expose_in_ui_attrs_group(Module::name() + " Settings");
    // this means the 'settings' button WILL be visible!
    enabled_->set_role_data(module::Attribute::DisabledValue, false);
}

void HUDPluginBase::hud_element_qml(
    const std::string qml_code, const HUDElementPosition position) {
    auto attr = add_qml_code_attribute("OverlayCode", qml_code);

    if (position == BottomLeft) {
        attr->expose_in_ui_attrs_group("hud_elements_bottom_left");
    } else if (position == BottomCenter) {
        attr->expose_in_ui_attrs_group("hud_elements_bottom_center");
    } else if (position == BottomRight) {
        attr->expose_in_ui_attrs_group("hud_elements_bottom_right");
    } else if (position == TopLeft) {
        attr->expose_in_ui_attrs_group("hud_elements_top_left");
    } else if (position == TopCenter) {
        attr->expose_in_ui_attrs_group("hud_elements_top_center");
    } else if (position == TopRight) {
        attr->expose_in_ui_attrs_group("hud_elements_top_right");
    }
}