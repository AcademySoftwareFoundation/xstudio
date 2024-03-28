// SPDX-License-Identifier: Apache-2.0
#include "ocio.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio;


void OCIOColourPipeline::media_source_changed(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {
    // If the OCIO config for the new media is new, we need to update the UI
    const MediaParams old_media_param = get_media_params(current_source_uuid_);
    const MediaParams new_media_param = get_media_params(source_uuid, colour_params);

    const bool need_ui_update =
        not current_source_uuid_ or
        current_source_media_params_.ocio_config_name != new_media_param.ocio_config_name;

    if (need_ui_update) {
        populate_ui(new_media_param);
    }

    current_source_uuid_         = source_uuid;
    current_config_name_         = new_media_param.ocio_config_name;
    current_source_media_params_ = new_media_param;

    // Extract the input colorspace as detected by the plugin and update the UI
    std::string detected_cs;

    OCIO::TransformRcPtr transform = source_transform(new_media_param);
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_COLORSPACE) {
        OCIO::ColorSpaceTransformRcPtr csc =
            std::static_pointer_cast<OCIO::ColorSpaceTransform>(transform);
        detected_cs = csc->getSrc();
    } else if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_DISPLAY_VIEW) {
        // TODO: ColSci
        // Note that in case the view uses looks, the colour space returned here
        // is not representing the input processing accurately and will be out
        // of sync with what the input transform actually is (inverse display view).
        OCIO::DisplayViewTransformRcPtr disp =
            std::static_pointer_cast<OCIO::DisplayViewTransform>(transform);
        const std::string view_cs = new_media_param.ocio_config->getDisplayViewColorSpaceName(
            disp->getDisplay(), disp->getView());
        detected_cs = view_cs;
    } else {
        spdlog::warn(
            "OCIOColourPipeline: Internal error trying to extract source colour space.");
    }

    // Do not notify to avoid this being interpreted as a manual user selection.
    if (not detected_cs.empty()) {
        source_colour_space_->set_value(
            new_media_param.ocio_config->getCanonicalName(detected_cs.c_str()), false);
    }

    // Update the per media assigned view
    if (!global_view_->value()) {
        // When the main viewport gets the event and change the view here,
        // it will be propagated to the popout viewer because the view_
        // attribute is linked accross viewports. If the popout viewport
        // hasn't got the source change event, or didn't process it yet,
        // it might receive the view_ attribute_changed event and go on
        // to update the per media parameters with the new view for the
        // wrong media. This then cause a mix up of view assigned to
        // the incorrect media.
        // Hence we make sure to not notify the change here.
        view_->set_value(new_media_param.output_view, false);
    }

    // Update the assigned source colour space depending on the current view
    if (adjust_source_->value()) {
        update_cs_from_view(new_media_param, view_->value());
    }
}

void OCIOColourPipeline::attribute_changed(
    const utility::Uuid &attribute_uuid, const int role) {

    MediaParams media_param = get_media_params(current_source_uuid_);

    if (attribute_uuid == display_->uuid()) {
        update_views(media_param.ocio_config);
    } else if (attribute_uuid == view_->uuid() && !view_->value().empty()) {
        if (!global_view_->value()) {
            media_param.output_view = view_->value();
            set_media_params(media_param);
        }
        if (adjust_source_->value()) {
            update_cs_from_view(media_param, view_->value());
        }
    } else if (attribute_uuid == source_colour_space_->uuid()) {
        media_param.user_input_cs = source_colour_space_->value();
        set_media_params(media_param);
    } else if (attribute_uuid == colour_bypass_->uuid()) {
        update_bypass(display_, colour_bypass_->value());
    } else if (
        exposure_ && attribute_uuid == exposure_->uuid() ||
        gamma_ && attribute_uuid == gamma_->uuid() ||
        saturation_ && attribute_uuid == saturation_->uuid()) {
        redraw_viewport();
    } else if (attribute_uuid == preferred_view_->uuid()) {
        bool enable_global = preferred_view_->value() != ui_text_.AUTOMATIC_VIEW;
        global_view_->set_value(enable_global, false);
    } else if (attribute_uuid == enable_gamma_->uuid()) {

        make_attribute_visible_in_viewport_toolbar(gamma_, enable_gamma_->value());

    } else if (attribute_uuid == enable_saturation_->uuid()) {

        make_attribute_visible_in_viewport_toolbar(saturation_, enable_saturation_->value());
    }
}

void OCIOColourPipeline::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context) {

    // If user hits 'R' hotkey and we're already looking at the red channel,
    // then we revert back to RGB, same for 'G' and 'B'.
    auto p = channel_hotkeys_.find(hotkey_uuid);
    if (p != channel_hotkeys_.end()) {
        if (channel_->value() == p->second) {
            channel_->set_value("RGB");
        } else {
            channel_->set_value(p->second);
        }
    } else if (hotkey_uuid == reset_hotkey_) {
        channel_->set_value("RGB");
        exposure_->set_value(exposure_->get_role_data<float>(module::Attribute::DefaultValue));
        gamma_->set_value(gamma_->get_role_data<float>(module::Attribute::DefaultValue));
        saturation_->set_value(
            saturation_->get_role_data<float>(module::Attribute::DefaultValue));
    } else if (hotkey_uuid == exposure_hotkey_) {
        exposure_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    } else if (hotkey_uuid == gamma_hotkey_) {
        gamma_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    } else if (hotkey_uuid == saturation_hotkey_) {
        saturation_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    }
}

void OCIOColourPipeline::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == exposure_hotkey_) {
        exposure_->set_role_data(module::Attribute::Activated, false);
        release_mouse_focus();
    } else if (hotkey_uuid == gamma_hotkey_) {
        gamma_->set_role_data(module::Attribute::Activated, false);
        release_mouse_focus();
    } else if (hotkey_uuid == saturation_hotkey_) {
        saturation_->set_role_data(module::Attribute::Activated, false);
        release_mouse_focus();
    }
}

bool OCIOColourPipeline::pointer_event(const ui::PointerEvent &e) {

    module::FloatAttribute *active_attr = nullptr;
    if (exposure_->get_role_data<bool>(module::Attribute::Activated)) {
        active_attr = exposure_;
    } else if (gamma_->get_role_data<bool>(module::Attribute::Activated)) {
        active_attr = gamma_;
    } else if (saturation_->get_role_data<bool>(module::Attribute::Activated)) {
        active_attr = saturation_;
    }

    // Nothing to be done
    if (!active_attr) {
        return false;
    }

    // Implementing exposure scrubbing in viewport
    static int x_down;
    static float e_down;
    static auto dragging = false;
    bool used            = false;

    if (e.type() == ui::Signature::EventType::ButtonDown &&
        e.buttons() == ui::Signature::Left) {
        x_down   = e.x();
        e_down   = active_attr->value();
        dragging = true;
        used     = true;
    } else if (dragging && e.buttons() == ui::Signature::Left) {
        const auto sensitivity =
            active_attr->get_role_data<float>(module::Attribute::FloatScrubSensitivity);
        const auto step = active_attr->get_role_data<float>(module::Attribute::FloatScrubStep);
        const auto min  = active_attr->get_role_data<float>(module::Attribute::FloatScrubMin);
        const auto max  = active_attr->get_role_data<float>(module::Attribute::FloatScrubMax);

        auto val = 0.0f;
        val      = round((e_down + (e.x() - x_down) * sensitivity) / step) * step;
        val      = std::max(std::min(val, max), min);
        active_attr->set_value(val);
        used = true;
    } else if (
        e.type() == ui::Signature::EventType::ButtonRelease &&
        (e.buttons() & ui::Signature::Left)) {
        dragging = false;
        used     = true;
    }

    if (e.type() == ui::Signature::EventType::DoubleClick) {
        static auto last_value = 0.0f;
        const auto def = active_attr->get_role_data<float>(module::Attribute::DefaultValue);

        if (active_attr->value() == def) {
            active_attr->set_value(last_value);
        } else {
            last_value = active_attr->value();
            active_attr->set_value(def);
        }
        used = true;
    }

    return used;
}

void OCIOColourPipeline::screen_changed(
    const std::string &name,
    const std::string &model,
    const std::string &manufacturer,
    const std::string &serialNumber) {

    const MediaParams media_param  = get_media_params(current_source_uuid_);
    const std::string monitor_name = manufacturer + " " + model;
    const std::string display      = default_display(media_param, monitor_name);

    auto menu_populated = [](module::StringChoiceAttribute *attr) {
        return attr->get_role_data<std::vector<std::string>>(module::Attribute::StringChoices)
                   .size() > 0;
    };

    if (menu_populated(display_)) {
        display_->set_value(display);
    }
    monitor_name_ = monitor_name;
}

void OCIOColourPipeline::connect_to_viewport(
    const std::string &viewport_name, const std::string &viewport_toolbar_name, bool connect) {

    Module::connect_to_viewport(viewport_name, viewport_toolbar_name, connect);

    if (viewport_name == "viewport0") {
        // this is the OCIO actor for the main viewport... we register ourselves
        // so other OCIO actors can talk to us
        system().registry().put("MAIN_VIEWPORT_OCIO_INSTANCE", this);
    }

    add_multichoice_attr_to_menu(
        display_, viewport_name + "_context_menu_section2", "OCIO Display");

    add_multichoice_attr_to_menu(view_, viewport_name + "_context_menu_section2", "OCIO View");

    add_multichoice_attr_to_menu(channel_, viewport_name + "_context_menu_section1", "Channel");

    if (viewport_name == "viewport0") {

        add_multichoice_attr_to_menu(view_, "Colour", "OCIO View");

        add_multichoice_attr_to_menu(display_, "Colour", "OCIO Display");

        add_multichoice_attr_to_menu(channel_, "Colour", "Channel");

        add_boolean_attr_to_menu(colour_bypass_, "Colour");

        add_boolean_attr_to_menu(global_view_, "Colour");

        add_boolean_attr_to_menu(adjust_source_, "Colour");

        add_multichoice_attr_to_menu(source_colour_space_, "Colour", "Source Colour Space");

        add_multichoice_attr_to_menu(preferred_view_, "Colour", "OCIO Preferred View");

        add_boolean_attr_to_menu(enable_saturation_, "panels_menu|Toolbar");

        add_boolean_attr_to_menu(enable_gamma_, "panels_menu|Toolbar");

    } else if (viewport_name == "viewport1") {

        add_multichoice_attr_to_menu(display_, "Colour", "OCIO Pop-Out Viewer Display");
    }
}

void OCIOColourPipeline::setup_ui() {
    // OCIO Source colour space

    source_colour_space_ =
        add_string_choice_attribute(ui_text_.SOURCE_CS, ui_text_.SOURCE_CS_SHORT);
    source_colour_space_->set_redraw_viewport_on_change(true);
    source_colour_space_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    source_colour_space_->set_role_data(module::Attribute::Enabled, false);
    source_colour_space_->set_role_data(module::Attribute::ToolbarPosition, 12.0f);
    source_colour_space_->set_role_data(module::Attribute::ToolTip, ui_text_.SOURCE_CS_TOOLTIP);

    // OCIO display selection (main viewer)

    display_ = add_string_choice_attribute(ui_text_.DISPLAY, ui_text_.DISPLAY_SHORT);
    display_->set_redraw_viewport_on_change(true);
    display_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    display_->set_role_data(module::Attribute::Enabled, true);
    display_->set_role_data(module::Attribute::ToolbarPosition, 10.0f);
    display_->set_role_data(module::Attribute::ToolTip, ui_text_.DISPLAY_TOOLTIP);

    // OCIO view selection

    view_ = add_string_choice_attribute(ui_text_.VIEW, ui_text_.VIEW);
    view_->set_redraw_viewport_on_change(true);
    view_->set_role_data(module::Attribute::Enabled, true);
    view_->set_role_data(module::Attribute::ToolbarPosition, 11.0f);
    view_->set_role_data(module::Attribute::ToolTip, ui_text_.VIEW_TOOLTIP);

    // Preferred view

    preferred_view_ = add_string_choice_attribute(
        ui_text_.PREF_VIEW, ui_text_.PREF_VIEW, ui_text_.DEFAULT_VIEW);
    preferred_view_->set_redraw_viewport_on_change(true);
    preferred_view_->set_role_data(
        module::Attribute::UuidRole, "06f57aaa-0be8-47ab-ac65-fb742bda410b");
    preferred_view_->set_role_data(module::Attribute::Enabled, true);
    preferred_view_->set_role_data(module::Attribute::ToolbarPosition, 11.0f);
    preferred_view_->set_role_data(module::Attribute::ToolTip, ui_text_.PREF_VIEW_TOOLTIP);
    preferred_view_->set_role_data(
        module::Attribute::StringChoices, ui_text_.PREF_VIEW_OPTIONS, false);
    preferred_view_->set_preference_path("/plugin/colour_pipeline/ocio/user_preferred_view");

    // Hot channel selection

    channel_ = add_string_choice_attribute(
        ui_text_.CHANNEL,
        ui_text_.CHANNEL_SHORT,
        ui_text_.RGB,
        {ui_text_.RGB,
         ui_text_.RED,
         ui_text_.GREEN,
         ui_text_.BLUE,
         ui_text_.ALPHA,
         ui_text_.LUMINANCE},
        {ui_text_.RGB, ui_text_.R, ui_text_.G, ui_text_.B, ui_text_.A, ui_text_.L});
    channel_->set_redraw_viewport_on_change(true);
    channel_->set_role_data(module::Attribute::Enabled, true);
    channel_->set_role_data(module::Attribute::ToolbarPosition, 8.0f);
    channel_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SELECT_CLR_TIP);

    // Exposure slider

    exposure_ = add_float_attribute(
        ui_text_.EXPOSURE, ui_text_.EXPOSURE_SHORT, 0.0f, -10.0f, 10.0f, 0.05f);
    exposure_->set_redraw_viewport_on_change(true);
    exposure_->set_role_data(module::Attribute::ToolbarPosition, 4.0f);
    exposure_->set_role_data(module::Attribute::Activated, false);
    exposure_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    exposure_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SET_EXP_TIP);

    // Gamma slider

    gamma_ = add_float_attribute(ui_text_.GAMMA, ui_text_.GAMMA_SHORT, 1.0f, 0.0f, 5.0f, 0.05f);
    gamma_->set_redraw_viewport_on_change(true);
    gamma_->set_role_data(module::Attribute::ToolbarPosition, 4.1f);
    gamma_->set_role_data(module::Attribute::Activated, false);
    gamma_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    gamma_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SET_GAMMA_TIP);

    enable_gamma_ =
        add_boolean_attribute(ui_text_.ENABLE_GAMMA, ui_text_.ENABLE_GAMMA_SHORT, false);
    enable_gamma_->set_redraw_viewport_on_change(true);
    enable_gamma_->set_preference_path("/plugin/colour_pipeline/ocio/enable_gamma");

    // Saturation slider

    saturation_ = add_float_attribute(
        ui_text_.SATURATION, ui_text_.SATURATION_SHORT, 1.0f, 0.0f, 10.0f, 0.05f);
    saturation_->set_redraw_viewport_on_change(true);
    saturation_->set_role_data(module::Attribute::ToolbarPosition, 4.2f);
    saturation_->set_role_data(module::Attribute::Activated, false);
    saturation_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    saturation_->set_role_data(
        module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SET_SATURATION_TIP);

    enable_saturation_ = add_boolean_attribute(
        ui_text_.ENABLE_SATURATION, ui_text_.ENABLE_SATURATION_SHORT, false);
    enable_saturation_->set_redraw_viewport_on_change(true);
    enable_saturation_->set_preference_path("/plugin/colour_pipeline/ocio/enable_saturation");

    // Colour bypass

    colour_bypass_ = add_boolean_attribute(ui_text_.CMS_OFF, ui_text_.CMS_OFF_SHORT, false);

    colour_bypass_->set_redraw_viewport_on_change(true);
    colour_bypass_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    colour_bypass_->set_role_data(module::Attribute::Enabled, false);
    colour_bypass_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_BYPASS_TOOLTIP);

    // View mode

    global_view_ = add_boolean_attribute(ui_text_.VIEW_MODE, ui_text_.GLOBAL_VIEW_SHORT, false);

    global_view_->set_redraw_viewport_on_change(true);
    global_view_->set_role_data(
        module::Attribute::UuidRole, "ac970f58-3243-4533-8bcb-296849b58277");
    global_view_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    global_view_->set_role_data(module::Attribute::Enabled, false);
    global_view_->set_role_data(module::Attribute::ToolTip, ui_text_.GLOBAL_VIEW_TOOLTIP);
    global_view_->set_preference_path("/plugin/colour_pipeline/ocio/user_view_mode");

    // Source colour space mode

    adjust_source_ = add_boolean_attribute(ui_text_.SOURCE_CS_MODE, ui_text_.SOURCE_CS_MODE_SHORT, true);

    adjust_source_->set_redraw_viewport_on_change(true);
    adjust_source_->set_role_data(
        module::Attribute::UuidRole, "4eada6a9-7969-4b29-9476-ef8a9344096c");
    adjust_source_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    adjust_source_->set_role_data(module::Attribute::Enabled, false);
    adjust_source_->set_role_data(module::Attribute::ToolTip, ui_text_.SOURCE_CS_MODE_TOOLTIP);
    adjust_source_->set_preference_path("/plugin/colour_pipeline/ocio/user_source_mode");

    ui_initialized_ = true;

    make_attribute_visible_in_viewport_toolbar(exposure_);
    make_attribute_visible_in_viewport_toolbar(channel_);
    make_attribute_visible_in_viewport_toolbar(display_);
    make_attribute_visible_in_viewport_toolbar(view_);

    // Here we register particular attributes to be 'linked'. The main viewer and
    // the pop-out viewer have their own instances of this class. We want certain
    // attributes to always have the same value between these two instances. When
    // the pop-out viewport is created, its colour pipeline instance is 'linked'
    // to the colour pipeline belonging to the main viewport - any changes on one
    // of the attributes below that happens in one instance is immediately synced
    // to the corresponding attribute on the other instance.
    link_attribute(source_colour_space_->uuid());
    link_attribute(exposure_->uuid());
    link_attribute(channel_->uuid());
    link_attribute(view_->uuid());
    link_attribute(gamma_->uuid());
    link_attribute(saturation_->uuid());
    link_attribute(global_view_->uuid());
    link_attribute(adjust_source_->uuid());
    link_attribute(enable_gamma_->uuid());
    link_attribute(enable_saturation_->uuid());
}

void OCIOColourPipeline::register_hotkeys() {

    for (const auto &hotkey_props : ui_text_.channel_hotkeys) {
        auto hotkey_id = register_hotkey(
            hotkey_props.key,
            hotkey_props.modifier,
            hotkey_props.name,
            hotkey_props.description);

        channel_hotkeys_[hotkey_id] = hotkey_props.channel_name;
    }

    reset_hotkey_ = register_hotkey(
        int('R'),
        ui::ControlModifier,
        "Reset Colour Viewing Setting",
        "Resets viewer exposure and channel mode");

    exposure_hotkey_ = register_hotkey(
        int('E'),
        ui::NoModifier,
        "Exposure Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer exposure");

    gamma_hotkey_ = register_hotkey(
        int('Y'),
        ui::NoModifier,
        "Gamma Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer gamma");

    saturation_hotkey_ = register_hotkey(
        int('S'),
        ui::AltModifier,
        "Saturation Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer saturation");
}


void OCIOColourPipeline::populate_ui(const MediaParams &media_param) {

    const auto ocio_config      = media_param.ocio_config;
    const auto ocio_config_name = media_param.ocio_config_name;

    // Store the display/view settings for the current config that's
    // about to be switched out.
    if (not current_config_name_.empty() and current_config_name_ != ocio_config_name) {
        auto &settings   = per_config_settings_[current_config_name_];
        settings.display = display_->value();
        settings.view    = view_->value();
    }

    if (is_worker())
        return;

    std::map<std::string, std::vector<std::string>> display_views;
    const auto displays          = parse_display_views(ocio_config, display_views);
    const auto all_colour_spaces = parse_all_colourspaces(ocio_config);

    const std::string default_disp     = ocio_config->getDefaultDisplay();
    const std::string default_view     = ocio_config->getDefaultView(default_disp.c_str());
    const std::string default_input_cs = ocio_config->getCanonicalName(OCIO::ROLE_DEFAULT);

    // OCIO Source colour space (do not notify as it will be updated later)
    source_colour_space_->set_role_data(
        module::Attribute::StringChoices, all_colour_spaces, false);
    source_colour_space_->set_value(default_input_cs, false);

    // OCIO display list
    display_->set_role_data(module::Attribute::StringChoices, displays, false);

    // Restore settings for the config if we've already used it, else use defaults.
    std::string display;
    std::string popout_display;
    std::string view;

    auto it = per_config_settings_.find(ocio_config_name);

    if (it != per_config_settings_.end() and
        display_views.find(it->second.display) != display_views.end()) {
        display = it->second.display;
        view    = it->second.view;
    } else {

        display = default_display(media_param, monitor_name_);
        // Do not try to re-use view from other config to avoid case where
        // an unmanaged media with Raw view match a Raw view in an actual
        // OCIO config.
        view = default_view;

        // .. however, let's see if we can use the view setting from the main
        // viewport if there's a match (useful for 'quickview' windows)
        auto main_ocio =
            system().registry().template get<caf::actor>("MAIN_VIEWPORT_OCIO_INSTANCE");
        if (main_ocio && main_ocio != self()) {

            try {
                caf::scoped_actor sys(system());

                auto data = utility::request_receive<utility::JsonStore>(
                    *sys, main_ocio, module::attribute_value_atom_v, "View");

                if (data.is_string()) {
                    auto p = std::find(
                        display_views[display].begin(),
                        display_views[display].end(),
                        data.get<std::string>());
                    if (p != display_views[display].end()) {
                        view = data.get<std::string>();
                    }
                }
            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }
    }

    // Don't notify while current_source_uuid_ is not up to date.
    display_->set_value(display, false);
    view_->set_role_data(module::Attribute::StringChoices, display_views[display]);
    view_->set_value(view, false);
}

std::vector<std::string> OCIOColourPipeline::parse_display_views(
    OCIO::ConstConfigRcPtr ocio_config,
    std::map<std::string, std::vector<std::string>> &display_views) const {
    std::vector<std::string> displays;
    display_views.clear();

    for (int i = 0; i < ocio_config->getNumDisplays(); ++i) {
        const std::string display = ocio_config->getDisplay(i);
        displays.push_back(display);

        display_views[display] = std::vector<std::string>();
        for (int j = 0; j < ocio_config->getNumViews(display.c_str()); ++j) {
            const std::string view = ocio_config->getView(display.c_str(), j);
            display_views[display].push_back(view);
        }
    }

    return displays;
}

std::vector<std::string>
OCIOColourPipeline::parse_all_colourspaces(OCIO::ConstConfigRcPtr ocio_config) const {
    std::vector<std::string> colourspaces;

    for (int i = 0; i < ocio_config->getNumColorSpaces(); ++i) {
        const char *cs_name = ocio_config->getColorSpaceNameByIndex(i);
        if (cs_name && *cs_name) {
            colourspaces.emplace_back(cs_name);
        }
    }

    return colourspaces;
}

void OCIOColourPipeline::update_cs_from_view(const MediaParams &media_param, const std::string &view) {

    const auto new_cs = input_space_for_view(media_param, view_->value());

    if (!new_cs.empty() && new_cs != source_colour_space_->value()) {
        MediaParams update_media_param = media_param;
        update_media_param.user_input_cs = new_cs;
        set_media_params(update_media_param);

        source_colour_space_->set_value(new_cs, false);
    }
}

void OCIOColourPipeline::update_views(OCIO::ConstConfigRcPtr ocio_config) {

    if (is_worker())
        return;

    std::map<std::string, std::vector<std::string>> display_views;
    parse_display_views(ocio_config, display_views);

    const std::string new_display = display_->value();
    const auto new_views          = display_views[new_display];

    view_->set_role_data(module::Attribute::StringChoices, display_views[new_display]);
    view_->set_role_data(module::Attribute::AbbrStringChoices, display_views[new_display]);

    // Check whether the current view is available under the new display or not.
    const std::string curr_view = view_->value();
    bool has_curr_view =
        !new_views.empty() &&
        std::find(new_views.begin(), new_views.end(), curr_view) != new_views.end();
    if (!has_curr_view) {
        std::string default_view = ocio_config->getDefaultView(new_display.c_str());
        if (!default_view.empty()) {
            view_->set_value(default_view);
        }
    }
}

void OCIOColourPipeline::update_bypass(module::StringChoiceAttribute *viewer, bool bypass) {
    const bool enable_options = not colour_bypass_->value();
    const std::string text    = colour_bypass_->value() ? ui_text_.CMS_OFF_ICON : "";

    const std::size_t display_options_count =
        viewer->get_role_data<std::vector<std::string>>(module::Attribute::StringChoices)
            .size();
    const std::size_t view_options_count =
        view_->get_role_data<std::vector<std::string>>(module::Attribute::StringChoices).size();
    const std::size_t source_colourspace_options_count =
        source_colour_space_
            ->get_role_data<std::vector<std::string>>(module::Attribute::StringChoices)
            .size();

    const std::vector<bool> display_options(display_options_count, enable_options);
    viewer->set_role_data(module::Attribute::StringChoicesEnabled, display_options, false);
    viewer->set_role_data(module::Attribute::OverrideValue, text, false);

    const std::vector<bool> view_options(view_options_count, enable_options);
    view_->set_role_data(module::Attribute::StringChoicesEnabled, view_options, false);
    view_->set_role_data(module::Attribute::OverrideValue, text, false);

    const std::vector<bool> colourspace_options(
        source_colourspace_options_count, enable_options);
    source_colour_space_->set_role_data(
        module::Attribute::StringChoicesEnabled, colourspace_options, false);
}
