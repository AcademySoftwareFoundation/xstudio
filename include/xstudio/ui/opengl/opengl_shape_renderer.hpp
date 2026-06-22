// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
// clang-format on

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/canvas/shapes.hpp"

namespace {
struct GLQuad;
struct GLEllipse;
struct GLPolygon;
struct GLPoint;
} // namespace

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLShapeRenderer {
          public:
            ~OpenGLShapeRenderer();

            void render_shapes(
                const std::vector<xstudio::ui::canvas::Quad> &quads,
                const std::vector<xstudio::ui::canvas::Polygon> &polygons,
                const std::vector<xstudio::ui::canvas::Ellipse> &ellipses,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                float viewport_du_dx,
                float image_aspectratio);

          private:
            void init_gl();
            void cleanup_gl();

// if we are GL 4.2 or above, we can use SSBO approach.
#ifdef __OPENGL_4_1__
            void upload_shape_data_to_tex(const std::vector<float> &data_buf);
            GLuint tex_id_;
            GLuint vao_, vbo_;
#else
            void upload_ssbo(
                const std::vector<GLQuad> &quads,
                const std::vector<GLEllipse> &ellipses,
                const std::vector<GLPolygon> &polygons,
                const std::vector<GLPoint> &points);
            std::array<GLuint, 4> ssbo_id_{0, 0, 0, 0};
#endif

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio