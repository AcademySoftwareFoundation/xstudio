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
    glBindVertexArray(0);

    // Offscreeen rendering
    static std::array<float, 24> vertices = {
        // positions   // texCoords
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    offscreen_renderer_ = std::make_unique<OpenGLOffscreenRenderer>(GL_RGBA8);
    offscreen_shader_   = std::make_unique<ui::opengl::GLShaderProgram>(
        offscreen_vertex_shader, offscreen_frag_shader);

    glGenVertexArrays(1, &offscreen_vao_);
    glGenBuffers(1, &offscreen_vbo_);
    glBindVertexArray(offscreen_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, offscreen_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void OpenGLStrokeRenderer::cleanup_gl() {

    if (stroke_shader_) {
        glDeleteBuffers(1, &stroke_vbo_);
        glDeleteVertexArrays(1, &stroke_vao_);
        stroke_shader_.reset();
    }
}


const int OpenGLStrokeRenderer::vertex_array_filler(
    const xstudio::ui::canvas::Stroke &stroke,
    std::vector<Imath::V2f> &line_start_end_per_vertex) {
    const Stroke::Point *v0 = stroke.points().data();
    const Stroke::Point *v1 = v0;

    if (stroke.points().size() > 1)
        v1++;

    int n_segments = std::max(size_t(1), (stroke.points().size() - 1));

    for (int i = 0; i < n_segments; ++i) {
        Imath::V2f pt_sz(v0->pressure, v1->pressure);
        for (int j = 0; j < 6; ++j) {
            line_start_end_per_vertex.push_back(v0->pos);
            line_start_end_per_vertex.push_back(v1->pos);
            line_start_end_per_vertex.push_back(pt_sz);
        }
        /*line_start_end_per_vertex.push_back(v0->pos);
        line_start_end_per_vertex.push_back(v1->pos);
        line_start_end_per_vertex.push_back(pt_sz);
        line_start_end_per_vertex.push_back(v0->pos);
        line_start_end_per_vertex.push_back(v1->pos);
        line_start_end_per_vertex.push_back(pt_sz);
        line_start_end_per_vertex.push_back(v0->pos);
        line_start_end_per_vertex.push_back(v1->pos);
        line_start_end_per_vertex.push_back(pt_sz);
        line_start_end_per_vertex.push_back(v0->pos);
        line_start_end_per_vertex.push_back(v1->pos);
        line_start_end_per_vertex.push_back(pt_sz);
        line_start_end_per_vertex.push_back(v0->pos);
        line_start_end_per_vertex.push_back(v1->pos);
        line_start_end_per_vertex.push_back(pt_sz);*/
        v0++;
        v1++;
    }

    return n_segments;
}

void OpenGLStrokeRenderer::render_erase_strokes(
    const std::vector<xstudio::ui::canvas::Stroke> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {
    std::vector<Imath::V2f> line_start_end_per_vertex;
    std::vector<int> n_vtx_per_stroke;

    for (const auto &stroke : strokes) {

        // Only render erase strokes here
        if (stroke.type() != StrokeType_Erase)
            continue;

        const int n_segments = vertex_array_filler(stroke, line_start_end_per_vertex);
        n_vtx_per_stroke.push_back(n_segments * 6);
    }

    // Prepare vertex objects
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        line_start_end_per_vertex.size() * sizeof(Imath::V2f),
        line_start_end_per_vertex.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);

    // Here we draw the erase strokes.
    // Essentially we just draw into the depth buffer, nothing goes into the frag buffer.
    // Later, when we draw visible strokes, the depth test will cause the 'erase' to work.
    GLint offset = 0;
    float depth  = 0.0f;

    utility::JsonStore shader_params2;

    auto p_n_vtx_per_stroke = n_vtx_per_stroke.begin();

    for (const auto &stroke : strokes) {

        depth += 0.001;

        // Only render erase strokes here
        if (stroke.type() != StrokeType_Erase) {
            continue;
        }

        utility::JsonStore shader_params;
        shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
        shader_params["to_canvas"]       = transform_window_to_viewport_space;
        shader_params["soft_edge"] =
            std::max(viewport_du_dx * 4.0f, stroke.thickness() * stroke.softness());
        // stroke.thickness * 0.1; //viewport_du_dx * 4.0f;
        shader_params["z_adjust"]            = depth;
        shader_params["brush_colour"]        = stroke.colour();
        shader_params["brush_opacity"]       = 0.0f;
        shader_params["thickness"]           = stroke.thickness();
        shader_params["size_sensitivity"]    = stroke.size_sensitivity();
        shader_params["opacity_sensitivity"] = stroke.opacity_sensitivity();
        stroke_shader_->set_shader_parameters(shader_params);

        glDrawArrays(GL_TRIANGLES, offset, *p_n_vtx_per_stroke);
        offset += *p_n_vtx_per_stroke;
        p_n_vtx_per_stroke++;
    }
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

        const int n_segments = vertex_array_filler(*stroke, line_start_end_per_vertex);
        n_vtx_per_stroke.push_back(n_segments * 6);
    }

    // Prepare vertex objects
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        line_start_end_per_vertex.size() * sizeof(Imath::V2f),
        line_start_end_per_vertex.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);

    // Here we draw the erase strokes.
    // Essentially we just draw into the depth buffer, nothing goes into the frag buffer.
    // Later, when we draw visible strokes, the depth test will cause the 'erase' to work.
    GLint offset = 0;
    float depth  = 0.0f;

    utility::JsonStore shader_params2;

    auto p_n_vtx_per_stroke = n_vtx_per_stroke.begin();

    for (const auto &stroke : strokes) {

        depth += 0.001;

        // Only render erase strokes here
        if (stroke->type() != StrokeType_Erase) {
            continue;
        }

        utility::JsonStore shader_params;
        shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
        shader_params["to_canvas"]       = transform_window_to_viewport_space;
        shader_params["soft_edge"]       = stroke->softness() == 0
                                               ? viewport_du_dx * 4.0f
                                               : stroke->thickness() * stroke->softness();
        // stroke.thickness * 0.1; //viewport_du_dx * 4.0f;
        shader_params["z_adjust"]            = depth;
        shader_params["thickness"]           = stroke->thickness();
        shader_params["size_sensitivity"]    = stroke->size_sensitivity();
        shader_params["opacity_sensitivity"] = stroke->opacity_sensitivity();
        stroke_shader_->set_shader_parameters(shader_params);

        glDrawArrays(GL_TRIANGLES, offset, *p_n_vtx_per_stroke);
        offset += *p_n_vtx_per_stroke;
        p_n_vtx_per_stroke++;
    }
}

void OpenGLStrokeRenderer::render_single_stroke(
    const xstudio::ui::canvas::Stroke &stroke,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float depth) {
    std::vector<Imath::V2f> line_start_end_per_vertex;

    const int n_segments = vertex_array_filler(stroke, line_start_end_per_vertex);

    // Prepare vertex objects
    glBindBuffer(GL_ARRAY_BUFFER, stroke_vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        line_start_end_per_vertex.size() * sizeof(Imath::V2f),
        line_start_end_per_vertex.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)ptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    stroke_shader_->use();

    // strokes are self-overlapping - we can't accumulate colour on the same pixel from
    // different segments of the same stroke, because if opacity is not 1.0
    // the strokes don't draw correctly so we must use depth-test to prevent
    // this.
    // Anti-aliasing the boundary is tricky as we don't want to put down
    // anti-alised edge pixels where there will be solid pixels due to some
    // other segment of the same stroke, or the depth test means we punch
    // little holes in the solid bit with anti-aliased edges where there
    // is self-overlapping
    // Thus we draw solid filled stroke (not anti-aliased) and then we
    // draw a slightly thicker stroke underneath (using depth test) and this
    // thick stroke has a slightly soft (fuzzy) edge that achieves anti-
    // aliasing.

    // so this prevents overlapping quads from same stroke accumulating together
    glBlendEquation(GL_MAX);

    // set up the shader uniforms - stroke thickness, colour etc
    utility::JsonStore shader_params;
    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["soft_edge"] =
        stroke.softness() == 0 ? viewport_du_dx * 4.0f : stroke.thickness() * stroke.softness();
    shader_params["z_adjust"]            = depth;
    shader_params["brush_colour"]        = stroke.colour();
    shader_params["brush_opacity"]       = stroke.opacity();
    shader_params["thickness"]           = stroke.thickness();
    shader_params["size_sensitivity"]    = stroke.size_sensitivity();
    shader_params["opacity_sensitivity"] = stroke.opacity_sensitivity();
    stroke_shader_->set_shader_parameters(shader_params);

    // For each adjacent PAIR of points in a stroke, we draw a quad  of
    // the required thickness (rectangle) that connects them. We then draw a quad centered
    // over every point in the stroke of width & height matching the line
    // thickness to plot a circle that fills in the gaps left between the
    // rectangles we have already joined, giving rounded start and end caps
    // to the stroke and also rounded 'elbows' at angled joins.
    // The vertex shader computes the 4 vertices for each quad directly from
    // the stroke points and thickness

    glDrawArrays(GL_TRIANGLES, 0, n_segments * 6);

    stroke_shader_->stop_using();
}

void OpenGLStrokeRenderer::render_strokes(
    const std::vector<Stroke> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                      "OpenGLStrokeRenderer::render_strokes START");

    if (!gl_initialized) {
        init_gl();
        gl_initialized = true;
    }

    // Offscreen texture init
    const Imath::V2i offscreen_resolution =
        calculate_viewport_size(transform_window_to_viewport_space);
    offscreen_renderer_->resize(offscreen_resolution);

    auto identity = Imath::M44f();
    identity.makeIdentity();

    // strokes are made up of partially overlapping triangles - as we
    // draw with opacity we use depth test to stop overlapping triangles
    // in the same stroke accumulating in the alpha blend
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glClearDepth(0.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Avoid clipping
    glDisable(GL_SCISSOR_TEST);

    // Start rendering in the offscreen texture
    offscreen_renderer_->begin();

    // This prevents writing to the color texture
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glClear(GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(stroke_vao_);

    render_erase_strokes(strokes, identity, transform_viewport_to_image_space, viewport_du_dx);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // End rendering on offscreen texture
    offscreen_renderer_->end();

    float depth = 0.0f;

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                      "OpenGLStrokeRenderer::render_strokes BEFORE LOOP");

    for (const auto &stroke : strokes) {

        // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
        //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
        //                      "OpenGLStrokeRenderer::render_strokes INIT LOOP");

        depth += 0.001;

        if (stroke.type() == StrokeType_Erase) {
            continue;
        }

        // To use the depth buffer filled by the erase brush
        glEnable(GL_DEPTH_TEST);

        // Avoid clipping
        glDisable(GL_SCISSOR_TEST);

        // Start rendering in the offscreen texture
        offscreen_renderer_->begin();

        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(stroke_vao_);

        render_single_stroke(
            stroke, identity, transform_viewport_to_image_space, viewport_du_dx, depth);

        // End rendering on offscreen texture
        offscreen_renderer_->end();

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        // Start rendering offscreen texture to default framebuffer
        glBlendEquation(GL_FUNC_ADD);

        render_offscreen_texture(transform_window_to_viewport_space, stroke.colour());

        // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
        //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
        //                      "OpenGLStrokeRenderer::render_strokes LOOP NEXT");
    }

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                     GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                     "OpenGLStrokeRenderer::render_strokes END");
}

void OpenGLStrokeRenderer::render_strokes(
    const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                      "OpenGLStrokeRenderer::render_strokes START");

    if (!gl_initialized) {
        init_gl();
        gl_initialized = true;
    }

    // Offscreen texture init
    const Imath::V2i offscreen_resolution =
        calculate_viewport_size(transform_window_to_viewport_space);
    offscreen_renderer_->resize(offscreen_resolution);

    auto identity = Imath::M44f();
    identity.makeIdentity();

    // strokes are made up of partially overlapping triangles - as we
    // draw with opacity we use depth test to stop overlapping triangles
    // in the same stroke accumulating in the alpha blend
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glClearDepth(0.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Avoid clipping
    glDisable(GL_SCISSOR_TEST);

    // Start rendering in the offscreen texture
    offscreen_renderer_->begin();

    // This prevents writing to the color texture
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glClear(GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(stroke_vao_);

    render_erase_strokes(strokes, identity, transform_viewport_to_image_space, viewport_du_dx);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // End rendering on offscreen texture
    offscreen_renderer_->end();

    float depth = 0.0f;

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                      "OpenGLStrokeRenderer::render_strokes BEFORE LOOP");

    for (const auto &stroke : strokes) {

        // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
        //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
        //                      "OpenGLStrokeRenderer::render_strokes INIT LOOP");

        depth += 0.001;

        if (stroke->type() == StrokeType_Erase) {
            continue;
        }

        // To use the depth buffer filled by the erase brush
        glEnable(GL_DEPTH_TEST);

        // Avoid clipping
        glDisable(GL_SCISSOR_TEST);

        // Start rendering in the offscreen texture
        offscreen_renderer_->begin();

        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(stroke_vao_);

        render_single_stroke(
            *stroke, identity, transform_viewport_to_image_space, viewport_du_dx, depth);

        // End rendering on offscreen texture
        offscreen_renderer_->end();

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        // Start rendering offscreen texture to default framebuffer
        glBlendEquation(GL_FUNC_ADD);

        render_offscreen_texture(transform_window_to_viewport_space, stroke->colour());

        // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
        //                      GL_DEBUG_SEVERITY_NOTIFICATION, -1,
        //                      "OpenGLStrokeRenderer::render_strokes LOOP NEXT");
    }

    // glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
    //                     GL_DEBUG_SEVERITY_NOTIFICATION, -1,
    //                     "OpenGLStrokeRenderer::render_strokes END");
}

void OpenGLStrokeRenderer::render_offscreen_texture(
    const Imath::M44f &transform_window_to_viewport_space,
    const utility::ColourTriplet &brush_colour) {
    offscreen_shader_->use();

    utility::JsonStore params;
    params["to_canvas"]        = transform_window_to_viewport_space;
    params["offscreenTexture"] = 11;
    params["brush_colour"]     = brush_colour;
    offscreen_shader_->set_shader_parameters(params);

    // Save current GL_ACTIVE_TEXTURE
    int active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

    // Offscreen rendered textures
    glActiveTexture(GL_TEXTURE11);

    glBindTexture(GL_TEXTURE_2D, offscreen_renderer_->texture_handle());

    // Restore saved Texture
    glActiveTexture(active_texture);

    glBindVertexArray(offscreen_vao_);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(stroke_vao_);

    offscreen_shader_->stop_using();
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

    // topright and bottomleft are now mapped to the position of the xstudio
    // viewport within the whole UI interface (the xstudio window). Our ref
    // coordinate system means the (-1.0,-1.0) is the bottom left of the
    // window, and (1.0, 1.0) is the top right.

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