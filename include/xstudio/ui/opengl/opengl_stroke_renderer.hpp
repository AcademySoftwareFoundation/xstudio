// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/opengl/shader_program_base.hpp"
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
                float viewport_du_dx,
                bool have_alpha_buffer);

          private:
            void init_gl();
            void cleanup_gl();

            GLuint vbo_;
            GLuint vao_;

            const void *last_data_{nullptr};
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio