// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_stroke_renderer.hpp"

#include "opengl_stroke_shaders.glsl"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;


OpenGLStrokeRenderer::~OpenGLStrokeRenderer() { cleanup_gl(); }

void OpenGLStrokeRenderer::init_gl() {

    // Stroke rendering
    stroke_shader_ =
        std::make_unique<ui::opengl::GLShaderProgram>(stroke_vertex_shader, stroke_frag_shader);
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
}

void OpenGLStrokeRenderer::cleanup_gl() {

    if (stroke_shader_) {
        glDeleteBuffers(1, &stroke_vbo_);
        glDeleteVertexArrays(1, &stroke_vao_);
        stroke_shader_.reset();
    }
}

const int OpenGLStrokeRenderer::vertex_array_filler(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke,
    std::vector<Imath::V2f> &line_start_end_per_vertex) {

    const Stroke::Point *v0 = stroke->points().data();
    const Stroke::Point *v1 = v0;

    if (stroke->points().size() > 1)
        v1++;

    int n_segments = std::max(size_t(1), (stroke->points().size() - 1));

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

    for (const auto stroke : strokes) {

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
    float *ptr = 0;
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
    GLint offset = 0;
    float depth  = 0.0f;

    auto p_n_vtx_per_stroke = n_vtx_per_stroke.begin();

    for (const auto stroke : strokes) {

        depth += 0.001;

        // Only render erase strokes here
        if (stroke->type() != StrokeType_Erase) {
            continue;
        }

        utility::JsonStore shader_params;
        shader_params["to_coord_system"]     = transform_viewport_to_image_space.inverse();
        shader_params["to_canvas"]           = transform_window_to_viewport_space;
        shader_params["soft_edge"]           = get_soft_edge(stroke, viewport_du_dx);
        shader_params["z_adjust"]            = depth;
        shader_params["brush_opacity"]       = 1.0f;
        shader_params["thickness"]           = stroke->thickness();
        shader_params["size_sensitivity"]    = 0.0f;
        shader_params["opacity_sensitivity"] = 0.0f;
        shader_params["just_black"]          = 2.0f;
        stroke_shader_->set_shader_parameters(shader_params);

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
    const std::vector<Imath::V2f> &vertex_data,
    int n_segments) {

    // Begin rendering to offscreen texture
    offscreen_renderer_->begin();

    glBindVertexArray(stroke_vao_);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertex_data.size() * sizeof(Imath::V2f),
        vertex_data.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);

    stroke_shader_->use();

    utility::JsonStore shader_params;
    shader_params["to_coord_system"]     = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]           = transform_window_to_viewport_space;
    shader_params["soft_edge"]           = get_soft_edge(stroke, viewport_du_dx);
    shader_params["z_adjust"]            = depth;
    shader_params["brush_opacity"]       = stroke->opacity();
    shader_params["thickness"]           = stroke->thickness();
    shader_params["size_sensitivity"]    = stroke->size_sensitivity();
    shader_params["opacity_sensitivity"] = stroke->opacity_sensitivity();
    shader_params["just_black"]          = 1.0f; // renders stroke quads as black
    stroke_shader_->set_shader_parameters(shader_params);

    // 1st sub-pass: no blending, no depth test. We render black to the whole
    // area of the stroke. This replaces the previous glClear(GL_COLOR_BUFFER_BIT)
    // by only clearing pixels within the stroke footprint.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, n_segments * 6);

    // 2nd sub-pass: enable blending with MAX equation. Stroke sections are
    // rendered with overlapping 'sausages' but where they overlap we don't
    // want accumulation of colour into the fragment - we just want the
    // maximum alpha to be rendered to the fragment.
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);

    utility::JsonStore params2;
    params2["just_black"] = 0.0f;
    stroke_shader_->set_shader_parameters(params2);

    glDrawArrays(GL_TRIANGLES, 0, n_segments * 6);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
    const std::vector<Imath::V2f> &vertex_data,
    int n_segments) {

    glBindVertexArray(stroke_vao_);

    // Upload vertex data (same stroke geometry as pass 1)
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertex_data.size() * sizeof(Imath::V2f),
        vertex_data.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)ptr);

    offscreen_shader_->use();

    // The offscreen shader now receives stroke uniforms plus the offscreen
    // texture, and renders the stroke geometry (not a fullscreen quad).
    // This is more efficient as it only composites pixels where the stroke
    // actually exists.
    utility::JsonStore shader_params;
    shader_params["to_coord_system"]     = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]           = transform_window_to_viewport_space;
    shader_params["soft_edge"]           = get_soft_edge(stroke, viewport_du_dx);
    shader_params["z_adjust"]            = depth;
    shader_params["brush_colour"]        = stroke->colour();
    shader_params["brush_opacity"]       = stroke->opacity();
    shader_params["thickness"]           = stroke->thickness();
    shader_params["size_sensitivity"]    = stroke->size_sensitivity();
    shader_params["opacity_sensitivity"] = stroke->opacity_sensitivity();
    shader_params["offscreenTexture"]    = 11;
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

    glDrawArrays(GL_TRIANGLES, 0, n_segments * 6);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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

    // Render each non-erase stroke using two-pass approach:
    // Pass 1: render to offscreen (black fill + MAX blend alpha)
    // Pass 2: composite to screen using stroke geometry + offscreen texture
    float depth = 0.0f;

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

    for (const auto stroke : strokes) {

        depth += 0.001;

        if (stroke->type() == StrokeType_Erase) {
            continue;
        }

        // Build vertex data once, share between both passes
        std::vector<Imath::V2f> line_start_end_per_vertex;
        const int n_segments = vertex_array_filler(stroke, line_start_end_per_vertex);

        // Pass 1: render stroke to offscreen texture with identity for
        // to_canvas — stroke fills the full FBO at full resolution.
        render_single_stroke_pass1(
            stroke,
            identity,
            transform_viewport_to_image_space,
            viewport_du_dx,
            depth,
            line_start_end_per_vertex,
            n_segments);

        // Restore viewport after offscreen rendering may have changed it
        glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);

        // Pass 2: composite to screen using stroke geometry + offscreen
        // texture. Uses transform_window_to_viewport_space for correct
        // screen positioning. The offscreen vertex shader computes
        // frag_pos without to_canvas so UV lookup matches pass 1.
        render_single_stroke_pass2(
            stroke,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dx,
            depth,
            line_start_end_per_vertex,
            n_segments);
    }
}


const Imath::V2i OpenGLStrokeRenderer::calculate_viewport_size(
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

    return Imath::V2i(viewport_width, viewport_height);
}

float OpenGLStrokeRenderer::get_soft_edge(
    const std::shared_ptr<xstudio::ui::canvas::Stroke> stroke, float viewport_du_dx) {

    float soft_edge     = stroke->thickness() * stroke->softness();
    float min_antialias = viewport_du_dx * 4.0;

    return soft_edge < min_antialias ? min_antialias : soft_edge;

    // return stroke->softness() == 0 ? viewport_du_dx * 4.0f : stroke->thickness() *
    // stroke->softness();
}