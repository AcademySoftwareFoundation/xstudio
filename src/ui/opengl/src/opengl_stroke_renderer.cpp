// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_stroke_renderer.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;


namespace {

const char *vertex_shader = R"(
    #version 430 core
    #extension GL_ARB_shader_storage_buffer_object : require
    uniform float z_adjust;
    uniform float thickness;
    uniform float soft_dim;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    flat out vec2 line_start;
    flat out vec2 line_end;
    out vec2 frag_pos;
    out float soft_edge;
    uniform bool do_soft_edge;
    uniform int point_count;
    uniform int offset_into_points;

    layout (std430, binding = 1) buffer ssboObject {
        vec2 vtxs[];
    } ssboData;

    void main()
    {
        // We draw a thick line by plotting a quad that encloses the line that
        // joins two pen stroke vertices - we use a distance-to-line calculation
        // for the fragments within the quad and employ a smoothstep to draw
        // an anti-aliased 'sausage' shape that joins the two stroke vertices
        // with a circular join between each connected pair of vertices

        int v_idx = gl_VertexID/4;
        int i = gl_VertexID%4;
        vec2 vtx;
        float quad_thickness = thickness + (do_soft_edge ? soft_dim : 0.00001f);
        float zz = z_adjust - (do_soft_edge ? 0.0005 : 0.0);

        line_start = ssboData.vtxs[offset_into_points+v_idx].xy; // current vertex in stroke
        line_end = ssboData.vtxs[offset_into_points+1+v_idx].xy; // next vertex in stroke

        if (line_start == line_end) {
            // draw a quad centred on the line point
            if (i == 0) {
                vtx = line_start+vec2(-quad_thickness, -quad_thickness);
            } else if (i == 1) {
                vtx = line_start+vec2(-quad_thickness, quad_thickness);
            } else if (i == 2) {
                vtx = line_end+vec2(quad_thickness, quad_thickness);
            } else {
                vtx = line_end+vec2(quad_thickness, -quad_thickness);
            }
        } else {
            // draw a quad around the line segment 
            vec2 v = normalize(line_end-line_start); // vector between the two vertices
            vec2 tr = normalize(vec2(v.y,-v.x))*quad_thickness; // tangent

            // now we 'emit' one of four vertices to make a quad. We do it by adding
            // or subtracting the tangent to the line segment , depending of the
            // vertex index in the quad

            if (i == 0) {
                vtx = line_start-tr-v*quad_thickness;
            } else if (i == 1) {
                vtx = line_start+tr-v*quad_thickness;
            } else if (i == 2) {
                vtx = line_end+tr;
            } else {
                vtx = line_end-tr;
            }
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
    }

    if (!ssbo_id_) {
        glCreateBuffers(1, &ssbo_id_);
    }
}

void OpenGLStrokeRenderer::cleanup_gl() {

    if (ssbo_id_) {
        glDeleteBuffers(1, &ssbo_id_);
        ssbo_id_ = 0;
    }
}

void OpenGLStrokeRenderer::resize_ssbo(std::size_t size) {
    const auto next_power_of_2 =
        static_cast<std::size_t>(std::pow(2.0f, std::ceil(std::log2(size))));

    if (ssbo_size_ < next_power_of_2) {
        ssbo_size_ = next_power_of_2;
        glNamedBufferData(ssbo_id_, ssbo_size_, nullptr, GL_DYNAMIC_DRAW);
    }
}

void OpenGLStrokeRenderer::upload_ssbo(const std::vector<Imath::V2f> &points) {

    const std::size_t size = points.size() * sizeof(Imath::V2f);
    resize_ssbo(size);

    const char *data  = reinterpret_cast<const char *>(points.data());
    const size_t hash = std::hash<std::string_view>{}(std::string_view(data, size));

    if (ssbo_data_hash_ != hash) {
        ssbo_data_hash_ = hash;

        void *buf = glMapNamedBuffer(ssbo_id_, GL_WRITE_ONLY);
        memcpy(buf, points.data(), size);
        glUnmapNamedBuffer(ssbo_id_);
    }
}

void OpenGLStrokeRenderer::render_strokes(
    const std::vector<Stroke> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    bool have_alpha_buffer) {

    const bool do_erase_strokes_first = !have_alpha_buffer;

    if (!shader_)
        init_gl();

    std::vector<Imath::V2f> strokes_vertices;
    for (const auto &stroke : strokes) {
        auto vertices = stroke.vertices();
        strokes_vertices.insert(strokes_vertices.end(), vertices.begin(), vertices.end());
    }

    upload_ssbo(strokes_vertices);

    // Buffer binding point 1, see vertex shader
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_id_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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

    if (do_erase_strokes_first) {
        depth += 0.001;
        glDepthFunc(GL_GREATER);
        for (const auto &stroke : strokes) {
            if (stroke.type != StrokeType_Erase) {
                offset += (stroke.points.size() + 1);
                continue;
            }
            shader_params2["z_adjust"]           = depth;
            shader_params2["brush_colour"]       = stroke.colour;
            shader_params2["brush_opacity"]      = 0.0f;
            shader_params2["thickness"]          = stroke.thickness;
            shader_params2["do_soft_edge"]       = false;
            shader_params2["point_count"]        = stroke.points.size() + 1;
            shader_params2["offset_into_points"] = offset;
            shader_->set_shader_parameters(shader_params2);
            glDrawArrays(GL_QUADS, 0, stroke.points.size() * 4);
            offset += (stroke.points.size() + 1);
        }
    }
    offset = 0;
    depth  = 0.0f;
    for (const auto &stroke : strokes) {

        depth += 0.001;
        if (do_erase_strokes_first && stroke.type == StrokeType_Erase) {
            offset += (stroke.points.size() + 1);
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
        shader_params2["z_adjust"]           = depth;
        shader_params2["brush_colour"]       = stroke.colour;
        shader_params2["brush_opacity"]      = stroke.opacity;
        shader_params2["thickness"]          = stroke.thickness;
        shader_params2["do_soft_edge"]       = false;
        shader_params2["point_count"]        = stroke.points.size() + 1;
        shader_params2["offset_into_points"] = offset;
        shader_->set_shader_parameters(shader_params2);

        // For each adjacent PAIR of points in a stroke, we draw a quad  of
        // the required thickness (rectangle) that connects them. We then draw a quad centered
        // over every point in the stroke of width & height matching the line
        // thickness to plot a circle that fills in the gaps left between the
        // rectangles we have already joined, giving rounded start and end caps
        // to the stroke and also rounded 'elbows' at angled joins.
        // The vertex shader computes the 4 vertices for each quad directly from
        // the stroke points and thickness
        glDrawArrays(GL_QUADS, 0, stroke.points.size() * 4);

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
        glDrawArrays(GL_QUADS, 0, stroke.points.size() * 4);

        offset += (stroke.points.size() + 1);
    }

    glBlendEquation(GL_FUNC_ADD);
    glBindVertexArray(0);

    shader_->stop_using();
}