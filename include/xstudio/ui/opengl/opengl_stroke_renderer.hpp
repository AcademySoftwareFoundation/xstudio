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
                const std::vector<xstudio::ui::canvas::Stroke> &strokes,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

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
                const xstudio::ui::canvas::Stroke &stroke,
                std::vector<Imath::V2f> &line_start_end_per_vertex);

            void render_erase_strokes(
                const std::vector<xstudio::ui::canvas::Stroke> &strokes,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

            void render_erase_strokes(
                const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

            void render_single_stroke(
                const xstudio::ui::canvas::Stroke &stroke,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx,
                float depth);

            void render_offscreen_texture(
                const Imath::M44f &transform_window_to_viewport_space,
                const utility::ColourTriplet &brush_colour);

            const Imath::V2i
            calculate_viewport_size(const Imath::M44f &transform_window_to_viewport_space);

            GLuint stroke_vbo_;
            GLuint stroke_vao_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> stroke_shader_;

            GLuint offscreen_vbo_;
            GLuint offscreen_vao_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> offscreen_shader_;

            // xstudio::ui::canvas::Stroke singlestroke;
            // std::vector<xstudio::ui::canvas::Stroke> singlestroke_list;
            const float pressure_ratio = 36.0f;

            opengl::OpenGLOffscreenRendererPtr offscreen_renderer_;
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio