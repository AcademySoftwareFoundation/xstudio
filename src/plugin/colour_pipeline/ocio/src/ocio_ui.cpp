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

    if (not detected_cs.empty()) {
        // Do not notify to avoid this being interpreted as a manual user selection.
        source_colour_space_->set_value(
            new_media_param.ocio_config->getCanonicalName(detected_cs.c_str()), false);
    }
}

void OCIOColourPipeline::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/) {
    // Assume that exposure does not require LUT rebuild in any circumstance.
    if (exposure_ && attribute_uuid != exposure_->uuid()) {
        std::scoped_lock lock(pipeline_cache_mutex_);
        pipeline_cache_.clear();
    }

    MediaParams media_param = get_media_params(current_source_uuid_);

    if (attribute_uuid == display_->uuid()) {
        update_views(media_param.ocio_config);
    } else if (attribute_uuid == source_colour_space_->uuid()) {
        media_param.user_input_cs = source_colour_space_->value();
        set_media_params(current_source_uuid_, media_param);
    } else if (attribute_uuid == colour_bypass_->uuid()) {
        update_bypass(display_, colour_bypass_->value());
        update_bypass(popout_viewer_display_, colour_bypass_->value());
    } else if (exposure_ && attribute_uuid == exposure_->uuid()) {
        redraw_viewport();
    }
}

void OCIOColourPipeline::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
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
        exposure_->set_value(0.0f);
    } else if (hotkey_uuid == exposure_hotkey_) {
        exposure_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus(); // setting the 'priorty' to 10 so we override other plugins while
                            // exposure scrubbing
    }
}

void OCIOColourPipeline::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == exposure_hotkey_) {
        exposure_->set_role_data(module::Attribute::Activated, false);
        release_mouse_focus();
    }
}

bool OCIOColourPipeline::pointer_event(const ui::PointerEvent &e) {
    bool used = false;

    // Implementing exposure scrubbing in viewport, while 'E' key is down.
    if (exposure_->get_role_data<bool>(module::Attribute::Activated)) {
        static int x_down;
        static float e_down;
        static bool dragging = false;

        if (e.type() == ui::Signature::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Left) {
            x_down   = e.x();
            e_down   = exposure_->value();
            dragging = true;
            used     = true;
        } else if (dragging && e.buttons() == ui::Signature::Left) {
            exposure_->set_value(round((e_down + (e.x() - x_down) * 0.01) * 4.0f) / 4.0f);
            used = true;
        } else if (
            e.type() == ui::Signature::EventType::ButtonRelease &&
            (e.buttons() & ui::Signature::Left)) {
            dragging = false;
            used     = true;
        }

        if (e.type() == ui::Signature::EventType::DoubleClick) {
            static float last_value = 0.0f;
            if (exposure_->value() == 0.0f) {
                exposure_->set_value(last_value);
            } else {
                last_value = exposure_->value();
                exposure_->set_value(0.0f);
            }
            used = true;
        }
    }

    return used;
}

void OCIOColourPipeline::screen_changed(
    const bool &is_primary_viewer,
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

    if (is_primary_viewer) {
        if (menu_populated(display_)) {
            display_->set_value(display);
        }
        main_monitor_name_ = monitor_name;
    } else {
        if (menu_populated(popout_viewer_display_)) {
            popout_viewer_display_->set_value(display);
        }
        popout_monitor_name_ = monitor_name;
    }
}

// Set up ui elements of the pluging
void OCIOColourPipeline::setup_ui() {
    // OCIO Source colour space

    source_colour_space_ =
        add_string_choice_attribute(ui_text_.SOURCE_CS, ui_text_.SOURCE_CS_SHORT);
    source_colour_space_->set_redraw_viewport_on_change(true);
    source_colour_space_->set_role_data(
        module::Attribute::UuidRole, "805f5778-3a6f-46b2-aa35-054659a72a1a");
    source_colour_space_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    source_colour_space_->set_role_data(module::Attribute::Enabled, false);
    source_colour_space_->set_role_data(module::Attribute::ToolbarPosition, 12.0f);
    source_colour_space_->set_role_data(
        module::Attribute::MenuPaths, ui_text_.MENU_PATH_SOURCE_CS);
    source_colour_space_->set_role_data(module::Attribute::ToolTip, ui_text_.SOURCE_CS_TOOLTIP);

    // OCIO display selection (main viewer)

    display_ = add_string_choice_attribute(ui_text_.DISPLAY, ui_text_.DISPLAY_SHORT);
    display_->set_redraw_viewport_on_change(true);
    display_->set_role_data(
        module::Attribute::UuidRole, "cfc4a308-4c19-4a7a-b73f-660428ff0ee1");
    display_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"main_toolbar", "colour_pipe_attributes"});
    display_->set_role_data(module::Attribute::Enabled, true);
    display_->set_role_data(module::Attribute::ToolbarPosition, 10.0f);
    display_->set_role_data(module::Attribute::MenuPaths, ui_text_.MENU_PATH_DISPLAY);
    display_->set_role_data(module::Attribute::ToolTip, ui_text_.DISPLAY_TOOLTIP);

    // OCIO display selection (second viewer)

    popout_viewer_display_ =
        add_string_choice_attribute(ui_text_.DISPLAY, ui_text_.DISPLAY_SHORT);
    // Avoid clash with the display_ attr on save/load
    popout_viewer_display_->set_role_data(
        module::Attribute::UuidRole, "fa7b29b2-b159-4098-b106-b82f0cdc2048");
    popout_viewer_display_->set_role_data(module::Attribute::SerializeKey, "PopoutDisplay");
    popout_viewer_display_->set_redraw_viewport_on_change(true);
    popout_viewer_display_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"popout_toolbar", "colour_pipe_attributes"});
    popout_viewer_display_->set_role_data(module::Attribute::Enabled, true);
    popout_viewer_display_->set_role_data(module::Attribute::ToolbarPosition, 10.0f);
    popout_viewer_display_->set_role_data(
        module::Attribute::MenuPaths, ui_text_.MENU_PATH_POPOUT_VIEWER_DISPLAY);
    popout_viewer_display_->set_role_data(module::Attribute::ToolTip, ui_text_.DISPLAY_TOOLTIP);

    // OCIO view selection

    view_ = add_string_choice_attribute(ui_text_.VIEW, ui_text_.VIEW);
    view_->set_redraw_viewport_on_change(true);
    view_->set_role_data(module::Attribute::UuidRole, "74e0d978-0402-41db-8761-9c6bd2f8a608");
    view_->set_role_data(
        module::Attribute::Groups,
        nlohmann::json{"main_toolbar", "popout_toolbar", "colour_pipe_attributes"});
    view_->set_role_data(module::Attribute::Enabled, true);
    view_->set_role_data(module::Attribute::ToolbarPosition, 11.0f);
    view_->set_role_data(module::Attribute::MenuPaths, ui_text_.MENU_PATH_VIEW);
    view_->set_role_data(module::Attribute::ToolTip, ui_text_.VIEW_TOOLTIP);

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
    channel_->set_role_data(
        module::Attribute::UuidRole, "5c9cecc2-a884-400a-8c46-e7d0d16f5b9f");
    channel_->set_role_data(
        module::Attribute::Groups,
        nlohmann::json{"main_toolbar", "popout_toolbar", "colour_pipe_attributes"});
    channel_->set_role_data(module::Attribute::Enabled, true);
    channel_->set_role_data(module::Attribute::ToolbarPosition, 8.0f);
    channel_->set_role_data(module::Attribute::MenuPaths, ui_text_.MENU_PATH_CHANNEL);
    channel_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SELECT_CLR_TIP);


    // Exposure slider

    exposure_ = add_float_attribute(
        ui_text_.EXPOSURE, ui_text_.EXPOSURE_SHORT, 0.0f, -10.0f, 10.0f, 0.05f);
    exposure_->set_redraw_viewport_on_change(true);
    exposure_->set_role_data(
        module::Attribute::UuidRole, "58bf6780-103e-4be6-a1e8-166999746d74");
    exposure_->set_role_data(module::Attribute::ToolbarPosition, 4.0f);
    exposure_->set_role_data(
        module::Attribute::Groups,
        nlohmann::json{"main_toolbar", "popout_toolbar", "colour_pipe_attributes"});
    exposure_->set_role_data(module::Attribute::Activated, false);
    exposure_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    exposure_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SET_EXP_TIP);


    // Colour bypass

    colour_bypass_ = add_boolean_attribute(ui_text_.CMS_OFF, ui_text_.CMS_OFF_SHORT, false);

    colour_bypass_->set_redraw_viewport_on_change(true);
    colour_bypass_->set_role_data(
        module::Attribute::UuidRole, "35228873-0b08-4abc-877b-14055ddc2772");
    colour_bypass_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"colour_pipe_attributes"});
    colour_bypass_->set_role_data(module::Attribute::Enabled, false);
    colour_bypass_->set_role_data(module::Attribute::MenuPaths, ui_text_.MENU_PATH_CMS_OFF);
    colour_bypass_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_BYPASS_TOOLTIP);

    ui_initialized_ = true;
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
}


void OCIOColourPipeline::populate_ui(const MediaParams &media_param) {
    const auto ocio_config      = media_param.ocio_config;
    const auto ocio_config_name = media_param.ocio_config_name;

    // Store the display/view settings for the current config that's
    // about to be switched out.
    if (not current_config_name_.empty() and current_config_name_ != ocio_config_name) {
        std::scoped_lock l(per_config_settings_mutex_);

        auto &settings                 = per_config_settings_[current_config_name_];
        settings.display               = display_->value();
        settings.popout_viewer_display = popout_viewer_display_->value();
        settings.view                  = view_->value();
    }

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
    popout_viewer_display_->set_role_data(module::Attribute::StringChoices, displays, false);

    // Restore settings for the config if we've already used it, else use defaults.
    std::scoped_lock l(per_config_settings_mutex_);

    auto it = per_config_settings_.find(ocio_config_name);

    if (it != per_config_settings_.end() and
        display_views.find(it->second.display) != display_views.end() and
        display_views.find(it->second.popout_viewer_display) != display_views.end()) {
        display_->set_value(it->second.display);
        popout_viewer_display_->set_value(it->second.popout_viewer_display);

        view_->set_role_data(
            module::Attribute::StringChoices, display_views[it->second.display]);
        view_->set_value(it->second.view);
    } else {
        if (!main_monitor_name_.empty()) {
            display_->set_value(default_display(media_param, main_monitor_name_));
        }
        if (!popout_monitor_name_.empty()) {
            popout_viewer_display_->set_value(
                default_display(media_param, popout_monitor_name_));
        }

        view_->set_role_data(
            module::Attribute::StringChoices, display_views[display_->value()]);
        view_->set_value(default_view);
    }
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

void OCIOColourPipeline::update_views(OCIO::ConstConfigRcPtr ocio_config) {
    std::map<std::string, std::vector<std::string>> display_views;
    parse_display_views(ocio_config, display_views);

    const std::string new_display = display_->value();
    const auto new_views          = display_views[new_display];

    view_->set_role_data(module::Attribute::StringChoices, display_views[new_display]);
    view_->set_role_data(module::Attribute::AbbrStringChoices, display_views[new_display]);

    // Check whether the current view is available under the new display or not.
    const std::string curr_view = view_->value();
    bool has_curr_view =
        std::find(new_views.begin(), new_views.end(), curr_view) != new_views.end();
    if (!has_curr_view) {
        std::string default_view = ocio_config->getDefaultView(new_display.c_str());
        view_->set_value(default_view);
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
