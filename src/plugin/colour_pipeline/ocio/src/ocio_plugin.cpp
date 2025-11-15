// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "ocio_plugin.hpp"
#include "ocio_shared_settings.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

using namespace xstudio::colour_pipeline::ocio;
using namespace xstudio;


namespace {
const static utility::Uuid PLUGIN_UUID{"b39d1e3d-58f8-475f-82c1-081a048df705"};
} // anonymous namespace


OCIOColourPipeline::OCIOColourPipeline(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourPipeline(cfg, init_settings) {

    viewport_name_    = init_settings["viewport_name"];
    window_id_        = init_settings.value("window_id", std::string());
    sync_with_others_ = init_settings.value("join_sync_group", true);

    global_controls_ = system().registry().template get<caf::actor>(OCIOGlobalControls::NAME());
    if (!global_controls_) {
        global_controls_ = spawn<OCIOGlobalControls>(init_settings);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    worker_pool_ = caf::actor_pool::make(
        system(),
        4,
        [=] {
            caf::actor worker = system().spawn<OCIOEngineActor>();
            workers_.push_back(worker);
            return worker;
        },
        caf::actor_pool::round_robin());
    link_to(worker_pool_);

#pragma GCC diagnostic pop

    setup_ui();

    mail(global_ocio_controls_atom_v, caf::actor_cast<caf::actor>(this)).send(global_controls_);
}

OCIOColourPipeline::~OCIOColourPipeline() { global_controls_ = caf::actor(); }

caf::message_handler OCIOColourPipeline::message_handler_extensions() {

    return caf::message_handler(
               {

                   [=](global_ocio_controls_atom atom,
                       const std::string &ocio_config) -> utility::JsonStore {
                       utility::JsonStore res;

                       if (ocio_config == current_config_name_) {
                           res["source_colour_space"] = source_colour_space_->value();
                           res["display"]             = display_->value();
                           res["view"]                = view_->value();
                           res["channel"]             = channel_->value();
                           res["exposure"]            = exposure_->value();
                           res["gamma"]               = gamma_->value();
                           res["saturation"]          = saturation_->value();
                       }

                       return res;
                   },
                   [=](global_ocio_controls_atom,
                       const std::string &_override,
                       const bool val) {
                       // special message that lets us force 'force_global_view_'. The video
                       // renderer uses this to force its colour pipeline to use the global
                       // view behaviour. This is used by the VideoRenderer, for example,
                       // because we don't want 'live' View picking during rendering (I think)
                       if (_override == "force_global_view") {
                           force_global_view_ = val;
                       }
                   },
                   [=](global_ocio_controls_atom, const utility::JsonStore &settings) {
                       if (settings.is_null())
                           return;

                       OCIOGlobalData old_settings = global_settings_;
                       from_json(settings, global_settings_);

                       m_engine_.set_default_config(global_settings_.default_config);
                       for (auto &a : workers_) {
                           anon_mail(global_ocio_controls_atom_v, settings).send(a);
                       }

                       // if the 'auto adjust source' global setting is ON, the user
                       // cannot set the source colourspace as this is automatically
                       // set by logic in this plugin. We disable/enable
                       // the Source Colourspace menu items thus:
                       source_colour_space_->set_role_data(
                           module::Attribute::StringChoicesEnabled,
                           std::vector<bool>(
                               source_colour_space_->options().size(),
                               !global_settings_.adjust_source),
                           false // don't call attribute_changed
                       );

                       if (global_settings_.colour_bypass != old_settings.colour_bypass) {
                           update_bypass(global_settings_.colour_bypass);
                       }

                       if (global_settings_ != old_settings) {
                           // this forces an update ot the UI - OCIO view/display menus
                           // etc.
                           media_source_changed(
                               current_source_uuid_, current_source_colour_mgmt_metadata_);
                       }
                   },
                   [=](global_ocio_controls_atom atom,
                       const std::string &attr_title,
                       const int attr_role,
                       const utility::JsonStore &attr_value,
                       const std::string &window_id) {
                       // quickview windows DON'T sync OCIO settings
                       if (!sync_with_others_)
                           return;

                       auto attr = get_attribute(attr_title);
                       if (attr) {
                           attr->update_role_data_from_json(attr_role, attr_value, false);
                           redraw_viewport();
                       }
                   },
                   [=](global_ocio_controls_atom atom,
                       const std::string &ocio_config,
                       const std::string &attr_title,
                       const int attr_role,
                       const utility::JsonStore &attr_value,
                       const std::string &window_id) {
                       // quickview windows DON'T sync OCIO settings
                       if (!sync_with_others_)
                           return;

                       // snapshot viewport settings don't affect other viewports
                       if (window_id == "snapshot_viewport")
                           return;

                       if (ocio_config == current_config_name_) {

                           // we don't sync OCIO Display if it's coming from a different window
                           if (attr_title == "Display" && window_id != window_id_)
                               return;

                           auto attr = get_attribute(attr_title);
                           if (attr) {
                               attr->update_role_data_from_json(attr_role, attr_value, false);
                               redraw_viewport();
                           }
                       }
                   }})
        .or_else(ColourPipeline::message_handler_extensions());
}

size_t OCIOColourPipeline::fast_display_transform_hash(const media::AVFrameID &media_ptr) {

    size_t hash = 0;

    if (!global_settings_.colour_bypass) {
        hash = m_engine_.compute_hash(
            media_ptr.params(),
            display_->value() + view_->value() + (global_settings_.adjust_source ? "adj" : "") +
                global_settings_.default_config);
    }

    return hash;
}

void OCIOColourPipeline::linearise_op_data(
    caf::typed_response_promise<ColourOperationDataPtr> &rp,
    const media::AVFrameID &media_ptr) {

    rp.delegate(
        worker_pool_,
        colour_pipe_linearise_data_atom_v,
        media_ptr.params(), // media_metadata
        global_settings_.colour_bypass,
        global_settings_.adjust_source,
        view_->value());
}

void OCIOColourPipeline::linear_to_display_op_data(
    caf::typed_response_promise<ColourOperationDataPtr> &rp,
    const media::AVFrameID &media_ptr) {

    // TODO: Adjust the view depending on settings and media
    // If global_view ON, use the plugin view_
    // If gobal_view OFF, use the media detected view (or user overrided)

    rp.delegate(
        worker_pool_,
        colour_pipe_display_data_atom_v,
        media_ptr.params(), // media_metadata
        display_->value(),
        view_->value(),
        global_settings_.colour_bypass);
}

utility::JsonStore OCIOColourPipeline::update_shader_uniforms(
    const media_reader::ImageBufPtr &image, std::any &user_data) {

    utility::JsonStore uniforms;
    if (channel_->value() == "Red") {
        uniforms["show_chan"] = 1;
    } else if (channel_->value() == "Green") {
        uniforms["show_chan"] = 2;
    } else if (channel_->value() == "Blue") {
        uniforms["show_chan"] = 3;
    } else if (channel_->value() == "Alpha") {
        uniforms["show_chan"] = 4;
    } else if (channel_->value() == "Luminance") {
        uniforms["show_chan"] = 5;
    } else {
        uniforms["show_chan"] = 0;
    }

    // TODO: ColSci
    // Saturation is not managed by OCIO currently
    uniforms["saturation"] = saturation_->value();

    m_engine_.update_shader_uniforms(user_data, uniforms, exposure_->value(), gamma_->value());

    return uniforms;
}

void OCIOColourPipeline::process_thumbnail(
    caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> &rp,
    const media::AVFrameID &media_ptr,
    const thumbnail::ThumbnailBufferPtr &buf) {

    // TODO: Adjust the view depending on settings and media
    // If global_view ON, use the plugin view_
    // If gobal_view OFF, use the media detected view (or user overrided)

    rp.delegate(
        worker_pool_,
        media_reader::process_thumbnail_atom_v,
        media_ptr.params(), // media_metadata
        buf,
        display_->value(),
        view_->value());
}

void OCIOColourPipeline::extend_pixel_info(
    media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) {

    try {

        m_engine_.extend_pixel_info(
            pixel_info,
            frame_id,
            display_->value(),
            view_->value(),
            global_settings_.adjust_source,
            exposure_->value(),
            gamma_->value(),
            saturation_->value());

    } catch (const std::exception &e) {

        spdlog::warn("OCIOColourPipeline: Failed to compute pixel probe: {}", e.what());
    }
}

void OCIOColourPipeline::register_hotkeys() {

    for (const auto &hotkey_props : ui_text_.channel_hotkeys) {
        auto hotkey_id = register_hotkey(
            hotkey_props.key,
            hotkey_props.modifier,
            hotkey_props.name,
            hotkey_props.description,
            false,
            "Viewer");

        channel_hotkeys_[hotkey_id] = hotkey_props.channel_name;
    }

    for (int i = 1; i <= 9; ++i) {
        auto suffix = std::string(fmt::format(" #{}", i));
        auto hotkey_id = register_hotkey(
            int('0')+i,
            ui::MetaModifier,
            std::string("Change OCIO View Transform")+suffix,
            "Changes OCIO view transform",
            false,
            "Viewer");
        view_hotkeys_[hotkey_id] = i;
    }

    reset_hotkey_ = register_hotkey(
        int('R'),
        ui::ControlModifier,
        "Reset Colour Viewing Setting",
        "Resets viewer exposure and channel mode",
        false,
        "Viewer");

    exposure_hotkey_ = register_hotkey(
        int('E'),
        ui::NoModifier,
        "Exposure Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer exposure",
        false,
        "Viewer");

    gamma_hotkey_ = register_hotkey(
        int('Y'),
        ui::NoModifier,
        "Gamma Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer gamma",
        false,
        "Viewer");

    saturation_hotkey_ = register_hotkey(
        int('S'),
        ui::AltModifier,
        "Saturation Scrubbing",
        "Hold this key down and click-scrub the mouse pointer left/right in the viewport to "
        "adjust viewer saturation",
        false,
        "Viewer");
}

void OCIOColourPipeline::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {

    // Ignore hotkey events that have not come from the viewport that this
    // OCIOColourPipeline is connected to.
    if (context != viewport_name_)
        return;

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
        reset();
    } else if (hotkey_uuid == exposure_hotkey_) {
        exposure_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    } else if (hotkey_uuid == gamma_hotkey_) {
        gamma_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    } else if (hotkey_uuid == saturation_hotkey_) {
        saturation_->set_role_data(module::Attribute::Activated, true);
        grab_mouse_focus();
    } else {
        auto v = view_hotkeys_.find(hotkey_uuid);
        if (v != view_hotkeys_.end()) {
            const auto &views = display_views_[display_->value()];
            int idx = v->second-1;
            if (idx >= 0 and idx < views.size()) {
                const auto &new_view = views[idx];
                if (new_view != view_->value()) {
                    view_->set_value(new_view);
                }
            }
        }
    }
}

void OCIOColourPipeline::hotkey_released(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const bool due_to_focus_change) {

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

    if (e.type() == ui::EventType::ButtonDown && e.buttons() == ui::Signature::Left) {
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
        e.type() == ui::EventType::ButtonRelease && (e.buttons() & ui::Signature::Left)) {
        dragging = false;
        used     = true;
    }

    if (e.type() == ui::EventType::DoubleClick) {
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

void OCIOColourPipeline::media_source_changed(
    const utility::Uuid &source_uuid, const utility::JsonStore &src_colour_mgmt_metadata) {

    current_source_uuid_                 = source_uuid;
    current_source_colour_mgmt_metadata_ = src_colour_mgmt_metadata;

    if (global_settings_.colour_bypass)
        return;

    if (!source_uuid)
        return;

    // Update the UI if the OCIO config changes
    //  (xStudio support per media OCIO config)
    populate_ui(src_colour_mgmt_metadata);

    // Adjust view
    if (!global_settings_.global_view) {
        const auto preferred_view =
            m_engine_.preferred_view(src_colour_mgmt_metadata, global_settings_.preferred_view);
        const auto view = src_colour_mgmt_metadata.get_or("override_view", preferred_view);
        view_->set_value(view, false);
    }

    // Determine the source colourspace.
    std::string src_cs = m_engine_.detect_source_colourspace(
        src_colour_mgmt_metadata, global_settings_.adjust_source, view_->value());

    if (!src_cs.empty()) {
        // We do not 'notify' this attribute change as it's not the user selecting
        // a source colourspace but (possibly) driven by dynamic plugin logic
        source_colour_space_->set_value(src_cs, false);
    }
}

void OCIOColourPipeline::attribute_changed(
    const utility::Uuid &attribute_uuid, const int role) {

    // These attributes are synchronized for viewports sharing the same
    // current OCIO config.
    if (attribute_uuid == source_colour_space_->uuid()) {

        // we only get notified here when the USER has changed the source
        // colourspace via the OCIO menu options in the UI
        update_media_metadata(
            current_source_uuid_,
            "/colour_pipeline/override_input_cs",
            source_colour_space_->value());

        synchronize_attribute(attribute_uuid, role, true);

    } else if (attribute_uuid == display_->uuid()) {

        if (role == module::Attribute::Value) {

            update_views(display_->value());
            synchronize_attribute(attribute_uuid, role, true);
        }

    } else if (attribute_uuid == view_->uuid()) {

        // Adjust the per media view override
        if (!global_settings_.global_view) {
            update_media_metadata(
                current_source_uuid_, "/colour_pipeline/override_view", view_->value());
        }

        synchronize_attribute(attribute_uuid, role, true);

        // Adjust the per media input colour space override
        if (global_settings_.adjust_source) {
            const std::string src_cs = m_engine_.input_space_for_view(
                current_source_colour_mgmt_metadata_, view_->value());
            if (!src_cs.empty()) {
                source_colour_space_->set_value(src_cs);
            }
        }

        // Remaning attributes are synchronized unconditionally
    } else {

        synchronize_attribute(attribute_uuid, role, false);
    }

    redraw_viewport();
}

void OCIOColourPipeline::screen_changed(
    const std::string &name,
    const std::string &model,
    const std::string &manufacturer,
    const std::string &serialNumber) {

    auto menu_populated = [](module::StringChoiceAttribute *attr) {
        return attr->get_role_data<std::vector<std::string>>(module::Attribute::StringChoices)
                   .size() > 0;
    };

    if (menu_populated(display_) && !monitor_name_.empty()) {

        // we only override the display if the screen info is *changing* ... if
        // it's being set for the first time we don't want to auto-set the
        // display as it has already been chosen either in populate_ui or
        // by the user

        const std::string detected_display = detect_display(
            name, model, manufacturer, serialNumber, current_source_colour_mgmt_metadata_);

        display_->set_value(detected_display);
    }

    monitor_name_         = name;
    monitor_model_        = model;
    monitor_manufacturer_ = manufacturer;
    monitor_serialNumber_ = serialNumber;
}

void OCIOColourPipeline::connect_to_viewport(
    const std::string &viewport_name,
    const std::string &viewport_toolbar_name,
    bool connect,
    caf::actor viewport) {

    viewport_name_ = viewport_name;

    Module::connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);

    // Here we can add attrs to show up in the viewer context menu (right click)
    std::string viewport_context_menu_model_name = viewport_name + "_context_menu";

    insert_menu_item(
        viewport_context_menu_model_name, "Source", "OCIO", 1.0f, source_colour_space_, false);
    insert_menu_item(
        viewport_context_menu_model_name, "Display", "OCIO", 2.0f, display_, false);
    insert_menu_item(viewport_context_menu_model_name, "View", "OCIO", 3.0f, view_, false);
    insert_menu_item(viewport_context_menu_model_name, "Channel", "", 21.5f, channel_, false);
    insert_menu_item(viewport_context_menu_model_name, "", "", 22.0f, nullptr, true); // divider

    set_submenu_position_in_parent(viewport_context_menu_model_name, "OCIO", 19.0f);

    make_attribute_visible_in_viewport_toolbar(view_);
    make_attribute_visible_in_viewport_toolbar(display_);

    // the OCIOGlobalControls actor instance needs to connect to the viewport
    // too so it can expose its controls in the viewport toolbar etc.
    mail(
        colour_pipeline::connect_to_viewport_atom_v,
        viewport_name,
        viewport_toolbar_name,
        connect,
        viewport)
        .send(global_controls_);
}

void OCIOColourPipeline::setup_ui() {

    // OCIO Source colour space

    source_colour_space_ =
        add_string_choice_attribute(ui_text_.SOURCE_CS, ui_text_.SOURCE_CS_SHORT);
    source_colour_space_->set_redraw_viewport_on_change(true);
    source_colour_space_->set_role_data(module::Attribute::ToolTip, ui_text_.SOURCE_CS_TOOLTIP);

    // OCIO display selection

    display_ = add_string_choice_attribute(ui_text_.DISPLAY, ui_text_.DISPLAY_SHORT);
    display_->set_redraw_viewport_on_change(true);
    display_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"colour_pipe_attributes"});
    display_->set_role_data(module::Attribute::Enabled, true);
    display_->set_role_data(module::Attribute::ToolbarPosition, 10.0f);
    display_->set_role_data(module::Attribute::ToolTip, ui_text_.DISPLAY_TOOLTIP);

    // OCIO view selection

    view_ = add_string_choice_attribute(ui_text_.VIEW, ui_text_.VIEW);
    view_->set_redraw_viewport_on_change(true);
    view_->set_role_data(module::Attribute::Enabled, true);
    view_->set_role_data(module::Attribute::ToolbarPosition, 11.0f);
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

    // Saturation slider

    saturation_ = add_float_attribute(
        ui_text_.SATURATION, ui_text_.SATURATION_SHORT, 1.0f, 0.0f, 10.0f, 0.05f);
    saturation_->set_redraw_viewport_on_change(true);
    saturation_->set_role_data(module::Attribute::ToolbarPosition, 4.2f);
    saturation_->set_role_data(module::Attribute::Activated, false);
    saturation_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    saturation_->set_role_data(
        module::Attribute::ToolTip, ui_text_.CS_MSG_CMS_SET_SATURATION_TIP);

    make_attribute_visible_in_viewport_toolbar(exposure_);
    make_attribute_visible_in_viewport_toolbar(channel_);
    make_attribute_visible_in_viewport_toolbar(gamma_);
    make_attribute_visible_in_viewport_toolbar(saturation_);
}

utility::JsonStore OCIOColourPipeline::get_display_and_view_options_for_media(
    const utility::JsonStore &src_colour_mgmt_metadata) const {

    // The video render plugin needs to query the OCIO options available for
    // a given piece of media, so that they can be presented to the user independently
    // of the state of the OCIO plugin ...

    const auto config_name = m_engine_.get_ocio_config_name(src_colour_mgmt_metadata);

    // Config has changed, so update views and displays
    std::vector<std::string> all_colourspaces;
    std::vector<std::string> displays;
    std::map<std::string, std::vector<std::string>> display_views;

    m_engine_.get_ocio_displays_view_colourspaces(
        src_colour_mgmt_metadata, all_colourspaces, displays, display_views);

    // Try to re-use the previously selected display and view (if any)
    // If no longer available, pick sensible defaults
    std::string display = display_->value();
    std::string view    = view_->value();
    if (std::find(displays.begin(), displays.end(), display) == displays.end()) {
        display = detect_display(
            monitor_name_,
            monitor_model_,
            monitor_manufacturer_,
            monitor_serialNumber_,
            src_colour_mgmt_metadata);
    }

    if (std::find(display_views[display].begin(), display_views[display].end(), view) ==
        display_views[display].end()) {
        view = m_engine_.preferred_view(
            src_colour_mgmt_metadata,
            global_settings_.global_view ? "Default" : global_settings_.preferred_view);
    }

    // we may have used this config before, and stored the display and view settings
    // against the config. If so, use those stored settings rather than the
    // preferred/defaults found above
    try {

        scoped_actor sys{system()};

        auto stored_config_settings = utility::request_receive<utility::JsonStore>(
            *sys, global_controls_, global_ocio_controls_atom_v, config_name, window_id_);

        if (stored_config_settings.contains("Display")) {
            display = stored_config_settings["Display"];
        }
        if (stored_config_settings.contains("View")) {
            view = stored_config_settings["View"];
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    utility::JsonStore result;
    result["display"]         = display;
    result["display_options"] = displays;
    result["view"]            = view;
    for (const auto &p : display_views) {
        result["views_per_display"][p.first] = p.second;
    }

    return result;
}


void OCIOColourPipeline::populate_ui(const utility::JsonStore &src_colour_mgmt_metadata) {

    const auto config_name = m_engine_.get_ocio_config_name(src_colour_mgmt_metadata);

    if (current_config_name_ != config_name) {

        current_config_name_ = config_name;

        // Config has changed, so update views and displays
        std::vector<std::string> all_colourspaces;
        std::vector<std::string> displays;
        m_engine_.get_ocio_displays_view_colourspaces(
            src_colour_mgmt_metadata, all_colourspaces, displays, display_views_);

        // Try to re-use the previously selected display and view (if any)
        // If no longer available, pick sensible defaults
        std::string display = display_->value();
        std::string view    = view_->value();
        if (std::find(displays.begin(), displays.end(), display) == displays.end()) {
            display = detect_display(
                monitor_name_,
                monitor_model_,
                monitor_manufacturer_,
                monitor_serialNumber_,
                src_colour_mgmt_metadata);
        }
        if (std::find(display_views_[display].begin(), display_views_[display].end(), view) ==
            display_views_[display].end()) {
            view = m_engine_.preferred_view(
                src_colour_mgmt_metadata,
                global_settings_.global_view ? "Default" : global_settings_.preferred_view);
        }

        source_colour_space_->set_role_data(
            module::Attribute::StringChoices, all_colourspaces, false);

        // set the 'enabled' value to false on each of the source colourspace
        // options if the global 'Adjust source colourspace mode' is active
        source_colour_space_->set_role_data(
            module::Attribute::StringChoicesEnabled,
            std::vector<bool>(all_colourspaces.size(), !global_settings_.adjust_source),
            false // don't call attribute_changed
        );

        // we may have used this config before, and stored the display and view settings
        // against the config. If so, use those stored settings rather than the
        // preferred/defaults found above
        try {

            scoped_actor sys{system()};

            auto stored_config_settings = utility::request_receive<utility::JsonStore>(
                *sys,
                global_controls_,
                global_ocio_controls_atom_v,
                current_config_name_,
                window_id_);

            if (stored_config_settings.contains("Display")) {
                display = stored_config_settings["Display"];
            }
            if (stored_config_settings.contains("View")) {
                view = stored_config_settings["View"];
            }

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

        display_->set_role_data(module::Attribute::StringChoices, displays, false);
        display_->set_value(display, false);
        view_->set_role_data(module::Attribute::StringChoices, display_views_[display], false);
        view_->set_value(view, false);
    }
}

void OCIOColourPipeline::update_views(const std::string &new_display) {

    if (!current_source_uuid_)
        return;

    const std::vector<std::string> &new_views = display_views_[new_display];
    view_->set_role_data(module::Attribute::StringChoices, new_views, false);

    // Reset the view if no longer available under the new display
    if (std::find(new_views.begin(), new_views.end(), view_->value()) == new_views.end()) {
        view_->set_value(m_engine_.preferred_view(
            current_source_colour_mgmt_metadata_,
            global_settings_.global_view ? "Default" : global_settings_.preferred_view));
    }
}

void OCIOColourPipeline::update_bypass(bool bypass) {

    // Just disable these settings when colour management is off.
    view_->set_role_data(module::Attribute::Enabled, !bypass, false);
    display_->set_role_data(module::Attribute::Enabled, !bypass, false);
}

void OCIOColourPipeline::update_media_metadata(
    const utility::Uuid &media_uuid, const std::string &key, const std::string &val) {

    if (!media_uuid)
        return;

    try {
        scoped_actor sys{system()};

        // first, get to the session
        auto session = utility::request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        // get the session to search its playlists for the media source
        auto media_source_actor = utility::request_receive<caf::actor>(
            *sys, session, media::get_media_source_atom_v, media_uuid);

        if (media_source_actor) {
            auto colour_data = utility::request_receive<bool>(
                *sys,
                media_source_actor,
                json_store::set_json_atom_v,
                utility::JsonStore(val),
                key);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void OCIOColourPipeline::synchronize_attribute(const utility::Uuid &uuid, int role, bool ocio) {

    // Don't bother synchronizing anything beyond values
    if (role != (int)module::Attribute::Value)
        return;

    const auto attr = get_attribute(uuid);
    if (!attr)
        return;

    const auto title = attr->get_role_data<std::string>((int)module::Attribute::Title);
    const auto value = utility::JsonStore(attr->role_data_as_json(role));

    if (ocio) {
        mail(
            global_ocio_controls_atom_v,
            current_config_name_,
            title,
            role,
            value,
            window_id_,
            sync_with_others_)
            .send(global_controls_);
    } else {
        mail(global_ocio_controls_atom_v, title, role, value, window_id_, sync_with_others_)
            .send(global_controls_);
    }
}

void OCIOColourPipeline::reset() {

    channel_->set_value("RGB");
    exposure_->set_value(exposure_->get_role_data<float>(module::Attribute::DefaultValue));
    gamma_->set_value(gamma_->get_role_data<float>(module::Attribute::DefaultValue));
    saturation_->set_value(saturation_->get_role_data<float>(module::Attribute::DefaultValue));
}

std::string OCIOColourPipeline::detect_display(
    const std::string &name,
    const std::string &model,
    const std::string &manufacturer,
    const std::string &serialNumber,
    const utility::JsonStore &meta) const {

    std::string detected_display = m_engine_.default_display(meta);

    if (meta.get_or("viewing_rules", false)) {

        try {

            auto hook = system().registry().template get<caf::actor>(media_hook_registry);

            if (hook) {
                caf::scoped_actor sys(system());
                const std::string display = utility::request_receive<std::string>(
                    *sys,
                    hook,
                    media_hook::detect_display_atom_v,
                    name,
                    model,
                    manufacturer,
                    serialNumber,
                    meta);
                if (!display.empty()) {
                    detected_display = display;
                }
            }

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    return detected_display;
}


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<OCIOColourPipeline>>(
                PLUGIN_UUID,
                "OCIOColourPipeline",
                plugin_manager::PluginFlags::PF_COLOUR_MANAGEMENT,
                false,
                "xStudio",
                "OCIO (v2) Colour Pipeline",
                semver::version("1.0.0"))}));
}
}