// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"
#include "annotation.hpp"


namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsRenderer : public plugin::ViewportOverlayRenderer {

          public:
            AnnotationsRenderer(const xstudio::ui::canvas::Canvas &interaction_canvas);

            void render_image_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            void render_viewport_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_normalised_coords,
                const float viewport_du_dpixel,
                const bool have_alpha_buffer) override;

            RenderPass preferred_render_pass() const override { return BeforeImage; }

            void update(
                const bool show_annotations,
                const xstudio::ui::canvas::HandleState &h,
                const utility::Uuid &current_edited_bookmark_uuid,
                const media::MediaKey &frame_being_annotated,
                bool laser_mode);

          private:
            std::unique_ptr<xstudio::ui::opengl::OpenGLCanvasRenderer> canvas_renderer_;

            // Canvas is thread safe
            const xstudio::ui::canvas::Canvas &interaction_canvas_;

            xstudio::ui::canvas::HandleState handle_;
            bool annotations_visible_;
            bool laser_drawing_mode_;
            utility::Uuid current_edited_bookmark_uuid_;
            media_reader::ImageBufPtr image_being_annotated_;
            media::MediaKey frame_being_annotated_;
            std::mutex mutex_;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio