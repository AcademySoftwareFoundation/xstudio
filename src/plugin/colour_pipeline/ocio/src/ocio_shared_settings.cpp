// SPDX-License-Identifier: Apache-2.0
#include "ocio_shared_settings.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio;


void xstudio::colour_pipeline::from_json(const nlohmann::json &j, OCIOGlobalData &d) {

    j.at("colour_bypass").get_to(d.colour_bypass);
    j.at("global_view").get_to(d.global_view);
    j.at("adjust_source").get_to(d.adjust_source);
    j.at("preferred_view").get_to(d.preferred_view);
}

void xstudio::colour_pipeline::to_json(nlohmann::json &j, const OCIOGlobalData &d) {

    j["colour_bypass"]  = d.colour_bypass;
    j["global_view"]    = d.global_view;
    j["adjust_source"]  = d.adjust_source;
    j["preferred_view"] = d.preferred_view;
}

OCIOGlobalControls::OCIOGlobalControls(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : StandardPlugin(cfg, "OCIOGlobalControls", init_settings) {

    system().registry().put(OCIOGlobalControls::NAME(), this);

    // make sure we get cleaned up when the global actor exits
    link_to(system().registry().template get<caf::actor>(global_registry));

    // Colour bypass

    colour_bypass_ = add_boolean_attribute(ui_text_.CMS_OFF, ui_text_.CMS_OFF_SHORT, false);
    colour_bypass_->set_redraw_viewport_on_change(true);
    colour_bypass_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"colour_pipe_attributes"});
    // colour_bypass_->set_role_data(module::Attribute::Enabled, false);
    colour_bypass_->set_role_data(module::Attribute::ToolTip, ui_text_.CS_BYPASS_TOOLTIP);
    // 'colour bypass' is a global setting.
    colour_bypass_->set_role_data(
        module::Attribute::UuidRole, utility::Uuid("222902ee-167b-4c74-91aa-04eb74fd4357"));

    // View mode

    global_view_ = add_boolean_attribute(ui_text_.VIEW_MODE, ui_text_.GLOBAL_VIEW_SHORT, true);
    global_view_->set_redraw_viewport_on_change(true);
    global_view_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"colour_pipe_attributes"});
    global_view_->set_role_data(module::Attribute::ToolTip, ui_text_.GLOBAL_VIEW_TOOLTIP);
    global_view_->set_preference_path("/plugin/colour_pipeline/global_view");

    // Preferred view

    preferred_view_ = add_string_choice_attribute(
        ui_text_.PREF_VIEW, ui_text_.PREF_VIEW, ui_text_.DEFAULT_VIEW);
    preferred_view_->set_redraw_viewport_on_change(true);
    preferred_view_->set_role_data(module::Attribute::Enabled, !global_view_->value());
    preferred_view_->set_role_data(module::Attribute::ToolbarPosition, 11.0f);
    preferred_view_->set_role_data(module::Attribute::ToolTip, ui_text_.PREF_VIEW_TOOLTIP);
    preferred_view_->set_role_data(
        module::Attribute::StringChoices, ui_text_.PREF_VIEW_OPTIONS, false);
    preferred_view_->set_preference_path("/plugin/colour_pipeline/ocio/preferred_view");

    // Source mode

    adjust_source_ =
        add_boolean_attribute(ui_text_.SOURCE_CS_MODE, ui_text_.SOURCE_CS_MODE_SHORT, true);
    adjust_source_->set_redraw_viewport_on_change(true);
    adjust_source_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"colour_pipe_attributes"});
    // adjust_source_->set_role_data(module::Attribute::Enabled, false);
    adjust_source_->set_role_data(module::Attribute::ToolTip, ui_text_.SOURCE_CS_MODE_TOOLTIP);
    adjust_source_->set_preference_path("/plugin/colour_pipeline/ocio/user_source_mode");

    // this attr is used to store the display/view that the user has selected
    // so the settings can be restored between xstudio instances
    user_view_display_settings_attr_ = add_attribute("User OCIO Settings");
    user_view_display_settings_attr_->set_preference_path(
        "/plugin/colour_pipeline/ocio/user_ocio_settings");

    // we need to call this base class method before calling insert_menu_item
    make_behavior();

    insert_menu_item("main menu bar", ui_text_.CMS_OFF, "Colour", 1.0f, colour_bypass_, false);
    insert_menu_item("main menu bar", ui_text_.VIEW_MODE, "Colour", 2.0f, global_view_, false);
    insert_menu_item(
        "main menu bar", ui_text_.PREF_VIEW, "Colour", 3.0f, preferred_view_, false);
    insert_menu_item(
        "main menu bar", ui_text_.SOURCE_CS_MODE, "Colour", 4.0f, adjust_source_, false);

    // make sure the colour menu appears in the right place in the main menu bar.
    set_submenu_position_in_parent("main menu bar", "Colour", 30.0f);

    set_down_handler([=](down_msg &msg) {
        auto p = watchers_.begin();
        while (p != watchers_.end()) {
            if (msg.source == *p)
                p = watchers_.erase(p);
            else
                p++;
        }
    });

    ui_initialized_ = true;

    // the user_view_display_settings_ attr should be updated from the preferences
    // store at this point
    user_view_display_settings_ =
        user_view_display_settings_attr_->role_data_as_json(module::Attribute::Value);

    synchronize_attributes();
    connect_to_ui();
}

void OCIOGlobalControls::on_exit() {
    system().registry().erase(OCIOGlobalControls::NAME());
    watchers_.clear();
}

caf::message_handler OCIOGlobalControls::message_handler_extensions() {

    return caf::message_handler(
               {[=](connect_to_viewport_atom,
                    const std::string &viewport_name,
                    const std::string &viewport_toolbar_name,
                    bool connect,
                    caf::actor vp) {
                    connect_to_viewport(viewport_name, viewport_toolbar_name, connect, vp);
                },
                // TODO: Most likely need more fine grained control over what's linked
                // or not between the different viewports, maybe a way to allow user
                // to override the default linking behaviour per attribute and set which
                // viewport is the reference. This handler would then accept optionaly
                // the list of attributes to synchronize.
                [=](global_ocio_controls_atom, caf::actor watcher) {
                    monitor(watcher);
                    watchers_.push_back(watcher);
                    send(watcher, global_ocio_controls_atom_v, settings_json());
                },
                // Allow new viewport to query the last Display/View settings for
                // a given config
                [=](global_ocio_controls_atom atom,
                    const std::string &ocio_config,
                    const std::string &window_id) -> utility::JsonStore {
                    utility::JsonStore res;

                    if (user_view_display_settings_.contains(ocio_config)) {

                        const auto &ds = user_view_display_settings_[ocio_config];

                        // N.B. View is the same for all viewers (for a given ocio_config)
                        // but Display is independent per window_id
                        if (ds.contains("View")) {
                            res["View"] = ds["View"];
                        }
                        if (ds.contains(window_id) && ds[window_id].contains("Display")) {
                            res["Display"] = ds[window_id]["Display"];
                        } else if (window_id.find("xstudio_quickview_window") != std::string::npos) {

                            // special case - a quickview window wants to set its
                            // display but no quickview has been set-up before for
                            // the current ocio_config so we fallback to the main window display
                            if (ds.contains("xstudio_main_window") &&
                                ds["xstudio_main_window"].contains("Display")) {
                                res["Display"] = ds["xstudio_main_window"]["Display"];
                            }
                        }
                        return res;
                    }
                    return utility::JsonStore();
                },
                [=](global_ocio_controls_atom atom,
                    const std::string &attr_title,
                    const int attr_role,
                    const utility::JsonStore &attr_value,
                    const std::string &window_id) {
                    for (auto &watcher : watchers_) {
                        if (watcher != current_sender()) {
                            send(watcher, atom, attr_title, attr_role, attr_value, window_id);
                        }
                    }
                },
                [=](global_ocio_controls_atom atom,
                    const std::string &ocio_config,
                    const std::string &attr_title,
                    const int attr_role,
                    const utility::JsonStore &attr_value,
                    const std::string &window_id) {
                    // we need to store Display and View (per OCIO config) between
                    // xstudio sessions. This attribute has a preference path so
                    // it's value is written (and retrieved from) user prefs.
                    // Note Display is independent for xstudio window ids, but
                    // View is the same across all windows
                    if (attr_title == "Display") {
                        user_view_display_settings_[ocio_config][window_id][attr_title] =
                            attr_value;
                        user_view_display_settings_attr_->set_role_data(
                            module::Attribute::Value, user_view_display_settings_, false);
                    } else if (attr_title == "View") {
                        user_view_display_settings_[ocio_config][attr_title] = attr_value;
                        user_view_display_settings_attr_->set_role_data(
                            module::Attribute::Value, user_view_display_settings_, false);
                    }

                    for (auto &watcher : watchers_) {
                        if (watcher != current_sender()) {
                            send(
                                watcher,
                                atom,
                                ocio_config,
                                attr_title,
                                attr_role,
                                attr_value,
                                window_id);
                        }
                    }
                }})
        .or_else(StandardPlugin::message_handler_extensions());
}

void OCIOGlobalControls::connect_to_viewport(
    const std::string &viewport_name,
    const std::string &viewport_toolbar_name,
    bool connect,
    caf::actor viewport) {

    Module::connect_to_viewport(viewport_name, viewport_toolbar_name, connect, viewport);
    std::string viewport_context_menu_model_name = viewport_name + "_context_menu";
}

void OCIOGlobalControls::attribute_changed(
    const utility::Uuid &attribute_uuid, const int role) {

    if (!ui_initialized_)
        return;

    if (attribute_uuid == global_view_->uuid()) {
        preferred_view_->set_role_data(module::Attribute::Enabled, !global_view_->value());
    }

    synchronize_attributes();
}

utility::JsonStore OCIOGlobalControls::settings_json() {

    utility::JsonStore j;
    j["colour_bypass"]  = colour_bypass_->value();
    j["global_view"]    = global_view_->value();
    j["adjust_source"]  = adjust_source_->value();
    j["preferred_view"] = preferred_view_->value();
    return j;
}

void OCIOGlobalControls::synchronize_attributes() {

    for (auto &watcher : watchers_) {
        send(watcher, global_ocio_controls_atom_v, settings_json());
    }
}