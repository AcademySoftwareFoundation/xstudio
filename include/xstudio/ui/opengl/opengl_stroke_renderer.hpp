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

namespace xstudio::ui::opengl {

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

    int vertex_array_filler(
        const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
        std::vector<Imath::V2f> &line_start_end_per_vertex);

    void render_erase_strokes(
        const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        float viewport_du_dx);

    // Two-pass stroke rendering (ported from WebGL2 implementation).
    // VBO is pre-uploaded by render_strokes(); these methods draw at
    // the given offset without re-uploading vertex data.
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
        int offset_verts,
        int n_verts);

    // Pass 2 (normal): Re-renders stroke geometry sampling the offscreen
    // texture, compositing directly to the main framebuffer with
    // premultiplied alpha blending.
    void render_single_stroke_pass2(
        const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        float viewport_du_dx,
        float depth,
        int offset_verts,
        int n_verts);

    // Pass 2 (effect): Re-renders stroke geometry sampling the offscreen
    // texture, compositing with burn/dodge blend modes.
    // blend_mode: 1=BurnAdd (subtractive), 2=BurnMult (multiply),
    //             3=DodgeAdd (additive), 4=DodgeMult (screen)
    void render_effect_stroke_pass2(
        const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        float viewport_du_dx,
        float depth,
        int offset_verts,
        int n_verts);

    Imath::V2i calculate_viewport_size(const Imath::M44f &transform_window_to_viewport_space);

    float get_soft_edge(
        const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke, float viewport_du_dx);

    GLuint stroke_vbo_;
    GLuint stroke_vao_;
    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> stroke_shader_;

    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> offscreen_shader_;

    opengl::OpenGLOffscreenRendererPtr offscreen_renderer_;
};

} // namespace xstudio::ui::opengl
