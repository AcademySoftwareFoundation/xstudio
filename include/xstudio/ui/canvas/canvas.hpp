// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <variant>
#include <optional>
#include <shared_mutex>

#include "xstudio/ui/canvas/stroke.hpp"
#include "xstudio/ui/canvas/caption.hpp"
#include "xstudio/ui/canvas/handle.hpp"
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

        typedef std::shared_ptr<CanvasUndoRedo> CanvasUndoRedoPtr;

        /* Class Canvas

        Note this class is thread safe EXCEPT for the begin()/end() iterators.
        When looping over the iterators call 'read_lock()' first and 'read_unlock()'
        afterwards.
        */
        class Canvas {

            using Item    = std::variant<Stroke, Caption>;
            using ItemVec = std::vector<Item>;

          public:
            Canvas() : uuid_(utility::Uuid::generate()) {}
            Canvas(const Canvas &o)
                : items_(o.items_),
                  current_item_(o.current_item_),
                  undo_stack_(o.undo_stack_),
                  redo_stack_(o.redo_stack_),
                  last_change_time_(o.last_change_time_),
                  uuid_(o.uuid_) {}

            bool operator==(const Canvas &o) const {
                std::shared_lock l(mutex_);
                return items_ == o.items_;
            }

            Canvas &operator=(const Canvas &o) {
                std::unique_lock l(mutex_);
                items_            = o.items_;
                current_item_     = o.current_item_;
                undo_stack_       = o.undo_stack_;
                redo_stack_       = o.redo_stack_;
                last_change_time_ = o.last_change_time_;
                uuid_             = o.uuid_;
                return *this;
            }

            ItemVec::const_iterator begin() const { return items_.begin(); }
            ItemVec::const_iterator end() const { return items_.end(); }

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

            void undo();
            void redo();

            // Drawing interface follows start / update / end pattern.
            // Calling end_draw() will append to the undo stack.

            // Stroke

            void start_stroke(
                const utility::ColourTriplet &colour,
                float thickness,
                float softness,
                float opacity);
            void start_erase_stroke(float thickness);
            void update_stroke(const Imath::V2f &pt);
            // Delete the strokes when reaching 0 opacity.
            bool fade_all_strokes(float opacity);

            // Shapes

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

            std::string caption_text() const;
            Imath::V2f caption_position() const;
            float caption_width() const;
            float caption_font_size() const;
            utility::ColourTriplet caption_colour() const;
            float caption_opacity() const;
            std::string caption_font_name() const;
            utility::ColourTriplet caption_background_colour() const;
            float caption_background_opacity() const;
            Imath::Box2f caption_bounding_box() const;
            // Returns top and bottom position of the text cursor.
            std::array<Imath::V2f, 2> caption_cursor_position() const;
            Imath::V2f caption_cursor_bottom() const;

            void update_caption_text(const std::string &text);
            void update_caption_position(const Imath::V2f &position);
            void update_caption_width(float wrap_width);
            void update_caption_font_size(float font_size);
            void update_caption_colour(const utility::ColourTriplet &colour);
            void update_caption_opacity(float opacity);
            void update_caption_font_name(const std::string &font_name);
            void update_caption_background_colour(const utility::ColourTriplet &colour);
            void update_caption_background_opacity(float opacity);

            bool has_selected_caption() const;

            // Caption selection logic cover these cases:
            // * Click on an existing caption: update the cursor position
            // * Click on a different caption: select the caption
            // * Click in an empty area: unselected the current caption
            bool select_caption(
                const Imath::V2f &pos,
                const Imath::V2f &handle_size,
                float viewport_pixel_scale);

            // Returns the hover status for the current selected caption.
            // * Hovering on the caption area
            // * Hovering on the caption handles (slightly outside the area)
            // * Hovering anywhere else outside the caption
            HandleHoverState hover_selected_caption_handle(
                const Imath::V2f &pos,
                const Imath::V2f &handle_size,
                float viewport_pixel_scale) const;

            // Returns the bounding box the the caption under the cursor.
            Imath::Box2f
            hover_caption_bounding_box(const Imath::V2f &pos, float viewport_pixel_scale) const;

            void move_caption_cursor(int key);

            void delete_caption();

            void end_draw();

            void changed();

            const utility::clock::time_point &last_change_time() const {
                return last_change_time_;
            }

            const utility::Uuid &uuid() const { return uuid_; }

            template <typename T> bool has_current_item() const {
                std::shared_lock l(mutex_);
                return has_current_item_nolock<T>();
            }

            template <typename T> bool has_current_item_nolock() const {
                return current_item_ && std::holds_alternative<T>(current_item_.value());
            }

            template <typename T> T get_current() const {
                std::shared_lock l(mutex_);
                return std::get<T>(current_item_.value());
            }

          private:
            void end_draw_no_lock();

            HandleHoverState hover_selected_caption_handle_nolock(
                const Imath::V2f &pos,
                const Imath::V2f &handle_size,
                float viewport_pixel_scale) const;

            template <typename T> T &current_item() {
                return std::get<T>(current_item_.value());
            }
            template <typename T> const T &current_item() const {
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

            std::vector<CanvasUndoRedoPtr> undo_stack_;
            std::vector<CanvasUndoRedoPtr> redo_stack_;

            std::string::const_iterator cursor_position_;

            mutable std::shared_mutex mutex_;
        };

        typedef std::shared_ptr<Canvas> CanvasPtr;

        void from_json(const nlohmann::json &j, Canvas &c);
        void to_json(nlohmann::json &j, const Canvas &c);


    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio