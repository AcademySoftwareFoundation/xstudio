// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_stroke_renderer.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;

namespace {

const char *vertex_shader = R"(
    #version 410 core
    uniform float z_adjust;
    uniform float thickness;
    uniform float soft_dim;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    layout (location = 0) in vec2 lstart;
    layout (location = 1) in vec2 lend;
    flat out vec2 line_start;
    flat out vec2 line_end;
    out vec2 frag_pos;
    out float soft_edge;
    uniform bool do_soft_edge;
    uniform int offset_into_points;

    void main()
    {
        // 6 verts are needed for each line segment - we are drawing a rectangle
        // that bounds the line and is 'thickness' wide and thickness longer
        // than the line segment

        int i = gl_VertexID%6;
        vec2 vtx;
        float quad_thickness = thickness + (do_soft_edge ? soft_dim : 0.00001f);
        float zz = z_adjust - (do_soft_edge ? 0.0005 : 0.0);

        line_start = lstart; // start point of the line
        line_end = lend; // end point of the line

        vec2 v = (line_end == line_start ? vec2(1.0, 0.0) : normalize(line_end-line_start))*quad_thickness; // vector between the two vertices
        vec2 tr = vec2(v.y,-v.x); // tangent

        if (i == 0) {
            vtx = line_start-v+tr;
        } else if (i == 1) {
            vtx = line_end+v+tr;
        } else if (i == 2) {
            vtx = line_end+v-tr;
        } else if (i == 3) {
            vtx = line_end+v-tr;
        } else if (i == 4) {
            vtx = line_start-v-tr;
        } else if (i == 5) {
            vtx = line_start-v+tr;
        }

        soft_edge = (do_soft_edge ? soft_dim : 0.00001f);
        gl_Position = vec4(vtx,0.0,1.0)*to_coord_system*to_canvas;
        gl_Position.z = (zz)*gl_Position.w;
        frag_pos = vtx;
    }
)";

const char *frag_shader = R"(
    #version 330 core
    flat in vec2 line_start;
    flat in vec2 line_end;
    in vec2 frag_pos;
    out vec4 FragColor;
    uniform vec3 brush_colour;
    uniform float brush_opacity;
    in float soft_edge;
    uniform float thickness;
    uniform bool do_soft_edge;

    float distToLine(vec2 pt)
    {

        float l2 = (line_end.x - line_start.x)*(line_end.x - line_start.x) +
            (line_end.y - line_start.y)*(line_end.y - line_start.y);

        if (l2 == 0.0) return length(pt-line_start);

        vec2 a = pt-line_start;
        vec2 L = line_end-line_start;

        float dot = (a.x*L.x + a.y*L.y);

        float t = max(0.0, min(1.0, dot / l2));
        vec2 p = line_start + t*L;
        return length(pt-p);

    }

    void main(void)
    {
        float r = distToLine(frag_pos);

        if (do_soft_edge) {
            r = smoothstep(
                    thickness + soft_edge,
                    thickness,
                    r);
        } else {
            r = r < thickness ? 1.0f: 0.0f;
        }

        if (r == 0.0f) discard;
        if (do_soft_edge && r == 1.0f) {
            discard;
        }
        float a = brush_opacity*r;
        FragColor = vec4(
            brush_colour*a,
            a
            );

    }
)";

} // anonymous namespace


OpenGLStrokeRenderer::~OpenGLStrokeRenderer() { cleanup_gl(); }

void OpenGLStrokeRenderer::init_gl() {

    if (!shader_) {
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
        glGenBuffers(1, &vbo_);
        glGenVertexArrays(1, &vao_);
    }
}

void OpenGLStrokeRenderer::cleanup_gl() {

    if (shader_) {
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
        shader_.reset();
    }
}

void OpenGLStrokeRenderer::render_strokes(
    const std::vector<Stroke> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    bool have_alpha_buffer) {

    if (!shader_)
        init_gl();

    std::vector<Imath::V2f> line_start_end_per_vertex;
    std::vector<int> n_vtx_per_stroke;

    for (const auto &stroke : strokes) {
        const Imath::V2f *v0 = stroke.points.data();
        const Imath::V2f *v1 = v0;
        if (stroke.points.size() > 1)
            v1++;
        const int n_segments = std::max(size_t(1), (stroke.points.size() - 1));
        n_vtx_per_stroke.push_back(n_segments * 6);

        for (int i = 0; i < n_segments; ++i) {
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            line_start_end_per_vertex.push_back(*v0);
            line_start_end_per_vertex.push_back(*v1);
            v0++;
            v1++;
        }
    }

    glBindVertexArray(vao_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        line_start_end_per_vertex.size() * sizeof(Imath::V2f),
        line_start_end_per_vertex.data(),
        GL_STREAM_DRAW);
    float *ptr = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)ptr);
    ptr += 2;
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)ptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // strokes are made up of partially overlapping triangles - as we
    // draw with opacity we use depth test to stop overlapping triangles
    // in the same stroke accumulating in the alpha blend
    glEnable(GL_DEPTH_TEST);
    glClearDepth(0.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    utility::JsonStore shader_params;
    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["soft_dim"]        = viewport_du_dx * 4.0f;

    shader_->use();
    shader_->set_shader_parameters(shader_params);

    utility::JsonStore shader_params2;
    utility::JsonStore shader_params3;
    shader_params3["do_soft_edge"] = true;

    GLint offset = 0;
    float depth  = 0.0f;

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Here we draw the erase strokes. Essentially we just draw into the
    // depth buffer, nothing goes into the frag buffer.
    // Later when we draw visible strokes the depth test will cause the
    // 'erase' to work.
    glDepthFunc(GL_GREATER);
    auto p_n_vtx_per_stroke = n_vtx_per_stroke.begin();
    for (const auto &stroke : strokes) {
        depth += 0.001;
        if (stroke.type != StrokeType_Erase) {
            offset += *p_n_vtx_per_stroke;
            p_n_vtx_per_stroke++;
            ;
            continue;
        }
        shader_params2["z_adjust"]      = depth;
        shader_params2["brush_colour"]  = stroke.colour;
        shader_params2["brush_opacity"] = 0.0f;
        shader_params2["thickness"]     = stroke.thickness;
        shader_params2["do_soft_edge"]  = false;
        shader_->set_shader_parameters(shader_params2);

        glDrawArrays(GL_TRIANGLES, offset, *p_n_vtx_per_stroke);
        offset += *p_n_vtx_per_stroke;
        p_n_vtx_per_stroke++;
    }

    offset             = 0;
    depth              = 0.0f;
    p_n_vtx_per_stroke = n_vtx_per_stroke.begin();
    for (const auto &stroke : strokes) {

        depth += 0.001;
        if (stroke.type == StrokeType_Erase) {
            // now we skip erase strokes
            offset += *p_n_vtx_per_stroke;
            p_n_vtx_per_stroke++;
            continue;
        }

        /* ---- First pass, draw solid stroke ---- */

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

        // It is not perfect because of the use of glBlendEquation(GL_MAX);
        // lower down when plotting the soft edge - this is because even the
        // soft edge plotting overlaps in an awkward way and you get bad artifacts
        // if you try other strategies ....
        // Drawing different, bright colours over each other where opacity is
        // not 1.0 shows up a subtle but noticeable flourescent glow effect.
        // Solutions on a postcard please!

        // so this prevents overlapping quads from same stroke accumulating together
        glDepthFunc(GL_GREATER);

        if (stroke.type == StrokeType_Erase) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        } else {
            glBlendEquation(GL_FUNC_ADD);
        }

        // set up the shader uniforms - strok thickness, colour etc
        shader_params2["z_adjust"]      = depth;
        shader_params2["brush_colour"]  = stroke.colour;
        shader_params2["brush_opacity"] = stroke.opacity;
        shader_params2["thickness"]     = stroke.thickness;
        shader_params2["do_soft_edge"]  = false;
        shader_->set_shader_parameters(shader_params2);

        // For each adjacent PAIR of points in a stroke, we draw a quad  of
        // the required thickness (rectangle) that connects them. We then draw a quad centered
        // over every point in the stroke of width & height matching the line
        // thickness to plot a circle that fills in the gaps left between the
        // rectangles we have already joined, giving rounded start and end caps
        // to the stroke and also rounded 'elbows' at angled joins.
        // The vertex shader computes the 4 vertices for each quad directly from
        // the stroke points and thickness

        glDrawArrays(GL_TRIANGLES, offset, *p_n_vtx_per_stroke);

        /* ---- Second pass, draw soft edged stroke underneath ---- */

        // Edge fragments have transparency and we want the most opaque fragment
        // to be plotted, we achieve this by letting them all plot
        glDepthFunc(GL_GEQUAL);

        if (stroke.type == StrokeType_Erase) {
            // glBlendEquation(GL_MAX);
        } else {
            glBlendEquation(GL_MAX);
        }

        shader_params3["do_soft_edge"] = true;
        shader_params3["soft_dim"] = viewport_du_dx * 4.0f + stroke.softness * stroke.thickness;
        shader_->set_shader_parameters(shader_params3);
        glDrawArrays(GL_TRIANGLES, offset, (stroke.points.size() - 1) * 6);

        offset += *p_n_vtx_per_stroke;
        *p_n_vtx_per_stroke++;
    }

    glBlendEquation(GL_FUNC_ADD);
    glBindVertexArray(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    shader_->stop_using();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}
