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

    // These attributes are a convenient way to track the corresponding preferences
    // that store the last used note category and colour set by the user so that
    // when we create a new note we can populate the category and colour with the last used
    // values.
    note_category_ = add_string_attribute("note category", "note category", "");
    note_category_->set_preference_path("/core/bookmark/note_category");
    note_colour_ = add_string_attribute("note colour", "note colour", "");
    note_colour_->set_preference_path("/core/bookmark/note_colour");

    listen_to_playhead_events(true);

    // This allows any other component of xSTUDIO to find this plugin instance
    system().registry().put("ANNOTATIONS_CORE_PLUGIN", this);

    live_edit_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(live_edit_event_group_);

}

// AnnotationsCore::~AnnotationsCore() {}

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
            const utility::JsonStore &data) { 
            receive_annotation_data(data); 

            if (draw_events_event_group_) {
                // use anon_mail here, so the event 'source' is draw_events_event_group_ and not
                // the AnnotationCorePlugin instance
                anon_mail(utility::event_atom_v, ui::viewport::annotation_atom_v, data).send(draw_events_event_group_);
            }
        },
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
                // The stream viewport may already have rendered its (single,
                // while paused) frame before this message arrived - seed the
                // frame-change anchor now so clients get the current frame's
                // snapshot instead of waiting for the next frame change.
                maybe_broadcast_streamed_frame(viewport_name, false);
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
            } else if (
                action == "FORCE_SHOW_ANNOTATIONS" || action == "FORCE_HIDE_ANNOTATIONS" ||
                action == "CLEAR_VISIBILITY_OVERRIDE") {
                // per-viewport override of the global annotations Visibility
                // toggle - lets a consumer that owns a viewport (e.g. an
                // offscreen render/export viewport) pin annotation visibility
                // regardless of the (session-shared) Visibility attribute.
                if (visibility_override_per_viewport_.find(viewport_name) ==
                    visibility_override_per_viewport_.end()) {
                    visibility_override_per_viewport_[viewport_name] =
                        new std::atomic_int(VO_DEFAULT);
                }
                *(visibility_override_per_viewport_[viewport_name]) =
                    action == "FORCE_SHOW_ANNOTATIONS"   ? VO_FORCE_SHOW
                    : action == "FORCE_HIDE_ANNOTATIONS" ? VO_FORCE_HIDE
                                                         : VO_DEFAULT;
            } else if (action == "BROADCAST_ANNOTATIONS") {
                // A sync client joined or reconnected: broadcast a fresh
                // authoritative frame-scope snapshot (with the layout table)
                // so it doesn't depend on the sync plugin's cached copy.
                broadcast_committed_annotation(
                    AnnotationBasePtr(), utility::Uuid(), media_reader::ImageBufPtr());
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
        },
        [=](utility::get_event_group_atom,
            ui::viewport::annotation_atom) -> caf::actor {
            // This message allows other components (notably python plugins) to join a special
            // event group that forwards all draw interaction events incoming to the plugin.
            // Thus the 3rd party component can get all updates on annotation strokes.
            if (!draw_events_event_group_) {
                draw_events_event_group_ = spawn<broadcast::BroadcastActor>(this);
                link_to(draw_events_event_group_);
            }
            return draw_events_event_group_;

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
        modify_stroke_or_shape(payload, user_edit_data);
        if (user_edit_data->item_type == Canvas::ItemType::Laser)
            broadcast_live_laser_stroke(user_id);
        else
            broadcast_live_stroke(user_edit_data, user_id);
    } else if (event == "PaintPoint") {
        modify_stroke_or_shape(payload, user_edit_data);
        if (user_edit_data->item_type == Canvas::ItemType::Laser)
            broadcast_live_laser_stroke(user_id);
        else
            broadcast_live_stroke(user_edit_data, user_id);
    } else if (event == "PaintEnd") {
        if (user_edit_data->item_type == Canvas::ItemType::Laser) {
            broadcast_live_laser_stroke(user_id, true);
            // the stroke is now complete (mouse released): move it into the
            // fading set, which is what starts the fade.
            commit_laser_stroke(user_edit_data);
        } else {
            broadcast_live_stroke(user_edit_data, user_id, true);
            push_live_edit_to_bookmark(user_edit_data);
        }
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
        if (user_edit_data->item_type == Canvas::ItemType::Laser &&
            user_edit_data->live_laser_stroke) {
            broadcast_live_laser_stroke(user_id, true);
            commit_laser_stroke(user_edit_data);
            user_edit_data->item_type = Canvas::ItemType::None;
        }
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
    // float size_pressure;
    // float opacity_pressure;
    if (payload.contains("points")) {
        pos = Imath::V2f(
            payload.at("points").at(0).at("x").get<float>(),
            payload.at("points").at(0).at("y").get<float>());
        // size_pressure    = 1.0f;
        // opacity_pressure = 1.0f;
    } else {
        pos =
            Imath::V2f(payload["point"]["x"].get<float>(), payload["point"]["y"].get<float>());
        // size_pressure    = payload["point"]["size_pressure"].get<float>();
        // opacity_pressure = payload["point"]["opacity_pressure"].get<float>();
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

    } else if (item_type == "BurnAdd") {

        auto intensity = payload["paint"]["intensity"].get<float>();
        auto softness  = payload["paint"]["softness"].get<float>();
        user_edit_data->live_stroke.reset(Stroke::BurnAdd(intensity, size, softness));
        user_edit_data->item_type = Canvas::ItemType::Burn;

    } else if (item_type == "BurnMult") {

        auto intensity = payload["paint"]["intensity"].get<float>();
        auto softness  = payload["paint"]["softness"].get<float>();
        user_edit_data->live_stroke.reset(Stroke::BurnMult(intensity, size, softness));
        user_edit_data->item_type = Canvas::ItemType::Burn;

    } else if (item_type == "DodgeAdd") {

        auto intensity = payload["paint"]["intensity"].get<float>();
        auto softness  = payload["paint"]["softness"].get<float>();
        user_edit_data->live_stroke.reset(Stroke::DodgeAdd(intensity, size, softness));
        user_edit_data->item_type = Canvas::ItemType::Dodge;

    } else if (item_type == "DodgeMult") {

        auto intensity = payload["paint"]["intensity"].get<float>();
        auto softness  = payload["paint"]["softness"].get<float>();
        user_edit_data->live_stroke.reset(Stroke::DodgeMult(intensity, size, softness));
        user_edit_data->item_type = Canvas::ItemType::Dodge;

    } else if (item_type == "Draw") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, c[3]));
        user_edit_data->item_type = Canvas::ItemType::Draw;

    } else if (item_type == "Brush") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        auto softness            = payload["paint"]["softness"].get<float>();
        auto size_sensitivity    = payload["paint"]["size_sensitivity"].get<float>();
        auto opacity_sensitivity = payload["paint"]["opacity_sensitivity"].get<float>();
        user_edit_data->live_stroke.reset(
            Stroke::Brush(colour, size, softness, c[3], size_sensitivity, opacity_sensitivity));
        user_edit_data->item_type = Canvas::ItemType::Brush;

    } else if (item_type == "Square") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, c[3]));
        user_edit_data->item_type = Canvas::ItemType::Square;

    } else if (item_type == "Circle") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, c[3]));
        user_edit_data->item_type = Canvas::ItemType::Circle;

    } else if (item_type == "Arrow") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, c[3]));
        user_edit_data->item_type = Canvas::ItemType::Arrow;

    } else if (item_type == "Line") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_stroke.reset(Stroke::Pen(colour, size, 0.0f, c[3]));
        user_edit_data->item_type = Canvas::ItemType::Line;

    } else if (item_type == "Laser") {

        auto c = payload["paint"]["rgba"].get<std::vector<float>>();
        utility::ColourTriplet colour(c[0], c[1], c[2]);
        user_edit_data->live_laser_stroke.reset(
            Stroke::Brush(colour, size, 0.0f, c[3], 0.0f, 1.0f));
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

    if (user_edit_data->live_stroke && payload.contains("id")) {
        user_edit_data->live_stroke->set_id(payload["id"].get<std::string>());
    } else if (user_edit_data->live_laser_stroke) {
        const std::string id = payload.contains("id") ? payload["id"].get<std::string>()
                                                      : to_string(utility::Uuid::generate());
        user_edit_data->live_laser_stroke->set_id(id);
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

            points.emplace_back(
                p, i.value("size_pressure", 1.0f), i.value("opacity_pressure", 1.0f));
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

        points.emplace_back(
            p,
            payload.at("point").value("size_pressure", 1.0f),
            payload.at("point").value("opacity_pressure", 1.0f));
    }

    Imath::V2f shape_anchor = user_edit_data->start_point;

    if (user_edit_data->item_type == Canvas::ItemType::Brush ||
        user_edit_data->item_type == Canvas::ItemType::Draw ||
        user_edit_data->item_type == Canvas::ItemType::Burn ||
        user_edit_data->item_type == Canvas::ItemType::Dodge) {

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

        if (user_edit_data->live_laser_stroke) {
            user_edit_data->live_laser_stroke->add_points(points);
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
        auto my_annotation = dynamic_cast<const Annotation *>(bookmark->annotation_.get());
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

    auto pos = payload["pointer_position"].get<Imath::V2f>();
    // auto vp_pix_scale = payload["viewport_pix_scale"].get<float>();

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

    auto my_annotation         = dynamic_cast<const Annotation *>(existing_annotation.get());
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

    // see long note in push_live_edit_to_bookmark
    if (img.frame_id() == user_edit_data->fresh_bookmark_frame_id) {
        user_edit_data->edited_bookmark_id = user_edit_data->fresh_bookmark_id;
    } else {
        user_edit_data->edited_bookmark_id = utility::Uuid();
    }

    AnnotationBasePtr annotation_to_add_to;

    // loop over bookmarks already on the image that the given user is annotating
    for (const auto &anno : user_edit_data->annotated_image.bookmarks()) {

        // does the bookmark already have an annotation on it?
        auto my_annotation = dynamic_cast<const Annotation *>(anno->annotation_.get());
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

    if (user_edit_data->edited_bookmark_id != user_edit_data->fresh_bookmark_id) {
        // we haven't needed to re-use 'fresh_bookmark_id' (we only need this
        // for rare occasion new bookmark hasn't been synced back to us via
        // 'images_going_on_screen') so re-set it.
        user_edit_data->fresh_bookmark_id = utility::Uuid();
    }

    // for xSTUDIO Sync plugin, we need to send it the whole of the annotation
    // that we're about to start adding a stroke to so it can send annotations
    // data to web clients (so that they can render the annotation locally)
    if (user_edit_data->edited_bookmark_id.is_null()) {
        annotation_about_to_be_edited(
            annotation_to_add_to, next_bookmark_uuid_, user_edit_data->annotated_image);
    } else {
        annotation_about_to_be_edited(
            annotation_to_add_to,
            user_edit_data->edited_bookmark_id,
            user_edit_data->annotated_image);
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
        // the in-progress stroke (full opacity) ...
        if (user_edit_data->live_laser_stroke) {
            if (!data) {
                data = new LaserStrokesRenderDataSet();
            }
            data->add_laser_stroke(user_edit_data->live_laser_stroke);
        }
        // ... and the committed strokes that are fading out.
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

    const int vis_override = visibility_override(viewport_name);
    if (vis_override == VO_FORCE_HIDE ||
        (hide_all_drawings_ && vis_override != VO_FORCE_SHOW))
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

    maybe_broadcast_streamed_frame(viewport_name, playhead_playing);

    if (hide_all_per_viewport_.find(viewport_name) == hide_all_per_viewport_.end()) {
        hide_all_per_viewport_[viewport_name] = new std::atomic_bool(false);
    }
    *(hide_all_per_viewport_[viewport_name]) =
        (show_annotations_during_playback_ ? false : playhead_playing);

    // what if a new image is going on screen, and we have an active edit going
    // on with the given viewport? We need to wipe the active edit so that we
    // don't see the caption overlays
    bool images_went_off_the_screen = false;
    auto p                          = live_edit_data_.begin();
    while (p != live_edit_data_.end()) {
        // Keep entries that still own an in-progress or fading laser stroke —
        // they are viewport overlays unrelated to the annotated image, and
        // must outlive frame changes until fully faded.
        if (p->second->viewport_name == viewport_name &&
            !p->second->live_laser_stroke &&
            p->second->laser_strokes.empty()) {
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

    if (visibility_override_per_viewport_.find(viewport_name) ==
        visibility_override_per_viewport_.end()) {
        visibility_override_per_viewport_[viewport_name] = new std::atomic_int(VO_DEFAULT);
    }

    return plugin::ViewportOverlayRendererPtr(new AnnotationsRenderer(
        viewport_name,
        cursor_blink_,
        hide_all_drawings_,
        hide_strokes_per_viewport_[viewport_name],
        hide_all_per_viewport_[viewport_name],
        visibility_override_per_viewport_[viewport_name]));
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

        current_edited_annotation_uuid_ = bookmark_for_undo_id;
        broadcast_committed_annotation(
            modified_annotation, bookmark_for_undo_id, user_edit_data->annotated_image);

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

        current_edited_annotation_uuid_ = bookmark_for_undo_id;
        broadcast_committed_annotation(
            modified_annotation, bookmark_for_undo_id, user_edit_data->annotated_image);

    } else {

        delete mod_annotation;
    }
}

void AnnotationsCore::broadcast_live_stroke(
    const LiveEditData &user_edit_data,
    const utility::Uuid &user_id,
    const bool stroke_completed) {

    auto anno = new Annotation();
    if (user_edit_data->live_stroke)
        anno->canvas().append_item(*(user_edit_data->live_stroke));

    AnnotationBasePtr anno_ptr(anno);

    int image_index = 0;
    std::vector<Imath::M44f> layout_table;
    streaming_grid_layout(user_edit_data->annotated_image, image_index, layout_table);

    mail(
        utility::event_atom_v,
        annotation_data_atom_v,
        anno_ptr,
        user_id,
        stroke_completed,
        image_index,
        layout_table)
        .send(live_edit_event_group_);

    if (draw_events_event_group_) {
        // Here we send the whole, serialised annotation data. This can be consumed
        // by a Python plugin implementing annotation event syncing, for example.
        // TODO: full python bindings for AnnotationBasePtr to avoid expensive 
        // serialisation
        const bool is_shape = user_edit_data->item_type == Canvas::ItemType::Square ||
                            user_edit_data->item_type == Canvas::ItemType::Circle ||
                            user_edit_data->item_type == Canvas::ItemType::Arrow ||
                            user_edit_data->item_type == Canvas::ItemType::Line ||
                            user_edit_data->item_type == Canvas::ItemType::Ellipse;

        if (!is_shape || stroke_completed) {
            
            utility::Uuid plugin_uuid;
            utility::JsonStore anno_json = anno_ptr->serialise(plugin_uuid);
            anno_json["user_id"] = user_id;
            anno_json["stroke_completed"] = stroke_completed;
            // anon mail
            anon_mail(
                utility::event_atom_v,
                annotation_data_atom_v,
                anno_json,
                user_id,
                stroke_completed)
                .send(draw_events_event_group_);
        }        
    }
}

void AnnotationsCore::broadcast_live_laser_stroke(
    const utility::Uuid &user_id, const bool stroke_completed) {

    auto p = live_edit_data_.find(user_id);
    if (p == live_edit_data_.end())
        return;
    const auto &user_edit_data = p->second;
    if (!user_edit_data->live_laser_stroke)
        return;

    Annotation *anno = new Annotation();
    anno->canvas().append_item(*(user_edit_data->live_laser_stroke));

    mail(
        utility::event_atom_v,
        laser_stroke_atom_v,
        AnnotationBasePtr(anno),
        user_id,
        stroke_completed)
        .send(live_edit_event_group_);
}

const media_reader::ImageBufDisplaySetPtr *AnnotationsCore::streaming_images() const {

    // Prefer the streaming (offscreen) viewport - the surface the web client
    // overlays - identified by the stroke-suppression flag the encoder set on
    // it. Its per-cell layout matches any viewport in the same compare mode.
    for (const auto &hs : hide_strokes_per_viewport_) {
        if (hs.second && hs.second->load() != 0) {
            auto it = viewport_current_images_.find(hs.first);
            if (it != viewport_current_images_.end() && it->second &&
                it->second->num_onscreen_images() > 0) {
                return &it->second;
            }
        }
    }
    // No fallback to other viewports: their grid layout transforms depend on
    // THEIR aspect, so a snapshot gathered from the main viewport carries a
    // slightly different table than one from the stream viewport - clients
    // re-scale every stroke when the two alternate. Better no table (callers
    // reuse the last one sent) than a wrong one.
    return nullptr;
}

void AnnotationsCore::streaming_grid_layout(
    const media_reader::ImageBufPtr &annotated_image,
    int &image_index,
    std::vector<Imath::M44f> &layout_table) const {

    image_index = 0;
    layout_table.clear();

    const media_reader::ImageBufDisplaySetPtr *images = streaming_images();
    if (!images) {
        // Stream viewport unknown right now: reuse the last table broadcast
        // for it rather than sending none - an empty table makes clients fall
        // back to identity and momentarily re-scale every stroke.
        layout_table = last_streamed_layout_table_;
        return;
    }

    const int n = (*images)->num_onscreen_images();
    for (int i = 0; i < n; ++i) {
        const auto &im = (*images)->onscreen_image(i);
        layout_table.push_back(im.layout_transform());
        if (annotated_image &&
            im.frame_id().key() == annotated_image.frame_id().key()) {
            image_index = i;
        }
    }
}

void AnnotationsCore::maybe_broadcast_streamed_frame(
    const std::string &viewport_name, const bool playhead_playing) {

    // Streaming viewport (the one the encoder flagged for stroke suppression):
    // the stream carries no strokes, so the web clients depend entirely on our
    // broadcasts. Called from images_going_on_screen AND from the
    // DONT_RENDER_STROKES handler: while paused the stream viewport renders
    // its first frame exactly once, and that render can beat the (async)
    // DONT_RENDER_STROKES message - without the second call site the anchor
    // would stay silent until the first frame change, leaving joining clients
    // with no strokes.
    // Best-effort by design: this runs inside the playhead's show handler, so
    // an escaped exception would kill the plugin actor and abort xSTUDIO. A
    // missed broadcast only delays the web update until the next anchor.
    try {
        auto hs = hide_strokes_per_viewport_.find(viewport_name);
        if (hs == hide_strokes_per_viewport_.end() || !hs->second ||
            hs->second->load() == 0)
            return;
        auto imit = viewport_current_images_.find(viewport_name);
        if (imit == viewport_current_images_.end() || !imit->second)
            return;
        const auto &images = imit->second;

        std::vector<Imath::M44f> layout_table;
        std::vector<std::string> frame_keys;
        layout_table.reserve(images->num_onscreen_images());
        frame_keys.reserve(images->num_onscreen_images());
        for (int i = 0; i < images->num_onscreen_images(); ++i) {
            layout_table.push_back(images->onscreen_image(i).layout_transform());
            frame_keys.push_back(to_string(images->onscreen_image(i).frame_id().key()));
        }

        if (playhead_playing) {
            // No frame anchors during playback (clients hide strokes while
            // playing); clearing the keys makes the first paused frame
            // broadcast its snapshot.
            last_streamed_frame_keys_.clear();
        } else if (!frame_keys.empty() && frame_keys != last_streamed_frame_keys_) {
            // FRAME-CHANGE ANCHOR: the streamed frame changed - broadcast
            // the new frame's full snapshot (which carries the layout
            // table), flagged so clients apply it in sync with the
            // playout-delayed video instead of immediately.
            last_streamed_frame_keys_ = frame_keys;
            if (!layout_table.empty())
                last_streamed_layout_table_ = layout_table;
            broadcast_committed_annotation(
                AnnotationBasePtr(),
                utility::Uuid(),
                media_reader::ImageBufPtr(),
                /*frame_changed=*/true);
        } else if (!layout_table.empty() && layout_table != last_streamed_layout_table_) {
            // Same frame, new layout (compare-mode/grid switch): a
            // layout-only update re-maps the clients' stored strokes
            // without touching the stroke sets.
            last_streamed_layout_table_ = layout_table;
            mail(utility::event_atom_v, annotation_data_atom_v, layout_table)
                .send(live_edit_event_group_);
        }
    } catch (const std::exception &e) {
        spdlog::warn(
            "{} streamed-frame broadcast failed: {}", __PRETTY_FUNCTION__, e.what());
    }
}

void AnnotationsCore::broadcast_committed_annotation(
    const bookmark::AnnotationBasePtr &anno,
    const utility::Uuid &bookmark_uuid,
    const media_reader::ImageBufPtr &annotated_image,
    const bool frame_changed) {

    // Best-effort: called from paint/undo/show handlers - an escaped exception
    // here would kill the plugin actor and abort xSTUDIO. A missed snapshot is
    // recovered by the next anchor.
    try {

    int edited_image_index = 0;
    std::vector<Imath::M44f> layout_table;
    streaming_grid_layout(annotated_image, edited_image_index, layout_table);
    // Keep the last-broadcast cache fresh so snapshots gathered while the
    // stream viewport is momentarily unknown reuse this table (see
    // streaming_grid_layout) instead of dropping to empty/identity.
    if (!layout_table.empty())
        last_streamed_layout_table_ = layout_table;

    // Authoritative frame-scope snapshot: every annotation on the streaming
    // viewport's on-screen images. For the annotation just modified we use the
    // caller's fresh pointer, not the (possibly stale, queued-write) one held
    // by the image's bookmark list.
    std::vector<bookmark::AnnotationBasePtr> annotations;
    std::vector<int> image_indices;
    bool edited_found = false;

    const media_reader::ImageBufDisplaySetPtr *images = streaming_images();
    if (!images && !anno) {
        // Blind gather: we cannot see the streamed frame (e.g. a client joined
        // before the stream viewport rendered its first frame - the stream
        // starts ON client connect) and there is no fresh annotation to relay.
        // Broadcasting an empty snapshot here would wipe the strokes clients
        // just received from the sync plugin's cache replay. Stay silent: the
        // frame-change anchor fires as soon as the stream viewport renders its
        // first paused frame and delivers the real snapshot.
        return;
    }
    if (images) {
        const int n = (*images)->num_onscreen_images();
        for (int i = 0; i < n; ++i) {
            const auto &im = (*images)->onscreen_image(i);
            for (const auto &bookmark : im.bookmarks()) {
                if (bookmark->detail_.uuid_ == bookmark_uuid) {
                    edited_found = true;
                    if (anno) {
                        annotations.push_back(anno);
                        image_indices.push_back(i);
                    }
                } else if (
                    dynamic_cast<const Annotation *>(bookmark->annotation_.get())) {
                    // Only OUR annotation type: other plugins' annotations
                    // (e.g. Grading bookmarks) have no strokes to stream and
                    // an unspecified user_data() contract on the sync side.
                    annotations.push_back(bookmark->annotation_);
                    image_indices.push_back(i);
                }
            }
        }
    }
    // A brand-new bookmark is not attached to the images yet (queued write) -
    // its annotation must still be part of the snapshot.
    if (!edited_found && anno) {
        annotations.push_back(anno);
        image_indices.push_back(edited_image_index);
    }

    mail(
        utility::event_atom_v,
        annotation_data_atom_v,
        annotations,
        image_indices,
        layout_table,
        frame_changed)
        .send(live_edit_event_group_);

    } catch (const std::exception &e) {
        spdlog::warn("{} snapshot broadcast failed: {}", __PRETTY_FUNCTION__, e.what());
    }
}

void AnnotationsCore::make_bookmark_for_annotations(
    const media::AVFrameID &frame_id, const utility::Uuid &bm_id) {

    bookmark::BookmarkDetail detail;
    detail.uuid_ = bm_id;

    caf::scoped_actor sys{system()};
    std::string media_name;
    std::string media_path;

    try {
        media_name = request_receive<std::string>(
            *sys, frame_id.media_actor(), utility::name_atom_v);
        media_path = utility::uri_to_posix_path(frame_id.uri());
    } catch (...) {
        // pass
    }

    // If the media actor's name is not empty and is different from the media's
    // path then assume it should be used for the name of the annotation's note.
    // Otherwise extract the first portion of the path's filename.
    std::string note_name;
    if (! media_name.empty() && media_name != media_path) {
        note_name = media_name;
    } else {
        note_name = fs::path(media_path).stem().string();
        if (note_name.find(".") != std::string::npos) {
            note_name = std::string(note_name, 0, note_name.find("."));
        }
    }

    detail.category_ = note_category_->value();
    detail.colour_   = note_colour_->value();

    create_bookmark_on_frame(frame_id, note_name, detail, false);
}

namespace xstudio::ui::viewport {

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

} // namespace xstudio::ui::viewport

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
            auto my_annotation = dynamic_cast<const Annotation *>(bookmark->annotation_.get());
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

    // Make sure we don't try to re-use this deleted annotation/bookmark
    user_edit_data->fresh_bookmark_id = utility::Uuid();

    undoable_action<ClearAnnotation>(
        user_edit_data,
        mod_annotation,
        user_edit_data->annotated_image.frame_id(),
        this,
        user_edit_data->edited_bookmark_id,
        bookmark_is_empty);

    // for Sync plugin, broadcast new state of annotation after clear
    broadcast_committed_annotation(
        anno_ptr, user_edit_data->edited_bookmark_id, user_edit_data->annotated_image);

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

    AnnotationBasePtr anno_ptr(mod_annotation);
    update_bookmark_annotation(user_edit_data->edited_bookmark_id, anno_ptr, false);

    // Post-mutation snapshot for the sync/web clients: anno_ptr holds the
    // annotation WITH the just-committed stroke/caption, regardless of the
    // queued bookmark write above. This is the message that lets a web client
    // retire its local prediction copy of the stroke (read-your-writes).
    broadcast_committed_annotation(
        anno_ptr, user_edit_data->edited_bookmark_id, user_edit_data->annotated_image);

    // user_edit_data->live_canvas->full_clear();
    if (concat) {
        // really awkward! We've just made a new bookmark. This will be 'broadcast'
        // by the bookmark manager, the change is picked up by the playhead(s) and
        // they in turn broadcast new images with the new bookmark attached. That
        // image with the new bookmark will make it to this class pretty quickly
        // (see 'images_going_on_screen') so that next time we enter
        // 'pick_image_to_annotate' the NEXT stroke will be added to the correct
        // bookmark & annotation (the one we just made here). However ... due
        // to the async nature of this whole set-up, sometimes the bookmark
        // hasn't made it back to us. So to get around that we make a record of
        // the fresh bookmark and the image that it is attached to so we can check
        // it in 'pick_image_to_annotate' on the next stroke.
        user_edit_data->fresh_bookmark_id       = user_edit_data->edited_bookmark_id;
        user_edit_data->fresh_bookmark_frame_id = user_edit_data->annotated_image.frame_id();
    }
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

void AnnotationsCore::commit_laser_stroke(LiveEditData &user_edit_data) {
    // move the in-progress stroke into the fading set. Until now it was held
    // in live_laser_stroke at full opacity; once in laser_strokes it starts
    // fading on the next animation tick.
    if (user_edit_data->live_laser_stroke) {
        user_edit_data->laser_strokes.push_back(
            std::move(user_edit_data->live_laser_stroke));
    }
}

void AnnotationsCore::fade_all_laser_strokes() {

    int n = 0;
    for (auto &p : live_edit_data_) {
        // An in-progress stroke (mouse still held) is kept in live_laser_stroke
        // and must not fade. We still count it so the animation loop (and its
        // per-tick redraw) stays alive while the user is drawing.
        if (p.second->live_laser_stroke)
            n++;
        auto &strokes = p.second->laser_strokes;
        auto q        = strokes.begin();
        while (q != strokes.end()) {
            if ((*q)->fade(0.01f)) {
                q = strokes.erase(q);
            } else {
                n++;
                q++;
            }
        }
    }
    // no in-progress or fading laser strokes remain.
    if (!n)
        laser_stroke_animation_ = false;
}

void AnnotationsCore::annotation_about_to_be_edited(
    const AnnotationBasePtr &anno,
    const utility::Uuid &anno_uuid,
    const media_reader::ImageBufPtr &annotated_image) {

    // Tracking only. Snapshots for the sync/web clients are broadcast by the
    // authoritative anchors instead: frame change (images_going_on_screen),
    // stroke commit (push_live_edit_to_bookmark), undo/redo/clear and client
    // join - an edit-start broadcast is redundant with those and just adds
    // message churn while drawing.
    current_edited_annotation_uuid_ = anno_uuid;
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
