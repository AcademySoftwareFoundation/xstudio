// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_stroke_renderer.hpp"

#include "opengl_stroke_shaders.glsl"
#include "opengl_offscreen_shaders.glsl"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;


OpenGLStrokeRenderer::~OpenGLStrokeRenderer() { cleanup_gl(); }

void OpenGLStrokeRenderer::init_gl() {

    // Stroke rendering
    stroke_shader_ =
        std::make_unique<ui::opengl::GLShaderProgram>(stroke_vertex_shader, stroke_frag_shader);
    stroke_shader_->set_transpose_matrices(true);
    glGenVertexArrays(1, &stroke_vao_);
    glGenBuffers(1, &stroke_vbo_);
    glBindVertexArray(stroke_vao_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);

    // Offscreen rendering
    offscreen_renderer_ = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA8);
    offscreen_shader_   = std::make_unique<ui::opengl::GLShaderProgram>(
        offscreen_vertex_shader, offscreen_frag_shader);
    offscreen_shader_->set_transpose_matrices(true);
}

void OpenGLStrokeRenderer::cleanup_gl() {

    if (stroke_shader_) {
        glDeleteBuffers(1, &stroke_vbo_);
        glDeleteVertexArrays(1, &stroke_vao_);
        stroke_shader_.reset();
    }
}

int OpenGLStrokeRenderer::vertex_array_filler(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
    std::vector<Imath::V2f> &line_start_end_per_vertex) {

    const auto &points = stroke->points();
    if (points.empty())
        return 0;

    const Stroke::Point *v0 = points.data();
    const Stroke::Point *v1 = v0;

    if (points.size() > 1)
        v1++;

    int n_segments = std::max(size_t(1), (points.size() - 1));

    for (int i = 0; i < n_segments; ++i) {
        Imath::V2f pt_sz(v0->size_pressure, v1->size_pressure);
        Imath::V2f pt_op(v0->opacity_pressure, v1->opacity_pressure);
        for (int j = 0; j < 6; ++j) {
            line_start_end_per_vertex.push_back(v0->pos);
            line_start_end_per_vertex.push_back(v1->pos);
            line_start_end_per_vertex.push_back(pt_sz);
            line_start_end_per_vertex.push_back(pt_op);
        }
        v0++;
        v1++;
    }

    return n_segments;
}

void OpenGLStrokeRenderer::render_erase_strokes(
    const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    std::vector<Imath::V2f> line_start_end_per_vertex;
    std::vector<int> n_vtx_per_stroke;

    for (const auto &stroke : strokes) {

        // Only render erase strokes here
        if (stroke->type() != StrokeType_Erase)
            continue;

        const int n_segments = vertex_array_filler(stroke, line_start_end_per_vertex);
        n_vtx_per_stroke.push_back(n_segments * 6);
    }

    if (n_vtx_per_stroke.empty())
        return;

    // Prepare vertex objects
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        line_start_end_per_vertex.size() * sizeof(Imath::V2f),
        line_start_end_per_vertex.data(),
        GL_STREAM_DRAW);
    float *ptr = nullptr;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);

    stroke_shader_->use();

    // Here we draw the erase strokes.
    // Essentially we just draw into the depth buffer, nothing goes into the frag buffer.
    // Later, when we draw visible strokes, the depth test will cause the 'erase' to work.

    // Set shared uniforms once — only z_adjust and thickness vary per stroke
    utility::JsonStore shared_params;
    shared_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shared_params["to_canvas"]       = transform_window_to_viewport_space;
    shared_params["brush_opacity"]   = 1.0f;
    shared_params["just_black"]      = 2.0f;
    stroke_shader_->set_shader_parameters(shared_params);

    GLint offset = 0;
    float depth  = 0.0f;

    auto p_n_vtx_per_stroke = n_vtx_per_stroke.begin();

    for (const auto &stroke : strokes) {

        depth += 0.001;

        // Only render erase strokes here
        if (stroke->type() != StrokeType_Erase) {
            continue;
        }

        utility::JsonStore per_stroke_params;
        per_stroke_params["z_adjust"]  = depth;
        per_stroke_params["thickness"] = stroke->thickness();
        per_stroke_params["soft_edge"] = get_soft_edge(stroke, viewport_du_dx);
        stroke_shader_->set_shader_parameters(per_stroke_params);

        glDrawArrays(GL_TRIANGLES, offset, *p_n_vtx_per_stroke);
        offset += *p_n_vtx_per_stroke;
        p_n_vtx_per_stroke++;
    }

    stroke_shader_->stop_using();
}

void OpenGLStrokeRenderer::render_single_stroke_pass1(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float depth,
    int offset_verts,
    int n_verts) {

    // Begin rendering to offscreen texture
    offscreen_renderer_->begin();

    glBindVertexArray(stroke_vao_);

    stroke_shader_->use();

    utility::JsonStore shader_params;
    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["soft_edge"]       = get_soft_edge(stroke, viewport_du_dx);
    shader_params["z_adjust"]        = depth;
    shader_params["brush_opacity"]   = stroke->opacity();
    shader_params["thickness"]       = stroke->thickness();
    shader_params["just_black"]      = 1.0f; // renders stroke quads as black
    stroke_shader_->set_shader_parameters(shader_params);

    // 1st sub-pass: no blending, no depth test. We render black to the whole
    // area of the stroke. This replaces the previous glClear(GL_COLOR_BUFFER_BIT)
    // by only clearing pixels within the stroke footprint.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, offset_verts, n_verts);

    // 2nd sub-pass: enable blending with MAX equation. Stroke sections are
    // rendered with overlapping 'sausages' but where they overlap we don't
    // want accumulation of colour into the fragment - we just want the
    // maximum alpha to be rendered to the fragment.
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);

    utility::JsonStore params2;
    params2["just_black"] = 0.0f;
    stroke_shader_->set_shader_parameters(params2);

    glDrawArrays(GL_TRIANGLES, offset_verts, n_verts);

    glBindVertexArray(0);

    stroke_shader_->stop_using();

    // End offscreen rendering
    offscreen_renderer_->end();
}

void OpenGLStrokeRenderer::render_single_stroke_pass2(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float depth,
    int offset_verts,
    int n_verts) {

    glBindVertexArray(stroke_vao_);

    offscreen_shader_->use();

    utility::JsonStore shader_params;
    shader_params["to_coord_system"]  = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]        = transform_window_to_viewport_space;
    shader_params["soft_edge"]        = get_soft_edge(stroke, viewport_du_dx);
    shader_params["z_adjust"]         = depth;
    shader_params["brush_colour"]     = stroke->colour();
    shader_params["brush_opacity"]    = stroke->opacity();
    shader_params["thickness"]        = stroke->thickness();
    shader_params["offscreenTexture"] = 11;
    shader_params["blend_mode"]       = 0;
    offscreen_shader_->set_shader_parameters(shader_params);

    // Save current GL_ACTIVE_TEXTURE
    int active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, offscreen_renderer_->texture_handle());

    // Restore saved texture unit
    glActiveTexture(active_texture);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glDrawArrays(GL_TRIANGLES, offset_verts, n_verts);

    glBindVertexArray(0);

    offscreen_shader_->stop_using();
}

void OpenGLStrokeRenderer::render_effect_stroke_pass2(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float depth,
    int offset_verts,
    int n_verts) {

    const int blend_mode = stroke->blend_mode();

    glBindVertexArray(stroke_vao_);

    offscreen_shader_->use();

    utility::JsonStore shader_params;
    shader_params["to_coord_system"]  = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]        = transform_window_to_viewport_space;
    shader_params["soft_edge"]        = get_soft_edge(stroke, viewport_du_dx);
    shader_params["z_adjust"]         = depth;
    shader_params["brush_colour"]     = stroke->colour();
    shader_params["brush_opacity"]    = 1.0f;
    shader_params["thickness"]        = stroke->thickness();
    shader_params["offscreenTexture"] = 11;
    shader_params["blend_mode"]       = blend_mode;
    offscreen_shader_->set_shader_parameters(shader_params);

    // Save current GL_ACTIVE_TEXTURE
    int active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, offscreen_renderer_->texture_handle());

    // Restore saved texture unit
    glActiveTexture(active_texture);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);

    // Set blend function based on effect type
    switch (blend_mode) {
    case 1: // BurnAdd (subtractive): dst - strength
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        break;
    case 2: // BurnMult (multiply): dst * (1 - strength)
        glBlendFuncSeparate(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ONE);
        break;
    case 3: // DodgeAdd (additive): dst + strength
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        break;
    case 4: // DodgeMult (screen): dst * (1 + strength)
        glBlendFuncSeparate(GL_DST_COLOR, GL_ONE, GL_ZERO, GL_ONE);
        break;
    }

    glDrawArrays(GL_TRIANGLES, offset_verts, n_verts);

    // Restore default blend state for subsequent strokes
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glBindVertexArray(0);

    offscreen_shader_->stop_using();
}

void OpenGLStrokeRenderer::render_strokes(
    const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    if (!gl_initialized) {
        init_gl();
        gl_initialized = true;
    }

    // Offscreen texture init
    const Imath::V2i offscreen_resolution =
        calculate_viewport_size(transform_window_to_viewport_space);
    offscreen_renderer_->resize(offscreen_resolution);

    // strokes are made up of partially overlapping triangles - as we
    // draw with opacity we use depth test to stop overlapping triangles
    // in the same stroke accumulating in the alpha blend
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // Avoid clipping
    glDisable(GL_SCISSOR_TEST);

    glClear(GL_DEPTH_BUFFER_BIT);

    // Erase strokes are rendered directly to the main framebuffer's depth
    // buffer (not inside offscreen begin/end). We only write to the depth
    // buffer here; later, when drawing visible strokes, the depth test
    // causes the 'erase' to work.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glBindVertexArray(stroke_vao_);

    render_erase_strokes(
        strokes,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dx);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Pre-build all visible stroke vertex data into a single buffer and
    // upload once, then draw each stroke at its offset. This avoids
    // re-uploading the VBO per stroke.
    struct StrokeInfo {
        std::shared_ptr<xstudio::ui::canvas::Stroke> stroke;
        int offset_verts;
        int n_verts;
        float depth;
    };

    std::vector<Imath::V2f> combined_vertex_data;
    std::vector<StrokeInfo> stroke_infos;
    float depth = 0.0f;

    for (const auto &stroke : strokes) {

        depth += 0.001;

        if (stroke->type() == StrokeType_Erase)
            continue;
        if (stroke->points().empty())
            continue;

        const int offset_verts = static_cast<int>(combined_vertex_data.size()) / 4;
        const int n_segments   = vertex_array_filler(stroke, combined_vertex_data);
        const int n_verts      = n_segments * 6;

        stroke_infos.push_back({stroke, offset_verts, n_verts, depth});
    }

    if (stroke_infos.empty())
        return;

    // Single VBO upload for all visible strokes
    glBindVertexArray(stroke_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        combined_vertex_data.size() * sizeof(Imath::V2f),
        combined_vertex_data.data(),
        GL_STREAM_DRAW);
    float *ptr = nullptr;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);

    // Pass 1 uses identity for to_canvas so the stroke fills the entire
    // offscreen texture at full resolution. Pass 2 uses the real
    // transform_window_to_viewport_space for correct screen positioning.
    // The offscreen vertex shader computes frag_pos without to_canvas,
    // so the NDC->UV mapping matches between passes.
    auto identity = Imath::M44f();
    identity.makeIdentity();

    // Save viewport for restoration after offscreen pass
    std::array<int, 4> saved_viewport;
    glGetIntegerv(GL_VIEWPORT, saved_viewport.data());

    for (const auto &info : stroke_infos) {

        // Pass 1: render stroke mask to offscreen FBO via MAX blend
        render_single_stroke_pass1(
            info.stroke,
            identity,
            transform_viewport_to_image_space,
            viewport_du_dx,
            info.depth,
            info.offset_verts,
            info.n_verts);

        // Restore viewport after offscreen rendering may have changed it
        glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);

        // Pass 2: composite to screen (blend mode varies by type)
        if (info.stroke->blend_mode()) {
            render_effect_stroke_pass2(
                info.stroke,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dx,
                info.depth,
                info.offset_verts,
                info.n_verts);
        } else {
            render_single_stroke_pass2(
                info.stroke,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dx,
                info.depth,
                info.offset_verts,
                info.n_verts);
        }
    }
}


Imath::V2i OpenGLStrokeRenderer::calculate_viewport_size(
    const Imath::M44f &transform_window_to_viewport_space) {
    // the gl viewport corresponds to the parent window size.
    std::array<int, 4> gl_viewport;
    glGetIntegerv(GL_VIEWPORT, gl_viewport.data());

    // store the overall window size
    Imath::V2f window_size(gl_viewport[2], gl_viewport[3]);

    // Our ref coord system maps -1.0, -1.0 to the bottom left of the viewport and
    // 1.0, 1.0 to the top right
    Imath::V4f botomleft(-1.0, -1.0, 0.0f, 1.0f);
    Imath::V4f topright(1.0, 1.0, 0.0f, 1.0f);

    topright  = topright * transform_window_to_viewport_space;
    botomleft = botomleft * transform_window_to_viewport_space;

    // Now we convert to normalized values, which is a bit easier to understand
    // so that bottom left pixel in the window is at (0.0, 0.0) and top right
    // pixel in the window is (1.0, 1.0)
    botomleft.x = (botomleft.x + 1.0) / 2.0;
    botomleft.y = (botomleft.y + 1.0) / 2.0;
    topright.x  = (topright.x + 1.0) / 2.0;
    topright.y  = (topright.y + 1.0) / 2.0;

    // Now convert to window pixels like this.
    botomleft.x = botomleft.x * float(window_size.x);
    botomleft.y = botomleft.y * float(window_size.y);
    topright.x  = topright.x * float(window_size.x);
    topright.y  = topright.y * float(window_size.y);

    const int viewport_width  = (int)round(topright.x - botomleft.x);
    const int viewport_height = (int)round(topright.y - botomleft.y);

    return {viewport_width, viewport_height};
}

float OpenGLStrokeRenderer::get_soft_edge(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke, float viewport_du_dx) {

    float soft_edge     = stroke->thickness() * stroke->softness();
    float min_antialias = viewport_du_dx * 4.0;

    return soft_edge < min_antialias ? min_antialias : soft_edge;
}
