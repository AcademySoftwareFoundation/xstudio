#pragma once

#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

#define CAPTION_HANDLE_SIZE 50.0f

namespace xstudio {
namespace ui {
    namespace viewport {

        enum class HandleHoverState {
            NotHovered,
            HoveredInCaptionArea,
            HoveredOnMoveHandle,
            HoveredOnResizeHandle,
            HoveredOnDeleteHandle
        };

        typedef std::vector<std::shared_ptr<canvas::Stroke>> StrokeVec;
        typedef std::vector<std::shared_ptr<canvas::Caption>> CaptionVec;
        typedef std::vector<HandleHoverState> HandleStateVec;

        class LaserStrokesRenderDataSet : public utility::BlindDataObject {
          public:
            LaserStrokesRenderDataSet() {}

            void add_laser_strokes(
                const std::vector<std::shared_ptr<ui::canvas::Stroke>> &laser_strokes) {
                for (const auto &l : laser_strokes) {
                    laser_strokes_.emplace_back(std::make_shared<canvas::Stroke>(*l));
                }
            }

            const StrokeVec &laser_strokes() const { return laser_strokes_; }

          private:
            StrokeVec laser_strokes_;
        };

        class PerImageAnnotationRenderDataSet : public utility::BlindDataObject {
          public:
            PerImageAnnotationRenderDataSet() {}

            void add_stroke(const canvas::Stroke *stroke) {
                strokes_.push_back(std::make_shared<canvas::Stroke>(*stroke));
            }

            void
            add_erase_stroke(const canvas::Stroke *stroke, const utility::Uuid &bookmark_uuid) {
                live_erase_strokes_[bookmark_uuid].push_back(
                    std::make_shared<canvas::Stroke>(*stroke));
            }

            void
            add_live_caption(const canvas::Caption *caption, const HandleHoverState hstate) {
                captions_.push_back(std::make_shared<canvas::Caption>(*caption));
                handles_.push_back(hstate);
            }

            void add_hovered_caption_box(const Imath::Box2f &box) {
                hovered_boxes_.push_back(box);
            }

            const std::set<std::size_t> &skip_captions() const { return skip_captions_; }

            void add_skip_render_caption_id(const std::size_t caption_hash) {
                skip_captions_.insert(caption_hash);
            }

            const CaptionVec &captions() const { return captions_; }

            const HandleStateVec &handle_states() const { return handles_; }

            const StrokeVec &strokes() const { return strokes_; }

            const std::vector<Imath::Box2f> &hovered_caption_boxes() const {
                return hovered_boxes_;
            }

            const StrokeVec &
            live_erase_strokes(const utility::Uuid &affected_bookmark_id) const {
                const auto p = live_erase_strokes_.find(affected_bookmark_id);
                if (p != live_erase_strokes_.end()) {
                    return p->second;
                }
                return dummy;
            }

          private:
            const StrokeVec dummy;
            const CaptionVec dummy2;
            std::map<utility::Uuid, StrokeVec> live_erase_strokes_;
            StrokeVec strokes_;
            CaptionVec captions_;
            HandleStateVec handles_;
            std::vector<Imath::Box2f> hovered_boxes_;
            std::set<std::size_t> skip_captions_;
        };

        class AnnotationExtrasRenderDataSet : public utility::BlindDataObject {
          public:
            AnnotationExtrasRenderDataSet() {}
        };

    } // namespace viewport
} // end namespace ui
} // end namespace xstudio