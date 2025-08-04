// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"
#include "annotation.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"


namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsRenderer : public plugin::ViewportOverlayRenderer {

          public:
            AnnotationsRenderer(
                const xstudio::ui::canvas::Canvas &interaction_canvas,
                PixelPatch &pixel_patch,
                const std::string &viewport_name);

            void render_image_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const float device_pixel_ratio,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            void render_viewport_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_normalised_coords,
                const float viewport_du_dpixel,
                const float device_pixel_ratio,
                const bool have_alpha_buffer) override;

            RenderPass preferred_render_pass() const override { return BeforeImage; }

          private:
            void render_pixel_picker_patch(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel);

            std::unique_ptr<xstudio::ui::opengl::OpenGLCanvasRenderer> canvas_renderer_;

            // Canvas is thread safe
            const xstudio::ui::canvas::Canvas &interaction_canvas_;

            // PixelPatch is (sort-of) thread safe - we need to lock it when using
            PixelPatch &pixel_patch_;

            xstudio::ui::canvas::HandleState handle_;
            bool annotations_visible_;
            bool laser_drawing_mode_;
            utility::Uuid current_edited_bookmark_uuid_;
            media_reader::ImageBufPtr image_being_annotated_;
            media::MediaKey frame_being_annotated_;

            void init_overlay_opengl();

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;

            GLuint vbo_ = {0};
            GLuint vao_ = {0};
            const std::string viewport_name_;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio