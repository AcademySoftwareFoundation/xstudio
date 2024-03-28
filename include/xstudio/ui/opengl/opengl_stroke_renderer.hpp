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
            void resize_ssbo(std::size_t size);
            void upload_ssbo(const std::vector<Imath::V2f> &points);

            const void *last_data_{nullptr};

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
            GLuint ssbo_id_{0};
            GLuint ssbo_size_{0};
            std::size_t ssbo_data_hash_{0};
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio