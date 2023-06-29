// SPDX-License-Identifier: Apache-2.0
#include "annotations_tool.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#include <GL/glew.h>
#include <GL/gl.h>

using namespace xstudio;
using namespace xstudio::ui::viewport;


AnnotationsTool::AnnotationsTool(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "AnnotationsTool", init_settings) {

    module::QmlCodeAttribute *button = add_qml_code_attribute(
        "MyCode",
        R"(
import AnnotationsTool 1.0
AnnotationsButton {
    anchors.fill: parent
}
)");

    button->expose_in_ui_attrs_group("media_tools_buttons");
    button->set_role_data(module::Attribute::ToolbarPosition, 1000.0);

    load_fonts();

    font_choice_ = add_string_choice_attribute(
        "font_choices",
        "font_choices",
        fonts_.size() ? fonts_.begin()->first : std::string(""),
        utility::map_key_to_vec(fonts_));
    font_choice_->expose_in_ui_attrs_group("annotations_tool_fonts");

    draw_pen_size_ = add_integer_attribute("Draw Pen Size", "Draw Pen Size", 10, 1, 300);

    shapes_pen_size_ = add_integer_attribute("Shapes Pen Size", "Shapes Pen Size", 20, 1, 300);

    erase_pen_size_ = add_integer_attribute("Erase Pen Size", "Erase Pen Size", 80, 1, 300);

    text_size_ = add_integer_attribute("Text Size", "Text Size", 40, 1, 300);

    pen_opacity_ = add_integer_attribute("Pen Opacity", "Pen Opacity", 100, 0, 100);

    pen_colour_ = add_colour_attribute(
        "Pen Colour", "Pen Colour", utility::ColourTriplet(0.5f, 0.4f, 1.0f));

    draw_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    shapes_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    erase_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_size_->expose_in_ui_attrs_group("annotations_tool_settings");

    draw_pen_size_->set_preference_path("/plugin/annotations/draw_pen_size");
    shapes_pen_size_->set_preference_path("/plugin/annotations/shapes_pen_size");
    erase_pen_size_->set_preference_path("/plugin/annotations/erase_pen_size");
    text_size_->set_preference_path("/plugin/annotations/text_size");
    pen_opacity_->set_preference_path("/plugin/annotations/pen_opacity");
    pen_colour_->set_preference_path("/plugin/annotations/pen_colour");

    // we can register a preference path with each of these attributes. xStudio
    // will then automatically intialised the attribute values from preference
    // file(s) and also, if the attribute is changed, the new value will be
    // written out to preference files.


    active_tool_ = add_string_choice_attribute(
        "Active Tool",
        "Active Tool",
        utility::map_value_to_vec(tool_names_).front(),
        utility::map_value_to_vec(tool_names_));

    active_tool_->expose_in_ui_attrs_group("annotations_tool_settings");
    active_tool_->expose_in_ui_attrs_group("annotations_tool_types");

    shape_tool_ = add_integer_attribute("Shape Tool", "Shape Tool", 0, 0, 2);
    shape_tool_->expose_in_ui_attrs_group("annotations_tool_settings");
    shape_tool_->set_preference_path("/plugin/annotations/shape_tool");

    draw_mode_ = add_string_choice_attribute(
        "Draw Mode",
        "Draw Mode",
        utility::map_value_to_vec(draw_mode_names_).front(),
        utility::map_value_to_vec(draw_mode_names_));
    draw_mode_->expose_in_ui_attrs_group("anno_scribble_mode_backend");
    draw_mode_->set_preference_path("/plugin/annotations/draw_mode");
    draw_mode_->set_role_data(
        module::Attribute::StringChoicesEnabled, std::vector<bool>({true, true, false}));


    // Here we declare QML code to instantiate the actual item that draws
    // the overlay on the viewport. Any attribute that has qml_code role data
    // and that is exposed in the "viewport_overlay_plugins" attributes group will be
    // instantiated as a child of the xStudio 'Viewport' QML Item, and will
    // therefore be stacked on top of the viewport and has visibility on any
    // properties of the main Viewport class.
    auto viewport_code = add_qml_code_attribute(
        "MyCode",
        R"(
            import AnnotationsTool 1.0
            AnnotationsTextItems {
            }
        )");
    viewport_code->expose_in_ui_attrs_group("viewport_overlay_plugins");

    tool_is_active_ =
        add_boolean_attribute("annotations_tool_active", "annotations_tool_active", false);

    tool_is_active_->expose_in_ui_attrs_group("annotations_tool_settings");
    tool_is_active_->set_role_data(
        module::Attribute::MenuPaths,
        std::vector<std::string>({"panels_main_menu_items|Draw Tools"}));

    action_attribute_ = add_string_attribute("action_attribute", "action_attribute", "");
    action_attribute_->expose_in_ui_attrs_group("annotations_tool_settings");

    display_mode_attribute_ = add_string_choice_attribute(
        "Display Mode",
        "Disp. Mode",
        "With Drawing Tools",
        {"Only When Paused", "Always", "With Drawing Tools"});
    display_mode_attribute_->expose_in_ui_attrs_group("annotations_tool_draw_mode");
    display_mode_attribute_->set_preference_path("/plugin/annotations/display_mode");

    // this attr is used to implement the blinking cursor for caption edit mode
    text_cursor_blink_attr_ =
        add_boolean_attribute("text_cursor_blink_attr", "text_cursor_blink_attr", false);

    moving_scaling_text_attr_ =
        add_integer_attribute("moving_scaling_text", "moving_scaling_text", 0);
    moving_scaling_text_attr_->expose_in_ui_attrs_group("annotations_tool_settings");

    // setting the active tool to -1 disables drawing via 'attribute_changed'
    attribute_changed(active_tool_->uuid(), module::Attribute::Value);

    // provide an extension to the base class message handler to handle timed
    // callbacks to fade the laser pen strokes
    message_handler_ = {
        [=](utility::event_atom, bool) {
            // this message is sent when the user finishes a laser bruish stroke
            if (!fade_looping_) {
                fade_looping_ = true;
                anon_send(this, utility::event_atom_v);
            }
        },
        [=](utility::event_atom, bool) {
            // this message is sent when the user finishes a laser bruish stroke
            if (!fade_looping_) {
                fade_looping_ = true;
                anon_send(this, utility::event_atom_v);
            }
        },
        [=](utility::event_atom) {
            // note Annotation::fade_strokes() returns false when all strokes have vanished
            if (is_laser_mode() && current_edited_annotation_ &&
                current_edited_annotation_->fade_strokes(pen_opacity_->value() / 100.f)) {
                delayed_anon_send(this, std::chrono::milliseconds(25), utility::event_atom_v);
            } else {
                fade_looping_ = false;
            }
            redraw_viewport();
        }};

    make_behavior();
    listen_to_playhead_events(true);
}

AnnotationsTool::~AnnotationsTool() = default;

void AnnotationsTool::load_fonts() {

    auto font_files = Fonts::available_fonts();
    for (const auto &f : font_files) {
        try {
            auto font = new SDFBitmapFont(f.second, 96);
            fonts_[f.first].reset(font);
        } catch (std::exception &e) {
            spdlog::warn("Failed to load font: {}.", e.what());
        }
    }
}

void AnnotationsTool::register_hotkeys() {

    toggle_active_hotkey_ = register_hotkey(
        int('D'),
        ui::NoModifier,
        "Toggle Annotations Tool",
        "Show or hide the annotations toolbox. You can start drawing annotations immediately "
        "whenever the toolbox is visible.");

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

void AnnotationsTool::on_playhead_playing_changed(const bool is_playing) {
    playhead_is_playing_ = is_playing;
}

utility::BlindDataObjectPtr AnnotationsTool::prepare_render_data(
    const media_reader::ImageBufPtr &image, const bool offscreen) const {

    const bool annoations_visible =
        offscreen || ((display_mode_ == Always) ||
                      (display_mode_ == WithDrawTool && tool_is_active_->value()) ||
                      (display_mode_ == OnlyWhenPaused && !playhead_is_playing_)) &&
                         (current_edited_annotation_ || current_viewed_annotations_.size());

    utility::BlindDataObjectPtr r;
    if (annoations_visible) {

        auto *render_data_set = new AnnotationRenderDataSet();
        for (auto &anno_uuid : current_viewed_annotations_) {

            if (current_edited_annotation_ &&
                current_edited_annotation_->bookmark_uuid_ == anno_uuid)
                continue;

            auto p = annotations_render_data_.find(anno_uuid);
            if (p != annotations_render_data_.end()) {
                render_data_set->add_annotation_render_data(p->second);
            }
        }

        if (current_edited_annotation_) {
            render_data_set->add_annotation_render_data(
                current_edited_annotation_->render_data());
        }
        r.reset(static_cast<utility::BlindDataObject *>(render_data_set));
    } else {
        for (auto &r : renderers_) {
            r->set_edited_annotation_render_data(AnnotationRenderDataPtr());
        }
    }


    return r;
}

plugin::ViewportOverlayRendererPtr
AnnotationsTool::make_overlay_renderer(const int /*viewer_index*/) {
    renderers_.push_back(new AnnotationsRenderer());
    return plugin::ViewportOverlayRendererPtr(renderers_.back());
}

void AnnotationsTool::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == toggle_active_hotkey_) {

        tool_is_active_->set_value(!tool_is_active_->value());

    } else if (hotkey_uuid == undo_hotkey_ && tool_is_active_->value()) {

        undo();
        redraw_viewport();

    } else if (hotkey_uuid == redo_hotkey_ && tool_is_active_->value()) {

        redo();
        redraw_viewport();
    }
}

void AnnotationsTool::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {}

bool AnnotationsTool::pointer_event(const ui::PointerEvent &e) {

    if (!tool_is_active_->value())
        return false;

    // we don't a render to screen to start until we've processed this pointer
    // event, the reason is that we might get this pointer event at about the
    // same time as a redraw is happening - for low latency drawing we want to
    // make sure the annotation renderer get's the updated graphics for the next
    // refresh. That means whatever happens in this function *must* be quick as
    // we could be holding up the whole UI!
    interact_start();

    bool redraw = false;

    const Imath::V2f pointer_pos = e.position_in_viewport_coord_sys();

    if (e.type() == ui::Signature::EventType::ButtonRelease && current_edited_annotation_ &&
        active_tool_->value() != "Text") {

        end_stroke(pointer_pos);
        redraw = true;

    } else if (
        e.buttons() == ui::Signature::Button::Left &&
        (active_tool_->value() == "Draw" || active_tool_->value() == "Erase")) {

        if (e.type() == ui::Signature::EventType::ButtonDown) {
            start_freehand_pen_stroke(pointer_pos);
        } else if (e.type() == ui::Signature::EventType::Drag) {
            freehand_pen_stroke_point(pointer_pos);
        }
        redraw = true;

    } else if (
        e.buttons() == ui::Signature::Button::Left && active_tool_->value() == "Shapes") {

        if (e.type() == ui::Signature::EventType::ButtonDown) {
            start_shape_placement(pointer_pos);
        } else if (e.type() == ui::Signature::EventType::Drag) {
            update_shape_placement(pointer_pos);
        }
        redraw = true;

    } else if (e.buttons() == ui::Signature::Button::Left && active_tool_->value() == "Text") {

        if (e.type() == ui::Signature::EventType::ButtonDown) {
            start_or_edit_caption(pointer_pos, e.viewport_pixel_scale());
            grab_mouse_focus();
        } else if (e.type() == ui::Signature::EventType::Drag) {
            caption_drag(pointer_pos);
        }
        redraw = true;

    } else if (e.buttons() == ui::Signature::Button::None && active_tool_->value() == "Text") {

        redraw = check_pointer_hover_on_text(pointer_pos, e.viewport_pixel_scale());
    }

    interact_end();
    if (redraw)
        redraw_viewport();
    return false;
}

void AnnotationsTool::interact_start() {

    if (interacting_with_renderers_)
        return;
    for (auto &r : renderers_) {
        r->lock();
    }
    interacting_with_renderers_ = true;
}

void AnnotationsTool::interact_end() {

    if (!interacting_with_renderers_)
        return;
    if (current_edited_annotation_) {
        for (auto &r : renderers_) {
            r->set_edited_annotation_render_data(
                current_edited_annotation_->render_data(), active_tool_->value() == "Text");
        }
    } else {
        for (auto &r : renderers_) {
            r->set_edited_annotation_render_data(AnnotationRenderDataPtr());
        }
    }
    for (auto &r : renderers_) {
        r->unlock();
    }
    interacting_with_renderers_ = false;
}

void AnnotationsTool::start_or_edit_caption(
    const Imath::V2f &p, const float viewport_pixel_scale) {

    if (current_edited_annotation_ && hover_state_ != Caption::NotHovered) {

        if (hover_state_ == Caption::HoveredOnMoveHandle) {
            caption_drag_pointer_start_pos_ = p;
            caption_drag_caption_start_pos_ =
                current_edited_annotation_->edited_caption_position();
        } else if (hover_state_ == Caption::HoveredOnResizeHandle) {
            caption_drag_pointer_start_pos_ = p;
            caption_drag_width_height_      = Imath::V2f(
                current_edited_annotation_->edited_caption_width(),
                current_edited_annotation_->edited_caption_bounding_box().max.y);
        } else if (hover_state_ == Caption::HoveredOnDeleteHandle) {
            current_edited_annotation_->delete_edited_caption();
            clear_caption_overlays();
            release_keyboard_focus();
            return;
        } else if (
            hover_state_ == Caption::HoveredInCaptionArea &&
            current_edited_annotation_->test_click_in_caption(p)) {
            pen_colour_->set_value(current_edited_annotation_->edited_caption_colour());
            text_size_->set_value(current_edited_annotation_->edited_caption_font_size());
            font_choice_->set_value(current_edited_annotation_->edited_caption_font_name());
        }

        for (auto &r : renderers_) {
            r->set_under_mouse_caption_bdb(Imath::Box2f());
        }

    } else {

        if (!current_edited_annotation_)
            create_new_annotation();

        // if there was an 'on screen annotation' this has now
        // been made into the current_edited_annotation_ so we
        // should check if the user was clicking on an existing
        // caption ...
        check_pointer_hover_on_text(p, viewport_pixel_scale);

        // ... ok user was clicking on an existing caption, re-enter  this
        // function to run the bit in the other half of this if() block
        if (current_edited_annotation_ && hover_state_ != Caption::NotHovered) {
            start_or_edit_caption(p, viewport_pixel_scale);
            return;
        }

        current_edited_annotation_->start_new_caption(
            p,
            text_size_->value() * 0.01f,
            text_size_->value(),
            pen_colour_->value(),
            pen_opacity_->value() / 100.0f,
            JustifyLeft,
            font_choice_->value());
    }

    grab_keyboard_focus();
    update_caption_overlay();
    text_cursor_blink_attr_->set_value(!text_cursor_blink_attr_->value());

    current_edited_annotation_->update_render_data();
}

void AnnotationsTool::caption_drag(const Imath::V2f &p) {

    if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {

        const auto delta = p - caption_drag_pointer_start_pos_;
        if (hover_state_ == Caption::HoveredOnMoveHandle) {
            current_edited_annotation_->set_edited_caption_position(
                caption_drag_caption_start_pos_ + delta);
        } else if (hover_state_ == Caption::HoveredOnResizeHandle) {
            current_edited_annotation_->set_edited_caption_width(
                caption_drag_width_height_.x + delta.x);
        }

        update_caption_overlay();
    }
}

void AnnotationsTool::update_caption_overlay() {

    if (current_edited_annotation_) {

        auto edited_capt_bdb = current_edited_annotation_->edited_caption_bounding_box();
        Imath::V2f t, b;
        current_edited_annotation_->caption_cursor_position(t, b);
        interact_start();
        for (auto &r : renderers_) {
            r->set_current_edited_caption_bdb(edited_capt_bdb);
            r->set_cursor_position(t, b);
        }
        interact_end();
    } else {
        interact_start();
        for (auto &r : renderers_) {
            r->set_current_edited_caption_bdb(Imath::Box2f());
            r->set_cursor_position(Imath::V2f(0.0f, 0.0f), Imath::V2f(0.0f, 0.0f));
        }
        interact_end();
    }
}

void AnnotationsTool::start_shape_placement(const Imath::V2f &p) {

    if (!current_edited_annotation_)
        create_new_annotation();

    shape_anchor_ = p;
    shape_stroke_.reset(new PenStroke(
        pen_colour_->value(),
        float(shapes_pen_size_->value()) / PEN_STROKE_THICKNESS_SCALE,
        float(pen_opacity_->value()) / 100.0f));
    current_edited_annotation_->current_stroke_ = shape_stroke_;
    update_shape_placement(p);
}

void AnnotationsTool::update_shape_placement(const Imath::V2f &pointer_pos) {

    if (shape_tool_->value() == Square) {

        shape_stroke_->make_square(shape_anchor_, pointer_pos);

    } else if (shape_tool_->value() == Circle) {

        shape_stroke_->make_circle(shape_anchor_, (shape_anchor_ - pointer_pos).length());

    } else if (shape_tool_->value() == Arrow) {

        shape_stroke_->make_arrow(shape_anchor_, pointer_pos);

    } else if (shape_tool_->value() == Line) {

        shape_stroke_->make_line(shape_anchor_, pointer_pos);
    }
    current_edited_annotation_->update_render_data();
}

bool AnnotationsTool::check_pointer_hover_on_text(
    const Imath::V2f &pointer_pos, const float viewport_pixel_scale) {

    auto old     = hover_state_;
    auto old_bdb = under_mouse_caption_bdb_;
    if (current_edited_annotation_) {

        hover_state_ = current_edited_annotation_->mouse_hover_on_selected_caption(
            pointer_pos, viewport_pixel_scale);
        if (hover_state_ == Caption::NotHovered) {
            under_mouse_caption_bdb_ = current_edited_annotation_->mouse_hover_on_captions(
                pointer_pos, viewport_pixel_scale);
            for (auto &r : renderers_) {
                r->set_under_mouse_caption_bdb(under_mouse_caption_bdb_);
            }
            if (!under_mouse_caption_bdb_.isEmpty()) {
                hover_state_ = Caption::HoveredInCaptionArea;
            }
        }
        if (hover_state_ != old) {
            moving_scaling_text_attr_->set_value(int(hover_state_));
        }


    } else {
        // hover over non edited captions?
        hover_state_             = Caption::NotHovered;
        under_mouse_caption_bdb_ = Imath::Box2f();
        for (auto &anno_uuid : current_viewed_annotations_) {

            auto p = annotations_render_data_.find(anno_uuid);
            if (p != annotations_render_data_.end()) {
                for (const auto &cap_info : p->second->caption_info_) {
                    if (cap_info.bounding_box.intersects(pointer_pos)) {
                        under_mouse_caption_bdb_ = cap_info.bounding_box;
                        break;
                    }
                }
            }
        }
    }

    if (hover_state_ != old || under_mouse_caption_bdb_ != old_bdb) {
        for (auto &r : renderers_) {
            r->set_under_mouse_caption_bdb(under_mouse_caption_bdb_);
            r->set_caption_hover_state(hover_state_);
        }
        return true;
    }
    return false;
}

void AnnotationsTool::end_stroke(const Imath::V2f &) {

    if (current_edited_annotation_) {

        current_edited_annotation_->finished_current_stroke();

        // update annotation data attached to bookmark
        if (!is_laser_mode()) {
            push_annotation_to_bookmark(std::shared_ptr<bookmark::AnnotationBase>(
                static_cast<bookmark::AnnotationBase *>(
                    new Annotation(*current_edited_annotation_))));
        } else {
            // start up the laser fade timer loop - see the event handler
            // in the constructor here to see how this works
            anon_send(caf::actor_cast<caf::actor>(this), utility::event_atom_v, true);
        }
    }
}

void AnnotationsTool::start_freehand_pen_stroke(const Imath::V2f &point) {

    if (!current_edited_annotation_)
        create_new_annotation();

    if (active_tool_->value() == "Draw") {
        current_edited_annotation_->start_pen_stroke(
            pen_colour_->value(),
            float(draw_pen_size_->value()) / PEN_STROKE_THICKNESS_SCALE,
            float(pen_opacity_->value()) / 100.0);
    } else if (active_tool_->value() == "Erase") {
        current_edited_annotation_->start_erase_stroke(
            erase_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE);
    }

    freehand_pen_stroke_point(point);
}

void AnnotationsTool::freehand_pen_stroke_point(const Imath::V2f &point) {

    if (current_edited_annotation_) {
        current_edited_annotation_->add_point_to_current_stroke(point);
    }
}

void AnnotationsTool::text_entered(const std::string &text, const std::string &context) {

    if (active_tool_->value() == "Text" && current_edited_annotation_) {
        current_edited_annotation_->modify_caption_text(text);
        update_caption_overlay();
    }
    redraw_viewport();
}

void AnnotationsTool::key_pressed(
    const int key, const std::string &context, const bool auto_repeat) {
    if (active_tool_->value() == "Text" && current_edited_annotation_) {
        if (key == 16777216) {
            // escape key
            end_stroke();
            release_keyboard_focus();
        }
        current_edited_annotation_->key_down(key);
        update_caption_overlay();
    }
}

void AnnotationsTool::update_attrs_from_preferences(const utility::JsonStore &j) {

    Module::update_attrs_from_preferences(j);

    // this ensures that 'display_mode_' member data is up to date after being
    // updated from prefs
    attribute_changed(display_mode_attribute_->uuid(), module::Attribute::Value);
}

void AnnotationsTool::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    const std::string active_tool = active_tool_->value();

    if (attribute_uuid == tool_is_active_->uuid()) {

        if (tool_is_active_->value()) {
            if (active_tool == "None")
                active_tool_->set_value("Draw");
            grab_mouse_focus();
        } else {
            release_mouse_focus();
            release_keyboard_focus();
            end_stroke();
            moving_scaling_text_attr_->set_value(0);
            clear_caption_overlays();
        }

    } else if (attribute_uuid == active_tool_->uuid()) {

        if (tool_is_active_->value()) {

            if (active_tool == "None") {
                release_mouse_focus();
            } else {
                grab_mouse_focus();
            }

            if (active_tool == "Text") {
            } else {
                end_stroke();
                release_keyboard_focus();
                moving_scaling_text_attr_->set_value(0);
                clear_caption_overlays();
            }
        }

    } else if (
        attribute_uuid == action_attribute_->uuid() && action_attribute_->value() != "") {

        // renderer_->lock();

        if (action_attribute_->value() == "Clear") {
            clear_onscreen_annotations();
        } else if (action_attribute_->value() == "Undo") {
            undo();
        } else if (action_attribute_->value() == "Redo") {
            redo();
        }
        action_attribute_->set_value("");

        /* if (current_edited_annotation_)
            renderer_->set_immediate_render_data(current_edited_annotation_->render_data());

        renderer_->unlock();        */

    } else if (attribute_uuid == display_mode_attribute_->uuid()) {

        if (display_mode_attribute_->value() == "Only When Paused") {
            display_mode_ = OnlyWhenPaused;
        } else if (display_mode_attribute_->value() == "Always") {
            display_mode_ = Always;
        } else if (display_mode_attribute_->value() == "With Drawing Tools") {
            display_mode_ = WithDrawTool;
        }

    } else if (attribute_uuid == draw_mode_->uuid()) {

        if (current_edited_annotation_) {
            if (is_laser_mode()) {
                end_stroke(); // this ensure current edited caption is
                              // finished
                release_keyboard_focus();

                // This 'saves' the current edited annotation by pushing to the bookmark
                push_annotation_to_bookmark(std::shared_ptr<bookmark::AnnotationBase>(
                    static_cast<bookmark::AnnotationBase *>(
                        new Annotation(*current_edited_annotation_))));

                edited_annotation_cache_[current_edited_annotation_->bookmark_uuid_] =
                    current_edited_annotation_;
                last_edited_annotation_uuid_ = current_edited_annotation_->bookmark_uuid_;

                // Now we store the annotation's render data for immediate display
                // since we are about to 'clear' it from the edited annotation
                annotations_render_data_[current_edited_annotation_->bookmark_uuid_] =
                    current_edited_annotation_->render_data();
                current_viewed_annotations_.push_back(
                    current_edited_annotation_->bookmark_uuid_);
            }
            clear_edited_annotation();
        }

    } else if (attribute_uuid == text_cursor_blink_attr_->uuid()) {

        for (auto &r : renderers_) {
            if (!interacting_with_renderers_)
                r->lock();
            r->blink_text_cursor(text_cursor_blink_attr_->value());
            if (!interacting_with_renderers_)
                r->unlock();
        }
        if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {

            // send a delayed message to ourselves to continue the blinking
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(250),
                module::change_attribute_value_atom_v,
                attribute_uuid,
                utility::JsonStore(!text_cursor_blink_attr_->value()),
                true);
        }

    } else if (attribute_uuid == pen_colour_->uuid()) {

        if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {

            current_edited_annotation_->set_edited_caption_colour(pen_colour_->value());
        }

    } else if (attribute_uuid == text_size_->uuid()) {

        if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {
            current_edited_annotation_->set_edit_caption_font_size(text_size_->value());
            update_caption_overlay();
        }
    } else if (attribute_uuid == pen_opacity_->uuid()) {

        if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {

            current_edited_annotation_->set_edited_caption_opacity(
                pen_opacity_->value() / 100.0f);
        }
    } else if (attribute_uuid == font_choice_->uuid()) {

        if (current_edited_annotation_ && current_edited_annotation_->have_edited_caption()) {

            current_edited_annotation_->set_edited_caption_font(font_choice_->value());
            update_caption_overlay();
        }
    }

    if ((attribute_uuid == active_tool_->uuid() || attribute_uuid == draw_mode_->uuid()) &&
        current_edited_annotation_) {

        if (active_tool_->value() == "Draw" && draw_mode_->value() == "Laser") {

            // user might have switch from shapes mode to draw (laser mode) -- need to save
            // whatever was in the current annotations before laser drwaing
            push_annotation_to_bookmark(std::shared_ptr<bookmark::AnnotationBase>(
                static_cast<bookmark::AnnotationBase *>(
                    new Annotation(*current_edited_annotation_))));
            edited_annotation_cache_[current_edited_annotation_->bookmark_uuid_] =
                current_edited_annotation_;
            last_edited_annotation_uuid_ = current_edited_annotation_->bookmark_uuid_;
            clear_edited_annotation();

        } else if (current_edited_annotation_->is_laser_annotation()) {
            clear_edited_annotation();
        }
    }

    redraw_viewport();
}

void AnnotationsTool::undo() {
    if (current_edited_annotation_) {
        current_edited_annotation_->undo();
        // update annotation data attached to bookmark

        push_annotation_to_bookmark(
            std::shared_ptr<bookmark::AnnotationBase>(static_cast<bookmark::AnnotationBase *>(
                new Annotation(*current_edited_annotation_))));
        if (current_edited_annotation_->empty()) {
            clear_onscreen_annotations();
        }
    } else {
        if (current_bookmark_uuid_ && edited_annotation_cache_.find(current_bookmark_uuid_) !=
                                          edited_annotation_cache_.end()) {
            current_edited_annotation_ = edited_annotation_cache_[current_bookmark_uuid_];
            undo();
        } else {
            restore_onscreen_annotations();
        }
    }
}

void AnnotationsTool::redo() {
    if (current_edited_annotation_) {
        current_edited_annotation_->redo();
        // update annotation data attached to bookmark
        push_annotation_to_bookmark(
            std::shared_ptr<bookmark::AnnotationBase>(static_cast<bookmark::AnnotationBase *>(
                new Annotation(*current_edited_annotation_))));
    } else {
        if (edited_annotation_cache_.find(current_bookmark_uuid_) !=
            edited_annotation_cache_.end()) {
            current_edited_annotation_ = edited_annotation_cache_[current_bookmark_uuid_];
            redo();
        } else {
            restore_onscreen_annotations();
        }
    }
}

void AnnotationsTool::clear_caption_overlays() {

    interact_start();
    for (auto &r : renderers_) {
        r->set_current_edited_caption_bdb(Imath::Box2f());
        r->set_under_mouse_caption_bdb(Imath::Box2f());
        r->set_cursor_position(Imath::V2f(0.0f, 0.0f), Imath::V2f(0.0f, 0.0f));
    }
    interact_end();
}

void AnnotationsTool::on_screen_media_changed(
    caf::actor media,
    const utility::MediaReference &media_reference,
    const std::string media_name) {
    on_screen_media_ref_  = media_reference;
    on_screen_media_name_ = media_name;
}

void AnnotationsTool::create_new_annotation() {

    if (is_laser_mode()) {
        current_edited_annotation_.reset(new Annotation(fonts_, true));
        current_edited_annotation_->bookmark_uuid_ = utility::Uuid();
        return;
    }

    if (current_bookmark_uuid_.is_null()) {
        if (std::find(
                current_viewed_annotations_.begin(),
                current_viewed_annotations_.end(),
                last_edited_annotation_uuid_) != current_viewed_annotations_.end()) {
            current_bookmark_uuid_ = last_edited_annotation_uuid_;
        }
    }

    cleared_annotations_serialised_data_.clear();
    if (current_bookmark_uuid_.is_null()) {

        bookmark::BookmarkDetail bmd;
        // this will make a bookmark of single frame duration on the current frame
        bmd.start_    = media_frame_ * on_screen_media_ref_.rate().to_flicks();
        bmd.duration_ = timebase::flicks(0);
        std::stringstream subject;
        std::string name = on_screen_media_name_;
        if (name.rfind("/") != std::string::npos) {
            name = std::string(name, name.rfind("/") + 1);
        }
        subject << name << " annotation @ " << media_logical_frame_;
        bmd.subject_ = subject.str();
        // this will result on
        current_bookmark_uuid_ = StandardPlugin::create_bookmark_on_current_frame(bmd);
    }

    AnnotationPtr annotation_from_cache;
    auto p = edited_annotation_cache_.find(current_bookmark_uuid_);
    if (p != edited_annotation_cache_.end()) {
        annotation_from_cache = p->second;
    }

    // fetch the annotation from the bookmarks
    std::shared_ptr<bookmark::AnnotationBase> curr_anno =
        fetch_annotation(current_bookmark_uuid_);
    Annotation *cast_anno = curr_anno ? dynamic_cast<Annotation *>(curr_anno.get()) : nullptr;
    if (cast_anno) {

        // does the annotation from the bookmarks match the one from our cache?
        // If so, use the one from our cache that still has undo/redo edit
        // history. Otherwise assume that the one from bookmarks is the 'true'
        // annotation.
        if (annotation_from_cache && *cast_anno == *annotation_from_cache) {
            current_edited_annotation_ = annotation_from_cache;
        } else {
            // n.b. bookmarks manager owns 'curr_anno' so we make our own
            // editable copy here
            current_edited_annotation_.reset(new Annotation(*cast_anno));
        }

    } else {
        // make a new blank annotation
        current_edited_annotation_.reset(new Annotation(fonts_));
        current_edited_annotation_->bookmark_uuid_ = current_bookmark_uuid_;
    }

    if (std::find(
            current_viewed_annotations_.begin(),
            current_viewed_annotations_.end(),
            current_bookmark_uuid_) == current_viewed_annotations_.end()) {
        current_viewed_annotations_.push_back(current_bookmark_uuid_);
    }
}

std::shared_ptr<bookmark::AnnotationBase>
AnnotationsTool::build_annotation(const utility::JsonStore &anno_data) {
    return std::shared_ptr<bookmark::AnnotationBase>(
        static_cast<bookmark::AnnotationBase *>(new Annotation(anno_data, fonts_)));
}

void AnnotationsTool::on_screen_frame_changed(
    const timebase::flicks playhead_position,
    const int playhead_logical_frame,
    const int media_frame,
    const int media_logical_frame,
    const utility::Timecode &timecode) {
    playhead_logical_frame_ = playhead_logical_frame;
    media_frame_            = media_frame;
    media_logical_frame_    = media_logical_frame;
    playhead_position_      = playhead_position;
}

void AnnotationsTool::on_screen_annotation_changed(
    std::vector<std::shared_ptr<bookmark::AnnotationBase>> annotations) {

    if (current_edited_annotation_ && !current_edited_annotation_->is_laser_annotation()) {

        // we need to check if the timeline has scrubbed off the in/out range
        // of our currently edited annotation, if so we need to store our edited
        // annotation on the bookmark and erase it here
        bool edited_annotation_still_on_screen = false;
        for (auto &a : annotations) {
            if (a->bookmark_uuid_ == current_edited_annotation_->bookmark_uuid_) {
                edited_annotation_still_on_screen = true;
                break;
            }
        }

        if (!edited_annotation_still_on_screen) {

            // make sure current edited caption is completed
            end_stroke();
            release_keyboard_focus();

            // we have moved off the frame range of the current edited annotation
            // so we make a full copy and push to the bookmark for storage
            push_annotation_to_bookmark(std::shared_ptr<bookmark::AnnotationBase>(
                static_cast<bookmark::AnnotationBase *>(
                    new Annotation(*current_edited_annotation_))));
            clear_edited_annotation();
        }
    }

    if (annotations.size()) {

        current_viewed_annotations_.clear();
        current_bookmark_uuid_ = utility::Uuid();
        for (auto &a : annotations) {
            auto *cast_anno = dynamic_cast<Annotation *>(a.get());
            if (cast_anno) {
                if (current_bookmark_uuid_.is_null())
                    current_bookmark_uuid_ = cast_anno->bookmark_uuid_;
                // note we make our own copy of the annotation since what's being
                // passed in here is a shared pty owned by the bookmark and could
                // be changed elsewhere
                annotations_render_data_[cast_anno->bookmark_uuid_] = cast_anno->render_data();
                current_viewed_annotations_.push_back(cast_anno->bookmark_uuid_);
            }
        }
        if (current_bookmark_uuid_.is_null()) {
            current_bookmark_uuid_ = annotations[0]->bookmark_uuid_;
        }

    } else {
        current_viewed_annotations_.clear();
        current_bookmark_uuid_ = utility::Uuid();
    }

    update_caption_overlay();
}

void AnnotationsTool::clear_onscreen_annotations() {

    if (!(is_laser_mode() && current_edited_annotation_)) {
        cleared_annotations_serialised_data_.push_back(
            clear_annotations_and_bookmarks(current_viewed_annotations_));

        for (const auto &uuid : current_viewed_annotations_) {
            auto p = annotations_render_data_.find(uuid);
            if (p != annotations_render_data_.end())
                annotations_render_data_.erase(p);
        }
    }

    clear_edited_annotation();
}

void AnnotationsTool::restore_onscreen_annotations() {
    if (cleared_annotations_serialised_data_.size()) {
        auto last_cleared = cleared_annotations_serialised_data_.back();
        cleared_annotations_serialised_data_.pop_back();
        restore_annotations_and_bookmarks(last_cleared);
    }
    redraw_viewport();
}

void AnnotationsTool::clear_edited_annotation() {

    release_keyboard_focus();

    if (current_edited_annotation_ && !current_edited_annotation_->is_laser_annotation()) {
        edited_annotation_cache_[current_edited_annotation_->bookmark_uuid_] =
            current_edited_annotation_;
        last_edited_annotation_uuid_ = current_edited_annotation_->bookmark_uuid_;
        annotations_render_data_[current_edited_annotation_->bookmark_uuid_] =
            current_edited_annotation_->render_data();
    }

    current_edited_annotation_.reset();
    current_bookmark_uuid_ = utility::Uuid();

    for (auto &r : renderers_) {
        if (!interacting_with_renderers_)
            r->lock();
        r->set_edited_annotation_render_data(AnnotationRenderDataPtr());
        if (!interacting_with_renderers_)
            r->unlock();
    }

    redraw_viewport();
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<AnnotationsTool>>(
                AnnotationsTool::PLUGIN_UUID,
                "AnnotationsTool",
                plugin_manager::PluginType::PT_VIEWPORT_OVERLAY,
                true,
                "Ted Waine",
                "On Screen Annotations Plugin")}));
}
}