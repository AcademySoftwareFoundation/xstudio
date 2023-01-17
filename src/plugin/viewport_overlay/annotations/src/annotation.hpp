// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/font.hpp"
#include "pen_stroke.hpp"
#include "caption.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationRenderData {
          public:
            AnnotationRenderData()                              = default;
            AnnotationRenderData(const AnnotationRenderData &o) = default;

            utility::Uuid uuid_;
            std::vector<Imath::V2f> pen_stroke_vertices_;

            struct StrokeInfo {
                int stroke_point_count_;
                utility::ColourTriplet brush_colour_;
                float brush_opacity_;
                float brush_thickness_;
                float stroke_depth_;
                bool is_erase_stroke_;
            };

            struct CaptionInfo {
                std::vector<float> precomputed_vertex_buffer;
                utility::ColourTriplet colour;
                float opacity;
                float text_size;
                Imath::Box2f bounding_box;
                float width;
                std::string font_name;
            };

            std::vector<StrokeInfo> stroke_info_;

            std::vector<CaptionInfo> caption_info_;

            Imath::V3f last;

            void clear() {
                pen_stroke_vertices_.clear();
                stroke_info_.clear();
                caption_info_.clear();
            }
        };

        typedef std::shared_ptr<const AnnotationRenderData> AnnotationRenderDataPtr;

        class AnnotationRenderDataSet : public utility::BlindDataObject {
          public:
            AnnotationRenderDataSet()                                 = default;
            AnnotationRenderDataSet(const AnnotationRenderDataSet &o) = default;

            void add_annotation_render_data(AnnotationRenderDataPtr data) {
                annotations_render_data_.push_back(data);
            }

            std::vector<AnnotationRenderDataPtr>::const_iterator begin() const {
                return annotations_render_data_.cbegin();
            }

            std::vector<AnnotationRenderDataPtr>::const_iterator end() const {
                return annotations_render_data_.cend();
            }

            AnnotationRenderDataPtr edited_annotation_render_data_;

            bool show_caption_handles_ = false;

          private:
            std::vector<AnnotationRenderDataPtr> annotations_render_data_;
        };

        class Annotation;

        class UndoRedo {

          public:
            virtual void redo(Annotation *) = 0;
            virtual void undo(Annotation *) = 0;
        };

        typedef std::shared_ptr<UndoRedo> UndoRedoPtr;

        class Annotation : public bookmark::AnnotationBase {

          public:
            Annotation(std::map<std::string, std::shared_ptr<SDFBitmapFont>> &fonts);
            Annotation(
                const utility::JsonStore &s,
                std::map<std::string, std::shared_ptr<SDFBitmapFont>> &fonts);
            Annotation(const Annotation &o);

            bool operator==(const Annotation &o) const { return strokes_ == o.strokes_; }

            bool empty() const { return strokes_.empty() && captions_.empty(); }

            bool test_click_in_caption(const Imath::V2f pointer_position);

            void start_new_caption(
                const Imath::V2f position,
                const float wrap_width,
                const float font_size,
                const utility::ColourTriplet colour,
                const float opacity,
                const Justification justification,
                const std::string &font_name);

            void modify_caption_text(const std::string &t);

            void key_down(const int key);

            Imath::Box2f mouse_hover_on_captions(
                const Imath::V2f cursor_position, const float viewport_pixel_scale) const;

            Caption::HoverState mouse_hover_on_selected_caption(
                const Imath::V2f cursor_position, const float viewport_pixel_scale) const;

            void start_pen_stroke(
                const utility::ColourTriplet &c, const float thickness, const float opacity);

            void start_erase_stroke(const float thickness);

            void add_point_to_current_stroke(const Imath::V2f pt);

            void stroke_finished();

            [[nodiscard]] utility::JsonStore
            serialise(utility::Uuid &plugin_uuid) const override;

            [[nodiscard]] AnnotationRenderDataPtr
            render_data(const bool is_edited_annotation = false) const;

            void clear();

            void undo();

            void redo();

            void update_render_data();

            bool fade_strokes(const float selected_opacity);

            std::shared_ptr<PenStroke> current_stroke_;
            std::shared_ptr<Caption> current_caption_;
            std::vector<PenStroke> strokes_;
            std::vector<std::shared_ptr<Caption>> captions_;

            inline static const Imath::V2f captionHandleSize = {Imath::V2f(30.0f, 30.0f)};

            bool have_edited_caption() const { return bool(current_caption_); }
            Imath::V2f edited_caption_position() const {
                return current_caption_ ? current_caption_->position_ : Imath::V2f();
            }
            Imath::Box2f edited_caption_bounding_box() const {
                return current_caption_ ? current_caption_->bounding_box_ : Imath::Box2f();
            }
            float edited_caption_width() const {
                return current_caption_ ? current_caption_->wrap_width_ : 0.0f;
            }
            utility::ColourTriplet edited_caption_colour() const {
                return current_caption_ ? current_caption_->colour_ : utility::ColourTriplet();
            }
            float edited_caption_opacity() const {
                return current_caption_ ? current_caption_->opacity_ : 1.0f;
            }

            float edited_caption_font_size() const {
                return current_caption_ ? current_caption_->font_size_ : 50.0f;
            }

            void set_edited_caption_position(const Imath::V2f p);
            void set_edited_caption_width(const float w);
            void set_edited_caption_colour(const utility::ColourTriplet &c);
            void set_edited_caption_opacity(const float opac);
            void set_edit_caption_font_size(const float sz);

            bool caption_cursor_position(Imath::V2f &top, Imath::V2f &bottom) const;

          private:
            bool no_fonts() const { return fonts_.empty(); }
            std::shared_ptr<SDFBitmapFont> font(const std::shared_ptr<Caption> &caption) const;

            friend class UndoRedoStroke;
            friend class UndoRedoClear;

            void finished_current_stroke();

            AnnotationRenderDataPtr cached_render_data_;

            std::map<std::string, std::shared_ptr<SDFBitmapFont>> fonts_;

            friend class AnnotationsRenderer;
            friend class AnnotationSerialiser;

            bool stroking_ = {false};

            AnnotationRenderData render_data_;

            std::vector<UndoRedoPtr> undo_stack_;
            std::vector<UndoRedoPtr> redo_stack_;

            std::string::const_iterator cursor_position_;
        };

        typedef std::shared_ptr<Annotation> AnnotationPtr;

        class UndoRedoStroke : public UndoRedo {

          public:
            UndoRedoStroke(const PenStroke &stroke) : stroke_data_(stroke) {}

            void redo(Annotation *) override;
            void undo(Annotation *) override;

            PenStroke stroke_data_;
        };

        class UndoRedoClear : public UndoRedo {

          public:
            UndoRedoClear(const std::vector<PenStroke> &strokes) : strokes_data_(strokes) {}

            void redo(Annotation *) override;
            void undo(Annotation *) override;

            std::vector<PenStroke> strokes_data_;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio