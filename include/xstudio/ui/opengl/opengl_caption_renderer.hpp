// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "xstudio/ui/opengl/opengl_texthandle_renderer.hpp"
#include "xstudio/ui/canvas/caption.hpp"
#include "xstudio/ui/canvas/handle.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLCaptionRenderer {
          public:
            ~OpenGLCaptionRenderer();

            void render_captions(
                const std::vector<xstudio::ui::canvas::Caption> &captions,
                const xstudio::ui::canvas::HandleState &handle_state,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx);

          private:
            void init_gl();
            void cleanup_gl();

            void render_background(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const utility::ColourTriplet &background_colour,
                const float background_opacity,
                const Imath::Box2f &bounding_box);

            typedef std::shared_ptr<OpenGLTextRendererSDF> FontRenderer;
            std::map<std::string, FontRenderer> text_renderers_;
            std::unique_ptr<OpenGLTextHandleRenderer> texthandle_renderer_;

            std::unique_ptr<GLShaderProgram> bg_shader_;
            GLuint bg_vertex_buffer_{0};
            GLuint bg_vertex_array_{0};
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio