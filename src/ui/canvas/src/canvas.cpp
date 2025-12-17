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

void Canvas::full_clear() {

    std::unique_lock l(mutex_);
    clear_undo_stack();
    clear_redo_stack();
    items_.clear();
    current_item_.reset();

}

void Canvas::clear(const bool clear_history) {

    std::unique_lock l(mutex_);

    if (clear_history) {
        clear_undo_stack();
        clear_redo_stack();
    } else {
        undo_stack_.push(std::make_unique<UndoRedoClear>(items_));
    }

    items_.clear();
    next_shape_id_ = 0;

    changed();
}

void Canvas::undo() {

    std::unique_lock l(mutex_);

    if (undo_stack_.empty())
        return;

    undo_stack_.top()->undo(this);
    redo_stack_.push(std::move(undo_stack_.top()));
    undo_stack_.pop();

    changed();
}

void Canvas::redo() {

    std::unique_lock l(mutex_);

    if (redo_stack_.empty())
        return;

    redo_stack_.top()->redo(this);
    undo_stack_.push(std::move(redo_stack_.top()));
    redo_stack_.pop();

    changed();
}

void Canvas::update_stroke(const Imath::V2f &pt, const float pressure) {

    std::unique_lock l(mutex_);
    if (has_current_item_nolock<Stroke>()) {
        current_item<Stroke>().add_point(pt, pressure);
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

uint32_t Canvas::start_quad(
    const utility::ColourTriplet &colour, const std::vector<Imath::V2f> &corners) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();

    Quad q;
    q._id    = next_shape_id_++;
    q.bl     = corners[0];
    q.tl     = corners[1];
    q.tr     = corners[2];
    q.br     = corners[3];
    q.colour = colour;

    current_item_ = q;

    changed();

    return q._id;
}

void Canvas::update_quad(
    uint32_t id,
    const utility::ColourTriplet &colour,
    const std::vector<Imath::V2f> &corners,
    float softness,
    float opacity,
    bool invert) {

    std::unique_lock l(mutex_);

    for (auto &item : items_) {
        if (std::holds_alternative<Quad>(item)) {
            auto &quad = std::get<Quad>(item);

            if (quad._id == id) {

                quad.bl = corners[0];
                quad.tl = corners[1];
                quad.tr = corners[2];
                quad.br = corners[3];

                quad.colour   = colour;
                quad.softness = softness;
                quad.opacity  = opacity;
                quad.invert   = invert;

                changed();
            }
        }
    }
}

uint32_t Canvas::start_polygon(
    const utility::ColourTriplet &colour, const std::vector<Imath::V2f> &points) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();

    Polygon q;
    q._id    = next_shape_id_++;
    q.points = points;
    q.colour = colour;

    current_item_ = q;

    changed();

    return q._id;
}

void Canvas::update_polygon(
    uint32_t id,
    const utility::ColourTriplet &colour,
    const std::vector<Imath::V2f> &points,
    float softness,
    float opacity,
    bool invert) {

    std::unique_lock l(mutex_);

    for (auto &item : items_) {
        if (std::holds_alternative<Polygon>(item)) {
            auto &polygon = std::get<Polygon>(item);

            if (polygon._id == id) {

                polygon.points = points;

                polygon.colour   = colour;
                polygon.softness = softness;
                polygon.opacity  = opacity;
                polygon.invert   = invert;

                changed();
            }
        }
    }
}

uint32_t Canvas::start_ellipse(
    const utility::ColourTriplet &colour,
    const Imath::V2f &center,
    const Imath::V2f &radius,
    float angle) {

    std::unique_lock l(mutex_);
    end_draw_no_lock();

    Ellipse e;
    e._id    = next_shape_id_++;
    e.center = center;
    e.radius = radius;
    e.angle  = angle;
    e.colour = colour;

    current_item_ = e;

    changed();

    return e._id;
}

void Canvas::update_ellipse(
    uint32_t id,
    const utility::ColourTriplet &colour,
    const Imath::V2f &center,
    const Imath::V2f &radius,
    float angle,
    float softness,
    float opacity,
    bool invert) {

    std::unique_lock l(mutex_);

    for (auto &item : items_) {
        if (std::holds_alternative<Ellipse>(item)) {
            auto &ellipse = std::get<Ellipse>(item);

            if (ellipse._id == id) {

                ellipse.center = center;
                ellipse.radius = radius;
                ellipse.angle  = angle;

                ellipse.colour   = colour;
                ellipse.softness = softness;
                ellipse.opacity  = opacity;
                ellipse.invert   = invert;

                changed();
            }
        }
    }
}

bool Canvas::remove_shape(uint32_t id) {

    std::unique_lock l(mutex_);

    int remove_idx = -1;

    for (int i = 0; i < items_.size(); ++i) {
        if (std::holds_alternative<Quad>(items_[i])) {
            auto &quad = std::get<Quad>(items_[i]);

            if (quad._id == id) {
                remove_idx = i;
                break;
            }
        } else if (std::holds_alternative<Polygon>(items_[i])) {
            auto &polygon = std::get<Polygon>(items_[i]);

            if (polygon._id == id) {
                remove_idx = i;
                break;
            }
        } else if (std::holds_alternative<Ellipse>(items_[i])) {
            auto &ellipse = std::get<Ellipse>(items_[i]);

            if (ellipse._id == id) {
                remove_idx = i;
                break;
            }
        }
    }

    if (remove_idx >= 0) {
        items_.erase(items_.begin() + remove_idx);
        changed();
        return true;
    }

    return false;
}

void Canvas::end_draw() {

    std::unique_lock l(mutex_);
    end_draw_no_lock();
    changed();
}

void Canvas::end_draw_no_lock() {

    // Empty caption deletion doesn't need undo/redo
    if (has_current_item_nolock<Caption>()) {
        if (current_item<Caption>().text().empty()) {
            current_item_.reset();
        }
    }

    if (current_item_) {
        undo_stack_.push(std::make_unique<UndoRedoAdd>(current_item_.value()));
        clear_redo_stack();
        items_.push_back(current_item_.value());
        current_item_.reset();
    }

}

void Canvas::changed() {

    last_change_time_ = utility::clock::now();
    std::ostringstream oss;
    oss << last_change_time_.time_since_epoch().count() << (void *)this;
    hash_ = std::hash<std::string>{}(oss.str());
}

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

    if (j.contains("quads") && j["quads"].is_array()) {
        for (const auto &item : j["quads"]) {
            Quad quad = item.template get<Quad>();
            quad._id  = c.next_shape_id_++;
            c.items_.push_back(quad);
        }
    }

    if (j.contains("polygons") && j["polygons"].is_array()) {
        for (const auto &item : j["polygons"]) {
            Polygon polygon = item.template get<Polygon>();
            polygon._id     = c.next_shape_id_++;
            c.items_.push_back(polygon);
        }
    }

    if (j.contains("ellipses") && j["ellipses"].is_array()) {
        for (const auto &item : j["ellipses"]) {
            Ellipse ellipse = item.template get<Ellipse>();
            ellipse._id     = c.next_shape_id_++;
            c.items_.push_back(ellipse);
        }
    }

    c.changed();
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Canvas &c) {

    j["pen_strokes"] = nlohmann::json::array();
    j["captions"]    = nlohmann::json::array();
    j["quads"]       = nlohmann::json::array();
    j["polygons"]    = nlohmann::json::array();
    j["ellipses"]    = nlohmann::json::array();

    for (const auto &item : c) {
        std::visit(
            [&j](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Stroke>)
                    j["pen_strokes"].push_back(nlohmann::json(arg));
                else if constexpr (std::is_same_v<T, Caption>)
                    j["captions"].push_back(nlohmann::json(arg));
                else if constexpr (std::is_same_v<T, Quad>)
                    j["quads"].push_back(nlohmann::json(arg));
                else if constexpr (std::is_same_v<T, Polygon>)
                    j["polygons"].push_back(nlohmann::json(arg));
                else if constexpr (std::is_same_v<T, Ellipse>)
                    j["ellipses"].push_back(nlohmann::json(arg));
                else
                    static_assert(always_false_v<T>, "Missing serialiser for canvas item!");
            },
            item);
    }
}
