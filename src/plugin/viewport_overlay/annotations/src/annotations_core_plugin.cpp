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
#include "xstudio/ui/canvas/stroke.hpp"
#include "xstudio/ui/helpers.hpp"

#include "annotations_core_plugin.hpp"
#include "annotation_render_data.hpp"
#include "annotations_ui_plugin.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;

namespace fs = std::filesystem;

AnnotationsCore::AnnotationsCore(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, fmt::format("AnnotationsCore"), init_settings) {

    make_behavior();

    listen_to_playhead_events(true);

    // This allows any other component of xSTUDIO to find this plugin instance
    system().registry().put("ANNOTATIONS_CORE_PLUGIN", this);

    live_edit_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(live_edit_event_group_);
}

AnnotationsCore::~AnnotationsCore() {}

caf::message_handler AnnotationsCore::message_handler_extensions() {

    // provide an extension to the base class message handler to handle timed
    // callbacks to fade the laser pen strokes
    return caf::message_handler(
        [=](utility::event_atom, bool) {
            // special message for laser mode. Used to animate the fading of
            // the laser strokes. We send this message repeatedly in a loop.
            fade_all_laser_strokes();
            if (laser_stroke_animation_) {
                // continue calling ourselves in a loop
                delayed_anon_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::milliseconds(16),
                    utility::event_atom_v,
                    true);
            }
            redraw_viewport();
        },
        [=](utility::event_atom) {
            cursor_blink_ = !cursor_blink_;
            if (cursor_blinking_) {
                delayed_anon_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::milliseconds(500),
                    utility::event_atom_v);
            }
            redraw_viewport();
        },
        [=](utility::event_atom, ui::viewport::annotation_atom, const std::string &data) {},
        [=](bookmark::add_bookmark_atom) {
            // we sent this to ourselves to push live annotation data to the corresponding
            // bookmark
            while (bookmark_update_queue_.size()) {
                auto live_edit_data = *bookmark_update_queue_.begin();
                push_live_edit_to_bookmark(live_edit_data);
                bookmark_update_queue_.erase(bookmark_update_queue_.begin());
            }
        },
        [=](utility::event_atom,
            ui::viewport::annotation_atom,
            const utility::JsonStore &data) { receive_annotation_data(data); },
        [=](ui::viewport::annotation_atom,
            ui::viewport::viewport_atom,
            const std::string &viewport_name,
            const std::string &action) {
            // this is a special message to support hiding of strokes when
            // no playback - this is needed by the sync plugin where we don't
            // want strokes in the video stream because they are rendered
            // directly by the client web browser
            if (action == "DONT_RENDER_STROKES") {
                if (hide_strokes_per_viewport_.find(viewport_name) ==
                    hide_strokes_per_viewport_.end()) {
                    hide_strokes_per_viewport_[viewport_name] = new std::atomic_int(0);
                }
                *(hide_strokes_per_viewport_[viewport_name]) = 1;
            } else if (action == "DO_RENDER_STROKES") {
                if (hide_strokes_per_viewport_.find(viewport_name) ==
                    hide_strokes_per_viewport_.end()) {
                    hide_strokes_per_viewport_[viewport_name] = new std::atomic_int(0);
                }
                *(hide_strokes_per_viewport_[viewport_name]) = 0;
            } else if (action == "DONT_RENDER_LIVE_STROKES") {
                if (hide_strokes_per_viewport_.find(viewport_name) ==
                    hide_strokes_per_viewport_.end()) {
                    hide_strokes_per_viewport_[viewport_name] = new std::atomic_int(0);
                }
                *(hide_strokes_per_viewport_[viewport_name]) = 2;
            }
        },
        [=](utility::event_atom,
            ui::viewport::viewport_atom,
            media::transform_matrix_atom,
            const std::string viewport_name,
            const Imath::M44f &proj_matrix) {
            // these update events come from the global playhead events group
            viewport_transforms_[viewport_name] = proj_matrix;
        },
        [=](broadcast::join_broadcast_atom,
            ui::viewport::annotation_atom,
            caf::actor joiner,
            bool join) {
            // SYNC plugin uses this so it gets updates on live annotations as they are drawn
            anon_mail(broadcast::join_broadcast_atom_v, joiner).send(live_edit_event_group_);
        });
}

void AnnotationsCore::receive_annotation_data(const utility::JsonStore &d) {

    const auto event                = d.value("event", "");
    const auto user_id              = d.value("user_id", utility::Uuid());
    const auto &payload             = d["payload"];
    const std::string viewport_name = payload.is_null() ? "" : payload.value("viewport", "");

    auto &user_edit_data = live_edit_data(user_id);
    if (viewport_name != "") {
        user_edit_data->viewport_name = viewport_name;
    }

    if (event == "PaintStart") {
        start_stroke_or_shape(payload, user_edit_data);
    }

    if (event == "PaintStart" || event == "PaintPoint") {
        modify_stroke_or_shape(payload, user_edit_data);
        broadcast_live_stroke(user_edit_data, user_id);
    } else if (event == "PaintEnd") {
        broadcast_live_stroke(user_edit_data, user_id, true);
        push_live_edit_to_bookmark(user_edit_data);
        user_edit_data->item_type = Canvas::ItemType::None;
    } else if (event == "CaptionStartEdit") {
        start_editing_existing_caption(payload, user_edit_data);
    } else if (event == "CaptionMove") {
        caption_drag(payload, user_edit_data);
    } else if (event == "CaptionEndMove") {
        caption_end_drag(payload, user_edit_data);
    } else if (event == "CaptionProperty") {
        set_caption_property(payload, user_edit_data);
    } else if (event == "CaptionTextEntry") {
        caption_text_entered(payload, user_edit_data);
    } else if (event == "CaptionEndEdit") {
        clear_live_caption(user_edit_data);
    } else if (event == "CaptionKeyPress") {
        caption_key_press(payload, user_edit_data);
    } else if (event == "CaptionInteract") {
        caption_mouse_pressed(payload, user_edit_data);
    } else if (event == "CaptionPointerHover") {
        caption_hover(payload, user_edit_data);
    } else if (event == "ToolChanged") {
        clear_live_caption(user_edit_data);
    } else if (event == "PaintUndo") {
        undo(user_edit_data);
    } else if (event == "PaintRedo") {
        redo(user_edit_data);
    } else if (event == "PaintClear") {
        clear_annotation(user_edit_data);
    } else if (event == "HideDrawings") {
        hide_all_drawings_ = true;
    } else if (event == "ShowDrawings") {
        hide_all_drawings_ = false;
    } else if (event == "SetDisplayMode") {

        if (payload.value("display_mode", "Only When Paused") == "Only When Paused") {
            show_annotations_during_playback_ = false;
        } else {
            show_annotations_during_playback_ = true;
        }
    }

    redraw_viewport();
}

void AnnotationsCore::start_stroke_or_shape(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    const auto item_type = payload.value("item_type", "");
    Imath::V2f pos;
    if (payload.contains("points")) {
        pos = Imath::V2f(
            payload.at("points").at(0).at("x").get<float>(),
            payload.at("points").at(0).at("y").get<float>());
    } else {
        pos =
            Imath::V2f(payload["point"]["x"].get<float>(), payload["point"]["y"].get<float>());
    }

    auto size = payload["paint"]["size"].get<float>();

    // we may have multiple images on the screen (e.g. Grid mode) ...
    // We must pick the image that was clicked on as the frame that will
    // be annotated
    pick_image_to_annotate(pos, user_edit_data);

    // "position" is raw mouse coordinate in viewport area. We need to
    // convert this to the xstudio image coordinate system for the image
    // that is being annotated
    Imath::V2f pointer_position = transform_pointer_to_image_coord(pos, user_edit_data);

    user_edit_data->start_point = pointer_position;

    if (item_type == "Erase") {

        user_edit_data->live_stroke.reset(Stroke::Erase(size));
        user_edit_data->item_type = Canvas::ItemType::Erase;

    } else {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        const float opacity = c[3];

        if (item_type == "Draw") {

            user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0, opacity));
            user_edit_data->item_type = Canvas::ItemType::Draw;

        } else if (item_type == "Brush") {

            auto softness            = payload["paint"]["softness"].get<float>();
            auto size_sensitivity    = payload["paint"]["size_sensitivity"].get<float>();
            auto opacity_sensitivity = payload["paint"]["opacity_sensitivity"].get<float>();
            user_edit_data->live_stroke.reset(
                Stroke::Brush(
                    colour, size, softness, opacity, size_sensitivity, opacity_sensitivity));
            user_edit_data->item_type = Canvas::ItemType::Brush;

        } else if (item_type == "Square") {

            user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, opacity));
            user_edit_data->item_type = Canvas::ItemType::Square;

        } else if (item_type == "Circle") {

            user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, opacity));
            user_edit_data->item_type = Canvas::ItemType::Circle;

        } else if (item_type == "Arrow") {

            user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, opacity));
            user_edit_data->item_type = Canvas::ItemType::Arrow;

        } else if (item_type == "Line") {

            user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, opacity));
            user_edit_data->item_type = Canvas::ItemType::Line;

        } else if (item_type == "Laser") {

            user_edit_data->laser_strokes.emplace_back(
                Stroke::Brush(colour, size, 0.0f, opacity, 0.0f, 1.0f));

            user_edit_data->item_type = Canvas::ItemType::Laser;

            if (!laser_stroke_animation_) {
                laser_stroke_animation_ = true;
                delayed_anon_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::milliseconds(16),
                    utility::event_atom_v,
                    true);
            }
        }
    }

    if (user_edit_data->live_stroke && payload.contains("id")) {
        user_edit_data->live_stroke->set_id(payload["id"].get<std::string>());
    }
}

void AnnotationsCore::modify_stroke_or_shape(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    std::vector<Stroke::Point> points;

    if (payload.contains("points")) {
        for (const auto &i : payload.at("points")) {
            Imath::V2f p;
            if (user_edit_data->item_type == Canvas::ItemType::Laser)
                p = transform_pointer_to_viewport_coord(
                    Imath::V2f(i.at("x").get<float>(), i.at("y").get<float>()), user_edit_data);
            else
                p = transform_pointer_to_image_coord(
                    Imath::V2f(i.at("x").get<float>(), i.at("y").get<float>()), user_edit_data);

            points.emplace_back(p, i.value("pressure", 1.0f));
        }
    } else {
        Imath::V2f p;
        if (user_edit_data->item_type == Canvas::ItemType::Laser)
            p = transform_pointer_to_viewport_coord(
                Imath::V2f(
                    payload.at("point").at("x").get<float>(),
                    payload.at("point").at("y").get<float>()),
                user_edit_data);
        else
            p = transform_pointer_to_image_coord(
                Imath::V2f(
                    payload.at("point").at("x").get<float>(),
                    payload.at("point").at("y").get<float>()),
                user_edit_data);

        points.emplace_back(p, payload.at("point").value("pressure", 1.0f));
    }

    Imath::V2f shape_anchor = user_edit_data->start_point;

    if (user_edit_data->item_type == Canvas::ItemType::Brush ||
        user_edit_data->item_type == Canvas::ItemType::Draw) {

        user_edit_data->live_stroke->add_points(points);

    } else if (user_edit_data->item_type == Canvas::ItemType::Square) {

        user_edit_data->live_stroke->make_square(shape_anchor, points.front().pos);

    } else if (user_edit_data->item_type == Canvas::ItemType::Circle) {

        user_edit_data->live_stroke->make_circle(
            shape_anchor, (shape_anchor - points.front().pos).length());

    } else if (user_edit_data->item_type == Canvas::ItemType::Arrow) {

        user_edit_data->live_stroke->make_arrow(shape_anchor, points.front().pos);

    } else if (user_edit_data->item_type == Canvas::ItemType::Line) {

        user_edit_data->live_stroke->make_line(shape_anchor, points.front().pos);

    } else if (user_edit_data->item_type == Canvas::ItemType::Erase) {

        user_edit_data->live_stroke->add_points(points);

    } else if (user_edit_data->item_type == Canvas::ItemType::Laser) {

        if (!user_edit_data->laser_strokes.empty()) {
            user_edit_data->laser_strokes.back()->add_points(points);
        }
    }
}


void AnnotationsCore::start_editing_existing_caption(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {
    const auto viewport_name = payload.value("viewport", std::string(""));
    auto pos                 = payload["pointer_position"].get<Imath::V2f>();

    if (user_edit_data->live_caption) {

        // first, check if the user is interacting with the current 'live'
        // edited caption
        Imath::V2f pointer_position = transform_pointer_to_image_coord(
            pos, user_edit_data, user_edit_data->annotated_image);
        if (user_edit_data->live_caption->bounding_box().intersects(pointer_position)) {
            // User is actually clicking somewhere in the area of the current edited caption
            user_edit_data->live_caption->set_cursor_position(pointer_position);
            start_cursor_blink();
            return;
        } else if (
            user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnMoveHandle) {
            // even though we've been told to start a new caption, it looks like the user has
            // actually got their pointer hovered over the handle of the current edited
            // caption..
            user_edit_data->drag_start  = pointer_position;
            user_edit_data->start_point = user_edit_data->live_caption->position();
            return;
        } else if (
            user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnResizeHandle) {
            user_edit_data->drag_start    = pointer_position;
            user_edit_data->start_point.x = user_edit_data->live_caption->wrap_width();
            return;
        } else if (
            user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnDeleteHandle) {
            remove_live_caption(user_edit_data);
            return;
        }
    }

    // we may have multiple images on the screen (e.g. Grid mode) ...
    // We must pick the image that was clicked on as the frame that will
    // be annotated
    pick_image_to_annotate(pos, user_edit_data);

    // find the caption under the pointer ...
    utility::Uuid bookmark_uuid;
    auto under_pointer_caption = caption_under_pointer(pos, user_edit_data, bookmark_uuid);
    if (under_pointer_caption) {

        clear_live_caption(user_edit_data);

        Imath::V2f pointer_position = transform_pointer_to_image_coord(pos, user_edit_data);

        // User has clicked on an existing caption. We make a
        // copy of the caption to interact with.

        // We need to store the hash of the existing caption
        // in the bookmark - we use to stop the original rendering
        // while our interaction caption is being drawn instead.
        user_edit_data->skip_render_caption_id = under_pointer_caption->hash();

        user_edit_data->live_caption.reset(new Caption(*under_pointer_caption));
        user_edit_data->live_caption->set_cursor_position(pointer_position);
        user_edit_data->edited_bookmark_id = bookmark_uuid;
        start_cursor_blink();
    }
}

Caption const *AnnotationsCore::caption_under_pointer(
    const Imath::V2f &raw_coord,
    LiveEditData &user_edit_data,
    utility::Uuid &bookmark_uuid,
    std::size_t skip_caption_hash) {

    media_reader::ImageBufPtr img = image_under_pointer(raw_coord, user_edit_data);

    if (!user_edit_data->live_caption) {
        user_edit_data->annotated_image = img;
    }

    const auto pointer_position_in_image =
        transform_pointer_to_image_coord(raw_coord, user_edit_data, img);

    // loop over bookmarks already on the image that the given user is annotating
    for (const auto &bookmark : img.bookmarks()) {

        // does the bookmark already have an annotation on it?
        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(bookmark->annotation_.get());
        if (my_annotation) {
            auto p = my_annotation->canvas().begin();
            while (p != my_annotation->canvas().end()) {
                if (std::holds_alternative<canvas::Caption>(*p)) {
                    const auto &caption = std::get<canvas::Caption>(*p);

                    // Is caption already duplicated into user_edit_data->live_caption ? If so,
                    // we don't want to detect mouse click in the original as we already tested
                    // if user
                    if (skip_caption_hash == caption.hash()) {
                        p++;
                        continue;
                    }

                    if (caption.bounding_box().intersects(pointer_position_in_image)) {

                        bookmark_uuid = bookmark->detail_.uuid_;
                        return &caption;
                    }
                }
                p++;
            }
        }
    }
    return nullptr;
}

void AnnotationsCore::caption_drag(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {
    if (!user_edit_data->live_caption ||
        user_edit_data->caption_handle_over_state_ == HandleHoverState::NotHovered)
        return;

    auto pos          = payload["pointer_position"].get<Imath::V2f>();
    auto vp_pix_scale = payload["viewport_pix_scale"].get<float>();

    Imath::V2f pointer_position = transform_pointer_to_image_coord(pos, user_edit_data);

    if (user_edit_data->caption_handle_over_state_ == HandleHoverState::HoveredOnMoveHandle) {
        user_edit_data->live_caption->set_position(
            user_edit_data->start_point + pointer_position - user_edit_data->drag_start);
    } else if (
        user_edit_data->caption_handle_over_state_ == HandleHoverState::HoveredOnResizeHandle) {
        user_edit_data->live_caption->set_wrap_width(
            std::max(
                0.05f,
                user_edit_data->start_point.x +
                    (pointer_position.x - user_edit_data->drag_start.x)));
    }
}

void AnnotationsCore::caption_end_drag(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {
    if (!user_edit_data->live_caption)
        return;

    if (user_edit_data->caption_handle_over_state_ == HandleHoverState::HoveredOnMoveHandle ||
        user_edit_data->caption_handle_over_state_ == HandleHoverState::HoveredOnResizeHandle) {

        push_live_edit_to_bookmark(user_edit_data);
    }
}

void AnnotationsCore::set_caption_property(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    if (!user_edit_data->live_caption)
        return;

    if (payload.contains("font_size")) {
        user_edit_data->live_caption->set_font_size(payload["font_size"].get<float>());
    }
    if (payload.contains("colour")) {
        user_edit_data->live_caption->set_colour(
            payload["colour"].get<utility::ColourTriplet>());
    }
    if (payload.contains("opacity")) {
        user_edit_data->live_caption->set_opacity(payload["opacity"].get<float>());
    }
    if (payload.contains("font_name")) {
        user_edit_data->live_caption->set_font_name(payload["font_name"].get<std::string>());
    }
    if (payload.contains("background_colour")) {
        user_edit_data->live_caption->set_bg_colour(
            payload["background_colour"].get<utility::ColourTriplet>());
    }
    if (payload.contains("background_opacity")) {
        user_edit_data->live_caption->set_bg_opacity(
            payload["background_opacity"].get<float>());
    }
    redraw_viewport();
    schedule_bookmark_update(user_edit_data);
}

void AnnotationsCore::caption_text_entered(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    if (!user_edit_data->live_caption)
        return;

    const std::string text          = payload.value("text", "");
    const std::string viewport_name = payload.value("viewport", "");
    if (viewport_name == user_edit_data->viewport_name) {
        user_edit_data->live_caption->modify_text(text);
    }
    redraw_viewport();

    //
    schedule_bookmark_update(user_edit_data);
}

void AnnotationsCore::schedule_bookmark_update(LiveEditData &user_edit_data) {

    if (bookmark_update_queue_.find(user_edit_data) == bookmark_update_queue_.end()) {
        bookmark_update_queue_.insert(user_edit_data);
        if (bookmark_update_queue_.size() == 1) {
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(500),
                bookmark::add_bookmark_atom_v);
        }
    }
}

void AnnotationsCore::caption_key_press(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {
    if (!user_edit_data->live_caption)
        return;
    const int key                   = payload.value("key", -1);
    const std::string viewport_name = payload.value("viewport", "");
    if (viewport_name == user_edit_data->viewport_name) {
        user_edit_data->live_caption->move_cursor(key);
    }
    redraw_viewport();
}

void AnnotationsCore::caption_mouse_pressed(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    bool make_new_caption = false;

    auto pos = payload["pointer_position"].get<Imath::V2f>();

    // we may have multiple images on the screen (e.g. Grid mode) ...
    // We must pick the image that was clicked on as the frame that will
    // be annotated
    pick_image_to_annotate(pos, user_edit_data);

    Imath::V2f pointer_position = transform_pointer_to_image_coord(pos, user_edit_data);

    if (user_edit_data->live_caption) {

        user_edit_data->drag_start = pointer_position;

        if (user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnMoveHandle) {
            user_edit_data->start_point = user_edit_data->live_caption->position();
        } else if (
            user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnResizeHandle) {
            user_edit_data->start_point.x = user_edit_data->live_caption->wrap_width();
        } else if (user_edit_data->caption_handle_over_state_ == HandleHoverState::NotHovered) {
            push_live_edit_to_bookmark(user_edit_data);
            make_new_caption = true;
        } else if (
            user_edit_data->caption_handle_over_state_ ==
            HandleHoverState::HoveredOnDeleteHandle) {
            remove_live_caption(user_edit_data);
            return;
        }


    } else {

        make_new_caption = true;
    }

    if (make_new_caption) {

        // user didn't click on an existing caption. Therefore, we create a new one
        // to start editing

        const auto font_name = payload.value("font_name", "");
        const auto font_size = payload.value("font_size", 0.01f);
        const auto colour  = payload.value("colour", utility::ColourTriplet(1.0f, 0.0f, 0.0f));
        const auto opacity = payload.value("opacity", 1.0f);
        const auto wrap_width    = payload.value("wrap_width", 0.1f);
        const auto justification = payload.value("justification", int(JustifyLeft));
        const auto background_colour =
            payload.value("background_colour", utility::ColourTriplet(0.0f, 0.0f, 0.0f));
        const auto background_opacity = payload.value("background_opacity", 0.5f);

        clear_live_caption(user_edit_data);

        user_edit_data->live_caption.reset(new Caption(
            pointer_position,
            wrap_width,
            font_size,
            colour,
            opacity,
            Justification(justification),
            font_name,
            background_colour,
            background_opacity));
        user_edit_data->skip_render_caption_id = 0;
        user_edit_data->live_caption->set_cursor_position(pointer_position);
        start_cursor_blink();
    }
}

xstudio::ui::viewport::HandleHoverState mouse_hover(
    const Caption &capt,
    const Imath::V2f &pos,
    const Imath::V2f &handle_size,
    const float viewport_pixel_scale) {

    const Imath::V2f cp_move   = capt.bounding_box().min - pos;
    const Imath::V2f cp_resize = pos - capt.bounding_box().max;
    const Imath::V2f cp_delete =
        pos - Imath::V2f(
                  capt.bounding_box().max.x,
                  capt.bounding_box().min.y - handle_size.y * viewport_pixel_scale);
    const Imath::Box2f handle_extent =
        Imath::Box2f(Imath::V2f(0.0f, 0.0f), handle_size * viewport_pixel_scale);

    if (handle_extent.intersects(cp_move)) {
        return xstudio::ui::viewport::HandleHoverState::HoveredOnMoveHandle;
    } else if (handle_extent.intersects(cp_resize)) {
        return xstudio::ui::viewport::HandleHoverState::HoveredOnResizeHandle;
    } else if (handle_extent.intersects(cp_delete)) {
        return xstudio::ui::viewport::HandleHoverState::HoveredOnDeleteHandle;
    } else if (capt.bounding_box().intersects(pos)) {
        return xstudio::ui::viewport::HandleHoverState::HoveredInCaptionArea;
    }
    return xstudio::ui::viewport::HandleHoverState::NotHovered;
}

void AnnotationsCore::caption_hover(
    const utility::JsonStore &payload, LiveEditData &user_edit_data) {

    auto pos           = payload["pointer_position"].get<Imath::V2f>();
    auto vp_pix_scale  = payload["viewport_pix_scale"].get<float>();
    auto viewport_name = payload["viewport"].get<std::string>();

    Imath::V2f pointer_position = transform_pointer_to_image_coord(pos, user_edit_data);

    const auto old     = user_edit_data->caption_handle_over_state_;
    const auto old_box = under_mouse_caption_bdb_;

    user_edit_data->caption_handle_over_state_ = HandleHoverState::NotHovered;

    if (user_edit_data->live_caption) {

        // are we hovered on the 'live' caption that is currently being edited
        user_edit_data->caption_handle_over_state_ = mouse_hover(
            *user_edit_data->live_caption,
            pointer_position,
            Imath::V2f(50.0f, 50.0f),
            vp_pix_scale);
    }

    if (user_edit_data->caption_handle_over_state_ == HandleHoverState::NotHovered) {

        utility::Uuid uuid;
        Caption const *capt = caption_under_pointer(pos, user_edit_data, uuid);
        if (capt) {
            user_edit_data->caption_handle_over_state_ = HandleHoverState::HoveredInCaptionArea;
            under_mouse_caption_bdb_                   = capt->bounding_box();
        } else {
            under_mouse_caption_bdb_ = Imath::Box2f();
        }
    } else {
        under_mouse_caption_bdb_ = Imath::Box2f();
    }

    if (user_edit_data->caption_handle_over_state_ != old ||
        under_mouse_caption_bdb_ != old_box) {
        redraw_viewport();
    }
}

media_reader::ImageBufPtr AnnotationsCore::image_under_pointer(
    const Imath::V2f &raw_pointer_position,
    const LiveEditData &user_edit_data,
    bool *curr_im_is_onscreen) {

    // raw_pointer_position should span 0.0-1.0 across the viewport width and height,
    // i.e. it is mormalised pointer position (s,t coords, if you like)

    // convert to xSTUDIO viewport coords (Spans from -1.0 to 1.0 in x & y)
    const Imath::V2f viewport_pointer_position =
        transform_pointer_to_viewport_coord(raw_pointer_position, user_edit_data);

    media_reader::ImageBufPtr result;

    const media_reader::ImageBufDisplaySetPtr &onscreen_image_set =
        get_viewport_image_set(user_edit_data->viewport_name);

    if (!onscreen_image_set || !onscreen_image_set->layout_data()) {
        return result;
    }

    const auto &im_idx = onscreen_image_set->layout_data()->image_draw_order_hint_;
    for (auto &idx : im_idx) {
        // loop over onscreen images. translate pointer position to image
        // space coords
        const auto &cim = onscreen_image_set->onscreen_image(idx);

        if (cim) {

            // apply the image transform to get the pointer position in image
            // coords
            Imath::V4f pt(viewport_pointer_position.x, viewport_pointer_position.y, 0.0f, 1.0f);
            pt *= cim.layout_transform().inverse();

            // does the pointer land on the image?
            float a = 1.0f / image_aspect(cim);
            if (pt.x / pt.w >= -1.0f && pt.x / pt.w <= 1.0f && pt.y / pt.w >= -a &&
                pt.y / pt.w <= a) {
                result = cim;
                break;
            }

            // check if image_being_annotated_ (from last time we entered this
            // method) is in the onscreen set - i.e. the last image we interacted
            // with is still on-screen
            if (curr_im_is_onscreen &&
                user_edit_data->annotated_image.frame_id() == cim.frame_id()) {
                *curr_im_is_onscreen = true;
            }
        }
    }

    if (!result && (!curr_im_is_onscreen || !(*curr_im_is_onscreen))) {
        // fallback to hero image, if no other image under the pointer and
        result = onscreen_image_set->hero_image();
    }

    return result;
}

Annotation *AnnotationsCore::modifiable_annotation(LiveEditData &user_edit_data) {

    // first, check if the given bookmark is actually visible on any images that
    // are currently on screen. If not, we return nullptr because for the purposes
    // of undo/redo mechanism we don't want to undo/redo changes to an annotation
    // that is (no longer) on the screen because a different frame is now being
    // viewed

    bool anno_on_screen     = false;
    auto onscreen_image_set = get_viewport_image_set(user_edit_data->viewport_name);
    if (!onscreen_image_set)
        return nullptr;
    const auto &im_idx = onscreen_image_set->layout_data()->image_draw_order_hint_;
    for (auto &idx : im_idx) {
        // loop over onscreen images, checking for a bookmark match
        const auto &cim = onscreen_image_set->onscreen_image(idx);
        for (const auto &bookmark : cim.bookmarks()) {
            if (bookmark->detail_.uuid_ == user_edit_data->edited_bookmark_id) {
                anno_on_screen = true;
                break;
            }
        }
        if (anno_on_screen)
            break;
    }

    if (!anno_on_screen)
        return nullptr;

    AnnotationBasePtr existing_annotation =
        get_bookmark_annotation(user_edit_data->edited_bookmark_id);

    const Annotation *my_annotation =
        dynamic_cast<const Annotation *>(existing_annotation.get());
    Annotation *mod_annotation = my_annotation ? new Annotation(*my_annotation) : nullptr;

    return mod_annotation;
}

void AnnotationsCore::remove_live_caption(LiveEditData &user_edit_data) {

    Annotation *mod_annotation = modifiable_annotation(user_edit_data);

    undoable_action<DeleteCaption>(
        user_edit_data, mod_annotation, user_edit_data->live_caption->id());
    user_edit_data->live_caption.reset();

    update_bookmark_annotation(
        user_edit_data->edited_bookmark_id, AnnotationBasePtr(mod_annotation), false);
}

void AnnotationsCore::clear_live_caption(LiveEditData &user_edit_data) {

    if (user_edit_data->live_caption) {
        push_live_edit_to_bookmark(user_edit_data);
        user_edit_data->live_caption.reset();
    }
    under_mouse_caption_bdb_                   = Imath::Box2f();
    user_edit_data->caption_handle_over_state_ = HandleHoverState::NotHovered;
}

void AnnotationsCore::pick_image_to_annotate(
    const Imath::V2f &raw_pointer_position, LiveEditData &user_edit_data) {

    bool current_image_is_still_on_screen = false;
    media_reader::ImageBufPtr img         = image_under_pointer(
        raw_pointer_position, user_edit_data, &current_image_is_still_on_screen);

    if (img && user_edit_data->annotated_image.frame_id().key() != img.frame_id().key()) {
        clear_live_caption(user_edit_data);
    }

    if (img || !current_image_is_still_on_screen) {
        user_edit_data->annotated_image = img;
    }

    user_edit_data->edited_bookmark_id = utility::Uuid();

    AnnotationBasePtr annotation_to_add_to;

    // loop over bookmarks already on the image that the given user is annotating
    for (const auto &anno : user_edit_data->annotated_image.bookmarks()) {

        // does the bookmark already have an annotation on it?
        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(anno->annotation_.get());
        if (my_annotation) {
            user_edit_data->edited_bookmark_id = anno->detail_.uuid_;
            annotation_to_add_to               = anno->annotation_;
            break;
        }
    }

    if (user_edit_data->edited_bookmark_id.is_null()) {
        // we didn't find an existing annotation to edit. We now check if there
        // is a bookmark WITHOUT an annotation the we can use to start adding
        // annotations to
        if (!user_edit_data->annotated_image.bookmarks().empty() &&
            user_edit_data->annotated_image.bookmarks()[0]->detail_.user_type_.value_or("") !=
                "Grading") {
            user_edit_data->edited_bookmark_id =
                user_edit_data->annotated_image.bookmarks()[0]->detail_.uuid_;
        }
    }

    // for xSTUDIO Sync plugin, we need to send it the whole of the annotation
    // that we're about to start adding a stroke to so it can send annotations
    // data to web clients (so that they can render the annotation locally)
    if (user_edit_data->edited_bookmark_id.is_null()) {
        annotation_about_to_be_edited(annotation_to_add_to, next_bookmark_uuid_);
    } else {
        annotation_about_to_be_edited(annotation_to_add_to, user_edit_data->edited_bookmark_id);
    }
}

Imath::V2f AnnotationsCore::transform_pointer_to_image_coord(
    const Imath::V2f &raw_pointer_position,
    const LiveEditData &user_edit_data,
    const media_reader::ImageBufPtr &image) {

    Imath::V2f viewport_coord =
        transform_pointer_to_viewport_coord(raw_pointer_position, user_edit_data);
    Imath::V4f pt(viewport_coord.x, viewport_coord.y, 0.0f, 1.0f);
    pt *= image.layout_transform().inverse();

    return Imath::V2f(pt.x / pt.w, pt.y / pt.w);
}

Imath::V2f AnnotationsCore::transform_pointer_to_image_coord(
    const Imath::V2f &raw_pointer_position, const LiveEditData &user_edit_data) {

    return transform_pointer_to_image_coord(
        raw_pointer_position, user_edit_data, user_edit_data->annotated_image);
}

Imath::V2f AnnotationsCore::transform_pointer_to_viewport_coord(
    const Imath::V2f &raw_pointer_position, const LiveEditData &user_edit_data) {

    // raw_pointer_position should span 0.0-1.0 across the viewport width and height,
    // i.e. it is mormalised pointer position (s,t coords, if you like)

    // convert to xSTUDIO viewport coords (Spans from -1.0 to 1.0 in x & y)
    Imath::V2f viewport_pointer_position(
        raw_pointer_position.x * 2.0f - 1.0f, 1.0f - raw_pointer_position.y * 2.0f);

    // Now apply viewport pan/zoom
    auto q = viewport_transforms_.find(user_edit_data->viewport_name);
    if (q != viewport_transforms_.end()) {
        Imath::V4f pp(viewport_pointer_position.x, viewport_pointer_position.y, 0.0f, 1.0f);
        pp                          = pp * q->second;
        viewport_pointer_position.x = pp.x / pp.w;
        viewport_pointer_position.y = pp.y / pp.w;
    }

    return viewport_pointer_position;
}


utility::BlindDataObjectPtr AnnotationsCore::onscreen_render_data(
    const media_reader::ImageBufDisplaySetPtr & /*image_set*/,
    const std::string & /*viewport_name*/,
    const utility::Uuid & /*playhead_uuid*/) const {

    LaserStrokesRenderDataSet *data = nullptr;

    for (const auto &p : live_edit_data_) {

        const auto &user_edit_data = p.second;
        if (!user_edit_data->laser_strokes.empty()) {
            if (!data) {
                data = new LaserStrokesRenderDataSet();
            }
            data->add_laser_strokes(user_edit_data->laser_strokes);
        }
    }
    return utility::BlindDataObjectPtr(data);
}


utility::BlindDataObjectPtr AnnotationsCore::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string &viewport_name,
    const utility::Uuid & /*playhead_uuid*/,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    if (hide_all_drawings_)
        return utility::BlindDataObjectPtr();

    PerImageAnnotationRenderDataSet *data = nullptr;
    if (!current_edited_annotation_uuid_.is_null()) {
        data = new PerImageAnnotationRenderDataSet();
        data->set_skip_annotation_uuid(current_edited_annotation_uuid_);
    }

    for (const auto &p : live_edit_data_) {

        const auto &user_edit_data = p.second;

        if (user_edit_data->annotated_image.frame_id().key() != image.frame_id().key())
            continue;

        const auto &edited_bookmark_id = user_edit_data->edited_bookmark_id;

        // we make a full copy of the 'live' edited canvas here. Don't worry,
        // our live canvases only have one stroke or caption (the one being
        // created right now by the given user)
        if (user_edit_data->live_stroke) {

            if (!data)
                data = new PerImageAnnotationRenderDataSet();

            if (!edited_bookmark_id.is_null() &&
                user_edit_data->item_type == Canvas::ItemType::Erase) {

                // To make things awkward, we need to inject 'live' erase strokes into the
                // render command so that the erase gets applied to whatever bookmark the erase
                // stroke will effect when it is complete. Before it is complete (before the
                // user lifts the pen or releases the mouse button) the erase stroke is not part
                // of the bookmark.
                data->add_erase_stroke(user_edit_data->live_stroke.get(), edited_bookmark_id);

            } else {

                data->add_stroke(user_edit_data->live_stroke.get());
            }
        }

        if (user_edit_data->viewport_name == viewport_name) {

            if (user_edit_data->live_caption) {

                if (!data)
                    data = new PerImageAnnotationRenderDataSet();
                data->add_live_caption(
                    user_edit_data->live_caption.get(),
                    user_edit_data->caption_handle_over_state_);
                data->add_skip_render_caption_id(user_edit_data->skip_render_caption_id);

            } else if (!under_mouse_caption_bdb_.isEmpty()) {

                if (!data)
                    data = new PerImageAnnotationRenderDataSet();
                data->add_hovered_caption_box(under_mouse_caption_bdb_);
            }
        }
    }
    return utility::BlindDataObjectPtr(data);
}

void AnnotationsCore::images_going_on_screen(
    const media_reader::ImageBufDisplaySetPtr &images,
    const std::string viewport_name,
    const bool playhead_playing) {

    viewport_current_images_[viewport_name] = images;

    if (hide_all_per_viewport_.find(viewport_name) == hide_all_per_viewport_.end()) {
        hide_all_per_viewport_[viewport_name] = new std::atomic_bool(false);
    }
    *(hide_all_per_viewport_[viewport_name]) =
        show_annotations_during_playback_ ? false : playhead_playing;

    // what if a new image is going on screen, and we have an active edit going
    // on with the given viewport? We need to wipe the active edit so that we
    // don't see the caption overlays
    bool images_went_off_the_screen = false;
    auto p                          = live_edit_data_.begin();
    while (p != live_edit_data_.end()) {
        if (p->second->viewport_name == viewport_name &&
            p->second->item_type != Canvas::ItemType::Laser) {
            bool still_on_screen = false;
            for (int i = 0; i < images->num_onscreen_images(); ++i) {

                if (images->onscreen_image(i).frame_id().key() ==
                    p->second->annotated_image.frame_id().key()) {
                    // updating the annotated image means the attached bookmark
                    // is up-to-date
                    p->second->annotated_image = images->onscreen_image(i);
                    still_on_screen            = true;
                    break;
                }
            }
            if (!still_on_screen) {
                p                          = live_edit_data_.erase(p);
                cursor_blinking_           = false;
                images_went_off_the_screen = true;
            }

            else
                p++;
        } else {
            p++;
        }
    }

    // if the on-screen frame(s) have changed, is the bookmark that we were
    // editing still on screen? If not, we need to inform plugins that
    if (!current_edited_annotation_uuid_.is_null() && images_went_off_the_screen) {

        bool current_edited_bookmark_is_on_screen = false;
        for (int i = 0; i < images->num_onscreen_images(); ++i) {
            for (const auto &bookmark : images->onscreen_image(i).bookmarks()) {
                if (bookmark->detail_.uuid_ == current_edited_annotation_uuid_) {
                    current_edited_bookmark_is_on_screen = true;
                }
            }
        }
        if (!current_edited_bookmark_is_on_screen) {
            annotation_about_to_be_edited(nullptr, utility::Uuid());
        }
    }
}

plugin::ViewportOverlayRendererPtr
AnnotationsCore::make_overlay_renderer(const std::string &viewport_name) {

    // Note ... using these atomics is awkward. The trouble is the instance of
    // the overlay renderer is owned by the xSTUDIO UI (Viewport) and can be
    // destroyed without us knowing. So how do we communicate with it when
    // some state changes?
    // TODO: find a better (neater) way!
    if (hide_strokes_per_viewport_.find(viewport_name) == hide_strokes_per_viewport_.end()) {
        hide_strokes_per_viewport_[viewport_name] = new std::atomic_int(0);
    }

    if (hide_all_per_viewport_.find(viewport_name) == hide_all_per_viewport_.end()) {
        hide_all_per_viewport_[viewport_name] = new std::atomic_bool(false);
    }

    return plugin::ViewportOverlayRendererPtr(new AnnotationsRenderer(
        viewport_name,
        cursor_blink_,
        hide_all_drawings_,
        hide_strokes_per_viewport_[viewport_name],
        hide_all_per_viewport_[viewport_name]));
}

AnnotationBasePtr AnnotationsCore::build_annotation(const utility::JsonStore &anno_data) {
    return AnnotationBasePtr(
        static_cast<bookmark::AnnotationBase *>(new Annotation(anno_data)));
}

void AnnotationsCore::undo(LiveEditData &user_edit_data) {
    if (user_edit_data->live_caption) {
        push_live_edit_to_bookmark(user_edit_data);
        user_edit_data->live_caption.reset();
    }

    // get the bookmark id for the next undo-able event in the undo/redo history
    const utility::Uuid bookmark_for_undo_id =
        undo_redo_impl_.get_bookmark_id_for_next_undo(user_edit_data->user_id);

    Annotation *mod_annotation = modifiable_annotation(user_edit_data);

    if (undo_redo_impl_.undo(user_edit_data->user_id, &mod_annotation)) {

        AnnotationBasePtr modified_annotation(mod_annotation);
        update_bookmark_annotation(bookmark_for_undo_id, modified_annotation, false);

        if (current_edited_annotation_uuid_ != bookmark_for_undo_id) {
            annotation_about_to_be_edited(modified_annotation, bookmark_for_undo_id);
        } else {
            mail(utility::event_atom_v, annotation_data_atom_v, modified_annotation)
                .send(live_edit_event_group_);
        }

    } else {

        delete mod_annotation;
    }
}


void AnnotationsCore::redo(LiveEditData &user_edit_data) {
    // get the bookmark id for the next undo-able event in the undo/redo history
    const utility::Uuid bookmark_for_undo_id =
        undo_redo_impl_.get_bookmark_id_for_next_redo(user_edit_data->user_id);

    Annotation *mod_annotation = modifiable_annotation(user_edit_data);

    if (undo_redo_impl_.redo(user_edit_data->user_id, &mod_annotation)) {

        AnnotationBasePtr modified_annotation(mod_annotation);
        update_bookmark_annotation(bookmark_for_undo_id, modified_annotation, false);

        if (current_edited_annotation_uuid_ != bookmark_for_undo_id) {
            annotation_about_to_be_edited(modified_annotation, bookmark_for_undo_id);
        } else {
            mail(utility::event_atom_v, annotation_data_atom_v, modified_annotation)
                .send(live_edit_event_group_);
        }

    } else {

        delete mod_annotation;
    }
}

void AnnotationsCore::broadcast_live_stroke(
    const LiveEditData &user_edit_data,
    const utility::Uuid &user_id,
    const bool stroke_completed) {

    Annotation *anno = new Annotation();
    if (user_edit_data->live_stroke)
        anno->canvas().append_item(*(user_edit_data->live_stroke));

    mail(
        utility::event_atom_v,
        annotation_data_atom_v,
        AnnotationBasePtr(anno),
        user_id,
        stroke_completed)
        .send(live_edit_event_group_);
}

void AnnotationsCore::make_bookmark_for_annotations(
    const media::AVFrameID &frame_id, const utility::Uuid &bm_id) {

    bookmark::BookmarkDetail detail;
    detail.uuid_ = bm_id;
    std::string note_name =
        fs::path(utility::uri_to_posix_path(frame_id.uri())).stem().string();
    if (note_name.find(".") != std::string::npos) {
        note_name = std::string(note_name, 0, note_name.find("."));
    }

    create_bookmark_on_frame(frame_id, note_name, detail, false);
}

namespace xstudio {
namespace ui {
    namespace viewport {

        class CreateBookmark : public UndoableAction {

          public:
            CreateBookmark(
                const media::AVFrameID &frameid, AnnotationsCore *plugin, utility::Uuid &bm_id)
                : frameid_(frameid), plugin_(plugin), bm_id_(bm_id) {}

            bool redo(Annotation **annotation) override {
                plugin_->make_bookmark_for_annotations(frameid_, bm_id_);
                *annotation = new Annotation();
                return true;
            }

            bool undo(Annotation ** /*annotation*/) override {
                plugin_->remove_bookmark(bm_id_);
                return true;
            }

            friend class AnnotationsCore;

            const media::AVFrameID frameid_;
            AnnotationsCore *plugin_;
            utility::Uuid bm_id_;
        };

        class ClearAnnotation : public UndoableAction {

          public:
            ClearAnnotation(
                const media::AVFrameID &frameid,
                AnnotationsCore *plugin,
                utility::Uuid &bm_id,
                const bool bookmark_is_empty)
                : frameid_(frameid),
                  plugin_(plugin),
                  bm_id_(bm_id),
                  bookmark_is_empty_(bookmark_is_empty) {}

            bool redo(Annotation **annotation) override {
                if (!(*annotation))
                    return false;
                canvas_ = (*annotation)->canvas();
                (*annotation)->canvas().clear();
                if (bookmark_is_empty_)
                    plugin_->remove_bookmark(bm_id_);
                return true;
            }

            bool undo(Annotation **annotation) override {
                if (!(*annotation))
                    (*annotation) = new Annotation();
                if (bookmark_is_empty_)
                    plugin_->make_bookmark_for_annotations(frameid_, bm_id_);
                (*annotation)->canvas() = canvas_;
                return true;
            }

            friend class AnnotationsCore;

            canvas::Canvas canvas_;
            const media::AVFrameID frameid_;
            AnnotationsCore *plugin_;
            utility::Uuid bm_id_;
            const bool bookmark_is_empty_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio

void AnnotationsCore::clear_annotation(LiveEditData &user_edit_data) {

    if (user_edit_data->edited_bookmark_id.is_null()) {

        // user has pressed clear but they haven't been annotating on the current
        // frame. The behaviour is to look for any annotation on the 'hero'
        // frame and clear that ...

        const media_reader::ImageBufDisplaySetPtr &onscreen_image_set =
            get_viewport_image_set(user_edit_data->viewport_name);
        if (!onscreen_image_set)
            return;

        for (const auto &bookmark : onscreen_image_set->hero_image().bookmarks()) {
            // does the bookmark already have an annotation on it?
            const Annotation *my_annotation =
                dynamic_cast<const Annotation *>(bookmark->annotation_.get());
            if (my_annotation) {

                // we've found a bookmark with an annotation.
                user_edit_data->edited_bookmark_id = bookmark->detail_.uuid_;
                user_edit_data->annotated_image    = onscreen_image_set->hero_image();
                break;
            }
        }

        if (user_edit_data->edited_bookmark_id.is_null())
            return;
    }

    bookmark::BookmarkDetail detail = get_bookmark_detail(user_edit_data->edited_bookmark_id);
    const bool bookmark_is_empty    = !(detail.note_ && !detail.note_->empty());

    Annotation *mod_annotation = modifiable_annotation(user_edit_data);
    AnnotationBasePtr anno_ptr(mod_annotation);

    undoable_action<ClearAnnotation>(
        user_edit_data,
        mod_annotation,
        user_edit_data->annotated_image.frame_id(),
        this,
        user_edit_data->edited_bookmark_id,
        bookmark_is_empty);

    // for Sync plugin, broadcast new state of annotation after clear
    mail(utility::event_atom_v, annotation_data_atom_v, anno_ptr).send(live_edit_event_group_);

    if (!bookmark_is_empty) {
        update_bookmark_annotation(user_edit_data->edited_bookmark_id, anno_ptr, false);
    }
}

void AnnotationsCore::push_live_edit_to_bookmark(LiveEditData &user_edit_data) {

    // if there is no image (e.g. 'laser' draw mode) then we can't push the
    // stroke onto a bookmark
    if (!user_edit_data->annotated_image ||
        user_edit_data->item_type == Canvas::ItemType::Laser)
        return;

    // skip empty caption
    if (user_edit_data->live_caption && user_edit_data->live_caption->text().empty())
        return;

    bool concat = false;
    // this will be null if we annotate a frame that doesn't already have
    // a suitable bookmark to append our annotations onto
    if (user_edit_data->edited_bookmark_id.is_null()) {

        user_edit_data->edited_bookmark_id = next_bookmark_uuid_;
        next_bookmark_uuid_                = utility::Uuid::generate();
        Annotation dummy;
        undoable_action<CreateBookmark>(
            user_edit_data,
            &dummy,
            user_edit_data->annotated_image.frame_id(),
            this,
            user_edit_data->edited_bookmark_id);
        concat = true;
    }

    Annotation *mod_annotation = modifiable_annotation(user_edit_data);

    if (user_edit_data->live_stroke) {

        if (!mod_annotation)
            mod_annotation = new Annotation();

        undoable_action<AddStroke>(
            concat, user_edit_data, mod_annotation, *(user_edit_data->live_stroke));

        user_edit_data->live_stroke.reset();

    } else if (user_edit_data->live_caption) {

        if (!mod_annotation)
            mod_annotation = new Annotation();

        undoable_action<ModifyOrAddCaption>(
            concat, user_edit_data, mod_annotation, *(user_edit_data->live_caption));

        user_edit_data->skip_render_caption_id = user_edit_data->live_caption->hash();
    }

    update_bookmark_annotation(
        user_edit_data->edited_bookmark_id, AnnotationBasePtr(mod_annotation), false);

    // user_edit_data->live_canvas->full_clear();
}

void AnnotationsCore::start_cursor_blink() {
    if (!cursor_blinking_) {
        cursor_blinking_ = true;
        delayed_anon_send(
            caf::actor_cast<caf::actor>(this),
            std::chrono::milliseconds(300),
            utility::event_atom_v);
    }
}

void AnnotationsCore::fade_all_laser_strokes() {

    int n = 0;
    for (auto &p : live_edit_data_) {
        auto q = p.second->laser_strokes.begin();
        while (q != p.second->laser_strokes.end()) {

            // we only erase old laser strokes if the user isn't holding the poiner
            // down (in Laser mode)
            bool erase = p.second->item_type != Canvas::ItemType::Laser;
            erase &= (*q)->fade(0.01f);
            if (erase) {
                q = p.second->laser_strokes.erase(q);
            } else {
                n++;
                q++;
            }
        }
    }
    // laser strokes have all faded to nothing.
    if (!n)
        laser_stroke_animation_ = false;
}

void AnnotationsCore::annotation_about_to_be_edited(
    const AnnotationBasePtr &anno, const utility::Uuid &anno_uuid) {
    if (anno && anno_uuid != current_edited_annotation_uuid_) {
        current_edited_annotation_uuid_ = anno_uuid;
        mail(utility::event_atom_v, annotation_data_atom_v, anno).send(live_edit_event_group_);
    } else if (anno_uuid != current_edited_annotation_uuid_) {
        current_edited_annotation_uuid_ = anno_uuid;
        mail(utility::event_atom_v, annotation_data_atom_v, AnnotationBasePtr())
            .send(live_edit_event_group_);
    }
}


extern "C" {

plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<AnnotationsCore>>(
                 AnnotationsCore::PLUGIN_UUID,
                 "AnnotationsCore",
                 plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                 true, // this is the 'resident' flag, meaning one instance of the plugin is
                       // created at startup time
                 "Ted Waine",
                 "On Screen Annotations Plugin"),
             std::make_shared<plugin_manager::PluginFactoryTemplate<AnnotationsUI>>(
                 AnnotationsUI::PLUGIN_UUID,
                 "AnnotationsUI",
                 plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                 true, // this is the 'resident' flag, meaning one instance of the plugin is
                       // created at startup time
                 "Ted Waine",
                 "On Screen Annotations Plugin")}));
}
}
