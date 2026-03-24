// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_offscreen_renderer.hpp"
#include "xstudio/ui/canvas/stroke.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLStrokeRenderer {
          public:
            ~OpenGLStrokeRenderer();

            void render_strokes(
                const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

          private:
            // Could be done better...
            bool gl_initialized = false;

            void init_gl();
            void cleanup_gl();

            const int vertex_array_filler(
                const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
                std::vector<Imath::V2f> &line_start_end_per_vertex);

            void render_erase_strokes(
                const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

            // Two-pass stroke rendering (ported from WebGL2 implementation):
            //
            // Pass 1: Renders to offscreen texture. First sub-pass draws solid
            // black over the stroke footprint (replacing a full glClear), then
            // second sub-pass uses GL_MAX blending to write correct alpha values.
            void render_single_stroke_pass1(
                const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx,
                float depth,
                const std::vector<Imath::V2f> &vertex_data,
                int n_segments);

            // Pass 2: Re-renders stroke geometry sampling the offscreen texture,
            // compositing directly to the main framebuffer. This replaces the
            // previous fullscreen-quad composite and is more efficient as it
            // only touches pixels where the stroke actually exists.
            void render_single_stroke_pass2(
                const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx,
                float depth,
                const std::vector<Imath::V2f> &vertex_data,
                int n_segments);

            const Imath::V2i
            calculate_viewport_size(const Imath::M44f &transform_window_to_viewport_space);

            float get_soft_edge(
                const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
                float viewport_du_dx);

            GLuint stroke_vbo_;
            GLuint stroke_vao_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> stroke_shader_;

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> offscreen_shader_;

            opengl::OpenGLOffscreenRendererPtr offscreen_renderer_;
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio
