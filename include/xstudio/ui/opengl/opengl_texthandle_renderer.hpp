// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/canvas/handle.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLTextHandleRenderer {
          public:
            ~OpenGLTextHandleRenderer();

            void render_handles(
                const xstudio::ui::canvas::HandleState &handle_state,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

          private:
            void init_gl();
            void cleanup_gl();

            std::unique_ptr<GLShaderProgram> shader_;
            GLuint handles_vertex_buffer_obj_{0};
            GLuint handles_vertex_array_{0};
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio