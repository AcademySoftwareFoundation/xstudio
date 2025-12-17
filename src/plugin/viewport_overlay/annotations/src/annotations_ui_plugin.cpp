// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <caf/actor_registry.hpp>

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/ui/font.hpp"
#include "xstudio/ui/helpers.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#include "annotations_ui_plugin.hpp"
#include "annotation_opengl_renderer.hpp"
#include "annotation.hpp"
#include "annotation_render_data.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui;

namespace fs = std::filesystem;

AnnotationsUI::AnnotationsUI(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "AnnotationsUI", init_settings) {

    // Create tool attributes

    // Pen
    pen_size_ = add_integer_attribute("Pen Size", "Pen Size", 10, 1, 300);
    pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_size_->set_preference_path("/plugin/annotations/pen_size");

    pen_opacity_ = add_integer_attribute("Pen Opacity", "Pen Opacity", 100, 0, 100);
    pen_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_opacity_->set_preference_path("/plugin/annotations/pen_opacity");

    pen_colour_ = add_colour_attribute(
        "Pen Colour", "Pen Colour", utility::ColourTriplet(0.5f, 0.4f, 1.0f));
    pen_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_colour_->set_preference_path("/plugin/annotations/pen_colour");

    // Brush

    brush_softness_ = add_integer_attribute("Brush Softness", "Brush Softness", 0, 0, 100);
    brush_softness_->expose_in_ui_attrs_group("annotations_tool_settings");
    brush_softness_->set_preference_path("/plugin/annotations/brush_softness");

    brush_size_ = add_integer_attribute("Brush Size", "Brush Size", 45, 1, 300);
    brush_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    brush_size_->set_preference_path("/plugin/annotations/brush_pen_size");

    brush_size_sensitivity_ =
        add_integer_attribute("Brush Size Sensitivity", "Brush Size Sensitivity", 40, 0, 40);
    brush_size_sensitivity_->expose_in_ui_attrs_group("annotations_tool_settings");
    brush_size_sensitivity_->set_preference_path("/plugin/annotations/size_sensitivity");

    brush_opacity_ = add_integer_attribute("Brush Opacity", "Brush Opacity", 100, 0, 100);
    brush_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");
    brush_opacity_->set_preference_path("/plugin/annotations/brush_pen_opacity");

    brush_opacity_sensitivity_ = add_integer_attribute(
        "Brush Opacity Sensitivity", "Brush Opacity Sensitivity", 5, 0, 40);
    brush_opacity_sensitivity_->expose_in_ui_attrs_group("annotations_tool_settings");
    brush_opacity_sensitivity_->set_preference_path("/plugin/annotations/opacity_sensitivity");

    // brush_colour_ = add_colour_attribute("Brush Colour", "Brush Colour",
    // utility::ColourTriplet(0.5f, 0.4f, 1.0f));
    // brush_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    // brush_colour_->set_preference_path("/plugin/annotations/brush_colour");

    // Shapes
    shapes_width_ = add_integer_attribute("Shapes Width", "Shapes Width", 20, 1, 300);
    shapes_width_->expose_in_ui_attrs_group("annotations_tool_settings");
    shapes_width_->set_preference_path("/plugin/annotations/shapes_width_");

    // Text
    const auto &fonts_ = Fonts::available_fonts();
    font_choice_       = add_string_choice_attribute(
        "font_choices",
        "font_choices",
        fonts_.size() ? fonts_.begin()->first : std::string(""),
        utility::map_key_to_vec(fonts_));
    font_choice_->expose_in_ui_attrs_group("annotations_tool_fonts");
    font_choice_->set_preference_path("/plugin/annotations/text_font");

    text_size_ = add_integer_attribute("Text Size", "Text Size", 40, 1, 300);
    text_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_size_->set_preference_path("/plugin/annotations/text_size");

    text_bgr_opacity_ = add_integer_attribute(
        "Text Background Opacity", "Text Background Opacity", 100, 0, 100);
    text_bgr_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_bgr_opacity_->set_preference_path("/plugin/annotations/text_bgr_opacity");

    text_bgr_colour_ = add_colour_attribute(
        "Text Background Colour",
        "Text Background Colour",
        utility::ColourTriplet(0.0f, 0.0f, 0.0f));
    text_bgr_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_bgr_colour_->set_preference_path("/plugin/annotations/text_bgr_colour");

    moving_scaling_text_attr_ =
        add_integer_attribute("moving_scaling_text", "moving_scaling_text", 0);
    moving_scaling_text_attr_->expose_in_ui_attrs_group("annotations_tool_settings");

    // Erase
    erase_size_ = add_integer_attribute("Erase Size", "Erase Size", 80, 1, 300);
    erase_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    erase_size_->set_preference_path("/plugin/annotations/erase_pen_size");

    // Color picker
    colour_picker_take_average_ =
        add_boolean_attribute("Colour Pick Average", "Clr. Average", false);
    colour_picker_take_show_magnifier_ =
        add_boolean_attribute("Colour Pick Show Magnifier", "Show Mag", true);
    colour_picker_hide_drawings_ =
        add_boolean_attribute("Colour Pick Hide Drawings", "Hide Drawings", true);

    colour_picker_take_average_->expose_in_ui_attrs_group("annotations_colour_picker_prefs");
    colour_picker_take_show_magnifier_->expose_in_ui_attrs_group(
        "annotations_colour_picker_prefs");
    colour_picker_hide_drawings_->expose_in_ui_attrs_group("annotations_colour_picker_prefs");

    colour_picker_take_average_->set_preference_path("/plugin/annotations/colour_pick_average");
    colour_picker_take_show_magnifier_->set_preference_path(
        "/plugin/annotations/colour_pick_show_mag");
    colour_picker_hide_drawings_->set_preference_path(
        "/plugin/annotations/colour_pick_hide_drawings");

    // Toolset
    auto tn = utility::map_value_to_vec(tool_names_);
    tn.pop_back(); // remove 'None' from the list of tools to show in the UI
    active_tool_ = add_string_choice_attribute("Active Tool", "Active Tool", "None", tn);
    active_tool_->expose_in_ui_attrs_group("annotations_tool_settings");
    active_tool_->expose_in_ui_attrs_group("annotations_tool_types");

    // Undo and Redo
    action_attribute_ = add_string_attribute("action_attribute", "action_attribute", "");
    action_attribute_->expose_in_ui_attrs_group("annotations_tool_settings");

    // Display mode
    display_mode_attribute_ = add_string_choice_attribute(
        "Display Mode", "Disp. Mode", "With Drawing Tools", {"Only When Paused", "Always"});
    display_mode_attribute_->expose_in_ui_attrs_group("annotations_tool_draw_mode");
    display_mode_attribute_->set_preference_path("/plugin/annotations/display_mode");

    // setting the active tool to -1 disables drawing via 'attribute_changed'
    attribute_changed(active_tool_->uuid(), module::Attribute::Value);

    make_behavior();
    connect_to_ui();

    listen_to_playhead_events(true);

    // here's the code for the 'reskin' UI (xSTUDIO v2) where we declare the
    // drawing tools panel creation code.

    dockable_widget_attr_ = register_viewport_dockable_widget(
        "Annotate",
        "qrc:/icons/stylus_note.svg",   // icon for the button to activate the tool
        "Show/Hide Annotation Toolbox", // tooltip for the button,
        3.0f,                           // button position in the buttons bar
        true,
        // qml code to create the left/right dockable widget
        R"(
            import AnnotationsUI 2.0
            import QtQuick
            XsDrawingTools {
                horizontal: false
            }
            )",
        // qml code to create the top/bottom dockable widget (left empty here as we don't have
        // one)
        R"(
            import AnnotationsUI 2.0
            import QtQuick
            XsDrawingTools {
                horizontal: true
            }
            )",
        toggle_active_hotkey_);
}

AnnotationsUI::~AnnotationsUI() { colour_pipelines_.clear(); }

void AnnotationsUI::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {

    if (attribute_uuid == active_tool_->uuid()) {

        const std::string active_tool = active_tool_->value();
        current_tool_                 = None;
        for (const auto &p : tool_names_) {
            if (p.second == active_tool) {
                current_tool_ = Tool(p.first);
            }
        }

        utility::JsonStore payload;
        payload["tool"] = active_tool;
        send_event("ToolChanged", payload);

        if (current_tool() != Dropper) {

            utility::JsonStore payload;
            send_event("ShowDrawings",payload);
            pixel_patch_.hide();
        }

        if (current_tool() == None) {

            release_mouse_focus();
            release_keyboard_focus();
            set_viewport_cursor("");
            pixel_patch_.hide();

        } else {

            grab_mouse_focus();
            if (current_tool() == Dropper) {

                set_viewport_cursor("://cursors/point_scan.svg", 24, -1, -1);
                release_keyboard_focus();
                pixel_patch_.update(
                    std::vector<Imath::V4f>(),
                    Imath::V2f(0.0f, 0.0f),
                    false,
                    colour_picker_hide_drawings_->value());
                    
                if (colour_picker_hide_drawings_->value()) {

                    utility::JsonStore payload;
                    send_event("HideDrawings",payload);

                }

            } else if (current_tool() != Text) {
                set_viewport_cursor("Qt.CrossCursor");
                release_keyboard_focus();
                pixel_patch_.hide();
            } else {
                set_viewport_cursor("Qt.IBeamCursor");
                pixel_patch_.hide();
            }

            if (current_tool() != Dropper) {
                last_tool_ = current_tool();
            }
        }

    } else if (
        attribute_uuid == action_attribute_->uuid() && action_attribute_->value() != "") {

        // When user clicks 'Redo', 'Undo' buttons etc the action_attribute_ is 
        // set with the action name plus the name of the viewport for the toolbox
        if (action_attribute_->value().find("Clear") == 0) {
            clear_annotation(std::string(action_attribute_->value(), 6));
        } else if (action_attribute_->value().find("Undo") == 0) {
            undo(std::string(action_attribute_->value(), 5));
        } else if (action_attribute_->value().find("Redo") == 0) {
            redo(std::string(action_attribute_->value(), 5));
        }
        action_attribute_->set_value("");

    } else if (attribute_uuid == display_mode_attribute_->uuid()) {

        if (display_mode_attribute_->value() == "Only When Paused") {
            display_mode_ = OnlyWhenPaused;
        } else if (display_mode_attribute_->value() == "Always") {
            display_mode_ = Always;
        }

    } else if (current_tool() == Text) {
        
        std::string action;
        utility::JsonStore payload;

        if (attribute_uuid == pen_colour_->uuid()) {

            action = "CaptionProperty";
            payload["colour"] = pen_colour_->value();

        } else if (attribute_uuid == text_size_->uuid()) {

            action = "CaptionProperty";
            payload["font_size"] = text_size_->value();

        } else if (attribute_uuid == pen_opacity_->uuid()) {

            action = "CaptionProperty";
            payload["opacity"] = pen_opacity_->value()/100.0f;

        } else if (attribute_uuid == font_choice_->uuid()) {

            action = "CaptionProperty";
            payload["font_name"] = font_choice_->value();

        } else if (attribute_uuid == text_bgr_colour_->uuid()) {

            action = "CaptionProperty";
            payload["background_colour"] = text_bgr_colour_->value();

        } else if (attribute_uuid == text_bgr_opacity_->uuid()) {

            action = "CaptionProperty";
            payload["background_opacity"] = text_bgr_opacity_->value()/100.0f;
        }

        if (!payload.is_null()) {
            send_event(action, payload);
        }

    } else if (attribute_uuid == colour_picker_hide_drawings_->uuid()) {

        pixel_patch_.update(
            std::vector<Imath::V4f>(),
            Imath::V2f(0.0f, 0.0f),
            false,
            colour_picker_hide_drawings_->value());

        if (colour_picker_hide_drawings_->value()) {

            utility::JsonStore payload;
            send_event("HideDrawings", payload);

        } else {

            utility::JsonStore payload;
            send_event("ShowDrawings", payload);

        }

    }

}

void AnnotationsUI::update_attrs_from_preferences(const utility::JsonStore &j) {

    Module::update_attrs_from_preferences(j);

    // this ensures that 'display_mode_' member data is up to date after being
    // updated from prefs
    attribute_changed(display_mode_attribute_->uuid(), module::Attribute::Value);
}

void AnnotationsUI::register_hotkeys() {

    toggle_active_hotkey_ = register_hotkey(
        int('D'),
        ui::NoModifier,
        "Toggle Annotations Tool",
        "Show or hide the Annotate toolbox. You can start drawing annotations immediately "
        "whenever the toolbox is visible.",
        false,
        "Drawing Tools");

    undo_hotkey_ = register_hotkey(
        int('Z'),
        ui::ControlModifier,
        "Undo (Annotation edit)",
        "Undoes your last edits to an annotation",
        false,
        "Drawing Tools");

    redo_hotkey_ = register_hotkey(
        int('Z'),
        ui::ControlModifier | ui::ShiftModifier,
        "Redo (Annotation edit)",
        "Redoes your last undone edit on an annotation",
        false,
        "Drawing Tools");

    clear_hotkey_ = register_hotkey(
        int('D'),
        ui::ControlModifier,
        "Delete all strokes",
        "Delete the entire current drawing. If there is no text in the associated note it will "
        "also be removed.",
        false,
        "Drawing Tools");

    colour_picker_hotkey_ = register_hotkey(
        int('V'),
        ui::NoModifier,
        "Activate colour picker",
        "While this hotkey his held down, the annotation tool switches to activate the colour "
        "picker tool.",
        false,
        "Drawing Tools");
}

void AnnotationsUI::hotkey_pressed(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const std::string & /*window*/) {

    if (hotkey_uuid == toggle_active_hotkey_) {

        // tool_is_active_->set_value(!tool_is_active_->value());

    } else if (hotkey_uuid == undo_hotkey_ && current_tool() != None) {
        undo(context);
    } else if (hotkey_uuid == redo_hotkey_ && current_tool() != None) {
        redo(context);
    } else if (hotkey_uuid == clear_hotkey_ && current_tool() != None) {

    } else if (
        hotkey_uuid == colour_picker_hotkey_ && current_tool() != None &&
        current_tool() != Dropper) {

        last_tool_ = current_tool();
        active_tool_->set_value(tool_name(Dropper));
    }
}

void AnnotationsUI::hotkey_released(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const bool due_to_focus_change) {

    // if the user is holding down the colour_picker_hotkey_ and they move the mouse out of the
    // viewport area, we get a hotkey_released callback but due_to_focus_change will be true.
    if (hotkey_uuid == colour_picker_hotkey_ && current_tool() == Dropper &&
        !due_to_focus_change) {

        active_tool_->set_value(tool_name(last_tool_));
    }
}

void AnnotationsUI::send_event(const std::string &event, const utility::JsonStore &payload) {
 
    if (!core_plugin_) {
        core_plugin_ = system().registry().template get<caf::actor>("ANNOTATIONS_CORE_PLUGIN");
    }

    if (core_plugin_) {

        utility::JsonStore d(R"({"event": "", "payload": {}})"_json);
        d["event"] = event;
        d["user_id"] = user_id_;
        d["payload"] = payload;
        anon_mail(utility::event_atom_v, ui::viewport::annotation_atom_v, d).send(core_plugin_);
    }
    
}

void AnnotationsUI::start_item(const ui::PointerEvent &e) {

    utility::JsonStore payload;
    current_item_id_ = utility::Uuid::generate();
    payload["uuid"] = current_item_id_;
    payload["item_type"] = active_tool_->value();
    payload["point"]["x"] = float(e.x())/float(e.width());
    payload["point"]["y"] = float(e.y())/float(e.height());
    payload["viewport"] = e.context();

    const auto colour = std::vector<float>({pen_colour_->value().r, pen_colour_->value().g, pen_colour_->value().b, (current_tool() == Brush ? brush_opacity_->value() : pen_opacity_->value()) / 100.0});

    switch (current_tool()) {
        case Draw:
        case Laser:
            {
                payload["paint"]["rgba"] = colour;
                payload["paint"]["size"] = pen_size_->value() / PEN_STROKE_THICKNESS_SCALE;
            }
            break;
        case Brush:
            {
                payload["paint"]["rgba"] = colour;
                payload["paint"]["size"] = brush_size_->value() / PEN_STROKE_THICKNESS_SCALE;
                payload["paint"]["softness"] = float(brush_softness_->value()) / 10.0f;
                payload["paint"]["size_sensitivity"] = float(brush_size_sensitivity_->value()) / 10.0f;
                payload["paint"]["opacity_sensitivity"] = float(brush_opacity_sensitivity_->value()) / 10.0f;
            }
            break;        
        case Erase:
            payload["paint"]["size"] = erase_size_->value() / PEN_STROKE_THICKNESS_SCALE;
            break;
        case Circle:
        case Line:
        case Square:
        case Arrow:
            {
                payload["paint"]["rgba"] = colour;
                payload["paint"]["size"] = shapes_width_->value() / PEN_STROKE_THICKNESS_SCALE;
            }
            break;
        case Text:
            {
                payload["caption"]["font"] = font_choice_->value();
                payload["caption"]["size"] = text_size_->value();
                payload["caption"]["rgba"] = colour;
                payload["caption"]["bg_opacity"] = text_bgr_opacity_->value() / 100.0;
            }
            break;
        default:
            break;
    }

    send_event("PaintStart", payload);

}

void AnnotationsUI::modify_item(const ui::PointerEvent &e) {

    auto tp = utility::clock::now();
    auto vv = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    if (current_item_id_.is_null()) return;

    utility::JsonStore payload;
    payload["uuid"] = current_item_id_;
    payload["point"]["x"] = float(e.x())/float(e.width());
    payload["point"]["y"] = float(e.y())/float(e.height());
    payload["point"]["pressure"] = pressure_source(e);   
    payload["point"]["size"] = 0.0f;   
    send_event("PaintPoint", payload);

}

void AnnotationsUI::end_item() {

    if (current_item_id_.is_null()) return;
    utility::JsonStore payload;
    payload["uuid"] = current_item_id_;
    current_item_id_ = utility::Uuid();
    send_event("PaintEnd", payload);

}

void AnnotationsUI::clear_annotation(const std::string viewport_name) {

    utility::JsonStore payload;
    payload["viewport"] = viewport_name;
    send_event("PaintClear", payload);

}

void AnnotationsUI::undo(const std::string viewport_name) {

    utility::JsonStore payload;
    payload["viewport"] = viewport_name;
    send_event("PaintUndo", payload);

}

void AnnotationsUI::redo(const std::string viewport_name) {
    
    utility::JsonStore payload;
    payload["viewport"] = viewport_name;
    send_event("PaintRedo", payload);

}

bool AnnotationsUI::pointer_event(const ui::PointerEvent &e) {

    if (current_tool() == None)
        return false;

    const Imath::V2f pointer_pos = e.position_in_viewport_coord_sys();

    if (current_tool() == Dropper) {

        if (e.type() == ui::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            cumulative_picked_colour_ = Imath::V4f(0.0f, 0.0f, 0.0f, 0.0f);
        }
        update_colour_picker_info(e);
        redraw_viewport();

    } else if (e.type() == ui::EventType::ButtonDown && e.buttons() == ui::Signature::Button::Left) {

        if (current_tool() == Text) {

            if (check_click_on_caption(
                pointer_pos,
                e.context())) {

                utility::JsonStore payload;
                payload["pointer_position"] = Imath::V2f(float(e.x())/float(e.width()), float(e.y())/float(e.height()));
                payload["viewport"] = e.context();
                payload["viewport_pix_scale"] = e.viewport_pixel_scale();
                grab_keyboard_focus();
                send_event("CaptionStartEdit", payload);

            } else {

                utility::JsonStore payload;
                payload["pointer_position"] = Imath::V2f(float(e.x())/float(e.width()), float(e.y())/float(e.height()));
                payload["viewport"] = e.context();
                payload["viewport_pix_scale"] = e.viewport_pixel_scale();
                payload["font_name"] = font_choice_->value();
                payload["font_size"] = text_size_->value();
                payload["colour"] = pen_colour_->value();
                payload["opacity"] = pen_opacity_->value() / 100.0f;
                payload["wrap_width"] = text_size_->value() * 0.01f;
                payload["justification"] = int(JustifyLeft);
                payload["background_colour"] = text_bgr_colour_->value();
                payload["background_opacity"] = text_bgr_opacity_->value() / 100.0f;
                grab_keyboard_focus();
                send_event("CaptionInteract", payload);

            }

        } else {

            start_item(e);

        }

    } else if (e.type() == ui::EventType::Drag && e.buttons() == ui::Signature::Button::Left) {

        if (current_tool() == Text) {

            utility::JsonStore payload;
            payload["pointer_position"] = Imath::V2f(float(e.x())/float(e.width()), float(e.y())/float(e.height()));
            payload["viewport"] = e.context();
            payload["viewport_pix_scale"] = e.viewport_pixel_scale();
            send_event("CaptionMove", payload);

        } else if (current_tool() != None) {

            modify_item(e);

        }

    } else if (e.type() == ui::EventType::ButtonRelease) {

        if (current_tool() == Text) {
            utility::JsonStore payload;
            payload["pointer_position"] = Imath::V2f(float(e.x())/float(e.width()), float(e.y())/float(e.height()));
            payload["viewport"] = e.context();
            payload["viewport_pix_scale"] = e.viewport_pixel_scale();
            send_event("CaptionEndMove", payload);

        } else {
            end_item();
        }
    
    } else if (current_tool() == Text && e.buttons() == ui::Signature::Button::None) {

        utility::JsonStore payload;
        payload["pointer_position"] = Imath::V2f(float(e.x())/float(e.width()), float(e.y())/float(e.height()));
        payload["viewport"] = e.context();
        payload["viewport_pix_scale"] = e.viewport_pixel_scale();
        send_event("CaptionPointerHover", payload);

    }

    return false;
}

void AnnotationsUI::text_entered(const std::string &text, const std::string &context) {

    utility::JsonStore payload;
    payload["text"] = text;
    payload["viewport"] = context;
    send_event("CaptionTextEntry", payload);

}

void AnnotationsUI::key_pressed(
    const int key, const std::string &context, const bool auto_repeat) {

    if (current_tool() == Text) {

        utility::JsonStore payload;

        if (key == 16777216) { // escape key
            //end_drawing();
            release_keyboard_focus();
            payload["viewport"] = context;
            send_event("CaptionEndEdit", payload);

        } else {

            payload["key"] = key;
            payload["viewport"] = context;
            send_event("CaptionKeyPress", payload);

        }


    }
}

void AnnotationsUI::images_going_on_screen(
    const media_reader::ImageBufDisplaySetPtr &images,
    const std::string viewport_name,
    const bool playhead_playing) {

    // each viewport will call this function shortly before it refreshes to
    // draw the image data of 'images'.
    // Because bookmark data is attached to the images, we can work out
    // if the bookmark that we might be in the process of adding annotations
    // to is visible on screen for this viewport. If not, it could be that
    // the user has scrubbed the timeline since our last edit.

    playhead_is_playing_ = playhead_playing;
    viewport_current_images_[viewport_name] = images;

}

plugin::ViewportOverlayRendererPtr
AnnotationsUI::make_overlay_renderer(const std::string &viewport_name) {

    return plugin::ViewportOverlayRendererPtr(
        new AnnotationsExtrasRenderer(pixel_patch_, viewport_name));
}

void AnnotationsUI::viewport_dockable_widget_activated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value(tool_name(last_tool_));
    }
}

void AnnotationsUI::viewport_dockable_widget_deactivated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value("None");
    }
}

void AnnotationsUI::turn_off_overlay_interaction() { 
    active_tool_->set_value("None"); 
}

media_reader::ImageBufPtr AnnotationsUI::image_under_pointer(
    const std::string &viewport_name,
    const Imath::V2f &pointer_position,
    Imath::V2i *pixel_position) {

    media_reader::ImageBufPtr result;

    const media_reader::ImageBufDisplaySetPtr &onscreen_image_set = viewport_current_images_[viewport_name];
    if (!onscreen_image_set || !onscreen_image_set->layout_data()) return result;

    // check if pointer_position lands on one of the images in
    // the viewport

    const auto &im_idx = onscreen_image_set->layout_data()->image_draw_order_hint_;
    for (auto &idx : im_idx) {
        // loop over onscreen images. translate pointer position to image
        // space coords
        const auto &cim = onscreen_image_set->onscreen_image(idx);

        if (cim) {

            Imath::V4f pt(pointer_position.x, pointer_position.y, 0.0f, 1.0f);
            pt *= cim.layout_transform().inverse();

            // does the pointer land on the image?
            float a = 1.0f / image_aspect(cim);
            if (pt.x / pt.w >= -1.0f && pt.x / pt.w <= 1.0f && pt.y / pt.w >= -a &&
                pt.y / pt.w <= a) {
                result = cim;
                break;
            }

        }
    }

    if (result && pixel_position) {
        Imath::V4f pt(pointer_position.x, pointer_position.y, 0.0f, 1.0f);
        pt *= result.layout_transform().inverse();
        // pix pos in normalised coords (-1.0 = left edge, 1.0 = right edge)
        Imath::V2f pix_pos(pt.x / pt.w, pt.y / pt.w);
        pixel_position->x = (pix_pos.x + 1.0f) * 0.5f * result->image_size_in_pixels().x;
        pixel_position->y = (pix_pos.y * image_aspect(result) + 1.0f) *
                            float(result->image_size_in_pixels().y) * 0.5f;
    }

    return result;
}

float AnnotationsUI::pressure_source(const ui::PointerEvent &e) {
    
    if (current_tool() == Brush) {
        if (e.pointer_type() == Signature::PointerType::Pen)
            return e.pressure();
        else {
            // float norm = utility::approximate_norm(x, y) / MAXIMUM_VELOCITY;
            // norm = 0.5f - (norm > 1 ? 1 : norm);
            // return norm > 1.0f ? 1.0f : norm;
            return 1.0f;
        }
    }

    return 1.0f;
}

caf::message_handler AnnotationsUI::message_handler_extensions() {

    // provide an extension to the base class message handler to handle timed
    // callbacks to fade the laser pen strokes
    return caf::message_handler(
        [=](utility::event_atom, bool) {
            // this message is sent when the user finishes a laser bruish stroke
        },
        [=](utility::event_atom) {
            // note Annotation::fade_all_strokes() returns false when all strokes have vanished
        },
        [=](utility::event_atom,
            ui::viewport::viewport_atom,
            media::transform_matrix_atom,
            const std::string viewport_name,
            const Imath::M44f &proj_matrix) {
            // these update events come from the global playhead events group
            viewport_transforms_[viewport_name] = proj_matrix;
        }
    );
}

media_reader::ImageBufPtr AnnotationsUI::image_under_mouse(
    const std::string &viewport_name,
    const Imath::V2f &pos,
    const bool fallback_to_hero_image) const
{
    media_reader::ImageBufPtr result;

    Imath::V2f pointer_position(pos.x*2.0f-1.0f, 1.0f-pos.y*2.0f);
    auto q = viewport_transforms_.find(viewport_name);
    if (q != viewport_transforms_.end()) {
        Imath::V4f pp(pointer_position.x, pointer_position.y, 0.0f, 1.0f);
        pp = pp*q->second;
        pointer_position.x = pp.x/pp.w;
        pointer_position.y = pp.y/pp.w;
    }

    auto p = viewport_current_images_.find(viewport_name);
    if (p != viewport_current_images_.end() && p->second) {

        // first, check if pointer_position lands on one of the images in
        // the viewport
        const media_reader::ImageBufDisplaySetPtr &onscreen_image_set = p->second;
        if (!onscreen_image_set->layout_data()) {
            return result;
        }
        const auto &im_idx = onscreen_image_set->layout_data()->image_draw_order_hint_;
        for (auto &idx : im_idx) {
            // loop over onscreen images. translate pointer position to image
            // space coords
            const auto &cim = onscreen_image_set->onscreen_image(idx);

            if (cim) {

                Imath::V4f pt(pointer_position.x, pointer_position.y, 0.0f, 1.0f);
                pt *= cim.layout_transform().inverse();

                // does the pointer land on the image?
                float a = 1.0f / image_aspect(cim);
                if (pt.x / pt.w >= -1.0f && pt.x / pt.w <= 1.0f && pt.y / pt.w >= -a &&
                    pt.y / pt.w <= a) {
                    result = cim;
                }

            }
        }

        if (!result && fallback_to_hero_image) {
            result = onscreen_image_set->hero_image();
        }

    }
    return result;
}

bool AnnotationsUI::check_click_on_caption(
    const Imath::V2f &pos,
    const std::string &viewport_id) {

    media_reader::ImageBufPtr img = image_under_mouse(viewport_id, pos, true);

    Imath::V4f pt(pos.x, pos.y, 0.0f, 1.0f);
    pt *= img.layout_transform().inverse();    
    const Imath::V2f image_pointer_position(pt.x/pt.w, pt.y/pt.w);

    auto old_capt_id = focus_caption_id_;
    focus_caption_id_= 0;
    utility::Uuid bookmark_id;
    for (const auto &bookmark : img.bookmarks()) {
        // get to annotation data by dynamic casing the annotation_ pointer on
        // the bookmark to our 'Annotation' class.
        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(bookmark->annotation_.get());
        if (my_annotation) {

            auto p = my_annotation->canvas().begin();
            while (p != my_annotation->canvas().end()) {
                if (std::holds_alternative<canvas::Caption>(*p)) {
                    const auto &caption = std::get<canvas::Caption>(*p);
                    if (caption.bounding_box().intersects(image_pointer_position)) {
                        // set text attrs to match the caption we clicked on
                        font_choice_->set_value(caption.font_name(), false);
                        text_size_->set_value(caption.font_size(), false);
                        pen_colour_->set_value(caption.colour(), false);
                        pen_opacity_->set_value(caption.opacity()*100.0f, false);
                        text_bgr_colour_->set_value(caption.background_colour(), false);
                        text_bgr_opacity_->set_value(caption.background_opacity()*100.0f, false);
                        return true;
                    }
                }
                p++;
            }
        }
    }

    return false;

}

void AnnotationsUI::update_colour_picker_info(const ui::PointerEvent &e) {

    Imath::V2i pixel_position;
    media_reader::ImageBufPtr image =
        image_under_pointer(e.context(), e.position_in_viewport_coord_sys(), &pixel_position);

    // interleaved vtx colour and position goes in here to draw our
    // patch of pixels
    std::vector<Imath::V4f> overlay_vertex_data;

    if (!image) {

        pixel_patch_.update(
            overlay_vertex_data,
            e.position_in_viewport_coord_sys(),
            e.buttons() == ui::Signature::Button::Left,
            colour_picker_hide_drawings_->value(),
            e.context());

        if (e.buttons() == ui::Signature::Button::Left) {
            pen_colour_->set_value(utility::ColourTriplet(0.0f, 0.0f, 0.0f));
        }
        return;
    }

    auto colour_pipeline = get_colour_pipeline_actor(e.context());
    if (!colour_pipeline)
        return;

    // half width of the patch of pixels that we'll sample. If show magnifier
    // option is OFF then the patch is a single pixel
    const int patch_w = colour_picker_take_show_magnifier_->value() ? 3 : 0;

    // Here we make a list of pixel coordinates that we're interested in.
    // We will grab a 7x7 patch.
    std::vector<Imath::V2i> pixels;
    for (int i = -patch_w; i <= patch_w; ++i) {
        for (int j = -patch_w; j <= patch_w; ++j) {
            pixels.emplace_back(pixel_position.x + j, pixel_position.y - i);
        }
    }

    // The image buffer will create a PixelInfo struct with the raw RBG values
    // for our patch here
    auto pixel_info = image->pixel_info(pixel_position, pixels);

    // Triangle verts needed to draw a square
    static const std::vector<Imath::V4f> tri_vtxs = {
        Imath::V4f(0.0f, 0.0f, 0.0f, 1.0f),
        Imath::V4f(1.0f, 0.0f, 0.0f, 1.0f),
        Imath::V4f(0.0f, 1.0f, 0.0f, 1.0f),
        Imath::V4f(0.0f, 1.0f, 0.0f, 1.0f),
        Imath::V4f(1.0f, 1.0f, 0.0f, 1.0f),
        Imath::V4f(1.0f, 0.0f, 0.0f, 1.0f)};

    // Line vertex colour and position needed to draw a square outline. Used
    // to highlight the centre pixel in our patch (which is the colour we
    // will grab) in our overlay
    static const std::vector<Imath::V4f> centre_square = {
        Imath::V4f(-0.5f, -0.5f, 0.0f, 1.0f),
        Imath::V4f(0.5f, -0.5f, 0.0f, 1.0f),
        Imath::V4f(0.5f, 0.5f, 0.0f, 1.0f),
        Imath::V4f(-0.5f, 0.5f, 0.0f, 1.0f)};

    caf::scoped_actor sys(system());
    try {

        // the colour pipeline will convert the raw RGB values to display
        // (final screen) pixel values
        const auto pix_info = utility::request_receive<media_reader::PixelInfo>(
            *sys,
            colour_pipeline,
            colour_pipeline::pixel_info_atom_v,
            pixel_info,
            image.frame_id());

        const int num_pixels = pix_info.extra_pixel_display_rgba_values().size();
        int nc               = int(round(sqrt(num_pixels)));

        // Get to the middle pixel in the patch to get the display space
        // pixel colour to sample
        const int middle_pixel = patch_w + patch_w * (patch_w + patch_w + 1);
        Imath::V4f picked_pixel_colour(0.0f, 0.0f, 0.0f, 0.0f);
        if (middle_pixel < pix_info.extra_pixel_display_rgba_values().size()) {
            picked_pixel_colour = pix_info.extra_pixel_display_rgba_values()
                                      [patch_w + patch_w * (patch_w + patch_w + 1)];
        }

        if (num_pixels > 1) {
            overlay_vertex_data.reserve(num_pixels * 6 * 2);
            for (int i = 0; i < num_pixels; ++i) {
                const float col = (i % nc) - patch_w - 0.5f;
                const float row = (i / nc) - patch_w - 0.5f;
                for (int vv = 0; vv < 6; ++vv) {
                    overlay_vertex_data.push_back(
                        pix_info.extra_pixel_display_rgba_values()[i]);
                    overlay_vertex_data.push_back(
                        tri_vtxs[vv] + Imath::V4f(col, row, 0.0f, 0.0f));
                }
            }

            float h = (picked_pixel_colour.x * 0.29f + picked_pixel_colour.y * 0.6f +
                       picked_pixel_colour.z * 0.11f) < 0.5f
                          ? 1.0f
                          : 0.0f;
            Imath::V4f highlight_colour(h, h, h, 1.0f);
            for (auto &csv : centre_square) {
                overlay_vertex_data.push_back(highlight_colour);
                overlay_vertex_data.push_back(csv);
            }
        }

        // thread safe call to update the data in our PixelPatch object used
        // by the on-screen renderer.
        pixel_patch_.update(
            overlay_vertex_data,
            e.position_in_viewport_coord_sys(),
            e.buttons() == ui::Signature::Button::Left,
            colour_picker_hide_drawings_->value(),
            e.context());

        if (e.buttons() == ui::Signature::Button::Left) {

            if (colour_picker_take_average_->value()) {

                cumulative_picked_colour_.x +=
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.x));
                cumulative_picked_colour_.y +=
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.y));
                cumulative_picked_colour_.z +=
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.z));
                cumulative_picked_colour_.w += 1.0f;

                pen_colour_->set_value(utility::ColourTriplet(
                    cumulative_picked_colour_.x / cumulative_picked_colour_.w,
                    cumulative_picked_colour_.y / cumulative_picked_colour_.w,
                    cumulative_picked_colour_.z / cumulative_picked_colour_.w));

            } else {

                pen_colour_->set_value(utility::ColourTriplet(
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.x)),
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.y)),
                    std::max(0.0f, std::min(1.0f, picked_pixel_colour.z))));
            }
        }

    } catch (std::exception &e) {

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

caf::actor AnnotationsUI::get_colour_pipeline_actor(const std::string &viewport_name) {

    // get the actor object that runs colour management for the given (named) viewport
    if (colour_pipelines_.find(viewport_name) != colour_pipelines_.end()) {
        return colour_pipelines_[viewport_name];
    }
    auto colour_pipe_manager = system().registry().get<caf::actor>(colour_pipeline_registry);
    caf::scoped_actor sys(system());
    caf::actor r;
    try {
        r = utility::request_receive<caf::actor>(
            *sys,
            colour_pipe_manager,
            xstudio::colour_pipeline::colour_pipeline_atom_v,
            viewport_name);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    colour_pipelines_[viewport_name] = r;
    return r;
}

utility::BlindDataObjectPtr AnnotationsUI::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid & /*playhead_uuid*/,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    auto data = new AnnotationExtrasRenderDataSet();
    return utility::BlindDataObjectPtr(data);

}
