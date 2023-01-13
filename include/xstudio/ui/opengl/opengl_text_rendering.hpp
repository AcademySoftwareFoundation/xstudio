// SPDX-License-Identifier: Apache-2.0
#pragma once

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/font.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLTextRendererBitmap : public AlphaBitmapFont {

          public:
            OpenGLTextRendererBitmap(
                const std::string ttf_font_path, const int glyph_image_size_in_pixels);

            void render_text(
                const std::vector<float> &precomputed_vertex_buffer,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const utility::ColourTriplet &colour,
                const float viewport_du_dx,
                const float text_size,
                const float opacity) const override;

          private:
            unsigned int vao_, vbo_, texture_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
        };

        class OpenGLTextRendererSDF : public SDFBitmapFont {

          public:
            OpenGLTextRendererSDF(
                const std::string ttf_font_path, const int glyph_image_size_in_pixels);

            void render_text(
                const std::vector<float> &precomputed_vertex_buffer,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const utility::ColourTriplet &colour,
                const float viewport_du_dx,
                const float text_size,
                const float opacity) const override;

          private:
            unsigned int vao_, vbo_, texture_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
        };

        class OpenGLTextRendererVector : private VectorFont {

          public:
            OpenGLTextRendererVector(const std::string ttf_font_path);

            void render_text(
                const std::string &text,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const Imath::V2f box_topleft,
                const Imath::V2f box_topright,
                const float text_size,
                const utility::ColourTriplet &colour,
                const Justification &just,
                const float line_spacing);

          private:
            void compute_wrap_indeces(
                const std::string &text, const float box_width, const float scale);

            unsigned int vao_, vbo_, ebo_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
            std::vector<std::string::const_iterator> wrap_points_;
        };


    } // namespace opengl
} // namespace ui
} // namespace xstudio
