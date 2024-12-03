// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"

#include "grading.h"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_colour_op.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::colour_pipeline;
using namespace xstudio::ui::viewport;


namespace {

std::vector<float> array4d_to_vector4f(const std::array<double, 4> &arr) {
    return std::vector<float>{
        static_cast<float>(arr[0]),
        static_cast<float>(arr[1]),
        static_cast<float>(arr[2]),
        static_cast<float>(arr[3])};
}

std::array<double, 4> vector4f_to_array4d(const std::vector<float> &vec) {
    return std::array<double, 4>{vec[0], vec[1], vec[2], vec[3]};
}

} // anonymous namespace


GradingTool::GradingTool(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "GradingTool", init_settings) {

    // General

    tool_opened_count_ = add_integer_attribute("tool_opened_count", "tool_opened_count", 0);
    tool_opened_count_->expose_in_ui_attrs_group("grading_settings");

    grading_action_ = add_string_attribute("grading_action", "grading_action", "");
    grading_action_->expose_in_ui_attrs_group("grading_settings");

    grading_bypass_ = add_boolean_attribute("grading_bypass", "grading_bypass", false);
    grading_bypass_->expose_in_ui_attrs_group("grading_settings");

    media_colour_managed_ =
        add_boolean_attribute("media_colour_managed", "media_colour_managed", false);
    media_colour_managed_->expose_in_ui_attrs_group("grading_settings");

    // Grading elements

    grading_bookmark_ = add_string_attribute("grading_bookmark", "grading_bookmark", "");
    grading_bookmark_->expose_in_ui_attrs_group("grading_settings");

    colour_space_ = add_string_choice_attribute(
        "colour_space", "colour_space", "scene_linear", {"scene_linear", "compositing_log"});
    colour_space_->expose_in_ui_attrs_group("grading_settings");

    working_space_ = add_string_attribute("working_space", "working_space", "");
    working_space_->expose_in_ui_attrs_group("grading_settings");

    grade_in_ = add_integer_attribute("grade_in", "grade_in", -1);
    grade_in_->expose_in_ui_attrs_group("grading_settings");

    grade_out_ = add_integer_attribute("grade_out", "grade_out", -1);
    grade_out_->expose_in_ui_attrs_group("grading_settings");

    // Slope
    slope_ = add_float_vector_attribute(
        "Slope",
        "Slope",
        std::vector<float>({1.0, 1.0, 1.0, 1.0}),        // initial value
        std::vector<float>({0.0, 0.0, 0.0, 0.0}),        // min
        std::vector<float>({4.0, 4.0, 4.0, 4.0}),        // max
        std::vector<float>({0.005, 0.005, 0.005, 0.005}) // step
    );
    slope_->expose_in_ui_attrs_group("grading_settings");
    slope_->expose_in_ui_attrs_group("grading_wheels");

    // Offset
    offset_ = add_float_vector_attribute(
        "Offset",
        "Offset",
        std::vector<float>({0.0, 0.0, 0.0, 0.0}),        // initial value
        std::vector<float>({-0.2, -0.2, -0.2, -0.2}),    // min
        std::vector<float>({0.2, 0.2, 0.2, 0.2}),        // max
        std::vector<float>({0.005, 0.005, 0.005, 0.005}) // step
    );
    offset_->expose_in_ui_attrs_group("grading_settings");
    offset_->expose_in_ui_attrs_group("grading_wheels");

    // Power
    power_ = add_float_vector_attribute(
        "Power",
        "Power",
        std::vector<float>({1.0, 1.0, 1.0, 1.0}),        // initial value
        std::vector<float>({0.2, 0.2, 0.2, 0.2}),        // min
        std::vector<float>({4.0, 4.0, 4.0, 4.0}),        // max
        std::vector<float>({0.005, 0.005, 0.005, 0.005}) // step
    );
    power_->expose_in_ui_attrs_group("grading_settings");
    power_->expose_in_ui_attrs_group("grading_wheels");

    // Sat
    saturation_ = add_float_attribute("Saturation", "Sat.", 1.0f, 0.0f, 4.0f, 0.005f);
    saturation_->set_redraw_viewport_on_change(true);
    saturation_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    saturation_->expose_in_ui_attrs_group("grading_settings");
    saturation_->expose_in_ui_attrs_group("grading_sliders");

    // Exposure
    exposure_ = add_float_attribute("Exposure", "Exp.", 0.0f, -8.0f, 8.0f, 0.1f);
    exposure_->set_redraw_viewport_on_change(true);
    exposure_->set_role_data(module::Attribute::DefaultValue, 0.0f);
    exposure_->expose_in_ui_attrs_group("grading_settings");
    exposure_->expose_in_ui_attrs_group("grading_sliders");

    // Contrast
    contrast_ = add_float_attribute("Contrast", "Cont.", 1.0f, 0.001f, 4.0f, 0.005f);
    contrast_->set_redraw_viewport_on_change(true);
    contrast_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    contrast_->expose_in_ui_attrs_group("grading_settings");
    contrast_->expose_in_ui_attrs_group("grading_sliders");

    // Masking elements

    drawing_tool_ = add_string_choice_attribute(
        "drawing_tool",
        "drawing_tool",
        "Shape",
        utility::map_value_to_vec(drawing_tool_names_));
    drawing_tool_->expose_in_ui_attrs_group("mask_tool_settings");
    drawing_tool_->expose_in_ui_attrs_group("mask_tool_types");

    // Not using preference for drawing_tool attr for now, as it should
    // be set to 'Shape' always as mask painting is disabled
    // drawing_tool_->set_preference_path("/plugin/grading/drawing_tool");

    drawing_action_ = add_string_attribute("drawing_action", "drawing_action", "");
    drawing_action_->expose_in_ui_attrs_group("grading_settings");
    drawing_action_->expose_in_ui_attrs_group("mask_tool_settings");

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

    pen_softness_ = add_integer_attribute("Pen Softness", "Pen Softness", 100, 0, 500);
    pen_softness_->expose_in_ui_attrs_group("mask_tool_settings");
    pen_softness_->set_preference_path("/plugin/grading/pen_softness");

    shape_invert_ = add_boolean_attribute("shape_invert", "shape_invert", false);
    shape_invert_->set_redraw_viewport_on_change(true);
    shape_invert_->expose_in_ui_attrs_group("mask_tool_settings");

    polygon_init_ = add_boolean_attribute("polygon_init", "polygon_init", false);
    polygon_init_->set_redraw_viewport_on_change(true);
    polygon_init_->expose_in_ui_attrs_group("mask_tool_settings");

    mask_selected_shape_ =
        add_integer_attribute("mask_selected_shape", "mask_selected_shape", -1);
    mask_selected_shape_->expose_in_ui_attrs_group("mask_tool_settings");

    mask_shapes_visible_ =
        add_boolean_attribute("mask_shapes_visible", "mask_shapes_visible", true);
    mask_shapes_visible_->set_redraw_viewport_on_change(true);
    mask_shapes_visible_->expose_in_ui_attrs_group("mask_tool_settings");

    display_mode_attribute_ = add_string_choice_attribute(
        "display_mode",
        "display_mode",
        utility::map_value_to_vec(display_mode_names_).front(),
        utility::map_value_to_vec(display_mode_names_));
    display_mode_attribute_->expose_in_ui_attrs_group("mask_tool_settings");
    display_mode_attribute_->set_preference_path("/plugin/grading/display_mode");

    make_behavior();
    listen_to_playhead_events(true);

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

    connect_to_ui();

    // Register the QML code that instances the grading tool UI in an
    // xstudio 'panel'. It will show as 'Grading Tools' under the tabs
    register_ui_panel_qml(
        "Grading Tools",
        R"(
            import QtGraphicalEffects 1.15
            import QtQuick 2.15
            import Grading 2.0

            import xStudio 1.0

            Item {
                anchors.fill: parent

                XsGradientRectangle{
                    anchors.fill: parent
                }

                GradingTools {
                    anchors.fill: parent
                }
            }
        )",
        6.0f,
        "qrc:/icons/instant_mix.svg",
        2.0f);

    // here is where we declare the singleton overlay item that will draw
    // our graphics and handle mouse interaction
    qml_viewport_overlay_code(
        R"(
            import Grading 2.0

            GradingOverlay {
                anchors.fill: parent
            }
        )");
}

utility::BlindDataObjectPtr GradingTool::onscreen_render_data(
    const media_reader::ImageBufPtr &image, const std::string & /*viewport_name*/) const {

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
    const media_reader::ImageBufDisplaySetPtr &images,
    const std::string viewport_name,
    const bool playhead_playing) {

    // Only care about the main viewport(s), lightweight viewport will
    // be named like quick_viewport_n.
    if (!utility::starts_with(viewport_name, "viewport")) {
        return;
    }

    // It's useful to keep a hold of the images that are on-screen so if the
    // user starts drawing when there is a bookmark on screen then we can
    // add the strokes to that existing bookmark instead of making a brand
    // new note
    if (!playhead_playing && images) {
 
        viewport_current_images_ = images;
        playhead_media_frame_ = images->hero_image().frame_id().frame() - images->hero_image().frame_id().first_frame();

    } else {
        viewport_current_images_.reset();
    }

}

void GradingTool::on_screen_media_changed(
    caf::actor media_actor,
    const utility::MediaReference &media_ref,
    const utility::JsonStore &colour_params) {

    const std::string config_name = colour_params.get_or("ocio_config", std::string(""));
    const std::string working_space =
        colour_params.get_or("working_space", std::string("scene_linear"));
    // Only medias that are fully inverted to scene_linear currently support custom colour space
    // This exclude medias that are only inverted to display_linear.
    // TODO: Detect when display_linear input space and un-tone-mapped view are used
    const bool is_unmanaged =
        config_name == "" || config_name == "__raw__" || working_space != "scene_linear";

    working_space_->set_value(working_space);
    media_colour_managed_->set_value(!is_unmanaged);

}

void GradingTool::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {

    // Only care about attribute value update.
    if (role != module::Attribute::Value)
        return;

    if (attribute_uuid == tool_opened_count_->uuid()) {


        if (tool_opened_count_->value() < 0) {
            tool_opened_count_->set_value(0);
        }

        if (!grading_tools_active()) {

            release_mouse_focus();
            release_keyboard_focus();
            end_drawing();
        }

    } else if (attribute_uuid == grading_bookmark_->uuid()) {

        utility::Uuid bookmark_uuid(grading_bookmark_->value());
        select_bookmark(bookmark_uuid);

    } else if (attribute_uuid == grading_action_->uuid() && grading_action_->value() != "") {

        if (grading_action_->value() == "Clear") {

            clear_grade();
            refresh_current_grade_from_ui();

        } else if (utility::starts_with(grading_action_->value(), "Save CDL ")) {

            std::size_t prefix_length = std::string("Save CDL ").size();
            std::string filepath      = grading_action_->value().substr(
                prefix_length, grading_action_->value().size() - prefix_length);
            save_cdl(filepath);

        } else if (grading_action_->value() == "Add CC") {

            save_bookmark();
            grading_data_ = GradingData();
            create_bookmark();
            save_bookmark();

        } else if (grading_action_->value() == "Remove CC") {

            remove_bookmark();
        }

        grading_action_->set_value("");

    } else if (attribute_uuid == drawing_action_->uuid() && drawing_action_->value() != "") {

        if (drawing_action_->value() == "Clear") {
            clear_mask();
        } else if (drawing_action_->value() == "Undo") {
            undo();
        } else if (drawing_action_->value() == "Redo") {
            redo();
        } else if (drawing_action_->value() == "Add quad") {
            create_bookmark_if_empty();
            start_quad(
                {Imath::V2f(-0.5, -0.5),
                 Imath::V2f(-0.5, 0.5),
                 Imath::V2f(0.5, 0.5),
                 Imath::V2f(0.5, -0.5)});
            update_boomark_shape(current_bookmark());
        } else if (utility::starts_with(drawing_action_->value(), "Add Polygon ")) {
            create_bookmark_if_empty();
            const auto points = utility::split(drawing_action_->value().substr(12), ',');
            if (points.size() % 2 == 0) {
                std::vector<Imath::V2f> poly_points;
                for (size_t i = 0; i < points.size(); i += 2) {
                    poly_points.push_back(
                        Imath::V2f(std::stof(points[i]), std::stof(points[i + 1])));
                }
                start_polygon(poly_points);
            }
            update_boomark_shape(current_bookmark());
        } else if (drawing_action_->value() == "Add ellipse") {
            cancel_other_drawing_tools();
            create_bookmark_if_empty();
            start_ellipse(Imath::V2f(0.0, 0.0), Imath::V2f(0.35, 0.25), 90.0);
            update_boomark_shape(current_bookmark());
        } else if (drawing_action_->value() == "Remove shape") {
            create_bookmark_if_empty();
            remove_shape(mask_selected_shape_->value());
            update_boomark_shape(current_bookmark());
        }
        drawing_action_->set_value("");

    } else if (attribute_uuid == drawing_tool_->uuid()) {

        if (grading_tools_active()) {

            if (drawing_tool_->value() == "None") {
                release_mouse_focus();
            } else {
                grab_mouse_focus();
            }

            end_drawing();
            release_keyboard_focus();
        }

    } else if (attribute_uuid == display_mode_attribute_->uuid()) {

        refresh_current_grade_from_ui();

    } else if (grade_in_ && attribute_uuid == grade_in_->uuid()) {

        auto bmd = get_bookmark_detail(current_bookmark());
        if (bmd.media_reference_) {

            auto &media = bmd.media_reference_.value();

            if (grade_in_->value() == -1) {
                grade_out_->set_value(-1, false);
            } else if (grade_out_->value() == -1) {
                grade_out_->set_value(media.frame_count(), false);
            }

            if (grade_in_->value() > grade_out_->value()) {
                grade_out_->set_value(grade_in_->value());
            }

            if (grade_in_->value() != -1 && grade_out_->value() != -1) {
                bmd.start_    = grade_in_->value() * media.rate().to_flicks();
                bmd.duration_ = std::min(
                    (grade_out_->value() - grade_in_->value()) * media.rate().to_flicks(),
                    media.frame_count() * media.rate().to_flicks());
            } else {
                bmd.start_    = timebase::k_flicks_low;
                bmd.duration_ = timebase::k_flicks_max;
            }

            update_bookmark_detail(current_bookmark(), bmd);
        }

    } else if (grade_out_ && attribute_uuid == grade_out_->uuid()) {

        auto bmd = get_bookmark_detail(current_bookmark());
        if (bmd.media_reference_) {

            auto &media = bmd.media_reference_.value();

            if (grade_out_->value() == -1) {
                grade_in_->set_value(-1, false);
            } else if (grade_in_->value() == -1) {
                grade_in_->set_value(0, false);
            }

            if (grade_out_->value() < grade_in_->value()) {
                grade_in_->set_value(grade_out_->value());
            }

            if (grade_in_->value() != -1 && grade_out_->value() != -1) {
                bmd.start_ = grade_in_->value() * media.rate().to_flicks();
                bmd.duration_ =
                    (grade_out_->value() - grade_in_->value()) * media.rate().to_flicks();
            } else {
                bmd.start_    = timebase::k_flicks_low;
                bmd.duration_ = timebase::k_flicks_max;
            }
            update_bookmark_detail(current_bookmark(), bmd);
        }

    } else if (colour_space_ && attribute_uuid == colour_space_->uuid()) {

        refresh_current_grade_from_ui();
        save_bookmark();

    } else if (attribute_uuid == grading_bypass_->uuid()) {

        for (auto &a : grading_colour_op_actors_) {
            send(a, utility::event_atom_v, "bypass", grading_bypass_->value());
        }

    } else if (
        attribute_uuid == slope_->uuid() || attribute_uuid == offset_->uuid() ||
        attribute_uuid == power_->uuid() || attribute_uuid == saturation_->uuid() ||
        attribute_uuid == exposure_->uuid() || attribute_uuid == contrast_->uuid()) {

        refresh_current_grade_from_ui();
        create_bookmark_if_empty();
        save_bookmark();

    } else if (attribute_uuid == mask_selected_shape_->uuid()) {

        // Refresh UI settings according to selected shape
        const int id = mask_selected_shape_->value();
        if (id >= 0 && id < mask_shapes_.size()) {
            auto value = mask_shapes_[id]->role_data_as_json(module::Attribute::Value);
            pen_opacity_->set_value(
                value["opacity"], false); // 2nd flag means we don't get notified
            pen_softness_->set_value(value["softness"], false);
            // TODO: Fix colour json <-> qvariant conversion
            // pen_colour_->set_value(value["colour"]);
            shape_invert_->set_value(value["invert"], false);
        }

    } else if (attribute_uuid == pen_softness_->uuid()) {

        // set softness on current shape
        const int id = mask_selected_shape_->value();
        if (id >= 0 && id < mask_shapes_.size()) {
            auto user_data = mask_shapes_[id]->role_data_as_json(module::Attribute::Value);
            user_data["softness"] = pen_softness_->value();
            mask_shapes_[id]->set_role_data(module::Attribute::Value, user_data);
        }

    } else if (attribute_uuid == pen_opacity_->uuid()) {

        // set opacity on current shape
        const int id = mask_selected_shape_->value();
        if (id >= 0 && id < mask_shapes_.size()) {
            auto user_data = mask_shapes_[id]->role_data_as_json(module::Attribute::Value);
            user_data["opacity"] = pen_opacity_->value();
            mask_shapes_[id]->set_role_data(module::Attribute::Value, user_data);
        }

    } else if (attribute_uuid == shape_invert_->uuid()) {

        // set invert on current shape
        const int id = mask_selected_shape_->value();
        if (id >= 0 && id < mask_shapes_.size()) {
            auto user_data      = mask_shapes_[id]->role_data_as_json(module::Attribute::Value);
            user_data["invert"] = shape_invert_->value();
            mask_shapes_[id]->set_role_data(module::Attribute::Value, user_data);
        }

    } else if (attribute_uuid == polygon_init_->uuid()) {

        if (polygon_init_->value()) {
            cancel_other_drawing_tools();
            grab_mouse_focus();
        } else {
            release_mouse_focus();
        }

    } else {

        for (auto &attr : mask_shapes_) {
            if (attribute_uuid == attr->uuid()) {
                auto value     = attr->role_data_as_json(module::Attribute::Value);
                auto user_data = attr->role_data_as_json(module::Attribute::UserData);

                if (value["type"] == "quad") {
                    grading_data_.mask().update_quad(
                        user_data,
                        // TODO: Fix colour json <-> qvariant conversion
                        // value["colour"],
                        pen_colour_->value(),
                        {value["bl"], value["tl"], value["tr"], value["br"]},
                        value["softness"],
                        value["opacity"],
                        value["invert"]);
                } else if (value["type"] == "polygon") {
                    grading_data_.mask().update_polygon(
                        user_data,
                        // TODO: Fix colour json <-> qvariant conversion
                        // value["colour"],
                        pen_colour_->value(),
                        value["points"],
                        value["softness"],
                        value["opacity"],
                        value["invert"]);
                } else if (value["type"] == "ellipse") {
                    grading_data_.mask().update_ellipse(
                        user_data,
                        // TODO: Fix colour json <-> qvariant conversion
                        // value["colour"],
                        pen_colour_->value(),
                        value["center"],
                        value["radius"],
                        value["angle"],
                        value["softness"],
                        value["opacity"],
                        value["invert"]);
                }

                save_bookmark();

                break;
            }
        }
    }

    redraw_viewport();
}

void GradingTool::register_hotkeys() {

    bypass_hotkey_ = register_hotkey(
        int('B'),
        ui::ControlModifier,
        "Bypass all grades",
        "Bypass all grades applied on all media");

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
    const utility::Uuid &hotkey_uuid,
    const std::string & /*context*/,
    const std::string & /*window*/) {

    // This shortcut should be active even when no grading panel is visible
    if (hotkey_uuid == bypass_hotkey_) {

        grading_bypass_->set_value(!grading_bypass_->value());
        redraw_viewport();

    } else if (hotkey_uuid == undo_hotkey_ && grading_tools_active()) {

        undo();
        redraw_viewport();

    } else if (hotkey_uuid == redo_hotkey_ && grading_tools_active()) {

        redo();
        redraw_viewport();
    }
}

bool GradingTool::pointer_event(const ui::PointerEvent &e) {

    if (!grading_tools_active())
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

void GradingTool::turn_off_overlay_interaction() {
    polygon_init_->set_value(false);
    release_mouse_focus();
    release_keyboard_focus();
    end_drawing();
}

bool GradingTool::grading_tools_active() const { return tool_opened_count_->value() > 0; }

void GradingTool::start_stroke(const Imath::V2f &point) {

    if (drawing_tool_->value() == "Draw") {
        grading_data_.mask().start_stroke(
            pen_colour_->value(),
            draw_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_softness_->value() / 100.0,
            pen_opacity_->value() / 100.0);
    } else if (drawing_tool_->value() == "Erase") {
        grading_data_.mask().start_erase_stroke(
            erase_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE);
    }

    update_stroke(point);

    create_bookmark_if_empty();
}

void GradingTool::update_stroke(const Imath::V2f &point) {

    grading_data_.mask().update_stroke(point);
}

void GradingTool::start_quad(const std::vector<Imath::V2f> &corners) {

    uint32_t id = grading_data_.mask().start_quad(pen_colour_->value(), corners);

    utility::JsonStore shape_data;
    shape_data["type"]     = "quad";
    shape_data["bl"]       = corners[0];
    shape_data["tl"]       = corners[1];
    shape_data["tr"]       = corners[2];
    shape_data["br"]       = corners[3];
    shape_data["colour"]   = pen_colour_->value();
    shape_data["softness"] = pen_softness_->value();
    shape_data["opacity"]  = 100.f;
    shape_data["invert"]   = false;

    utility::JsonStore additional_roles;
    additional_roles["user_data"] = id;

    const std::string name = fmt::format("Quad{}", id);
    mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
    expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");

    end_drawing();
}

void GradingTool::start_polygon(const std::vector<Imath::V2f> &points) {

    uint32_t id = grading_data_.mask().start_polygon(pen_colour_->value(), points);

    utility::JsonStore shape_data;
    shape_data["type"]     = "polygon";
    shape_data["points"]   = points;
    shape_data["count"]    = points.size();
    shape_data["colour"]   = pen_colour_->value();
    shape_data["softness"] = pen_softness_->value();
    shape_data["opacity"]  = 100.f;
    shape_data["invert"]   = false;

    utility::JsonStore additional_roles;
    additional_roles["user_data"] = id;

    const std::string name = fmt::format("Polygon{}", id);
    mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
    expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");

    end_drawing();
}

void GradingTool::start_ellipse(
    const Imath::V2f &center, const Imath::V2f &radius, float angle) {

    uint32_t id =
        grading_data_.mask().start_ellipse(pen_colour_->value(), center, radius, angle);

    utility::JsonStore shape_data;
    shape_data["type"]     = "ellipse";
    shape_data["center"]   = center;
    shape_data["radius"]   = radius;
    shape_data["angle"]    = angle;
    shape_data["colour"]   = pen_colour_->value();
    shape_data["softness"] = pen_softness_->value();
    shape_data["opacity"]  = 100.f;
    shape_data["invert"]   = false;

    utility::JsonStore additional_roles;
    additional_roles["user_data"] = id;

    std::string name = fmt::format("Ellipse{}", id);
    mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
    expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");

    end_drawing();
}

void GradingTool::remove_shape(int id) {

    if (id >= 0 && id < mask_shapes_.size()) {
        auto canvas_id = mask_shapes_[id]->role_data_as_json(module::Attribute::UserData);
        grading_data_.mask().remove_shape(canvas_id);

        remove_attribute(mask_shapes_[id]->uuid());
        mask_shapes_.erase(mask_shapes_.begin() + id);

        save_bookmark();
    }
}

void GradingTool::end_drawing() {

    grading_data_.mask().end_draw();
    save_bookmark();
}

void GradingTool::undo() {

    grading_data_.mask().undo();
    save_bookmark();
}

void GradingTool::redo() {

    grading_data_.mask().redo();
    save_bookmark();
}

void GradingTool::clear_mask() {

    grading_data_.mask().clear();

    clear_shapes();

    save_bookmark();
}

void GradingTool::clear_shapes() {

    for (size_t i = 0; i < mask_shapes_.size(); ++i) {
        expose_attribute_in_model_data(mask_shapes_[i], "grading_tool_overlay_shapes", false);
        remove_attribute(mask_shapes_[i]->uuid());
    }
    mask_shapes_.clear();
}

void GradingTool::clear_grade() {

    slope_->set_value(
        slope_->get_role_data<std::vector<float>>(module::Attribute::DefaultValue));
    offset_->set_value(
        offset_->get_role_data<std::vector<float>>(module::Attribute::DefaultValue));
    power_->set_value(
        power_->get_role_data<std::vector<float>>(module::Attribute::DefaultValue));
    saturation_->set_value(saturation_->get_role_data<float>(module::Attribute::DefaultValue));
    exposure_->set_value(exposure_->get_role_data<float>(module::Attribute::DefaultValue));
    contrast_->set_value(contrast_->get_role_data<float>(module::Attribute::DefaultValue));
}

void GradingTool::save_cdl(const std::string &filepath) const {

    OCIO::CDLTransformRcPtr cdl = OCIO::CDLTransform::Create();

    std::array<double, 3> slope{
        slope_->value()[0] * slope_->value()[3] * std::pow(2.f, exposure_->value()),
        slope_->value()[1] * slope_->value()[3] * std::pow(2.f, exposure_->value()),
        slope_->value()[2] * slope_->value()[3] * std::pow(2.f, exposure_->value())};

    std::array<double, 3> offset{
        offset_->value()[0] + offset_->value()[3],
        offset_->value()[1] + offset_->value()[3],
        offset_->value()[2] + offset_->value()[3]};

    std::array<double, 3> power{
        power_->value()[0] * power_->value()[3],
        power_->value()[1] * power_->value()[3],
        power_->value()[2] * power_->value()[3]};

    cdl->setSlope(slope.data());
    cdl->setOffset(offset.data());
    cdl->setPower(power.data());
    cdl->setSat(saturation_->value());

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

void GradingTool::refresh_current_grade_from_ui() {

    auto &grade = grading_data_.grade();

    grade.slope    = vector4f_to_array4d(slope_->value());
    grade.offset   = vector4f_to_array4d(offset_->value());
    grade.power    = vector4f_to_array4d(power_->value());
    grade.sat      = saturation_->value();
    grade.exposure = exposure_->value();
    grade.contrast = contrast_->value();

    grading_data_.set_colour_space(colour_space_->value());
    grading_data_.set_mask_editing(display_mode_attribute_->value() == "Mask");
}

void GradingTool::refresh_ui_from_current_grade() {

    auto &grade = grading_data_.grade();

    slope_->set_value(array4d_to_vector4f(grade.slope), false);
    offset_->set_value(array4d_to_vector4f(grade.offset), false);
    power_->set_value(array4d_to_vector4f(grade.power), false);
    saturation_->set_value(float(grade.sat), false);
    exposure_->set_value(float(grade.exposure), false);
    contrast_->set_value(float(grade.contrast), false);

    colour_space_->set_value(grading_data_.colour_space(), false);
    display_mode_attribute_->set_value(grading_data_.mask_editing() ? "Mask" : "Grade", false);

    // Initialize shapes overlay
    using Quad    = xstudio::ui::canvas::Quad;
    using Polygon = xstudio::ui::canvas::Polygon;
    using Ellipse = xstudio::ui::canvas::Ellipse;

    clear_shapes();
    mask_selected_shape_->set_value(-1, false);

    // Please make sure that attribute_changed don't listen to
    // Group change notification to avoid dealock acquiring the
    // Canvas mutex when processing shapes event.
    grading_data_.mask().read_lock();

    for (const auto &item : grading_data_.mask()) {
        if (std::holds_alternative<Quad>(item)) {

            const Quad &quad = std::get<Quad>(item);

            utility::JsonStore shape_data;
            shape_data["type"]     = "quad";
            shape_data["bl"]       = quad.bl;
            shape_data["tl"]       = quad.tl;
            shape_data["tr"]       = quad.tr;
            shape_data["br"]       = quad.br;
            shape_data["colour"]   = quad.colour;
            shape_data["softness"] = quad.softness;
            shape_data["opacity"]  = quad.opacity;
            shape_data["invert"]   = quad.invert;

            utility::JsonStore additional_roles;
            additional_roles["user_data"] = quad._id;

            const std::string name = fmt::format("Quad{}", quad._id);
            mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
            expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");

        } else if (std::holds_alternative<Polygon>(item)) {

            const Polygon &polygon = std::get<Polygon>(item);

            utility::JsonStore shape_data;
            shape_data["type"]     = "polygon";
            shape_data["points"]   = polygon.points;
            shape_data["count"]    = polygon.points.size();
            shape_data["colour"]   = polygon.colour;
            shape_data["softness"] = polygon.softness;
            shape_data["opacity"]  = polygon.opacity;
            shape_data["invert"]   = polygon.invert;

            utility::JsonStore additional_roles;
            additional_roles["user_data"] = polygon._id;

            const std::string name = fmt::format("Polygon{}", polygon._id);
            mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
            expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");

        } else if (std::holds_alternative<Ellipse>(item)) {

            const Ellipse &ellipse = std::get<Ellipse>(item);

            utility::JsonStore shape_data;
            shape_data["type"]     = "ellipse";
            shape_data["center"]   = ellipse.center;
            shape_data["radius"]   = ellipse.radius;
            shape_data["angle"]    = ellipse.angle;
            shape_data["colour"]   = ellipse.colour;
            shape_data["softness"] = ellipse.softness;
            shape_data["opacity"]  = ellipse.opacity;
            shape_data["invert"]   = ellipse.invert;

            utility::JsonStore additional_roles;
            additional_roles["user_data"] = ellipse._id;

            std::string name = fmt::format("Ellipse{}", ellipse._id);
            mask_shapes_.push_back(add_attribute(name, shape_data, additional_roles));
            expose_attribute_in_model_data(mask_shapes_.back(), "grading_tool_overlay_shapes");
        }
    }
    grading_data_.mask().read_unlock();

    // Auto select the last shape, this will force shape controls
    // (softness, opacity, etc) to refresh when changing selected layer.
    if (!mask_shapes_.empty())
        mask_selected_shape_->set_value(mask_shapes_.size());
    else
        mask_selected_shape_->set_value(-1);

    auto bmd = get_bookmark_detail(current_bookmark());
    if (bmd.media_reference_ && bmd.start_ && bmd.start_.value() != timebase::k_flicks_low &&
        bmd.duration_ && bmd.duration_.value() != timebase::k_flicks_max) {

        auto &media = bmd.media_reference_.value();
        grade_in_->set_value(bmd.start_.value() / media.rate().to_flicks(), false);
        grade_out_->set_value(
            (bmd.start_.value() + bmd.duration_.value()) / media.rate().to_flicks(), false);
    } else {
        grade_in_->set_value(-1, false);
        grade_out_->set_value(-1, false);
    }
}

utility::Uuid GradingTool::current_bookmark() const {

    return utility::Uuid(grading_bookmark_->value());
}

utility::UuidList GradingTool::current_clip_bookmarks() {

    //const utility::UuidList bookmarks_list = get_bookmarks_on_current_media(current_viewport_);
    utility::UuidList filtered_list;
    if (viewport_current_images_ && viewport_current_images_->hero_image()) {

        for (auto &bookmark : viewport_current_images_->hero_image().bookmarks()) {

            if (bookmark->detail_.user_type_.value_or("") == "Grading") {
                filtered_list.push_back(bookmark->detail_.uuid_);
            }
        }
    }
    return filtered_list;
}

void GradingTool::create_bookmark_if_empty() {

    if (!current_bookmark()) {
        create_bookmark();
    }
}

void GradingTool::create_bookmark() {

    if (viewport_current_images_ && viewport_current_images_->hero_image()) {

        bookmark::BookmarkDetail bmd;
        // Hides bookmark from timeline
        bmd.colour_    = "transparent";
        bmd.visible_   = false;
        bmd.user_type_ = "Grading";

        utility::JsonStore user_data;
        user_data["grade_active"] = true;
        user_data["mask_active"]  = false;

        auto clip_layers        = current_clip_bookmarks();
        user_data["layer_name"] = "Grade Layer " + std::to_string(clip_layers.size() + 1);

        bmd.user_data_ = user_data;

        auto uuid = StandardPlugin::create_bookmark_on_frame(
            viewport_current_images_->hero_image().frame_id(),
            "Grading Note", // bookmark_subject
            bmd,            // detail
            true            // bookmark_entire_duration
        );

        grading_data_.bookmark_uuid_ = uuid;
        grading_data_.set_colour_space(working_space_->value());
        grading_bookmark_->set_value(utility::to_string(uuid), false);

        refresh_ui_from_current_grade();
    }

    spdlog::debug("Created bookmark {}", utility::to_string(grading_data_.bookmark_uuid_));
}

void GradingTool::select_bookmark(const utility::Uuid &uuid) {

    // spdlog::warn("Select bookmark {}", utility::to_string(uuid));

    if (grading_data_.bookmark_uuid_ == uuid)
        return;

    GradingData *grading_data_ptr = nullptr;
    if (uuid) {
        auto base_ptr    = get_bookmark_annotation(uuid);
        grading_data_ptr = dynamic_cast<GradingData *>(base_ptr.get());
    }

    if (grading_data_ptr) {
        grading_data_ = *grading_data_ptr;
    } else {
        grading_data_ = GradingData();
        grading_data_.set_colour_space(working_space_->value());
    }

    grading_data_.bookmark_uuid_ = uuid;
    grading_bookmark_->set_value(utility::to_string(uuid), false);

    refresh_ui_from_current_grade();
}

void GradingTool::update_boomark_shape(const utility::Uuid &uuid) {

    auto bmd = get_bookmark_detail(uuid);
    if (bmd.user_data_) {

        (*bmd.user_data_)["mask_active"] = !mask_shapes_.empty();
        update_bookmark_detail(uuid, bmd);
    }

    // First shape added on the layer, switch to Single frame mode
    if (mask_shapes_.size() == 1) {

        grade_in_->set_value(playhead_media_frame_);
        grade_out_->set_value(playhead_media_frame_);
    }
}

void GradingTool::save_bookmark() {

    if (current_bookmark()) {

        StandardPlugin::update_bookmark_annotation(
            current_bookmark(), std::make_shared<GradingData>(grading_data_), false);
        // spdlog::warn("Saved bookmark {}", utility::to_string(current_bookmark()));
    }
}

void GradingTool::remove_bookmark() {

    if (current_bookmark()) {

        // spdlog::warn("Removing bookmark {}", utility::to_string(current_bookmark()));
        StandardPlugin::remove_bookmark(current_bookmark());
    }

    auto clip_layers = current_clip_bookmarks();
    if (!clip_layers.empty()) {
        select_bookmark(clip_layers.back());
    } else {
        select_bookmark(utility::Uuid());
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