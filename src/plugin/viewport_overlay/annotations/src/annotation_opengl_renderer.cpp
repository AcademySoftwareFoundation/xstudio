// SPDX-License-Identifier: Apache-2.0
#include "annotation_opengl_renderer.hpp"

#include <memory>
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

namespace {
const char *thick_line_vertex_shader = R"(
    #version 430 core
    #extension GL_ARB_shader_storage_buffer_object : require
    out vec2 viewportCoordinate;
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
        viewportCoordinate = vtx;
        frag_pos = vtx;
    }
    )";

const char *thick_line_frag_shader = R"(
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

const char *text_handles_vertex_shader = R"(
    #version 430 core
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform vec2 box_position;
    uniform vec2 box_size;
    uniform vec2 aa_nudge;    
    uniform float du_dx;
    layout (location = 0) in vec2 aPos;
    //layout (location = 1) in vec2 bPos;
    out vec2 screen_pixel;

    void main()
    {

        // now we 'emit' one of four vertices to make a quad. We do it by adding
        // or subtracting the tangent to the line segment , depending of the
        // vertex index in the quad
        vec2 vertex_pos = aPos.xy;
        vertex_pos.x = vertex_pos.x*box_size.x;
        vertex_pos.y = vertex_pos.y*box_size.y;
        vertex_pos += box_position + aa_nudge*du_dx;
        screen_pixel = vertex_pos/du_dx;
        gl_Position = vec4(vertex_pos,0.0,1.0)*to_coord_system*to_canvas;
    }
    )";

const char *text_handles_frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform bool shadow;
    uniform int box_type;
    uniform float opacity;
    in vec2 screen_pixel;
    void main(void)
    {
        ivec2 offset_screen_pixel = ivec2(screen_pixel) + ivec2(5000,5000); // move away from origin
        if (box_type==1) {
            // draws a dotted line
            if (((offset_screen_pixel.x/20) & 1) == ((offset_screen_pixel.y/20) & 1)) {
                FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
            } else {
                FragColor = vec4(1.0f, 1.0f, 1.0f, opacity);
            }
        } else if (box_type==2) {
            FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
        } else if (box_type==3) {
            FragColor = vec4(0.7f, 0.7f, 0.7f, opacity);
        } else {
            FragColor = vec4(1.0f, 1.0f, 1.0f, opacity);
        }
            
    }

    )";

static struct AAJitterTable {

    struct {
        Imath::V2f operator()(int N, int i, int j) {
            auto x = -0.5f + (i + 0.5f) / N;
            auto y = -0.5f + (j + 0.5f) / N;
            return {x, y};
        }
    } gridLookup;

    AAJitterTable() {
        aa_nudge.resize(16);
        int lookup[16] = {11, 6, 10, 8, 9, 12, 7, 1, 3, 13, 5, 4, 2, 15, 0, 14};
        int ct         = 0;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                aa_nudge[lookup[ct]]["aa_nudge"] = gridLookup(4, i, j);
                ct++;
            }
        }
    }

    std::vector<utility::JsonStore> aa_nudge;

} aa_jitter_table;

} // namespace

void AnnotationsRenderer::render_opengl(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    if (!shader_)
        init_overlay_opengl();

    std::lock_guard<std::mutex> lock(immediate_data_gate_);
    utility::BlindDataObjectPtr render_data =
        frame.plugin_blind_data(utility::Uuid("46f386a0-cb9a-4820-8e99-fb53f6c019eb"));
    const auto *data = dynamic_cast<const AnnotationRenderDataSet *>(render_data.get());
    if (data) {
        for (const auto &p : *data) {
            render_annotation_to_screen(
                p,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                !have_alpha_buffer);
        }
        render_text_handles_to_screen(
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel);
        return;
    }
    if (current_edited_annotation_render_data_) {
        render_annotation_to_screen(
            current_edited_annotation_render_data_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            !have_alpha_buffer);
        render_text_handles_to_screen(
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel);
    }
}

void AnnotationsRenderer::render_annotation_to_screen(
    const AnnotationRenderDataPtr render_data,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const bool do_erase_strokes_first) {

    if (!render_data)
        return;

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
    shader_params["soft_dim"]        = viewport_du_dpixel * 4.0f;

    shader_->use();
    shader_->set_shader_parameters(shader_params);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);

    const auto sz = static_cast<unsigned int>(round(std::pow(
        2.0f,
        std::ceil(
            std::log(render_data->pen_stroke_vertices_.size() * sizeof(Imath::V2f)) /
            std::log(2.0f)))));

    if (sz > ssbo_size_) {

        ssbo_size_ = sz;
        glNamedBufferData(ssbo_id_, ssbo_size_, nullptr, GL_DYNAMIC_DRAW);
    }

    if (last_data_ != render_data->pen_stroke_vertices_.data()) {
        last_data_          = render_data->pen_stroke_vertices_.data();
        auto *buffer_io_ptr = (uint8_t *)glMapNamedBuffer(ssbo_id_, GL_WRITE_ONLY);
        memcpy(
            buffer_io_ptr,
            render_data->pen_stroke_vertices_.data(),
            render_data->pen_stroke_vertices_.size() * sizeof(Imath::V2f));
        glUnmapNamedBuffer(ssbo_id_);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_id_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    utility::JsonStore shader_params2;
    utility::JsonStore shader_params3;
    shader_params3["do_soft_edge"] = true;

    GLint offset = 0;

    if (do_erase_strokes_first) {
        glDepthFunc(GL_GREATER);
        for (const auto &stroke_info : render_data->stroke_info_) {
            if (!stroke_info.is_erase_stroke_) {
                offset += (stroke_info.stroke_point_count_);
                continue;
            }
            shader_params2["z_adjust"]           = stroke_info.stroke_depth_;
            shader_params2["brush_colour"]       = stroke_info.brush_colour_;
            shader_params2["brush_opacity"]      = 0.0f;
            shader_params2["thickness"]          = stroke_info.brush_thickness_;
            shader_params2["do_soft_edge"]       = false;
            shader_params2["point_count"]        = stroke_info.stroke_point_count_;
            shader_params2["offset_into_points"] = offset;
            shader_->set_shader_parameters(shader_params2);
            glDrawArrays(GL_QUADS, 0, (stroke_info.stroke_point_count_ - 1) * 4);
            offset += (stroke_info.stroke_point_count_);
        }
    }
    offset = 0;
    for (const auto &stroke_info : render_data->stroke_info_) {

        if (do_erase_strokes_first && stroke_info.is_erase_stroke_) {
            offset += (stroke_info.stroke_point_count_);
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

        if (stroke_info.is_erase_stroke_) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        } else {
            glBlendEquation(GL_FUNC_ADD);
        }

        // set up the shader uniforms - strok thickness, colour etc
        shader_params2["z_adjust"]           = stroke_info.stroke_depth_;
        shader_params2["brush_colour"]       = stroke_info.brush_colour_;
        shader_params2["brush_opacity"]      = stroke_info.brush_opacity_;
        shader_params2["thickness"]          = stroke_info.brush_thickness_;
        shader_params2["do_soft_edge"]       = false;
        shader_params2["point_count"]        = stroke_info.stroke_point_count_;
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
        glDrawArrays(GL_QUADS, 0, (stroke_info.stroke_point_count_ - 1) * 4);

        /* ---- Scond pass, draw soft edged stroke underneath ---- */

        // Edge fragments have transparency and we want the most opaque fragment
        // to be plotted, we achieve this by letting them all plot
        glDepthFunc(GL_GEQUAL);

        if (stroke_info.is_erase_stroke_) {
            // glBlendEquation(GL_MAX);
        } else {
            glBlendEquation(GL_MAX);
        }

        shader_params3["do_soft_edge"] = true;
        shader_->set_shader_parameters(shader_params3);
        glDrawArrays(GL_QUADS, 0, (stroke_info.stroke_point_count_ - 1) * 4);

        offset += (stroke_info.stroke_point_count_);
    }

    glBlendEquation(GL_FUNC_ADD);
    glBindVertexArray(0);

    shader_->stop_using();

    /* draw captions to screen */
    if (text_renderers_.size()) {
        for (const auto &caption_info : render_data->caption_info_) {

            auto p = text_renderers_.find(caption_info.font_name);
            auto text_renderer =
                (p == text_renderers_.end()) ? text_renderers_.begin()->second : p->second;

            text_renderer->render_text(
                caption_info.precomputed_vertex_buffer,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                caption_info.colour,
                viewport_du_dpixel,
                caption_info.text_size,
                caption_info.opacity);
        }
    }
}

void AnnotationsRenderer::render_text_handles_to_screen(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel) {

    utility::JsonStore shader_params;

    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["du_dx"]           = viewport_du_dpixel;
    shader_params["box_type"]        = 0;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    text_handles_shader_->use();
    text_handles_shader_->set_shader_parameters(shader_params);

    utility::JsonStore shader_params2;

    if (!current_caption_bdb_.isEmpty()) {

        // draw the box around the current edited caption
        shader_params2["box_position"] = current_caption_bdb_.min;
        shader_params2["box_size"]     = current_caption_bdb_.size();
        shader_params2["opacity"]      = 0.6;
        shader_params2["box_type"]     = 1;
        shader_params2["aa_nudge"]     = Imath::V2f(0.0f, 0.0f);


        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(handles_vertex_array_);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);

        const auto handle_size = Annotation::captionHandleSize * viewport_du_dpixel;

        // Draw the three
        static const auto hndls = std::vector<Caption::HoverState>(
            {Caption::HoveredOnMoveHandle,
             Caption::HoveredOnResizeHandle,
             Caption::HoveredOnDeleteHandle});

        static const auto vtx_offsets = std::vector<int>({4, 14, 24});
        static const auto vtx_counts  = std::vector<int>({20, 10, 4});

        const auto positions = std::vector<Imath::V2f>(
            {current_caption_bdb_.min - handle_size,
             current_caption_bdb_.max,
             {current_caption_bdb_.max.x, current_caption_bdb_.min.y - handle_size.y}});

        shader_params2["box_size"] = handle_size;

        glBindVertexArray(handles_vertex_array_);

        // draw a grey box for each handle
        shader_params2["opacity"] = 0.6f;
        for (size_t i = 0; i < hndls.size(); ++i) {
            shader_params2["box_position"] = positions[i];
            shader_params2["box_type"]     = 2;
            text_handles_shader_->set_shader_parameters(shader_params2);
            glDrawArrays(GL_QUADS, 0, 4);
        }

        static const auto aa_jitter = std::vector<Imath::V2f>(
            {{-0.33f, -0.33f},
             {-0.0f, -0.33f},
             {0.33f, -0.33f},
             {-0.33f, 0.0f},
             {0.0f, 0.0f},
             {0.33f, 0.0f},
             {-0.33f, 0.33f},
             {0.0f, 0.33f},
             {0.33f, 0.33f}});


        shader_params2["box_size"] = handle_size * 0.8f;
        // draw the lines for each handle
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        shader_params2["opacity"] = 1.0f / 16.0f;
        for (size_t i = 0; i < hndls.size(); ++i) {
            shader_params2["box_position"] = positions[i] + 0.1f * handle_size;
            shader_params2["box_type"]     = caption_hover_state_ == hndls[i] ? 4 : 3;
            text_handles_shader_->set_shader_parameters(shader_params2);
            // plot it 9 times with anti-aliasing jitter to get a better looking
            // result
            utility::JsonStore param;
            for (const auto &aa_nudge : aa_jitter_table.aa_nudge) {
                text_handles_shader_->set_shader_parameters(aa_nudge);
                glDrawArrays(GL_LINES, vtx_offsets[i], vtx_counts[i]);
            }
        }
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!under_mouse_caption_bdb_.isEmpty()) {
        shader_params2["box_position"] = under_mouse_caption_bdb_.min;
        shader_params2["box_size"]     = under_mouse_caption_bdb_.size();
        shader_params2["opacity"]      = 0.3;
        shader_params2["box_type"]     = 1;

        text_handles_shader_->set_shader_parameters(shader_params2);

        glBindVertexArray(handles_vertex_array_);

        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    if (cursor_position_[0] != Imath::V2f(0.0f, 0.0f)) {
        shader_params2["opacity"]      = 0.6f;
        shader_params2["box_position"] = cursor_position_[0];
        shader_params2["box_size"]     = cursor_position_[1] - cursor_position_[0];
        shader_params2["box_type"]     = text_cursor_blink_state_ ? 2 : 0;
        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(handles_vertex_array_);
        glLineWidth(3.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    glBindVertexArray(0);
}

void AnnotationsRenderer::init_overlay_opengl() {

    glGenBuffers(1, &ssbo_id_);

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(
        thick_line_vertex_shader, thick_line_frag_shader);

    text_handles_shader_ = std::make_unique<ui::opengl::GLShaderProgram>(
        text_handles_vertex_shader, text_handles_frag_shader);

    auto font_files = Fonts::available_fonts();
    for (const auto &f : font_files) {
        try {
            auto font = new ui::opengl::OpenGLTextRendererSDF(f.second, 96);
            text_renderers_[f.first].reset(font);
        } catch (std::exception &e) {
            spdlog::warn("Failed to load font: {}.", e.what());
        }
    }

    init_caption_handles_graphics();
}

void AnnotationsRenderer::init_caption_handles_graphics() {

    glGenBuffers(1, &handles_vertex_buffer_obj_);
    glGenVertexArrays(1, &handles_vertex_array_);

    static std::array<Imath::V2f, 38> handles_vertices = {

        // unit box for drawing boxes!
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(1.0f, 0.0f),
        Imath::V2f(1.0f, 1.0f),
        Imath::V2f(0.0f, 1.0f),

        // double headed arrow, vertical
        Imath::V2f(0.5f, 0.0f),
        Imath::V2f(0.5f, 1.0f),

        Imath::V2f(0.5f, 0.0f),
        Imath::V2f(0.5f - 0.2f, 0.2f),

        Imath::V2f(0.5f, 0.0f),
        Imath::V2f(0.5f + 0.2f, 0.2f),

        Imath::V2f(0.5f, 1.0f),
        Imath::V2f(0.5f - 0.2f, 1.0f - 0.2f),

        Imath::V2f(0.5f, 1.0f),
        Imath::V2f(0.5f + 0.2f, 1.0f - 0.2f),

        // double headed arrow, horizontal
        Imath::V2f(0.0f, 0.5f),
        Imath::V2f(1.0f, 0.5f),

        Imath::V2f(0.0f, 0.5f),
        Imath::V2f(0.2f, 0.5f - 0.2f),

        Imath::V2f(0.0f, 0.5f),
        Imath::V2f(0.2f, 0.5f + 0.2f),

        Imath::V2f(1.0f, 0.5f),
        Imath::V2f(1.0f - 0.2f, 0.5f - 0.2f),

        Imath::V2f(1.0f, 0.5f),
        Imath::V2f(1.0f - 0.2f, 0.5f + 0.2f),

        // crossed lines
        Imath::V2f(0.2f, 0.2f),
        Imath::V2f(0.8f, 0.8f),
        Imath::V2f(0.8f, 0.2f),
        Imath::V2f(0.2f, 0.8f),

    };

    glBindVertexArray(handles_vertex_array_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, handles_vertex_buffer_obj_);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(handles_vertices), handles_vertices.data(), GL_STATIC_DRAW);
    // 3. then set our vertex module pointers
    glEnableVertexAttribArray(0);
    // glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Imath::V2f), nullptr);
    // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 2*sizeof(Imath::V2f), (void
    // *)sizeof(Imath::V2f));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
