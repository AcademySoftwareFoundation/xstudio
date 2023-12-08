// SPDX-License-Identifier: Apache-2.0
#include "annotation.hpp"

#include <utility>
#include "annotation_serialiser.hpp"
#include "annotations_tool.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

Annotation::Annotation(
    std::map<std::string, std::shared_ptr<SDFBitmapFont>> &fonts, bool is_laser_annotatio)
    : bookmark::AnnotationBase(), fonts_(fonts), is_laser_annotation_(is_laser_annotatio) {}

Annotation::Annotation(
    const utility::JsonStore &s, std::map<std::string, std::shared_ptr<SDFBitmapFont>> &fonts)
    : bookmark::AnnotationBase(utility::JsonStore(), utility::Uuid()), fonts_(fonts) {
    AnnotationSerialiser::deserialise(this, s);
    update_render_data();
}

Annotation::Annotation(const Annotation &o)
    : bookmark::AnnotationBase(utility::JsonStore(), o.bookmark_uuid_) {
    strokes_ = o.strokes_;
    for (const auto &capt : o.captions_) {
        captions_.emplace_back(new Caption(*capt));
    }
    fonts_ = o.fonts_;
    update_render_data();
}

utility::JsonStore Annotation::serialise(utility::Uuid &plugin_uuid) const {

    plugin_uuid = AnnotationsTool::PLUGIN_UUID;
    return AnnotationSerialiser::serialise((const Annotation *)this);
}

bool Annotation::test_click_in_caption(const Imath::V2f pointer_position) {

    if (no_fonts())
        return false;
    finished_current_stroke();
    std::string::const_iterator cursor;
    float r = 0.1f;

    for (auto &caption : captions_) {

        if (!caption->bounding_box_.intersects(pointer_position))
            continue;

        const std::string::const_iterator cursor_pos =
            font(caption)->viewport_position_to_cursor(
                pointer_position,
                caption->text_,
                caption->position_,
                caption->wrap_width_,
                caption->font_size_,
                caption->justification_,
                1.0f);

        copy_of_edited_caption_.reset(new Caption(*caption.get()));
        current_caption_ = caption;
        cursor_position_ = cursor_pos;
        break;
    }

    return bool(current_caption_);
}

void Annotation::start_new_caption(
    const Imath::V2f position,
    const float wrap_width,
    const float font_size,
    const utility::ColourTriplet colour,
    const float opacity,
    const Justification justification,
    const std::string &font_name) {
    finished_current_stroke();
    copy_of_edited_caption_.reset();
    current_caption_.reset(new Caption(
        position, wrap_width, font_size, colour, opacity, justification, font_name));
    captions_.push_back(current_caption_);
    update_render_data();
}

void Annotation::modify_caption_text(const std::string &t) {
    if (current_caption_)
        current_caption_->modify_text(t, cursor_position_);
    update_render_data();
}

void Annotation::key_down(const int key) {
    if (no_fonts())
        return;

    if (current_caption_) {

        if (key == 16777235) {
            // up arrow
            cursor_position_ = font(current_caption_)
                                   ->cursor_up_or_down(
                                       cursor_position_,
                                       true,
                                       current_caption_->text_,
                                       current_caption_->wrap_width_,
                                       current_caption_->font_size_,
                                       current_caption_->justification_,
                                       1.0f);

        } else if (key == 16777237) {
            // down arrow
            cursor_position_ = font(current_caption_)
                                   ->cursor_up_or_down(
                                       cursor_position_,
                                       false,
                                       current_caption_->text_,
                                       current_caption_->wrap_width_,
                                       current_caption_->font_size_,
                                       current_caption_->justification_,
                                       1.0f);

        } else if (key == 16777236) {
            // right arrow
            if (cursor_position_ != current_caption_->text_.cend())
                cursor_position_++;

        } else if (key == 16777234) {
            // left arrow
            if (cursor_position_ != current_caption_->text_.cbegin())
                cursor_position_--;

        } else if (key == 16777232) {
            // home
            cursor_position_ = current_caption_->text_.cbegin();

        } else if (key == 16777233) {
            // end
            cursor_position_ = current_caption_->text_.cend();
        }
        update_render_data();
    }
}

Imath::Box2f Annotation::mouse_hover_on_captions(
    const Imath::V2f cursor_position, const float viewport_pixel_scale) const {

    Imath::Box2f result;
    for (const auto &caption : captions_) {
        if (caption->bounding_box_.min.x < cursor_position.x &&
            caption->bounding_box_.min.y < cursor_position.y &&
            caption->bounding_box_.max.x > cursor_position.x &&
            caption->bounding_box_.max.y > cursor_position.y) {

            result = caption->bounding_box_;
            break;
        }
    }
    return result;
}

Caption::HoverState Annotation::mouse_hover_on_selected_caption(
    const Imath::V2f cursor_position, const float viewport_pixel_scale) const {

    Caption::HoverState result = Caption::NotHovered;

    if (current_caption_) {
        // is the mouse hovering over the caption
        // move / resize handles?
        Imath::V2f cp = current_caption_->bounding_box_.min - cursor_position;
        const auto handle_extent =
            Imath::Box2f(Imath::V2f(0.0f, 0.0f), captionHandleSize * viewport_pixel_scale);

        if (handle_extent.intersects(
                cp)) { //}.x > 0.0f && cp.x < captionHandleSize.x*viewport_pixel_scale && cp.y >
                       // 0.0f && cp.y < captionHandleSize.y/viewport_pixel_scale) {
            result = Caption::HoveredOnMoveHandle;
        } else {
            cp = cursor_position - current_caption_->bounding_box_.max;
            if (handle_extent.intersects(cp)) {
                result = Caption::HoveredOnResizeHandle;
            } else {
                // delete handle is top left of the box
                cp = cursor_position - Imath::V2f(
                                           current_caption_->bounding_box_.max.x,
                                           current_caption_->bounding_box_.min.y -
                                               captionHandleSize.y * viewport_pixel_scale);
                if (handle_extent.intersects(cp)) {
                    result = Caption::HoveredOnDeleteHandle;
                }
            }
        }
    }

    return result;
}

void Annotation::start_pen_stroke(
    const utility::ColourTriplet &c, const float thickness, const float opacity) {
    finished_current_stroke();
    current_stroke_.reset(new PenStroke(c, thickness, opacity));
}

void Annotation::start_erase_stroke(const float thickness) {

    finished_current_stroke();
    current_stroke_.reset(new PenStroke(thickness));
}

void Annotation::add_point_to_current_stroke(const Imath::V2f pt) {
    if (current_stroke_) {
        current_stroke_->add_point(pt);
        update_render_data();
    }
}

AnnotationRenderDataPtr Annotation::render_data(const bool is_edited_annotation) const {
    return cached_render_data_;
}

void Annotation::update_render_data() {

    auto t0 = utility::clock::now();
    cached_render_data_.reset();

    // each stroke is drawn at a slightly increasing depth so that
    // strokes layer ontop of each other
    render_data_.clear();
    float depth = 0.0f;
    for (auto &stroke : strokes_) {

        depth += 0.001;
        // i < int(strokes_.size()) ? strokes_[i] : *current_stroke_.get();

        AnnotationRenderData::StrokeInfo info;

        info.is_erase_stroke_ = stroke.is_erase_stroke_;

        // this call converts the 'path' (mouse scribble path) into
        // solid gl elements (triangles and quads) for drawing to screen
        info.stroke_point_count_ = stroke.fetch_render_data(render_data_.pen_stroke_vertices_);

        info.brush_colour_    = stroke.colour_;
        info.brush_opacity_   = stroke.opacity_;
        info.brush_thickness_ = stroke.thickness_;
        info.stroke_depth_    = depth;

        render_data_.stroke_info_.push_back(info);
    }

    if (!no_fonts()) {
        for (const auto &caption : captions_) {

            render_data_.caption_info_.emplace_back(AnnotationRenderData::CaptionInfo());

            render_data_.caption_info_.back().bounding_box =
                font(caption)->precompute_text_rendering_vertex_layout(
                    render_data_.caption_info_.back().precomputed_vertex_buffer,
                    caption->text_,
                    caption->position_,
                    caption->wrap_width_,
                    caption->font_size_,
                    caption->justification_,
                    1.0f);
            caption->bounding_box_ = render_data_.caption_info_.back().bounding_box;
            render_data_.caption_info_.back().colour    = caption->colour_;
            render_data_.caption_info_.back().opacity   = caption->opacity_;
            render_data_.caption_info_.back().text_size = caption->font_size_;
            render_data_.caption_info_.back().font_name = caption->font_name_;
        }
    }

    if (current_stroke_) {

        depth += 0.001;
        auto &stroke = *current_stroke_.get();

        AnnotationRenderData::StrokeInfo info;

        info.is_erase_stroke_ = stroke.is_erase_stroke_;

        // this call converts the 'path' (mouse scribble path) into
        // solid gl elements (triangles and quads) for drawing to screen
        info.stroke_point_count_ = stroke.fetch_render_data(render_data_.pen_stroke_vertices_);

        info.brush_colour_    = stroke.colour_;
        info.brush_opacity_   = stroke.opacity_;
        info.brush_thickness_ = stroke.thickness_;
        info.stroke_depth_    = depth;

        render_data_.stroke_info_.push_back(info);
    }

    cached_render_data_.reset(new AnnotationRenderData(render_data_));
}

void Annotation::clear() {
    undo_stack_.emplace_back(static_cast<UndoRedo *>(new UndoRedoClear(strokes_)));
    strokes_.clear();
    captions_.clear();
    update_render_data();
}

void Annotation::undo() {
    if (undo_stack_.size()) {
        undo_stack_.back()->undo(this);
        redo_stack_.push_back(undo_stack_.back());
        undo_stack_.pop_back();
        update_render_data();
    }
}

void Annotation::redo() {
    if (redo_stack_.size()) {
        redo_stack_.back()->redo(this);
        undo_stack_.push_back(redo_stack_.back());
        redo_stack_.pop_back();
        update_render_data();
    }
}

std::shared_ptr<ui::SDFBitmapFont>
Annotation::font(const std::shared_ptr<Caption> &caption) const {
    auto p = fonts_.find(caption->font_name_);
    if (p != fonts_.end())
        return p->second;
    return fonts_.begin()->second;
}

void Annotation::finished_current_stroke() {
    if (current_stroke_) {
        undo_stack_.emplace_back(
            static_cast<UndoRedo *>(new UndoRedoStroke(*current_stroke_.get())));
        redo_stack_.clear();
        strokes_.emplace_back(*current_stroke_.get());
        current_stroke_.reset();
        update_render_data();
    }
    if (current_caption_) {

        if (current_caption_->text_.empty() && !copy_of_edited_caption_) {
            auto p = std::find(captions_.begin(), captions_.end(), current_caption_);
            if (p != captions_.end()) {
                captions_.erase(p);
            }
        } else {
            undo_stack_.emplace_back(static_cast<UndoRedo *>(
                new UndoRedoAddCaption(current_caption_, copy_of_edited_caption_)));
            redo_stack_.clear();
        }
        current_caption_.reset();
        copy_of_edited_caption_.reset();
        update_render_data();
    }
}

bool Annotation::fade_strokes(const float selected_opacity) {

    auto p = strokes_.begin();
    while (p != strokes_.end()) {
        if (p->opacity_ > selected_opacity * 0.95) {
            p->opacity_ -= 0.005f * selected_opacity;
            p++;
        } else if (p->opacity_ > 0.0f) {
            p->opacity_ -= 0.05f * selected_opacity;
            p++;
        } else {
            p = strokes_.erase(p);
        }
    }
    update_render_data();
    return !strokes_.empty();
}

void Annotation::set_edited_caption_position(const Imath::V2f p) {
    if (current_caption_)
        current_caption_->position_ = p;
    update_render_data();
}

void Annotation::set_edited_caption_width(const float w) {
    if (current_caption_)
        current_caption_->wrap_width_ = std::max(0.01f, w);
    update_render_data();
}

void Annotation::set_edited_caption_colour(const utility::ColourTriplet &c) {
    if (current_caption_)
        current_caption_->colour_ = c;
    update_render_data();
}

void Annotation::set_edited_caption_opacity(const float opac) {
    if (current_caption_)
        current_caption_->opacity_ = opac;
    update_render_data();
}

void Annotation::set_edit_caption_font_size(const float sz) {
    if (current_caption_)
        current_caption_->font_size_ = sz;
    update_render_data();
}

void Annotation::set_edited_caption_font(const std::string &font) {
    if (current_caption_)
        current_caption_->font_name_ = font;
    update_render_data();
}

void Annotation::delete_edited_caption() {

    if (current_caption_) {
        if (current_caption_->text_.empty() && !copy_of_edited_caption_) {
            // empty caption deletion doesn't need undo/redo
            auto p = std::find(captions_.begin(), captions_.end(), current_caption_);
            if (p != captions_.end()) {
                captions_.erase(p);
            }
        } else {
            copy_of_edited_caption_.reset();
            undo_stack_.emplace_back(static_cast<UndoRedo *>(
                new UndoRedoAddCaption(copy_of_edited_caption_, current_caption_)));
            redo_stack_.clear();
            auto p = std::find(captions_.begin(), captions_.end(), current_caption_);
            if (p != captions_.end()) {
                captions_.erase(p);
            }
            current_caption_.reset();
        }
        update_render_data();
    }
}


bool Annotation::caption_cursor_position(Imath::V2f &top, Imath::V2f &bottom) const {

    if (current_caption_) {

        Imath::V2f v = font(current_caption_)
                           ->get_cursor_screen_position(
                               current_caption_->text_,
                               current_caption_->position_,
                               current_caption_->wrap_width_,
                               current_caption_->font_size_,
                               current_caption_->justification_,
                               1.0f,
                               cursor_position_);

        top    = v;
        bottom = v - Imath::V2f(0.0f, current_caption_->font_size_ * 2.0f / 1920.0f * 0.8f);
        return true;
    } else {
        top    = Imath::V2f(0.0f, 0.0f);
        bottom = Imath::V2f(0.0f, 0.0f);
    }
    return false;
}

void UndoRedoStroke::redo(Annotation *anno) { anno->strokes_.push_back(stroke_data_); }

void UndoRedoStroke::undo(Annotation *anno) {
    if (anno->strokes_.size()) {
        anno->strokes_.pop_back();
        anno->update_render_data();
    }
}

void UndoRedoAddCaption::redo(Annotation *anno) {
    if (caption_old_state_) {
        Caption c           = *caption_;
        *caption_           = *caption_old_state_;
        *caption_old_state_ = c;
    } else {
        anno->captions_.push_back(caption_);
    }
}

void UndoRedoAddCaption::undo(Annotation *anno) {
    if (caption_old_state_ && caption_) {
        // undo a change to a caption
        Caption c           = *caption_;
        *caption_           = *caption_old_state_;
        *caption_old_state_ = c;
    } else if (caption_old_state_) {
        // undo a change to a caption deletion
        anno->captions_.push_back(caption_old_state_);
    } else {
        // undo a caption creation
        anno->captions_.pop_back();
    }
    anno->update_render_data();
}

void UndoRedoClear::redo(Annotation *anno) {
    anno->strokes_.clear();
    anno->update_render_data();
}

void UndoRedoClear::undo(Annotation *anno) {
    anno->strokes_ = strokes_data_;
    anno->update_render_data();
}