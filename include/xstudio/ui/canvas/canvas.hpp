// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <variant>
#include <optional>
#include <shared_mutex>
#include <vector>
#include <stack>

#include "xstudio/ui/canvas/shapes.hpp"
#include "xstudio/ui/canvas/stroke.hpp"
#include "xstudio/ui/canvas/caption.hpp"
#include "xstudio/utility/chrono.hpp"


namespace xstudio {
namespace ui {

    namespace opengl {
        class OpenGLCanvasRenderer;
    }

    namespace canvas {

        class Canvas;

        /* Class CanvasUndoRedo

        N.B. any subclass of this must access the Canvas passes into redo
        and undo directly to its member data as a friend class, not via public
        accessor methods. The reason is that redo and undo are excecuted by the
        Canvas class  itself *AFTER* it has acquired a unique_lock on its mutex.
        As such, if the CanvasUndoRedo class tries to use a public method on the
        Canvas during the undo or redo calls a deadlock will result. */
        class CanvasUndoRedo {
          public:
            virtual void redo(Canvas *) = 0;
            virtual void undo(Canvas *) = 0;
        };

        typedef std::unique_ptr<CanvasUndoRedo> CanvasUndoRedoPtr;
        typedef std::stack<CanvasUndoRedoPtr, std::vector<CanvasUndoRedoPtr>>
            CanvasUndoRedoPtrStack;

        /* Class Canvas
        Note this class is thread safe EXCEPT for the begin()/end() iterators.
        When looping over the iterators call 'read_lock()' first and 'read_unlock()'
        afterwards.
        */
        class Canvas {

          public:
            using Item    = std::variant<Stroke, Caption, Quad, Polygon, Ellipse>;
            using ItemVec = std::vector<Item>;

            enum class ItemType {
                None,
                Brush,
                Draw,
                Polygon,
                Quad,
                Ellipse,
                Square,
                Circle,
                Arrow,
                Line,
                Text,
                Erase,
                Laser
            };

            Canvas() : uuid_(utility::Uuid::generate()) {}
            Canvas(const Canvas &o)
                : items_(o.items_),
                  current_item_(o.current_item_),
                  uuid_(o.uuid_),
                  next_shape_id_(o.next_shape_id_) {

                // std::cerr << "Canvas copy constructor" << std::endl;
                // std::cerr << "** this: " << std::hex << this << std::endl;
                // std::cerr << "** other: " << std::hex << &o << std::endl;

                // make sure current_item_ is pushed into finished
                // strokes/captions on copy
                end_draw();
                last_change_time_ = o.last_change_time_;
            }

            bool operator==(const Canvas &o) const {
                std::shared_lock l(mutex_);
                return items_ == o.items_;
            }

            Canvas &operator=(const Canvas &o) {

                // std::cerr << "Canvas assignment operator" << std::endl;
                // std::cerr << "** this: " << std::hex << this << std::endl;
                // std::cerr << "** other: " << std::hex << &o << std::endl;

                std::unique_lock l(mutex_);
                items_         = o.items_;
                current_item_  = o.current_item_;
                uuid_          = o.uuid_;
                next_shape_id_ = o.next_shape_id_;
                // make sure current_item_ is pushed into finished
                // strokes/captions on copy
                changed();
                end_draw_no_lock();
                last_change_time_ = o.last_change_time_;
                return *this;
            }

            ItemVec::const_iterator begin() const { return items_.begin(); }
            ItemVec::const_iterator end() const { return items_.end(); }

            void append_item(const Item &item) {
                std::shared_lock l(mutex_);
                items_.push_back(item);
            }

            void overwrite_item(ItemVec::const_iterator p, const Item &item) {
                std::shared_lock l(mutex_);
                const auto idx = std::distance(begin(), p);
                if (idx >= 0 && idx < items_.size()) {
                    items_[idx] = item;
                }
            }

            void remove_item(ItemVec::const_iterator p) {
                std::shared_lock l(mutex_);
                const auto idx = std::distance(begin(), p);
                if (idx >= 0 && idx < items_.size()) {
                    items_.erase(p);
                }
            }

            void insert_item(ItemVec::const_iterator p, const Item &item) {
                std::shared_lock l(mutex_);
                const auto idx = std::distance(begin(), p);
                if (idx >= 0 && idx < items_.size()) {
                    auto pp = items_.begin();
                    pp += idx;
                    items_.insert(pp, item);
                }
            }

            // call this before using the above iterators
            void read_lock() const { mutex_.lock_shared(); }

            // call this after using the above iterators
            void read_unlock() const { mutex_.unlock_shared(); }

            bool empty() const {
                std::shared_lock l(mutex_);
                return items_.empty() && !current_item_;
            }

            size_t size() const {
                std::shared_lock l(mutex_);
                return items_.size();
            }

            void clear(const bool clear_history = false);
            void full_clear();

            void undo();
            void redo();

            // Drawing interface follows start / update / end pattern.
            // Calling end_draw() will append to the undo stack.

            // Stroke

            void update_stroke(const Imath::V2f &pt, const float pressure = 1.0f);
            // Delete the strokes when reaching 0 opacity.

            // Shapes (filled)

            uint32_t start_quad(
                const utility::ColourTriplet &colour, const std::vector<Imath::V2f> &corners);
            void update_quad(
                uint32_t id,
                const utility::ColourTriplet &colour,
                const std::vector<Imath::V2f> &corners,
                float softness,
                float opacity,
                bool invert);

            uint32_t start_polygon(
                const utility::ColourTriplet &colour, const std::vector<Imath::V2f> &points);
            void update_polygon(
                uint32_t id,
                const utility::ColourTriplet &colour,
                const std::vector<Imath::V2f> &points,
                float softness,
                float opacity,
                bool invert);

            uint32_t start_ellipse(
                const utility::ColourTriplet &colour,
                const Imath::V2f &center,
                const Imath::V2f &radius,
                float angle);

            void update_ellipse(
                uint32_t id,
                const utility::ColourTriplet &colour,
                const Imath::V2f &center,
                const Imath::V2f &radius,
                float angle,
                float softness,
                float opacity,
                bool invert);

            bool remove_shape(uint32_t id);

            // Shapes (outlined using strokes)

            void
            start_square(const utility::ColourTriplet &colour, float thickness, float opacity);
            void update_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

            void
            start_circle(const utility::ColourTriplet &colour, float thickness, float opacity);
            void update_circle(const Imath::V2f &center, float radius);

            void
            start_arrow(const utility::ColourTriplet &colour, float thickness, float opacity);
            void update_arrow(const Imath::V2f &start, const Imath::V2f &end);

            void
            start_line(const utility::ColourTriplet &colour, float thickness, float opacity);
            void update_line(const Imath::V2f &start, const Imath::V2f &end);

            // Text

            void start_caption(
                const Imath::V2f &position,
                const std::string &font_name,
                float font_size,
                const utility::ColourTriplet &colour,
                float opacity,
                float wrap_width,
                Justification justification,
                const utility::ColourTriplet &background_colour,
                float background_opacity);

            size_t hash() const { return hash_; }

            void end_draw();

            void changed();

            const utility::clock::time_point &last_change_time() const {
                return last_change_time_;
            }

            const utility::Uuid &uuid() const { return uuid_; }

            bool has_a_current_item() const { return (bool(current_item_)); }

            template <typename T> bool has_current_item() const {
                std::shared_lock l(mutex_);
                return has_current_item_nolock<T>();
            }

            template <typename T> bool has_current_item_nolock() const {
                return current_item_ && std::holds_alternative<T>(current_item_.value());
            }

            template <typename T> T get_current() const {
                return std::get<T>(current_item_.value());
            }

            template <typename T> const T &current_item() const {
                return std::get<T>(current_item_.value());
            }

            const Item &current_item_untyped() const { return current_item_.value(); }

            const size_t num_strokes() const {
                size_t result = 0;
                for (auto &item : items_) {
                    if (std::holds_alternative<Stroke>(item))
                        result++;
                }
                return result;
            }

          private:
            void end_draw_no_lock();

            template <typename T> T &current_item() {
                return std::get<T>(current_item_.value());
            }

            friend class UndoRedoAdd;
            friend class UndoRedoDel;
            friend class UndoRedoClear;

            friend void from_json(const nlohmann::json &j, Canvas &c);
            friend void to_json(nlohmann::json &j, const Canvas &c);

          private:
            utility::clock::time_point last_change_time_;
            utility::Uuid uuid_;

            std::optional<Item> current_item_;
            ItemVec items_;

            // Not copied in copy constructor and copy operator
            CanvasUndoRedoPtrStack undo_stack_;
            CanvasUndoRedoPtrStack redo_stack_;
            void clear_undo_stack() { /*std::cerr << "- clear_undo_stack" << std::endl;*/
                undo_stack_ = CanvasUndoRedoPtrStack();
            };
            void clear_redo_stack() { /*std::cerr << "- clear_redo_stack" << std::endl;*/
                redo_stack_ = CanvasUndoRedoPtrStack();
            };

            uint32_t next_shape_id_{0};
            size_t hash_{0};

            mutable std::shared_mutex mutex_;
        };

        typedef std::shared_ptr<Canvas> CanvasPtr;

        void from_json(const nlohmann::json &j, Canvas &c);
        void to_json(nlohmann::json &j, const Canvas &c);


    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio