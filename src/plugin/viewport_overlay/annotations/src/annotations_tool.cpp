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
#include "xstudio/ui/viewport/viewport_helpers.hpp"

#include "annotations_tool.hpp"
#include "annotation_render_data.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;

namespace fs = std::filesystem;

namespace {

bool find_annotation_by_uuid(
    const std::vector<AnnotationPtr> &annotations, const utility::Uuid &uuid) {

    const auto it =
        std::find_if(annotations.begin(), annotations.end(), [=](const auto &annotation) {
            return annotation->bookmark_uuid_ == uuid;
        });

    return it != annotations.end();
}

std::vector<utility::Uuid> annotations_uuids(const std::vector<AnnotationPtr> &annotations) {
    std::vector<utility::Uuid> res;
    for (const auto &anno : annotations) {
        res.push_back(anno->bookmark_uuid_);
    }
    return res;
}

static int __a_idx = 0;

} // anonymous namespace

AnnotationsTool::AnnotationsTool(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, fmt::format("AnnotationsTool{}", __a_idx++), init_settings) {

    const auto &fonts_ = Fonts::available_fonts();
    font_choice_       = add_string_choice_attribute(
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

    text_bgr_colour_ = add_colour_attribute(
        "Text Background Colour",
        "Text Background Colour",
        utility::ColourTriplet(0.0f, 0.0f, 0.0f));

    text_bgr_opacity_ = add_integer_attribute(
        "Text Background Opacity", "Text Background Opacity", 100, 0, 100);

    draw_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    shapes_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    erase_pen_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    pen_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_size_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_bgr_colour_->expose_in_ui_attrs_group("annotations_tool_settings");
    text_bgr_opacity_->expose_in_ui_attrs_group("annotations_tool_settings");

    draw_pen_size_->set_preference_path("/plugin/annotations/draw_pen_size");
    shapes_pen_size_->set_preference_path("/plugin/annotations/shapes_pen_size");
    erase_pen_size_->set_preference_path("/plugin/annotations/erase_pen_size");
    text_size_->set_preference_path("/plugin/annotations/text_size");
    pen_opacity_->set_preference_path("/plugin/annotations/pen_opacity");
    pen_colour_->set_preference_path("/plugin/annotations/pen_colour");
    text_bgr_colour_->set_preference_path("/plugin/annotations/text_bgr_colour");
    text_bgr_opacity_->set_preference_path("/plugin/annotations/text_bgr_opacity");

    // we can register a preference path with each of these attributes. xStudio
    // will then automatically intialised the attribute values from preference
    // file(s) and also, if the attribute is changed, the new value will be
    // written out to preference files.

    auto v = utility::map_value_to_vec(tool_names_);
    v.pop_back(); // remove 'None' from the list of tools to show in the UI
    active_tool_ = add_string_choice_attribute("Active Tool", "Active Tool", "None", v);
    active_tool_->expose_in_ui_attrs_group("annotations_tool_settings");
    active_tool_->expose_in_ui_attrs_group("annotations_tool_types");

    action_attribute_ = add_string_attribute("action_attribute", "action_attribute", "");
    action_attribute_->expose_in_ui_attrs_group("annotations_tool_settings");

    display_mode_attribute_ = add_string_choice_attribute(
        "Display Mode", "Disp. Mode", "With Drawing Tools", {"Only When Paused", "Always"});
    display_mode_attribute_->expose_in_ui_attrs_group("annotations_tool_draw_mode");
    display_mode_attribute_->set_preference_path("/plugin/annotations/display_mode");

    colour_picker_take_average_ =
        add_boolean_attribute("Colour Pick Average", "Clr. Average", false);
    colour_picker_take_show_magnifier_ =
        add_boolean_attribute("Colour Pick Show Magnifier", "Show Mag", true);
    colour_picker_hide_drawings_ =
        add_boolean_attribute("Colour Pick Hide Drawings", "Hide Drawings", true);
    colour_picker_take_average_->set_preference_path("/plugin/annotations/colour_pick_average");
    colour_picker_take_show_magnifier_->set_preference_path(
        "/plugin/annotations/colour_pick_show_mag");
    colour_picker_hide_drawings_->set_preference_path(
        "/plugin/annotations/colour_pick_hide_drawings");

    colour_picker_take_average_->expose_in_ui_attrs_group("annotations_colour_picker_prefs");
    colour_picker_take_show_magnifier_->expose_in_ui_attrs_group(
        "annotations_colour_picker_prefs");
    colour_picker_hide_drawings_->expose_in_ui_attrs_group("annotations_colour_picker_prefs");

    // this attr is used to implement the blinking cursor for caption edit mode
    text_cursor_blink_attr_ =
        add_boolean_attribute("text_cursor_blink_attr", "text_cursor_blink_attr", false);

    moving_scaling_text_attr_ =
        add_integer_attribute("moving_scaling_text", "moving_scaling_text", 0);
    moving_scaling_text_attr_->expose_in_ui_attrs_group("annotations_tool_settings");

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
            import AnnotationsTool 2.0
            import QtQuick
            XsDrawingTools {
                horizontal: false
            }
            )",
        // qml code to create the top/bottom dockable widget (left empty here as we don't have
        // one)
        R"(
            import AnnotationsTool 2.0
            import QtQuick
            XsDrawingTools {
                horizontal: true
            }
            )",
        toggle_active_hotkey_);
}

AnnotationsTool::~AnnotationsTool() { colour_pipelines_.clear(); }

caf::message_handler AnnotationsTool::message_handler_extensions() {

    // provide an extension to the base class message handler to handle timed
    // callbacks to fade the laser pen strokes
    return caf::message_handler(
        [=](utility::event_atom, bool) {
            // this message is sent when the user finishes a laser bruish stroke
            if (!fade_looping_) {
                fade_looping_ = true;
                anon_mail(utility::event_atom_v).send(this);
            }
        },
        [=](utility::event_atom) {
            // note Annotation::fade_all_strokes() returns false when all strokes have vanished
            if (is_laser_mode() &&
                interaction_canvas_.fade_all_strokes(pen_opacity_->value() / 100.f)) {
                anon_mail(utility::event_atom_v)
                    .delay(std::chrono::milliseconds(25))
                    .send(this);
            } else {
                fade_looping_ = false;
            }
            do_redraw();
        });
}

void AnnotationsTool::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {


    if (attribute_uuid == active_tool_->uuid()) {

        const std::string active_tool = active_tool_->value();
        current_tool_                 = None;
        for (const auto &p : tool_names_) {
            if (p.second == active_tool) {
                current_tool_ = Tool(p.first);
            }
        }

        if (current_tool() != Dropper)
            pixel_patch_.hide();

        if (current_tool() == None) {

            release_mouse_focus();
            release_keyboard_focus();
            end_drawing();
            clear_caption_handle();
            set_viewport_cursor("");
            pixel_patch_.hide();

        } else {

            grab_mouse_focus();
            if (current_tool() == Dropper) {

                set_viewport_cursor("://cursors/point_scan.svg", 24, -1, -1);
                end_drawing();
                release_keyboard_focus();
                clear_caption_handle();

                pixel_patch_.update(
                    std::vector<Imath::V4f>(),
                    Imath::V2f(0.0f, 0.0f),
                    false,
                    colour_picker_hide_drawings_->value());

            } else if (current_tool() != Text) {
                set_viewport_cursor("Qt.CrossCursor");
                end_drawing();
                release_keyboard_focus();
                clear_caption_handle();
                pixel_patch_.hide();
            } else {
                set_viewport_cursor("Qt.IBeamCursor");
                pixel_patch_.hide();
            }

            if (current_tool() == Laser) {

                // switching INTO laser draw mode ... save any annotation to the
                // bookmark if required
                update_bookmark_annotation_data();
                interaction_canvas_.clear(true);
                clear_caption_handle();
                current_bookmark_uuid_ = utility::Uuid();
            } else if (last_tool_ == Laser) {
                interaction_canvas_.clear(true);
            }

            if (current_tool() != Dropper) {
                last_tool_ = current_tool();
            }
        }

    } else if (
        attribute_uuid == action_attribute_->uuid() && action_attribute_->value() != "") {

        if (action_attribute_->value() == "Clear") {
            clear_onscreen_annotations();
        } else if (action_attribute_->value() == "Undo") {
            undo();
        } else if (action_attribute_->value() == "Redo") {
            redo();
        }
        action_attribute_->set_value("");

    } else if (attribute_uuid == display_mode_attribute_->uuid()) {

        if (display_mode_attribute_->value() == "Only When Paused") {
            display_mode_ = OnlyWhenPaused;
        } else if (display_mode_attribute_->value() == "Always") {
            display_mode_ = Always;
        }

    } else if (attribute_uuid == text_cursor_blink_attr_->uuid()) {

        handle_state_.cursor_blink_state = text_cursor_blink_attr_->value();

        if (interaction_canvas_.has_selected_caption()) {

            // send a delayed message to ourselves to continue the blinking
            anon_mail(
                module::change_attribute_value_atom_v,
                attribute_uuid,
                utility::JsonStore(!text_cursor_blink_attr_->value()),
                true)
                .delay(std::chrono::milliseconds(500))
                .send(this);
        }

    } else if (attribute_uuid == pen_colour_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {

            interaction_canvas_.update_caption_colour(pen_colour_->value());
        }

    } else if (attribute_uuid == text_size_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {
            interaction_canvas_.update_caption_font_size(text_size_->value());
            update_caption_handle();
        }
    } else if (attribute_uuid == pen_opacity_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {

            interaction_canvas_.update_caption_opacity(pen_opacity_->value() / 100.0f);
        }
    } else if (attribute_uuid == font_choice_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {

            interaction_canvas_.update_caption_font_name(font_choice_->value());
            update_caption_handle();
        }
    } else if (attribute_uuid == text_bgr_colour_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {

            interaction_canvas_.update_caption_background_colour(text_bgr_colour_->value());
        }
    } else if (attribute_uuid == text_bgr_opacity_->uuid()) {

        if (interaction_canvas_.has_selected_caption()) {

            interaction_canvas_.update_caption_background_opacity(
                text_bgr_opacity_->value() / 100.0f);
        }
    } else if (attribute_uuid == colour_picker_hide_drawings_->uuid()) {
        pixel_patch_.update(
            std::vector<Imath::V4f>(),
            Imath::V2f(0.0f, 0.0f),
            false,
            colour_picker_hide_drawings_->value());
    }


    do_redraw();
}

void AnnotationsTool::update_attrs_from_preferences(const utility::JsonStore &j) {

    Module::update_attrs_from_preferences(j);

    // this ensures that 'display_mode_' member data is up to date after being
    // updated from prefs
    attribute_changed(display_mode_attribute_->uuid(), module::Attribute::Value);
}

void AnnotationsTool::register_hotkeys() {

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
        int('X'),
        ui::ShiftModifier,
        "Clear all strokes",
        "Clears the entire current drawing. If there is no text in the assicated note it will "
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

void AnnotationsTool::hotkey_pressed(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const std::string & /*window*/) {


    if (hotkey_uuid == toggle_active_hotkey_) {

        // tool_is_active_->set_value(!tool_is_active_->value());

    } else if (hotkey_uuid == undo_hotkey_ && current_tool() != None) {

        undo();
        do_redraw();

    } else if (hotkey_uuid == redo_hotkey_ && current_tool() != None) {

        redo();
        do_redraw();

    } else if (hotkey_uuid == clear_hotkey_ && current_tool() != None) {

        clear_onscreen_annotations();

    } else if (
        hotkey_uuid == colour_picker_hotkey_ && current_tool() != None &&
        current_tool() != Dropper) {

        last_tool_ = current_tool();
        active_tool_->set_value(tool_name(Dropper));
    }
}

void AnnotationsTool::hotkey_released(
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

bool AnnotationsTool::pointer_event(const ui::PointerEvent &e) {

    if (current_tool() == None)
        return false;

    bool redraw = true;

    const Imath::V2f pointer_pos = e.position_in_viewport_coord_sys();

    if (current_tool() == Draw || current_tool() == Erase || is_laser_mode()) {
        if (e.type() == ui::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_stroke(image_transformed_ptr_pos(pointer_pos));
        } else if (
            e.type() == ui::EventType::Drag && e.buttons() == ui::Signature::Button::Left) {
            update_stroke(image_transformed_ptr_pos(pointer_pos));
        } else if (e.type() == ui::EventType::ButtonRelease) {
            end_drawing();
        }
    } else if (
        current_tool() == Square || current_tool() == Circle || current_tool() == Arrow ||
        current_tool() == Line) {
        if (e.type() == ui::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_shape(image_transformed_ptr_pos(pointer_pos));
        } else if (
            e.type() == ui::EventType::Drag && e.buttons() == ui::Signature::Button::Left) {
            update_shape(image_transformed_ptr_pos(pointer_pos));
        } else if (e.type() == ui::EventType::ButtonRelease) {
            end_drawing();
        }
    } else if (current_tool() == Text) {
        if (e.type() == ui::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_or_edit_caption(
                image_transformed_ptr_pos(pointer_pos), e.viewport_pixel_scale());
            grab_mouse_focus();
        } else if (
            e.type() == ui::EventType::Drag && e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            update_caption_action(image_transformed_ptr_pos(pointer_pos));
            update_caption_handle();
        } else if (e.buttons() == ui::Signature::Button::None) {
            redraw = update_caption_hovered(
                image_transformed_ptr_pos(pointer_pos), e.viewport_pixel_scale());
        }
    } else if (current_tool() == Dropper) {

        if (e.type() == ui::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            cumulative_picked_colour_ = Imath::V4f(0.0f, 0.0f, 0.0f, 0.0f);
        }
        update_colour_picker_info(e);

    } else {
        redraw = false;
    }

    if (redraw)
        do_redraw();

    return false;
}

Imath::V2f AnnotationsTool::image_transformed_ptr_pos(const Imath::V2f &p) const {
    if (image_being_annotated_ && !is_laser_mode()) {
        Imath::V4f pt(p.x, p.y, 0.0f, 1.0f);
        pt *= image_being_annotated_.layout_transform().inverse();
        return Imath::V2f(pt.x / pt.w, pt.y / pt.w);
    }
    return p;
}

void AnnotationsTool::text_entered(const std::string &text, const std::string &context) {

    if (current_tool() == Text) {
        interaction_canvas_.update_caption_text(text);
        update_caption_handle();
        do_redraw();
    }
}

void AnnotationsTool::key_pressed(
    const int key, const std::string &context, const bool auto_repeat) {

    if (current_tool() == Text) {
        if (key == 16777216) { // escape key
            end_drawing();
            release_keyboard_focus();
        }
        interaction_canvas_.move_caption_cursor(key);
        update_caption_handle();
        do_redraw();
    }
}

utility::BlindDataObjectPtr AnnotationsTool::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid & /*playhead_uuid*/,
    const bool is_hero_image) const {

    bool show_annotations =
        (display_mode_ == Always) || (display_mode_ == OnlyWhenPaused && !playhead_is_playing_);

    auto data = new AnnotationRenderDataSet(
        show_annotations,
        handle_state_,
        current_bookmark_uuid_,
        image.frame_id().key(),
        is_laser_mode());
    return utility::BlindDataObjectPtr(data);
}

void AnnotationsTool::images_going_on_screen(
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


    // It's useful to keep a hold of the images that are on-screen so if the
    // user starts drawing when there is a bookmark on screen then we can
    // add the strokes to that existing bookmark instead of making a brand
    // new note
    viewport_current_images_[viewport_name] = images;

    if (!interaction_canvas_.empty() && !current_bookmark_uuid_.is_null() &&
        current_interaction_viewport_name_ == viewport_name) {

        bool edited_anno_is_onscreen = false;
        // looks like we are editing an annotation. Is the annotation still on
        // screen (i.e. has the user scrubbed away from it)
        if (images && images->layout_data()) {
            const auto &im_idx = images->layout_data()->image_draw_order_hint_;
            for (auto &idx : im_idx) {
                // loop over onscreen images. Check if the current bookmark is
                // visible on any of them.
                const auto &cim = images->onscreen_image(idx);
                for (auto &bookmark : cim.bookmarks()) {

                    auto anno = dynamic_cast<Annotation *>(bookmark->annotation_.get());
                    if (bookmark->detail_.uuid_ == current_bookmark_uuid_) {
                        edited_anno_is_onscreen = true;
                        break;
                    }
                }
            }
        }

        if (!edited_anno_is_onscreen) {
            // the annotation that we were editing is no longer on-screen. The
            // user must have scrubbed away from it in the timeline. Thus we
            // push it to the bookmark and clear
            update_bookmark_annotation_data();
            current_bookmark_uuid_ = utility::Uuid();
            interaction_canvas_.clear(true);
            clear_caption_handle();

            // calling these updates the renderes with the now cleared
            // interaction canvas data
        }
    }
}

plugin::ViewportOverlayRendererPtr
AnnotationsTool::make_overlay_renderer(const std::string &viewport_name) {

    return plugin::ViewportOverlayRendererPtr(
        new AnnotationsRenderer(interaction_canvas_, pixel_patch_, viewport_name));
}

AnnotationBasePtr AnnotationsTool::build_annotation(const utility::JsonStore &anno_data) {
    return AnnotationBasePtr(
        static_cast<bookmark::AnnotationBase *>(new Annotation(anno_data)));
}

bool AnnotationsTool::is_laser_mode() const { return current_tool() == Laser; }

void AnnotationsTool::viewport_dockable_widget_activated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value(tool_name(last_tool_));
    }
}

void AnnotationsTool::viewport_dockable_widget_deactivated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value("None");
    }
}

void AnnotationsTool::turn_off_overlay_interaction() { active_tool_->set_value("None"); }

media_reader::ImageBufPtr AnnotationsTool::image_under_pointer(
    const std::string &viewport_name,
    const Imath::V2f &pointer_position,
    Imath::V2i *pixel_position) {

    media_reader::ImageBufPtr result;
    auto p = viewport_current_images_.find(viewport_name);
    if (p != viewport_current_images_.end() && p->second) {

        bool curr_im_is_onscreen = false;

        // first, check if pointer_position lands on one of the images in
        // the viewport
        const media_reader::ImageBufDisplaySetPtr &onscreen_image_set = p->second;
        if (!onscreen_image_set->layout_data())
            return result;
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

                // check if image_being_annotated_ (from last time we entered this
                // method) is in the onscreen set
                if (image_being_annotated_ == cim)
                    curr_im_is_onscreen = true;
            }
        }

        if (!result && curr_im_is_onscreen) {
            result = onscreen_image_set->hero_image();
        } else if (!result) {
            result = image_being_annotated_;
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
    }

    return result;
}


void AnnotationsTool::start_editing(
    const std::string &viewport_name, const Imath::V2f &pointer_position) {

    // ensure playback is stopped
    start_stop_playback(viewport_name, false);

    // if the viewport is in grid mode, with multiple images laid out, which one
    // was clicked in ?
    auto current_edited_bookmark_id = current_bookmark_uuid_;
    auto before                     = image_being_annotated_;
    image_being_annotated_          = image_under_pointer(viewport_name, pointer_position);

    if (image_being_annotated_ != before) {
        // image has changed since last time we modified an annotation ... we
        // need to run our logic to decide which annotation (if there is an
        // existing one on this frame) the start editing
        current_bookmark_uuid_ = utility::Uuid();
    }

    if (!current_bookmark_uuid_.is_null() &&
        current_interaction_viewport_name_ == viewport_name) {
        // bookmark id is has not changed, neither has the viewport that
        // the interaction is happening in.
        return;
    }

    current_interaction_viewport_name_ = viewport_name;

    if (is_laser_mode()) {

        // laser mode, no 'current' annotation
        current_bookmark_uuid_ = utility::Uuid();
        clear_caption_handle();
        image_being_annotated_ = media_reader::ImageBufPtr();

    } else {

        // The logic here will 'pick' an annotation to start modifying
        // if there is an annotation on the current image.
        // If we were already modifying an annotation and that same annotation
        // is attached to the new image then we will stick with that annotation
        Annotation *to_edit    = nullptr;
        current_bookmark_uuid_ = utility::Uuid();
        utility::Uuid first_bookmark_uuid;
        if (image_being_annotated_) {

            // first, we check if the last bookmark that was being annotated is
            // already attached to this image ...
            for (auto &bookmark : image_being_annotated_.bookmarks()) {

                auto anno = dynamic_cast<Annotation *>(bookmark->annotation_.get());
                if (anno) {
                    if (bookmark->detail_.uuid_ == current_edited_bookmark_id) {
                        current_bookmark_uuid_ = current_edited_bookmark_id;
                        // this on-screen image is carrying the same bookmark
                        // as the one we were already modifying, so we can now
                        // exit
                        return;
                    }
                }
            }


            // if we're here, we need to pick a new bookmark to start adding
            // annotations to
            for (auto &bookmark : image_being_annotated_.bookmarks()) {

                auto anno = dynamic_cast<Annotation *>(bookmark->annotation_.get());
                if (anno) {
                    // we've found a bookmark with an annotation - pick this and
                    // exit loop
                    to_edit                = anno;
                    current_bookmark_uuid_ = bookmark->detail_.uuid_;
                    break;
                } else if (first_bookmark_uuid.is_null() && !bookmark->annotation_) {
                    // note if bookmark->annotation_ is set then its annotation data
                    // from some other plugin (like grading tool) so we only use
                    // existing empty bookmark if there's not annotation data on it
                    first_bookmark_uuid = bookmark->detail_.uuid_;
                }
            }
        }

        if (current_tool() != Text)
            clear_caption_handle();

        // clone the whole annotation into our 'interaction_canvas_'
        if (to_edit) {
            interaction_canvas_ = to_edit->canvas();
        } else {
            // there is a bookmark which doesn't have annotations (yet). We will
            // add annotations to this bookmark
            interaction_canvas_.clear(true);
            current_bookmark_uuid_ = first_bookmark_uuid;
        }
    }
}


void AnnotationsTool::start_stroke(const Imath::V2f &point) {

    if (current_tool() == Erase) {
        interaction_canvas_.start_erase_stroke(
            erase_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE);
    } else {
        interaction_canvas_.start_stroke(
            pen_colour_->value(),
            draw_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            0.0f,
            pen_opacity_->value() / 100.0);
    }

    update_stroke(point);
}

void AnnotationsTool::update_stroke(const Imath::V2f &point) {

    interaction_canvas_.update_stroke(point);
}

void AnnotationsTool::start_shape(const Imath::V2f &p) {

    shape_anchor_ = p;

    if (current_tool() == Square) {

        interaction_canvas_.start_square(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (current_tool() == Circle) {

        interaction_canvas_.start_circle(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (current_tool() == Arrow) {

        interaction_canvas_.start_arrow(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (current_tool() == Line) {

        interaction_canvas_.start_line(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);
    }

    update_shape(p);
}

void AnnotationsTool::update_shape(const Imath::V2f &pointer_pos) {

    if (current_tool() == Square) {

        interaction_canvas_.update_square(shape_anchor_, pointer_pos);

    } else if (current_tool() == Circle) {

        interaction_canvas_.update_circle(
            shape_anchor_, (shape_anchor_ - pointer_pos).length());

    } else if (current_tool() == Arrow) {

        interaction_canvas_.update_arrow(shape_anchor_, pointer_pos);

    } else if (current_tool() == Line) {

        interaction_canvas_.update_line(shape_anchor_, pointer_pos);
    }
}

void AnnotationsTool::start_or_edit_caption(const Imath::V2f &pos, float viewport_pixel_scale) {

    auto &canvas = interaction_canvas_;
    bool selected_new_caption =
        canvas.select_caption(pos, handle_state_.handle_size, viewport_pixel_scale);

    if (!canvas.empty()) {
        update_bookmark_annotation_data();
    }

    // Selecting a new (existing) caption
    if (selected_new_caption) {

        handle_state_.hover_state = HandleHoverState::HoveredInCaptionArea;
        pen_colour_->set_value(canvas.caption_colour());
        text_size_->set_value(canvas.caption_font_size());
        font_choice_->set_value(canvas.caption_font_name());
        text_bgr_colour_->set_value(canvas.caption_background_colour());
        text_bgr_opacity_->set_value(canvas.caption_background_opacity() * 100.0);
    }
    // Interacting with the current caption
    else if (canvas.has_selected_caption()) {

        handle_state_.hover_state = canvas.hover_selected_caption_handle(
            pos, handle_state_.handle_size, viewport_pixel_scale);

        if (handle_state_.hover_state == HandleHoverState::HoveredOnMoveHandle) {
            caption_drag_pointer_start_pos_ = pos;
            caption_drag_caption_start_pos_ = canvas.caption_position();
        } else if (handle_state_.hover_state == HandleHoverState::HoveredOnResizeHandle) {
            caption_drag_pointer_start_pos_ = pos;
            caption_drag_width_height_ =
                Imath::V2f(canvas.caption_width(), canvas.caption_bounding_box().max.y);
        } else if (handle_state_.hover_state == HandleHoverState::HoveredOnDeleteHandle) {
            canvas.delete_caption();
            clear_caption_handle();
            release_keyboard_focus();
            return;
        }
    }
    // Creating a new caption
    else {

        // if there was already a current caption being edited we need
        // to bake that
        canvas.end_draw();

        canvas.start_caption(
            pos,
            font_choice_->value(),
            text_size_->value(),
            pen_colour_->value(),
            pen_opacity_->value() / 100.0f,
            text_size_->value() * 0.01f,
            JustifyLeft,
            text_bgr_colour_->value(),
            text_bgr_opacity_->value() / 100.0f);

        update_bookmark_annotation_data();
    }

    grab_keyboard_focus();

    update_caption_hovered(pos, viewport_pixel_scale);

    update_caption_handle();

    text_cursor_blink_attr_->set_value(!text_cursor_blink_attr_->value());
}

void AnnotationsTool::update_caption_action(const Imath::V2f &p) {

    if (interaction_canvas_.has_selected_caption()) {

        const auto delta = p - caption_drag_pointer_start_pos_;
        if (handle_state_.hover_state == HandleHoverState::HoveredOnMoveHandle) {
            interaction_canvas_.update_caption_position(
                caption_drag_caption_start_pos_ + delta);
        } else if (handle_state_.hover_state == HandleHoverState::HoveredOnResizeHandle) {
            interaction_canvas_.update_caption_width(caption_drag_width_height_.x + delta.x);
        }
    }
}

bool AnnotationsTool::update_caption_hovered(
    const Imath::V2f &pointer_pos, float viewport_pixel_scale) {

    const HandleState old_state = handle_state_;

    if (interaction_canvas_.has_selected_caption()) {
        handle_state_.current_caption_bdb = interaction_canvas_.caption_bounding_box();
        handle_state_.hover_state         = interaction_canvas_.hover_selected_caption_handle(
            pointer_pos, handle_state_.handle_size, viewport_pixel_scale);
    }
    handle_state_.under_mouse_caption_bdb =
        interaction_canvas_.hover_caption_bounding_box(pointer_pos, viewport_pixel_scale);
    if (handle_state_ != old_state) {
        moving_scaling_text_attr_->set_value(int(handle_state_.hover_state));
    }

    return (handle_state_ != old_state);
}

void AnnotationsTool::update_caption_handle() {

    const HandleState old_state = handle_state_;

    if (interaction_canvas_.has_selected_caption()) {
        handle_state_.current_caption_bdb = interaction_canvas_.caption_bounding_box();
        handle_state_.cursor_position     = interaction_canvas_.caption_cursor_position();
    } else {
        handle_state_.current_caption_bdb = Imath::Box2f();
        handle_state_.cursor_position     = {Imath::V2f{0.0f, 0.0f}, Imath::V2f{0.0f, 0.0f}};
    }

    if (handle_state_ != old_state) {
        do_redraw();
    }
}

void AnnotationsTool::clear_caption_handle() {

    moving_scaling_text_attr_->set_value(0);
    handle_state_ = HandleState();
}

void AnnotationsTool::end_drawing() {

    interaction_canvas_.end_draw();

    if (is_laser_mode()) {
        // start up the laser fade timer loop - see the event handler
        // in the constructor here to see how this works
        anon_mail(utility::event_atom_v, true).send(caf::actor_cast<caf::actor>(this));

    } else {
        update_bookmark_annotation_data();
    }
}

void AnnotationsTool::update_bookmark_annotation_data() {

    if (!current_bookmark_uuid_.is_null()) {

        // here we clone 'interaction_canvas_' and pass to the bookmark as
        // an AnnotationBase shared ptr - this gets attached to the bookmark
        // for us by the base class
        auto anno_clone         = new Annotation();
        anno_clone->canvas()    = interaction_canvas_;
        auto annotation_pointer = std::shared_ptr<bookmark::AnnotationBase>(
            static_cast<bookmark::AnnotationBase *>(anno_clone));

        StandardPlugin::update_bookmark_annotation(
            current_bookmark_uuid_, annotation_pointer, interaction_canvas_.empty());

        if (interaction_canvas_.empty()) {
            // annotation has been wiped either through 'clear' operation or
            // by undoing until there are no strokes.
            // If the bookmark has no notes, then StandardPlugin will also
            // erase the empty bookmark.
            // Thus we clear the current_bookmark_uuid_ so we know there is
            // possibly no bookmark to attach any new annotations to.
            current_bookmark_uuid_ = utility::Uuid();
        }

    } else if (!is_laser_mode() && !interaction_canvas_.empty()) {

        // there is no bookmark, meaning the user started annotating a frame
        // with no bookmark. Here the base class creates a new bookmark on the
        // current frame for us

        std::string note_name = "Annotated Frame";
        if (image_being_annotated_) {
            const media::AVFrameID &id = image_being_annotated_.frame_id();
            note_name = fs::path(utility::uri_to_posix_path(id.uri())).stem().string();
            if (note_name.find(".") != std::string::npos) {
                note_name = std::string(note_name, 0, note_name.find("."));
            }
            current_bookmark_uuid_ = StandardPlugin::create_bookmark_on_frame(
                image_being_annotated_.frame_id(),
                note_name,
                bookmark::BookmarkDetail(),
                false);
        }

        if (!current_bookmark_uuid_.is_null())
            update_bookmark_annotation_data();
    }
}


void AnnotationsTool::undo() {

    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.undo();
    update_bookmark_annotation_data();
    do_redraw();
}

void AnnotationsTool::redo() {

    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.redo();
    update_bookmark_annotation_data();
    do_redraw();
}


void AnnotationsTool::clear_onscreen_annotations() { clear_edited_annotation(); }

void AnnotationsTool::restore_onscreen_annotations() {
    // TODO: reinstate this behaviour
    do_redraw();
}

void AnnotationsTool::clear_edited_annotation() {

    release_keyboard_focus();
    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.clear();
    update_bookmark_annotation_data();
    do_redraw();
}

void AnnotationsTool::do_redraw() { redraw_viewport(); }

void AnnotationsTool::update_colour_picker_info(const ui::PointerEvent &e) {

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

caf::actor AnnotationsTool::get_colour_pipeline_actor(const std::string &viewport_name) {

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


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<AnnotationsTool>>(
                AnnotationsTool::PLUGIN_UUID,
                "AnnotationsTool",
                plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true, // this is the 'resident' flag, meaning one instance of the plugin is
                      // created at startup time
                "Ted Waine",
                "On Screen Annotations Plugin")}));
}
}