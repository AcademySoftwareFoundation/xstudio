// SPDX-License-Identifier: Apache-2.0

#include <utility>

#include "xstudio/ui/canvas/canvas.hpp"
#include "xstudio/ui/canvas/canvas_undo_redo.hpp"

using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::ui::canvas;


namespace {

template <class> inline constexpr bool always_false_v = false;

} // anonymous namespace


void Canvas::clear(const bool clear_history) {

    std::unique_lock l(mutex_);
    if (clear_history) {
        undo_stack_.clear();
        redo_stack_.clear();
    } else {
        undo_stack_.emplace_back(new UndoRedoClear(items_));
    }
    items_.clear();
    current_item_.reset();
    changed();
}

void Canvas::undo() {

    std::unique_lock l(mutex_);
    if (undo_stack_.size()) {
        undo_stack_.back()->undo(this);
        redo_stack_.push_back(undo_stack_.back());
        undo_stack_.pop_back();
    }
    changed();
}

void Canvas::redo() {

    std::unique_lock l(mutex_);
    if (redo_stack_.size()) {
        redo_stack_.back()->redo(this);
        undo_stack_.push_back(redo_stack_.back());
        redo_stack_.pop_back();
    }
    changed();
}

void Canvas::start_stroke(
    const utility::ColourTriplet &colour, float thickness, float softness, float opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Pen(colour, thickness, softness, opacity);
    changed();
}

void Canvas::start_erase_stroke(float thickness) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Erase(thickness);
    changed();
}

void Canvas::update_stroke(const Imath::V2f &pt) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().add_point(pt);
    }
    changed();
}

bool Canvas::fade_all_strokes(float opacity) {

    std::unique_lock l(mutex_);
    for (auto &item : items_) {
        if (std::holds_alternative<Stroke>(item)) {
            auto &stroke = std::get<Stroke>(item);

            if (stroke.opacity > opacity * 0.95) {
                stroke.opacity -= 0.005f * opacity;
            } else if (stroke.opacity > 0.0f) {
                stroke.opacity -= 0.05f * opacity;
            }
        }
    }

    // Number of stroke still visible (opacity greater than 0)
    size_t remaining_strokes = 0;
    items_.erase(
        std::remove_if(
            items_.begin(),
            items_.end(),
            [&remaining_strokes](auto &item) {
                if (std::holds_alternative<Stroke>(item)) {
                    const auto &stroke = std::get<Stroke>(item);
                    if (stroke.opacity <= 0.0f) {
                        return true;
                    } else {
                        remaining_strokes++;
                    }
                }
                return false;
            }),
        items_.end());
    changed();
    return remaining_strokes > 0;
}

void Canvas::start_square(
    const utility::ColourTriplet &colour, float thickness, float opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Pen(colour, thickness, 0.0f, opacity);
    changed();
}

void Canvas::update_square(const Imath::V2f &corner1, const Imath::V2f &corner2) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().make_square(corner1, corner2);
    }
    changed();
}

void Canvas::start_circle(
    const utility::ColourTriplet &colour, float thickness, float opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Pen(colour, thickness, 0.0f, opacity);
    changed();
}

void Canvas::update_circle(const Imath::V2f &center, float radius) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().make_circle(center, radius);
    }
    changed();
}

void Canvas::start_arrow(const utility::ColourTriplet &colour, float thickness, float opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Pen(colour, thickness, 0.0f, opacity);
    changed();
}

void Canvas::update_arrow(const Imath::V2f &start, const Imath::V2f &end) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().make_arrow(start, end);
    }
    changed();
}

void Canvas::start_line(const utility::ColourTriplet &colour, float thickness, float opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Stroke::Pen(colour, thickness, 0.0f, opacity);
}

void Canvas::update_line(const Imath::V2f &start, const Imath::V2f &end) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().make_line(start, end);
    }
    changed();
}

void Canvas::start_caption(
    const Imath::V2f &position,
    const std::string &font_name,
    float font_size,
    const utility::ColourTriplet &colour,
    float opacity,
    float wrap_width,
    Justification justification,
    const utility::ColourTriplet &background_colour,
    float background_opacity) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    current_item_ = Caption(
        position,
        wrap_width,
        font_size,
        colour,
        opacity,
        justification,
        font_name,
        background_colour,
        background_opacity);
    changed();
}

std::string Canvas::caption_text() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().text;
    }

    return "";
}

Imath::V2f Canvas::caption_position() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().position;
    }

    return Imath::V2f(0.0f, 0.0f);
}

float Canvas::caption_width() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().wrap_width;
    }

    return 0.0f;
}

float Canvas::caption_font_size() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().font_size;
    }

    return 0.0f;
}

utility::ColourTriplet Canvas::caption_colour() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().colour;
    }

    return utility::ColourTriplet();
}

float Canvas::caption_opacity() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().opacity;
    }

    return 0.0f;
}

std::string Canvas::caption_font_name() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().font_name;
    }

    return "";
}

utility::ColourTriplet Canvas::caption_background_colour() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().background_colour;
    }

    return utility::ColourTriplet();
}

float Canvas::caption_background_opacity() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().background_opacity;
    }

    return 0.0f;
}

Imath::Box2f Canvas::caption_bounding_box() const {

    std::shared_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        return current_item<Caption>().bounding_box();
    }

    return Imath::Box2f();
}

std::array<Imath::V2f, 2> Canvas::caption_cursor_position() const {

    std::shared_lock l(mutex_);
    std::array<Imath::V2f, 2> position = {Imath::V2f(0.0f, 0.0f), Imath::V2f(0.0f, 0.0f)};

    if (has_current_item_nolock<Caption>()) {
        const Caption &caption = current_item<Caption>();

        Imath::V2f v = SDFBitmapFont::font_by_name(caption.font_name)
                           ->get_cursor_screen_position(
                               caption.text,
                               caption.position,
                               caption.wrap_width,
                               caption.font_size,
                               caption.justification,
                               1.0f,
                               cursor_position_);

        position[0] = v;
        position[1] = v - Imath::V2f(0.0f, caption.font_size * 2.0f / 1920.0f * 0.8f);
    }

    return position;
}

void Canvas::update_caption_text(const std::string &text) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().modify_text(text, cursor_position_);
    }
    changed();
}

void Canvas::update_caption_position(const Imath::V2f &position) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().position = position;
    }
    changed();
}

void Canvas::update_caption_width(float wrap_width) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().wrap_width = wrap_width;
    }
    changed();
}

void Canvas::update_caption_font_size(float font_size) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().font_size = font_size;
    }
    changed();
}

void Canvas::update_caption_colour(const utility::ColourTriplet &colour) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().colour = colour;
    }
    changed();
}

void Canvas::update_caption_opacity(float opacity) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().opacity = opacity;
    }
    changed();
}

void Canvas::update_caption_font_name(const std::string &font_name) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().font_name = font_name;
    }
    changed();
}

void Canvas::update_caption_background_colour(const utility::ColourTriplet &colour) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().background_colour = colour;
    }
    changed();
}

void Canvas::update_caption_background_opacity(float opacity) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        current_item<Caption>().background_opacity = opacity;
    }
    changed();
}

bool Canvas::has_selected_caption() const {
    std::shared_lock l(mutex_);
    return has_current_item_nolock<Caption>();
}

bool Canvas::select_caption(
    const Imath::V2f &pos, const Imath::V2f &handle_size, float viewport_pixel_scale) {

    std::unique_lock l(mutex_);
    auto update_cursor_position = [&]() {
        const Caption &c = current_item<Caption>();
        cursor_position_ =
            SDFBitmapFont::font_by_name(c.font_name)
                ->viewport_position_to_cursor(
                    pos, c.text, c.position, c.wrap_width, c.font_size, c.justification, 1.0f);
    };

    auto find_interesecting_caption = [&]() {
        return std::find_if(items_.begin(), items_.end(), [pos](auto &item) {
            if (std::holds_alternative<Caption>(item)) {
                auto &caption = std::get<Caption>(item);
                return caption.bounding_box().intersects(pos);
            }
            return false;
        });
    };

    // Early exit if we already have selected this caption.
    // (But update the cursor position beforehand)
    if (has_current_item_nolock<Caption>()) {
        HandleHoverState state =
            hover_selected_caption_handle_nolock(pos, handle_size, viewport_pixel_scale);

        if (state == HandleHoverState::HoveredInCaptionArea) {
            update_cursor_position();
        }
        if (state != HandleHoverState::NotHovered) {
            return false;
        }
    }

    // Not selecting the current caption so it will be unselected.
    end_draw_no_lock();
    changed();

    // We selected an existing caption.
    if (auto it = find_interesecting_caption(); it != items_.end()) {
        current_item_ = *it;
        items_.erase(it);

        update_cursor_position();
        return true;
    }

    // Reaching this point means no existing caption was under the cursor.

    return false;
}

HandleHoverState Canvas::hover_selected_caption_handle(
    const Imath::V2f &pos, const Imath::V2f &handle_size, float viewport_pixel_scale) const {
    std::shared_lock l(mutex_);
    return hover_selected_caption_handle_nolock(pos, handle_size, viewport_pixel_scale);
}

HandleHoverState Canvas::hover_selected_caption_handle_nolock(
    const Imath::V2f &pos, const Imath::V2f &handle_size, float viewport_pixel_scale) const {

    if (has_current_item_nolock<Caption>()) {

        const auto &caption = current_item<Caption>();

        const Imath::V2f cp_move   = caption.bounding_box().min - pos;
        const Imath::V2f cp_resize = pos - caption.bounding_box().max;
        const Imath::V2f cp_delete =
            pos - Imath::V2f(
                      caption.bounding_box().max.x,
                      caption.bounding_box().min.y - handle_size.y * viewport_pixel_scale);
        const Imath::Box2f handle_extent =
            Imath::Box2f(Imath::V2f(0.0f, 0.0f), handle_size * viewport_pixel_scale);

        if (handle_extent.intersects(cp_move)) {
            return HandleHoverState::HoveredOnMoveHandle;
        } else if (handle_extent.intersects(cp_resize)) {
            return HandleHoverState::HoveredOnResizeHandle;
        } else if (handle_extent.intersects(cp_delete)) {
            return HandleHoverState::HoveredOnDeleteHandle;
        } else if (caption.bounding_box().intersects(pos)) {
            return HandleHoverState::HoveredInCaptionArea;
        }
    }

    return HandleHoverState::NotHovered;
}

Imath::Box2f
Canvas::hover_caption_bounding_box(const Imath::V2f &pos, float viewport_pixel_scale) const {

    std::shared_lock l(mutex_);
    for (auto &item : items_) {
        if (std::holds_alternative<Caption>(item)) {
            auto &caption = std::get<Caption>(item);

            if (caption.bounding_box().intersects(pos)) {
                return caption.bounding_box();
            }
        }
    }

    return Imath::Box2f();
}

void Canvas::move_caption_cursor(int key) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        auto &caption = current_item<Caption>();

        if (key == 16777235) {
            // up arrow
            cursor_position_ = SDFBitmapFont::font_by_name(caption.font_name)
                                   ->cursor_up_or_down(
                                       cursor_position_,
                                       true,
                                       caption.text,
                                       caption.wrap_width,
                                       caption.font_size,
                                       caption.justification,
                                       1.0f);

        } else if (key == 16777237) {
            // down arrow
            cursor_position_ = SDFBitmapFont::font_by_name(caption.font_name)
                                   ->cursor_up_or_down(
                                       cursor_position_,
                                       false,
                                       caption.text,
                                       caption.wrap_width,
                                       caption.font_size,
                                       caption.justification,
                                       1.0f);

        } else if (key == 16777236) {
            // right arrow
            if (cursor_position_ != caption.text.cend())
                cursor_position_++;

        } else if (key == 16777234) {
            // left arrow
            if (cursor_position_ != caption.text.cbegin())
                cursor_position_--;

        } else if (key == 16777232) {
            // home
            cursor_position_ = caption.text.cbegin();

        } else if (key == 16777233) {
            // end
            cursor_position_ = caption.text.cend();
        }
    }
    changed();
}

void Canvas::delete_caption() {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Caption>()) {
        auto &caption = current_item<Caption>();

        // Empty caption deletion doesn't need undo/redo
        if (caption.text.empty()) {
            current_item_.reset();
        } else {
            undo_stack_.emplace_back(new UndoRedoDel(current_item_.value()));
            redo_stack_.clear();
            current_item_.reset();
        }
    }
    changed();
}

void Canvas::end_draw() {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    changed();
}

void Canvas::end_draw_no_lock() {

    // Empty caption deletion doesn't need undo/redo
    if (has_current_item_nolock<Caption>()) {
        if (current_item<Caption>().text.empty()) {
            current_item_.reset();
        }
    }

    if (current_item_) {
        undo_stack_.emplace_back(new UndoRedoAdd(current_item_.value()));
        redo_stack_.clear();
        items_.push_back(current_item_.value());
        current_item_.reset();
    }
}

void Canvas::changed() { last_change_time_ = utility::clock::now(); }


void xstudio::ui::canvas::from_json(const nlohmann::json &j, Canvas &c) {

    if (j.contains("pen_strokes") && j["pen_strokes"].is_array()) {
        for (const auto &item : j["pen_strokes"]) {
            c.items_.push_back(item.template get<Stroke>());
        }
    }

    if (j.contains("captions") && j["captions"].is_array()) {
        for (const auto &item : j["captions"]) {
            c.items_.push_back(item.template get<Caption>());
        }
    }
    c.changed();
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Canvas &c) {

    j["pen_strokes"] = nlohmann::json::array();
    j["captions"]    = nlohmann::json::array();

    for (const auto &item : c) {
        std::visit(
            [&j](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Stroke>)
                    j["pen_strokes"].push_back(nlohmann::json(arg));
                else if constexpr (std::is_same_v<T, Caption>)
                    j["captions"].push_back(nlohmann::json(arg));
                else
                    static_assert(always_false_v<T>, "Missing serialiser for canvas item!");
            },
            item);
    }
}
