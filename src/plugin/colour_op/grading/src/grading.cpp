// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"

#include "grading.h"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_colour_op.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::colour_pipeline;
using namespace xstudio::ui::viewport;


GradingTool::GradingTool(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "GradingTool", init_settings) {

    module::QmlCodeAttribute *button = add_qml_code_attribute(
        "MyCode",
        R"(
    import Grading 1.0
    GradingButton {
        anchors.fill: parent
    }
    )");

    button->expose_in_ui_attrs_group("media_tools_buttons_0");
    button->set_role_data(module::Attribute::ToolbarPosition, 500.0);

    // General

    tool_is_active_ =
        add_boolean_attribute("grading_tool_active", "grading_tool_active", false);
    tool_is_active_->expose_in_ui_attrs_group("grading_settings");
    tool_is_active_->set_role_data(
        module::Attribute::MenuPaths,
        std::vector<std::string>({"panels_main_menu_items|Grading Tool"}));

    mask_is_active_ = add_boolean_attribute("mask_tool_active", "mask_tool_active", false);
    mask_is_active_->expose_in_ui_attrs_group("grading_settings");

    grading_action_ = add_string_attribute("grading_action", "grading_action", "");
    grading_action_->expose_in_ui_attrs_group("grading_settings");

    drawing_action_ = add_string_attribute("drawing_action", "drawing_action", "");
    drawing_action_->expose_in_ui_attrs_group("grading_settings");

    // Grading elements

    grading_panel_ = add_string_choice_attribute(
        "grading_panel",
        "grading_panel",
        utility::map_value_to_vec(grading_panel_names_).front(),
        utility::map_value_to_vec(grading_panel_names_));
    grading_panel_->expose_in_ui_attrs_group("grading_settings");
    grading_panel_->set_preference_path("/plugin/grading/grading_panel");

    grading_layer_ = add_string_choice_attribute("grading_layer", "grading_layer");
    grading_layer_->expose_in_ui_attrs_group("grading_settings");
    grading_layer_->expose_in_ui_attrs_group("grading_layers");

    grading_bypass_ = add_boolean_attribute("drawing_bypass", "drawing_bypass", false);
    grading_bypass_->expose_in_ui_attrs_group("grading_settings");

    grading_buffer_ = add_string_choice_attribute("grading_buffer", "grading_buffer");
    grading_buffer_->expose_in_ui_attrs_group("grading_settings");

    // Slope
    slope_red_ = add_float_attribute("Red Slope", "Red", 1.0f, 0.0f, 4.0f, 0.005f);
    slope_red_->set_redraw_viewport_on_change(true);
    slope_red_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    slope_red_->set_role_data(module::Attribute::ToolTip, "Red slope");
    slope_red_->expose_in_ui_attrs_group("grading_settings");
    slope_red_->expose_in_ui_attrs_group("grading_slope");
    slope_red_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 0.0f, 0.0f));

    slope_green_ = add_float_attribute("Green Slope", "Green", 1.0f, 0.0f, 4.0f, 0.005f);
    slope_green_->set_redraw_viewport_on_change(true);
    slope_green_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    slope_green_->set_role_data(module::Attribute::ToolTip, "Green slope");
    slope_green_->expose_in_ui_attrs_group("grading_settings");
    slope_green_->expose_in_ui_attrs_group("grading_slope");
    slope_green_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 1.0f, 0.0f));

    slope_blue_ = add_float_attribute("Blue Slope", "Blue", 1.0f, 0.0f, 4.0f, 0.005f);
    slope_blue_->set_redraw_viewport_on_change(true);
    slope_blue_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    slope_blue_->set_role_data(module::Attribute::ToolTip, "Blue slope");
    slope_blue_->expose_in_ui_attrs_group("grading_settings");
    slope_blue_->expose_in_ui_attrs_group("grading_slope");
    slope_blue_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 0.0f, 1.0f));

    slope_master_ = add_float_attribute(
        "Master Slope", "Master", 1.0f, std::pow(2.0, -6.0), std::pow(2.0, 6.0), 0.005f);
    slope_master_->set_redraw_viewport_on_change(true);
    slope_master_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    slope_master_->set_role_data(module::Attribute::ToolTip, "Master slope");
    slope_master_->expose_in_ui_attrs_group("grading_settings");
    slope_master_->expose_in_ui_attrs_group("grading_slope");
    slope_master_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    // Offset
    offset_red_ = add_float_attribute("Red Offset", "Red", 0.0f, -0.2f, 0.2f, 0.005f);
    offset_red_->set_redraw_viewport_on_change(true);
    offset_red_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    offset_red_->set_role_data(module::Attribute::ToolTip, "Red offset");
    offset_red_->expose_in_ui_attrs_group("grading_settings");
    offset_red_->expose_in_ui_attrs_group("grading_offset");
    offset_red_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 0.0f, 0.0f));

    offset_green_ = add_float_attribute("Green Offset", "Green", 0.0f, -0.2f, 0.2f, 0.005f);
    offset_green_->set_redraw_viewport_on_change(true);
    offset_green_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    offset_green_->set_role_data(module::Attribute::ToolTip, "Green offset");
    offset_green_->expose_in_ui_attrs_group("grading_settings");
    offset_green_->expose_in_ui_attrs_group("grading_offset");
    offset_green_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 1.0f, 0.0f));

    offset_blue_ = add_float_attribute("Blue Offset", "Blue", 0.0f, -0.2f, 0.2f, 0.005f);
    offset_blue_->set_redraw_viewport_on_change(true);
    offset_blue_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    offset_blue_->set_role_data(module::Attribute::ToolTip, "Blue offset");
    offset_blue_->expose_in_ui_attrs_group("grading_settings");
    offset_blue_->expose_in_ui_attrs_group("grading_offset");
    offset_blue_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 0.0f, 1.0f));

    offset_master_ = add_float_attribute("Master Offset", "Master", 0.0f, -0.2f, 0.2f, 0.005f);
    offset_master_->set_redraw_viewport_on_change(true);
    offset_master_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    offset_master_->set_role_data(module::Attribute::ToolTip, "Master offset");
    offset_master_->expose_in_ui_attrs_group("grading_settings");
    offset_master_->expose_in_ui_attrs_group("grading_offset");
    offset_master_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    // Power
    power_red_ = add_float_attribute("Red Power", "Red", 1.0f, 0.2f, 4.0f, 0.005f);
    power_red_->set_redraw_viewport_on_change(true);
    power_red_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    power_red_->set_role_data(module::Attribute::ToolTip, "Red power");
    power_red_->expose_in_ui_attrs_group("grading_settings");
    power_red_->expose_in_ui_attrs_group("grading_power");
    power_red_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 0.0f, 0.0f));

    power_green_ = add_float_attribute("Green Power", "Green", 1.0f, 0.2f, 4.0f, 0.005f);
    power_green_->set_redraw_viewport_on_change(true);
    power_green_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    power_green_->set_role_data(module::Attribute::ToolTip, "Green power");
    power_green_->expose_in_ui_attrs_group("grading_settings");
    power_green_->expose_in_ui_attrs_group("grading_power");
    power_green_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 1.0f, 0.0f));

    power_blue_ = add_float_attribute("Blue Power", "Blue", 1.0f, 0.2f, 4.0f, 0.005f);
    power_blue_->set_redraw_viewport_on_change(true);
    power_blue_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    power_blue_->set_role_data(module::Attribute::ToolTip, "Blue power");
    power_blue_->expose_in_ui_attrs_group("grading_settings");
    power_blue_->expose_in_ui_attrs_group("grading_power");
    power_blue_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(0.0f, 0.0f, 1.0f));

    power_master_ = add_float_attribute("Master Power", "Master", 1.0f, 0.2f, 4.0f, 0.005f);
    power_master_->set_redraw_viewport_on_change(true);
    power_master_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    power_master_->set_role_data(module::Attribute::ToolTip, "Master power");
    power_master_->expose_in_ui_attrs_group("grading_settings");
    power_master_->expose_in_ui_attrs_group("grading_power");
    power_master_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    // Basic controls
    // These directly maps to the CDL parameters above

    basic_exposure_ =
        add_float_attribute("Basic Exposure", "Exposure", 0.0f, -6.0f, 6.0f, 0.1f);
    basic_exposure_->set_redraw_viewport_on_change(true);
    basic_exposure_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    basic_exposure_->set_role_data(module::Attribute::ToolTip, "Exposure");
    basic_exposure_->expose_in_ui_attrs_group("grading_settings");
    basic_exposure_->expose_in_ui_attrs_group("grading_simple");
    basic_exposure_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    basic_offset_ = add_float_attribute("Basic Offset", "Offset", 0.0f, -0.2f, 0.2f, 0.005f);
    basic_offset_->set_redraw_viewport_on_change(true);
    basic_offset_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    basic_offset_->set_role_data(module::Attribute::ToolTip, "Offset");
    basic_offset_->expose_in_ui_attrs_group("grading_settings");
    basic_offset_->expose_in_ui_attrs_group("grading_simple");
    basic_offset_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    basic_power_ = add_float_attribute("Basic Power", "Gamma", 1.0f, 0.2f, 4.0f, 0.005f);
    basic_power_->set_redraw_viewport_on_change(true);
    basic_power_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    basic_power_->set_role_data(module::Attribute::ToolTip, "Gamma");
    basic_power_->expose_in_ui_attrs_group("grading_settings");
    basic_power_->expose_in_ui_attrs_group("grading_simple");
    basic_power_->set_role_data(
        module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    // Sat
    sat_ = add_float_attribute("Saturation", "Sat", 1.0f, 0.0f, 4.0f, 0.005f);
    sat_->set_redraw_viewport_on_change(true);
    sat_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    sat_->set_role_data(module::Attribute::ToolTip, "Saturation");
    sat_->expose_in_ui_attrs_group("grading_settings");
    sat_->expose_in_ui_attrs_group("grading_saturation");
    sat_->expose_in_ui_attrs_group("grading_simple");
    sat_->set_role_data(module::Attribute::Colour, utility::ColourTriplet(1.0f, 1.0f, 1.0f));

    // Masking elements

    drawing_tool_ = add_string_choice_attribute(
        "drawing_tool",
        "drawing_tool",
        utility::map_value_to_vec(drawing_tool_names_).front(),
        utility::map_value_to_vec(drawing_tool_names_));
    drawing_tool_->expose_in_ui_attrs_group("mask_tool_settings");
    drawing_tool_->expose_in_ui_attrs_group("mask_tool_types");

    draw_pen_size_ = add_integer_attribute("Draw Pen Size", "Draw Pen Size", 10, 1, 300);
    draw_pen_size_->expose_in_ui_attrs_group("mask_tool_settings");
    draw_pen_size_->set_preference_path("/plugin/grading/draw_pen_size");

    erase_pen_size_ = add_integer_attribute("Erase Pen Size", "Erase Pen Size", 80, 1, 300);
    erase_pen_size_->expose_in_ui_attrs_group("mask_tool_settings");
    erase_pen_size_->set_preference_path("/plugin/grading/erase_pen_size");

    pen_colour_ = add_colour_attribute(
        "Pen Colour", "Pen Colour", utility::ColourTriplet(0.5f, 0.4f, 1.0f));
    pen_colour_->expose_in_ui_attrs_group("mask_tool_settings");
    pen_colour_->set_preference_path("/plugin/grading/pen_colour");

    pen_opacity_ = add_integer_attribute("Pen Opacity", "Pen Opacity", 100, 0, 100);
    pen_opacity_->expose_in_ui_attrs_group("mask_tool_settings");
    pen_opacity_->set_preference_path("/plugin/grading/pen_opacity");

    pen_softness_ = add_integer_attribute("Pen Softness", "Pen Softness", 0, 0, 100);
    pen_softness_->expose_in_ui_attrs_group("mask_tool_settings");
    pen_softness_->set_preference_path("/plugin/grading/pen_softness");

    display_mode_attribute_ = add_string_choice_attribute(
        "display_mode",
        "display_mode",
        utility::map_value_to_vec(display_mode_names_).front(),
        utility::map_value_to_vec(display_mode_names_));
    display_mode_attribute_->expose_in_ui_attrs_group("mask_tool_settings");
    display_mode_attribute_->set_preference_path("/plugin/grading/display_mode");

    // This allows for quick toggle between masking & layering options enabled or disabled
    mvp_1_release_ = add_boolean_attribute("mvp_1_release", "mvp_1_release", true);
    mvp_1_release_->expose_in_ui_attrs_group("grading_settings");

    make_behavior();
    listen_to_playhead_events(true);

    reset_grade_layers();

    // we have to maintain a list of GradingColourOperator instances that are
    // alive to send them messages about our state (currently only the state
    // of the bypass attr)
    set_down_handler([=](down_msg &msg) {
        auto it = grading_colour_op_actors_.begin();
        while (it != grading_colour_op_actors_.end()) {
            if (msg.source == *it) {
                it = grading_colour_op_actors_.erase(it);
            } else {
                it++;
            }
        }
    });
}

utility::BlindDataObjectPtr GradingTool::prepare_overlay_data(
    const media_reader::ImageBufPtr &image, const bool offscreen) const {

    // This callback is made just before viewport redraw. We want to check
    // if the image to be drawn is from the same media to which a grade is
    // currently being edited by us. If so, we attach up-to-date data on
    // the edited grade for display.

    if (!grading_data_.identity() && image) {

        bool we_are_editing_grade_on_this_image = false;
        for (auto &bookmark : image.bookmarks()) {
            if (bookmark->detail_.uuid_ == grading_data_.bookmark_uuid_) {
                we_are_editing_grade_on_this_image = true;
                break;
            }
        }
        if (we_are_editing_grade_on_this_image) {

            auto render_data = std::make_shared<GradingMaskRenderData>();

            // N.B. this means we copy the entirity of grading_data_ (it's strokes
            // basically) on every redraw. Should be ok in the wider scheme of
            // things but not exactly efficient. Another approach would be making
            // GradingData thread safe (Canvas class already is) and share a
            // reference/pointer to grading_data_ here so when drawing happens we're
            // using the interaction member data of this class.
            render_data->interaction_grading_data_ = grading_data_;
            return render_data;
        }
    }
    return utility::BlindDataObjectPtr();
}

AnnotationBasePtr GradingTool::build_annotation(const utility::JsonStore &data) {

    return std::make_shared<GradingData>(data);
}

void GradingTool::images_going_on_screen(
    const std::vector<media_reader::ImageBufPtr> &images,
    const std::string viewport_name,
    const bool playhead_playing) {

    // this callback happens just before every viewport refresh

    // for now, we only care about monitoring what's going on
    // in the main viewport
    if (viewport_name == "viewport0") {
        if (images.size()) {

            current_on_screen_frame_  = images[0];
            int n                     = 0;
            GradingData *grading_data = nullptr;
            for (auto &bookmark : images[0].bookmarks()) {

                auto data = dynamic_cast<GradingData *>(bookmark->annotation_.get());
                if (data && !grading_data) {
                    grading_data = data;
                    n++;
                } else if (data) {
                    n++;
                }
            }

            if (n > 1) {
                spdlog::warn("Only one grading bookmark can be active at once, found {}", n);
            }

            if (grading_data && grading_data->bookmark_uuid_ != grading_data_.bookmark_uuid_) {

                // there is a grade attached to the image but its not the one
                // that we have been editing. Load the data for the new incoming
                // grade ready for us to edit it.
                load_grade_layers(grading_data);

            } else if (
                !grading_data && !grading_data_.identity() &&
                current_on_screen_frame_ != grading_data_creation_frame_) {

                // we have been editing a grade but there is no grading data for
                // the on screen frame and the frame has changed since we
                // created the edited grade. Thus we clear the edited grade as
                // the playhead must have moved off the media that we had been
                // grading
                reset_grade_layers();
            }
        }
    }
}

void GradingTool::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {

    if (attribute_uuid == tool_is_active_->uuid()) {

        if (tool_is_active_->value()) {
            if (drawing_tool_->value() == "None")
                drawing_tool_->set_value("Draw");
            grab_mouse_focus();
        } else {
            release_mouse_focus();
            release_keyboard_focus();
            end_drawing();
        }

    } else if (attribute_uuid == mask_is_active_->uuid()) {

        if (mask_is_active_->value()) {
            if (drawing_tool_->value() == "None") {
                drawing_tool_->set_value("Draw");
            }
            grab_mouse_focus();

        } else {
            release_mouse_focus();
            release_keyboard_focus();
            end_drawing();
        }

        refresh_current_layer_from_ui();

    } else if (attribute_uuid == grading_action_->uuid() && grading_action_->value() != "") {

        if (grading_action_->value() == "Clear") {

            clear_cdl();
            refresh_current_layer_from_ui();

        } else if (utility::starts_with(grading_action_->value(), "Save CDL ")) {

            std::size_t prefix_length = std::string("Save CDL ").size();
            std::string filepath      = grading_action_->value().substr(
                prefix_length, grading_action_->value().size() - prefix_length);
            save_cdl(filepath);

        } else if (grading_action_->value() == "Prev Layer") {

            toggle_grade_layer(active_layer_ - 1);

        } else if (grading_action_->value() == "Next Layer") {

            toggle_grade_layer(active_layer_ + 1);

        } else if (grading_action_->value() == "Add Layer") {

            add_grade_layer();

        } else if (grading_action_->value() == "Remove Layer") {

            delete_grade_layer();
        }

        grading_action_->set_value("");

    } else if (

        attribute_uuid == drawing_action_->uuid() && drawing_action_->value() != "") {

        if (drawing_action_->value() == "Clear") {
            clear_mask();
        } else if (drawing_action_->value() == "Undo") {
            undo();
        } else if (drawing_action_->value() == "Redo") {
            redo();
        }
        drawing_action_->set_value("");

    } else if (attribute_uuid == drawing_tool_->uuid()) {

        if (tool_is_active_->value()) {

            if (drawing_tool_->value() == "None") {
                release_mouse_focus();
            } else {
                grab_mouse_focus();
            }

            end_drawing();
            release_keyboard_focus();
        }

    } else if (attribute_uuid == display_mode_attribute_->uuid()) {

        refresh_current_layer_from_ui();

    } else if (attribute_uuid == grading_bypass_->uuid()) {

        for (auto &a : grading_colour_op_actors_) {
            send(a, utility::event_atom_v, "bypass", grading_bypass_->value());
        }

    } else if (
        (slope_red_ && slope_green_ && slope_blue_ && slope_master_) &&
        (offset_red_ && offset_green_ && offset_blue_ && offset_master_) &&
        (power_red_ && power_green_ && power_blue_ && power_master_) &&
        (basic_power_ && basic_offset_ && basic_exposure_) && (sat_) &&
        (attribute_uuid == slope_red_->uuid() || attribute_uuid == slope_green_->uuid() ||
         attribute_uuid == slope_blue_->uuid() || attribute_uuid == slope_master_->uuid() ||
         attribute_uuid == offset_red_->uuid() || attribute_uuid == offset_green_->uuid() ||
         attribute_uuid == offset_blue_->uuid() || attribute_uuid == offset_master_->uuid() ||
         attribute_uuid == power_red_->uuid() || attribute_uuid == power_green_->uuid() ||
         attribute_uuid == power_blue_->uuid() || attribute_uuid == power_master_->uuid() ||
         attribute_uuid == basic_power_->uuid() || attribute_uuid == basic_offset_->uuid() ||
         attribute_uuid == basic_exposure_->uuid() || attribute_uuid == sat_->uuid())) {

        // Make sure basic controls are in sync
        if (attribute_uuid == basic_power_->uuid() || attribute_uuid == basic_offset_->uuid() ||
            attribute_uuid == basic_exposure_->uuid()) {

            slope_master_->set_value(std::pow(2.0, basic_exposure_->value()), false);
            offset_master_->set_value(basic_offset_->value(), false);
            power_master_->set_value(basic_power_->value(), false);

        } else if (
            attribute_uuid == slope_master_->uuid() ||
            attribute_uuid == offset_master_->uuid() ||
            attribute_uuid == power_master_->uuid()) {

            basic_exposure_->set_value(std::log2(slope_master_->value()), false);
            basic_offset_->set_value(offset_master_->value(), false);
            basic_power_->set_value(power_master_->value(), false);
        }

        refresh_current_layer_from_ui();
        create_bookmark();
        save_bookmark();
    }

    redraw_viewport();
}

void GradingTool::register_hotkeys() {

    toggle_active_hotkey_ = register_hotkey(
        int('G'),
        ui::ControlModifier,
        "Toggle Grading Tool",
        "Show or hide the grading toolbox");

    toggle_mask_hotkey_ = register_hotkey(
        int('M'),
        ui::NoModifier,
        "Toggle masking",
        "Use drawing tools to apply a matte or apply grading to whole frame");

    undo_hotkey_ = register_hotkey(
        int('Z'),
        ui::ControlModifier,
        "Undo (Annotation edit)",
        "Undoes your last edits to an annotation");

    redo_hotkey_ = register_hotkey(
        int('Z'),
        ui::ControlModifier | ui::ShiftModifier,
        "Redo (Annotation edit)",
        "Redoes your last undone edit on an annotation");
}

void GradingTool::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {

    if (hotkey_uuid == toggle_active_hotkey_) {

        tool_is_active_->set_value(!tool_is_active_->value());

    } else if (hotkey_uuid == toggle_mask_hotkey_ && tool_is_active_->value()) {

        mask_is_active_->set_value(!mask_is_active_->value());

    } else if (hotkey_uuid == undo_hotkey_ && tool_is_active_->value()) {

        undo();
        redraw_viewport();

    } else if (hotkey_uuid == redo_hotkey_ && tool_is_active_->value()) {

        redo();
        redraw_viewport();
    }
}

bool GradingTool::pointer_event(const ui::PointerEvent &e) {

    if (!tool_is_active_->value() || !mask_is_active_->value())
        return false;

    bool redraw = true;

    const Imath::V2f pointer_pos = e.position_in_viewport_coord_sys();

    if (drawing_tool_->value() == "Draw" || drawing_tool_->value() == "Erase") {

        if (e.type() == ui::Signature::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_stroke(pointer_pos);
        } else if (
            e.type() == ui::Signature::EventType::Drag &&
            e.buttons() == ui::Signature::Button::Left) {
            update_stroke(pointer_pos);
        } else if (e.type() == ui::Signature::EventType::ButtonRelease) {
            end_drawing();
        }
    } else {
        redraw = false;
    }

    if (redraw)
        redraw_viewport();

    return false;
}

void GradingTool::start_stroke(const Imath::V2f &point) {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    if (drawing_tool_->value() == "Draw") {
        layer->mask().start_stroke(
            pen_colour_->value(),
            draw_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_softness_->value() / 100.0,
            pen_opacity_->value() / 100.0);
    } else if (drawing_tool_->value() == "Erase") {
        layer->mask().start_erase_stroke(erase_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE);
    }

    update_stroke(point);
}

void GradingTool::update_stroke(const Imath::V2f &point) {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    layer->mask().update_stroke(point);
}

void GradingTool::end_drawing() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    layer->mask().end_draw();
    save_bookmark();
}

void GradingTool::undo() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    if (mask_is_active_->value()) {

        layer->mask().undo();
    }

    // TODO: Support undo / redo for grading
}

void GradingTool::redo() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    if (mask_is_active_->value()) {

        layer->mask().redo();
    }

    // TODO: Support undo / redo for grading
}

void GradingTool::clear_mask() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    layer->mask().clear();
}

void GradingTool::clear_cdl() {

    slope_red_->set_value(slope_red_->get_role_data<float>(module::Attribute::DefaultValue));
    slope_green_->set_value(
        slope_green_->get_role_data<float>(module::Attribute::DefaultValue));
    slope_blue_->set_value(slope_blue_->get_role_data<float>(module::Attribute::DefaultValue));
    slope_master_->set_value(
        slope_master_->get_role_data<float>(module::Attribute::DefaultValue));

    offset_red_->set_value(offset_red_->get_role_data<float>(module::Attribute::DefaultValue));
    offset_green_->set_value(
        offset_green_->get_role_data<float>(module::Attribute::DefaultValue));
    offset_blue_->set_value(
        offset_blue_->get_role_data<float>(module::Attribute::DefaultValue));
    offset_master_->set_value(
        offset_master_->get_role_data<float>(module::Attribute::DefaultValue));

    power_red_->set_value(power_red_->get_role_data<float>(module::Attribute::DefaultValue));
    power_green_->set_value(
        power_green_->get_role_data<float>(module::Attribute::DefaultValue));
    power_blue_->set_value(power_blue_->get_role_data<float>(module::Attribute::DefaultValue));
    power_master_->set_value(
        power_master_->get_role_data<float>(module::Attribute::DefaultValue));

    basic_exposure_->set_value(
        basic_exposure_->get_role_data<float>(module::Attribute::DefaultValue));
    basic_offset_->set_value(
        basic_offset_->get_role_data<float>(module::Attribute::DefaultValue));
    basic_power_->set_value(
        basic_power_->get_role_data<float>(module::Attribute::DefaultValue));

    sat_->set_value(sat_->get_role_data<float>(module::Attribute::DefaultValue));
}

void GradingTool::save_cdl(const std::string &filepath) const {

    OCIO::CDLTransformRcPtr cdl = OCIO::CDLTransform::Create();

    std::array<double, 3> slope{
        slope_red_->value() * slope_master_->value(),
        slope_green_->value() * slope_master_->value(),
        slope_blue_->value() * slope_master_->value()};
    std::array<double, 3> offset{
        offset_red_->value() + offset_master_->value(),
        offset_green_->value() + offset_master_->value(),
        offset_blue_->value() + offset_master_->value()};
    std::array<double, 3> power{
        power_red_->value() * power_master_->value(),
        power_green_->value() * power_master_->value(),
        power_blue_->value() * power_master_->value()};

    cdl->setSlope(slope.data());
    cdl->setOffset(offset.data());
    cdl->setPower(power.data());
    cdl->setSat(sat_->value());

    OCIO::FormatMetadata &metadata = cdl->getFormatMetadata();
    metadata.setID("0");

    OCIO::GroupTransformRcPtr grp = OCIO::GroupTransform::Create();
    grp->appendTransform(cdl);

    // Write to disk using OCIO

    std::string localpath = filepath;
    localpath             = utility::replace_once(localpath, "file://", "");

    std::string format;
    if (utility::ends_with(localpath, "cdl")) {
        format = "ColorDecisionList";
    } else if (utility::ends_with(localpath, "cc")) {
        format = "ColorCorrection";
    } else if (utility::ends_with(localpath, "ccc")) {
        format = "ColorCorrectionCollection";
    }

    std::ofstream ofs(localpath);
    if (ofs.is_open()) {
        grp->write(OCIO::GetCurrentConfig(), format.c_str(), ofs);
    } else {
        spdlog::warn("Failed to create file: {}", localpath);
    }
}

void GradingTool::load_grade_layers(GradingData *grading_data) {

    // Load layer(s)

    grading_data_ = *grading_data;
    active_layer_ = grading_data_.size() - 1;

    // Shader (re) construction


    // Update UI

    std::vector<std::string> layer_choices;
    for (int i = 0; i < grading_data_.size(); ++i) {
        layer_choices.push_back(fmt::format("Layer {}", i + 1));
    }
    grading_layer_->set_role_data(module::Attribute::StringChoices, layer_choices, false);
    grading_layer_->set_value(layer_choices.back(), false);

    refresh_ui_from_current_layer();
}

void GradingTool::reset_grade_layers() {

    grading_data_ = GradingData();
    grading_layer_->set_role_data(
        module::Attribute::StringChoices, std::vector<std::string>(), false);
    grading_layer_->set_value("", false);
    grading_data_creation_frame_ = media_reader::ImageBufPtr();
    add_grade_layer();
}

void GradingTool::add_grade_layer() {

    if (grading_data_.size() >= maximum_layers_) {
        spdlog::warn("Maximum number of layers reached ({})", maximum_layers_);
        return;
    }

    // Add layer on top

    active_layer_ = grading_data_.size();
    grading_data_.push_layer();

    // Update UI

    auto layer_name = std::string(fmt::format("Layer {}", active_layer_ + 1));

    auto layer_choices = grading_layer_->get_role_data<std::vector<std::string>>(
        module::Attribute::StringChoices);
    layer_choices.push_back(layer_name);
    grading_layer_->set_role_data(module::Attribute::StringChoices, layer_choices, false);
    grading_layer_->set_value(layer_choices.back(), false);

    refresh_ui_from_current_layer();
}

void GradingTool::toggle_grade_layer(size_t layer) {

    if (layer >= grading_data_.size() || layer < 0) {
        spdlog::warn("Trying to toggle to non-existing layer {}", layer);
        return;
    }

    active_layer_ = layer;

    // Update UI

    auto layer_name = std::string(fmt::format("Layer {}", active_layer_ + 1));
    grading_layer_->set_value(layer_name, false);

    refresh_ui_from_current_layer();
}

void GradingTool::delete_grade_layer() {

    if (grading_data_.size() < 2) {
        spdlog::warn("Can't delete base grade layer");
        return;
    }

    // Delete top layer

    grading_data_.pop_layer();
    active_layer_ = grading_data_.size() - 1;

    // Update UI

    auto layer_choices = grading_layer_->get_role_data<std::vector<std::string>>(
        module::Attribute::StringChoices);
    layer_choices.pop_back();
    grading_layer_->set_role_data(module::Attribute::StringChoices, layer_choices, false);
    grading_layer_->set_value(layer_choices.back(), false);

    refresh_ui_from_current_layer();
}

ui::viewport::LayerData *GradingTool::current_layer() {

    return grading_data_.layer(active_layer_);
}

void GradingTool::refresh_current_layer_from_ui() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    auto &grade = layer->grade();

    grade.slope = {
        slope_red_->value(),
        slope_green_->value(),
        slope_blue_->value(),
        basic_exposure_->value()};
    grade.offset = {
        offset_red_->value(),
        offset_green_->value(),
        offset_blue_->value(),
        offset_master_->value()};
    grade.power = {
        power_red_->value(),
        power_green_->value(),
        power_blue_->value(),
        power_master_->value()};
    grade.sat = sat_->value();

    layer->set_mask_active(mask_is_active_->value());
    layer->set_mask_editing(display_mode_attribute_->value() == "Mask");
}

void GradingTool::refresh_ui_from_current_layer() {

    LayerData *layer = current_layer();
    if (!layer) {
        return;
    }

    auto &grade = layer->grade();

    slope_red_->set_value(grade.slope[0], false);
    slope_green_->set_value(grade.slope[1], false);
    slope_blue_->set_value(grade.slope[2], false);
    slope_master_->set_value(std::pow(2.0, grade.slope[3]), false);
    basic_exposure_->set_value(grade.slope[3], false);

    offset_red_->set_value(grade.offset[0], false);
    offset_green_->set_value(grade.offset[1], false);
    offset_blue_->set_value(grade.offset[2], false);
    offset_master_->set_value(grade.offset[3], false);
    basic_offset_->set_value(grade.offset[3], false);

    power_red_->set_value(grade.power[0], false);
    power_green_->set_value(grade.power[1], false);
    power_blue_->set_value(grade.power[2], false);
    power_master_->set_value(grade.power[3], false);
    basic_power_->set_value(grade.power[3], false);

    sat_->set_value(grade.sat, false);

    // mask_is_active_->set_value(layer->mask_active());
    display_mode_attribute_->set_value(layer->mask_editing() ? "Mask" : "Grade");
}

utility::Uuid GradingTool::current_bookmark() const { return grading_data_.bookmark_uuid_; }

void GradingTool::create_bookmark() {

    if (current_bookmark().is_null()) {

        bookmark::BookmarkDetail bmd;
        /*std::string name = on_screen_media_name_;
        if (name.rfind("/") != std::string::npos) {
            name = std::string(name, name.rfind("/") + 1);
        }
        std::ostringstream oss;
        oss << name << " grading @ " << media_logical_frame_;
        bmd.subject_ = oss.str();*/

        // Hides bookmark from timeline
        bmd.colour_  = "transparent";
        bmd.visible_ = false;

        grading_data_.bookmark_uuid_ = StandardPlugin::create_bookmark_on_current_media(
            "viewport0", "Grading Note", bmd, true);
        grading_data_creation_frame_ = current_on_screen_frame_;

        // StandardPlugin::update_bookmark_detail(grading_data_.bookmark_uuid_, bmd);
    }
}

void GradingTool::save_bookmark() {

    if (current_bookmark()) {

        StandardPlugin::update_bookmark_annotation(
            current_bookmark(),
            std::make_shared<GradingData>(grading_data_),
            grading_data_.identity() // this will delete the bookmark if true
        );
        if (grading_data_.identity()) {
            reset_grade_layers();
        }
    }
}

caf::message_handler GradingTool::message_handler_extensions() {
    return caf::message_handler({[=](const std::string &desc, caf::actor grading_colour_op) {
               if (desc == "follow_bypass") {
                   grading_colour_op_actors_.push_back(grading_colour_op);
                   monitor(grading_colour_op);
                   send(
                       grading_colour_op,
                       utility::event_atom_v,
                       "bypass",
                       grading_bypass_->value());
               }
           }})
        .or_else(StandardPlugin::message_handler_extensions());
}


static std::vector<std::shared_ptr<plugin_manager::PluginFactory>> factories(
    {std::make_shared<plugin_manager::PluginFactoryTemplate<GradingTool>>(
         GradingTool::PLUGIN_UUID,
         "GradingToolUI",
         plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
         true,
         "Remi Achard",
         "Plugin providing interface for creating interactive grading notes with painted "
         "masks.",
         semver::version("0.0.0"),
         "",
         ""),
     std::make_shared<plugin_manager::PluginFactoryTemplate<GradingColourOperator>>(
         GradingColourOperator::PLUGIN_UUID,
         "GradingToolColourOp",
         plugin_manager::PluginFlags::PF_COLOUR_OPERATION,
         false,
         "Remi Achard",
         "Colour operator to apply CDL with optional painted masking in viewport.")});

#define PLUGIN_DECLARE_END()                                                                   \
    extern "C" {                                                                               \
    plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {                 \
        return new plugin_manager::PluginFactoryCollection(                                    \
            std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(factories));           \
    }                                                                                          \
    }

PLUGIN_DECLARE_END()