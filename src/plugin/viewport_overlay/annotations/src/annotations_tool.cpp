// SPDX-License-Identifier: Apache-2.0

#include <filesystem>

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

} // anonymous namespace

static int __a_idx = 0;

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


    active_tool_ = add_string_choice_attribute(
        "Active Tool", "Active Tool", "None", utility::map_value_to_vec(tool_names_));
    active_tool_->expose_in_ui_attrs_group("annotations_tool_settings");
    active_tool_->expose_in_ui_attrs_group("annotations_tool_types");

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

    make_behavior();
    connect_to_ui();

    listen_to_playhead_events(true);

    // here's the code for the 'reskin' UI (xSTUDIO v2) where we declare the
    // drawing tools panel creation code.

    register_ui_panel_qml(
        "Drawing Tools",
        R"(
            import AnnotationsTool 2.0
            import QtGraphicalEffects 1.15
            import QtQuick 2.15
            Rectangle {
                anchors.fill: parent
                XsDrawingTools {
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 140
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#5C5C5C" }
                    GradientStop { position: 1.0; color: "#474747" }
                }
            }
        )",
        7.0f);

    dockable_widget_attr_ = register_viewport_dockable_widget(
        "Annotate",
        "qrc:/icons/stylus_note.svg",   // icon for the button to activate the tool
        "Show/Hide Annotation Toolbox", // tooltip for the button,
        3.0f,                           // button position in the buttons bar
        true,
        // qml code to create the left/right dockable widget
        R"(
            import AnnotationsTool 2.0
            import QtQuick 2.15
            XsDrawingToolsLR {
            }
            )",
        // qml code to create the top/bottom dockable widget (left empty here as we don't have
        // one)
        R"(
            import AnnotationsTool 2.0
            import QtQuick 2.15
            XsDrawingToolsTB {
            }
            )",
        toggle_active_hotkey_);
}

AnnotationsTool::~AnnotationsTool() {}

caf::message_handler AnnotationsTool::message_handler_extensions() {

    // provide an extension to the base class message handler to handle timed
    // callbacks to fade the laser pen strokes
    return caf::message_handler(
        [=](utility::event_atom, bool) {
            // this message is sent when the user finishes a laser bruish stroke
            if (!fade_looping_) {
                fade_looping_ = true;
                anon_send(this, utility::event_atom_v);
            }
        },
        [=](utility::event_atom) {
            // note Annotation::fade_all_strokes() returns false when all strokes have vanished
            if (is_laser_mode() &&
                interaction_canvas_.fade_all_strokes(pen_opacity_->value() / 100.f)) {
                delayed_anon_send(this, std::chrono::milliseconds(100), utility::event_atom_v);
            } else {
                fade_looping_ = false;
            }
            redraw_viewport();
        });
}

void AnnotationsTool::attribute_changed(const utility::Uuid &attribute_uuid, const int role) {

    const std::string active_tool = active_tool_->value();

    if (attribute_uuid == active_tool_->uuid()) {

        if (active_tool == "None") {
            release_mouse_focus();
            release_keyboard_focus();
            end_drawing();
            clear_caption_handle();
            set_viewport_cursor("");

        } else {
            last_tool_ = active_tool;
            grab_mouse_focus();
            if (active_tool != "Text") {
                set_viewport_cursor("Qt.CrossCursor");
                end_drawing();
                release_keyboard_focus();
                clear_caption_handle();
            } else {
                set_viewport_cursor("Qt.IBeamCursor");
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
        } else if (display_mode_attribute_->value() == "With Drawing Tools") {
            display_mode_ = WithDrawTool;
        }

    } else if (attribute_uuid == text_cursor_blink_attr_->uuid()) {

        handle_state_.cursor_blink_state = text_cursor_blink_attr_->value();

        if (interaction_canvas_.has_selected_caption()) {

            // send a delayed message to ourselves to continue the blinking
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(500),
                module::change_attribute_value_atom_v,
                attribute_uuid,
                utility::JsonStore(!text_cursor_blink_attr_->value()),
                true);
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
    }

    if (attribute_uuid == active_tool_->uuid()) {

        if (active_tool_->value() == "Laser") {

            // switching INTO laser draw mode ... save any annotation to the
            // bookmark if required
            update_bookmark_annotation_data();
            interaction_canvas_.clear(true);
            clear_caption_handle();
            current_bookmark_uuid_ = utility::Uuid();
        }
    }

    redraw_viewport();
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
}

void AnnotationsTool::hotkey_pressed(
    const utility::Uuid &hotkey_uuid,
    const std::string & /*context*/,
    const std::string & /*window*/) {
    if (hotkey_uuid == toggle_active_hotkey_) {

        // tool_is_active_->set_value(!tool_is_active_->value());

    } else if (hotkey_uuid == undo_hotkey_ && active_tool_->value() != "None") {

        undo();
        redraw_viewport();

    } else if (hotkey_uuid == redo_hotkey_ && active_tool_->value() != "None") {

        redo();
        redraw_viewport();
    } else if (hotkey_uuid == clear_hotkey_ && active_tool_->value() != "None") {

        clear_onscreen_annotations();
    }
}

void AnnotationsTool::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {}

bool AnnotationsTool::pointer_event(const ui::PointerEvent &e) {

    if (active_tool_->value() == "None")
        return false;

    bool redraw = true;

    const Imath::V2f pointer_pos = e.position_in_viewport_coord_sys();

    if (active_tool_->value() == "Draw" || active_tool_->value() == "Erase" ||
        is_laser_mode()) {
        if (e.type() == ui::Signature::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_stroke(image_transformed_ptr_pos(pointer_pos));
        } else if (
            e.type() == ui::Signature::EventType::Drag &&
            e.buttons() == ui::Signature::Button::Left) {
            update_stroke(image_transformed_ptr_pos(pointer_pos));
        } else if (e.type() == ui::Signature::EventType::ButtonRelease) {
            end_drawing();
        }
    } else if (
        active_tool_->value() == "Square" || active_tool_->value() == "Circle" ||
        active_tool_->value() == "Arrow" || active_tool_->value() == "Line") {
        if (e.type() == ui::Signature::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_shape(image_transformed_ptr_pos(pointer_pos));
        } else if (
            e.type() == ui::Signature::EventType::Drag &&
            e.buttons() == ui::Signature::Button::Left) {
            update_shape(image_transformed_ptr_pos(pointer_pos));
        } else if (e.type() == ui::Signature::EventType::ButtonRelease) {
            end_drawing();
        }
    } else if (active_tool_->value() == "Text") {
        if (e.type() == ui::Signature::EventType::ButtonDown &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            start_or_edit_caption(image_transformed_ptr_pos(pointer_pos), e.viewport_pixel_scale());
            grab_mouse_focus();
        } else if (
            e.type() == ui::Signature::EventType::Drag &&
            e.buttons() == ui::Signature::Button::Left) {
            start_editing(e.context(), pointer_pos);
            update_caption_action(image_transformed_ptr_pos(pointer_pos));
            update_caption_handle();
        } else if (e.buttons() == ui::Signature::Button::None) {
            redraw = update_caption_hovered(image_transformed_ptr_pos(pointer_pos), e.viewport_pixel_scale());
        }
    } else {
        redraw = false;
    }

    if (redraw)
        redraw_viewport();

    return false;
}

Imath::V2f AnnotationsTool::image_transformed_ptr_pos(const Imath::V2f &p) const {
    if (image_being_annotated_) {
        Imath::V4f pt(p.x, p.y, 0.0f, 1.0f);
        pt *= image_being_annotated_.layout_transform().inverse();
        return Imath::V2f(pt.x/pt.w, pt.y/pt.w);
    }
    return p;
}

void AnnotationsTool::text_entered(const std::string &text, const std::string &context) {

    if (active_tool_->value() == "Text") {
        interaction_canvas_.update_caption_text(text);
        update_caption_handle();
        redraw_viewport();
    }
}

void AnnotationsTool::key_pressed(
    const int key, const std::string &context, const bool auto_repeat) {

    if (active_tool_->value() == "Text") {
        if (key == 16777216) { // escape key
            end_drawing();
            release_keyboard_focus();
        }
        interaction_canvas_.move_caption_cursor(key);
        update_caption_handle();
        redraw_viewport();
    }
}

utility::BlindDataObjectPtr AnnotationsTool::onscreen_render_data(
    const media_reader::ImageBufPtr &image, const std::string &viewport_name) const {

    // Rendering the viewport (including viewport overlays) occurs in
    // a separate thread to the one that instances of this class live in.
    //
    // xSTUDIO calls this function (in our thread) so we can attach any and all
    // data we want to an image using a 'BlindDataObjectPtr'. We subclass
    // BlindDataObject with AnnotationRenderDataSet allowing us to bung whatever
    // draw time data we want and need. This is then later available in the
    // rendering thread in a thread safe manner (as long as we do it right here
    // and don't pass in pointers to member data of AnnotationsTool - with the
    // exception of the Canvas class which has been made thread safe)

    if (viewport_name != "snapshot_viewport") {
        // snapshot viewport must always draw annotations
        if (!((display_mode_ == Always) ||
              (display_mode_ == WithDrawTool && active_tool_->value() != "None") ||
              (display_mode_ == OnlyWhenPaused && !playhead_is_playing_))) {
            // don't draw annotations, return empty data
            return utility::BlindDataObjectPtr();
        }
    }

    if (image_being_annotated_ == image) {
        // As noted elsewhere, interaction_canvas_ (class = Canvas) is read/write
        // thread safe so we take a reference to it ready for render time.
        auto immediate_render_data = new AnnotationRenderDataSet(
            interaction_canvas_, // note a reference is taken here
            current_bookmark_uuid_,
            handle_state_,
            image_being_annotated_ ? to_string(image_being_annotated_->media_key()) : "");

        return utility::BlindDataObjectPtr(
            static_cast<utility::BlindDataObject *>(immediate_render_data));
    }
    return utility::BlindDataObjectPtr();
        
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
    if (!playhead_playing)
        viewport_current_images_[viewport_name] = images;
    else
        viewport_current_images_[viewport_name].reset();

    if (!interaction_canvas_.empty() && !current_bookmark_uuid_.is_null() &&
        current_interaction_viewport_name_ == viewport_name) {

        bool edited_anno_is_onscreen = false;
        // looks like we are editing an annotation. Is the annotation still on
        // screen (i.e. has the user scrubbed away from it)
        if (images && images->layout_data()) {
            const auto & im_idx = images->layout_data()->image_draw_order_hint_;
            for (auto &idx: im_idx) {
                // loop over onscreen images. Check if the current bookmark is
                // visible on any of them.
                const auto & cim = images->onscreen_image(idx);
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
            interaction_canvas_.clear(true);
            clear_caption_handle();
            current_bookmark_uuid_ = utility::Uuid();

            // calling these updates the renderes with the now cleared
            // interaction canvas data
        }
    }
}

plugin::ViewportOverlayRendererPtr AnnotationsTool::make_overlay_renderer() {
    return plugin::ViewportOverlayRendererPtr(new AnnotationsRenderer());
}

AnnotationBasePtr AnnotationsTool::build_annotation(const utility::JsonStore &anno_data) {
    return AnnotationBasePtr(
        static_cast<bookmark::AnnotationBase *>(new Annotation(anno_data)));
}

bool AnnotationsTool::is_laser_mode() const { return active_tool_->value() == "Laser"; }

void AnnotationsTool::viewport_dockable_widget_activated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value(last_tool_);
    }
}

void AnnotationsTool::viewport_dockable_widget_deactivated(std::string &widget_name) {

    if (widget_name == "Annotate") {
        active_tool_->set_value("None");
    }
}

void AnnotationsTool::turn_off_overlay_interaction() { active_tool_->set_value("None"); }

void AnnotationsTool::start_editing(const std::string &viewport_name, const Imath::V2f &pointer_position) {

    // ensure playback is stopped
    start_stop_playback(viewport_name, false);

    if (is_laser_mode())
        return;


    // if the viewport is in grid mode, with multiple images laid out, which one
    // was clicked in ?
    auto before = image_being_annotated_;
    media_reader::ImageBufPtr new_image_to_annotate;
    auto p = viewport_current_images_.find(viewport_name);
    if (p != viewport_current_images_.end() && p->second) {

        // first, check if pointer_position lands on one of the images in
        // the viewport
        bool curr_im_is_onscreen = false;
        const media_reader::ImageBufDisplaySetPtr &onscreen_image_set = p->second;
        const auto & im_idx = onscreen_image_set->layout_data()->image_draw_order_hint_;
        for (auto &idx: im_idx) {
            // loop over onscreen images. translate pointer position to image
            // space coords
            const auto & cim = onscreen_image_set->onscreen_image(idx);

            if (cim) {

                Imath::V4f pt(pointer_position.x, pointer_position.y, 0.0f, 1.0f);
                pt *= cim.layout_transform().inverse();

                // does the pointer land on the image?
                float a = 1.0f/cim->image_aspect();
                if (pt.x/pt.w >= -1.0f && pt.x/pt.w <= 1.0f && 
                    pt.y/pt.w >= -a && pt.y/pt.w <= a) {
                    new_image_to_annotate = cim;
                }
                // check if image_being_annotated_ (from last time we entered this
                // method) is in the onscreen set
                if (image_being_annotated_ == cim) curr_im_is_onscreen = true;
            }

        }

        if (new_image_to_annotate) {
            image_being_annotated_ = new_image_to_annotate;
        } else if (!curr_im_is_onscreen) {
            image_being_annotated_ = onscreen_image_set->hero_image();
        } else {
            // image_being_annotated_ is unchanged, as it belongs in the
            // onscreen iamge set but we just haven't clicked inside any
            // other images
        }
    } else {
        image_being_annotated_ = media_reader::ImageBufPtr();
    }

    if (image_being_annotated_ != before) {
        // looks like we are starting a new annotation
        current_bookmark_uuid_ = utility::Uuid();
    }

    if (!current_bookmark_uuid_.is_null() &&
        current_interaction_viewport_name_ == viewport_name) {
        return;
    }

    current_interaction_viewport_name_ = viewport_name;
    // Is there an annotation on screen that we should start appending to?
    Annotation *to_edit    = nullptr;
    current_bookmark_uuid_ = utility::Uuid();
    utility::Uuid first_bookmark_uuid;
    if (image_being_annotated_) {

        for (auto &bookmark : image_being_annotated_.bookmarks()) {

            auto anno = dynamic_cast<Annotation *>(bookmark->annotation_.get());
            if (anno) {
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

    clear_caption_handle();

    // clone the whole annotation into our 'interaction_canvas_'
    if (to_edit)
        interaction_canvas_ = to_edit->canvas();
    else {
        // there is a bookmark which doesn't have annotations (yet). We will
        // add annotations to this bookmark
        interaction_canvas_.clear(true);
        current_bookmark_uuid_ = first_bookmark_uuid;
    }
}


void AnnotationsTool::start_stroke(const Imath::V2f &point) {

    if (active_tool_->value() == "Erase") {
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

    if (active_tool_->value() == "Square") {

        interaction_canvas_.start_square(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (active_tool_->value() == "Circle") {

        interaction_canvas_.start_circle(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (active_tool_->value() == "Arrow") {

        interaction_canvas_.start_arrow(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);

    } else if (active_tool_->value() == "Line") {

        interaction_canvas_.start_line(
            pen_colour_->value(),
            shapes_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE,
            pen_opacity_->value() / 100.0f);
    }

    update_shape(p);
}

void AnnotationsTool::update_shape(const Imath::V2f &pointer_pos) {

    if (active_tool_->value() == "Square") {

        interaction_canvas_.update_square(shape_anchor_, pointer_pos);

    } else if (active_tool_->value() == "Circle") {

        interaction_canvas_.update_circle(
            shape_anchor_, (shape_anchor_ - pointer_pos).length());

    } else if (active_tool_->value() == "Arrow") {

        interaction_canvas_.update_arrow(shape_anchor_, pointer_pos);

    } else if (active_tool_->value() == "Line") {

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

    auto &canvas = interaction_canvas_;

    if (canvas.has_selected_caption()) {
        handle_state_.current_caption_bdb = canvas.caption_bounding_box();
        handle_state_.hover_state         = canvas.hover_selected_caption_handle(
            pointer_pos, handle_state_.handle_size, viewport_pixel_scale);
    }
    handle_state_.under_mouse_caption_bdb =
        canvas.hover_caption_bounding_box(pointer_pos, viewport_pixel_scale);
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
        redraw_viewport();
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
        anon_send(caf::actor_cast<caf::actor>(this), utility::event_atom_v, true);

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
                image_being_annotated_.frame_id(), note_name, bookmark::BookmarkDetail(), false);

        }

        if (!current_bookmark_uuid_.is_null())
            update_bookmark_annotation_data();

    }
}


void AnnotationsTool::undo() {

    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.undo();
    update_bookmark_annotation_data();
    redraw_viewport();
}

void AnnotationsTool::redo() {

    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.redo();
    update_bookmark_annotation_data();
    redraw_viewport();
}


void AnnotationsTool::clear_onscreen_annotations() { clear_edited_annotation(); }

void AnnotationsTool::restore_onscreen_annotations() {
    // TODO: reinstate this behaviour
    redraw_viewport();
}

void AnnotationsTool::clear_edited_annotation() {

    release_keyboard_focus();
    start_editing(current_interaction_viewport_name_);
    interaction_canvas_.clear();
    update_bookmark_annotation_data();
    redraw_viewport();
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