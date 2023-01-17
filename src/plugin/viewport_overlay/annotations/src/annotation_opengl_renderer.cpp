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
    flat out int plot_circle;
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
        if (v_idx >= (point_count-1)) {
            plot_circle = 1;
            v_idx = v_idx-point_count+1;
        } else {
            plot_circle = 0;
        }
        int i = gl_VertexID%4;
        vec2 vtx;
        float soft_size = 0.0f;
        float zz = z_adjust;

        if (do_soft_edge) {
            zz -= 0.0005;
            soft_size = soft_dim;
        } else {
            soft_size = 0.00001f;
        }

        if (plot_circle == 1) {
            line_start = ssboData.vtxs[offset_into_points+v_idx].xy-vec2((thickness + soft_size),0.0);
            line_end = ssboData.vtxs[offset_into_points+v_idx].xy+vec2((thickness + soft_size),0.0);
        } else {
            line_start = ssboData.vtxs[offset_into_points+v_idx].xy; // current vertex in stroke
            line_end = ssboData.vtxs[offset_into_points+1+v_idx].xy; // next vertex in stroke
        }

        vec2 v = normalize(line_end-line_start); // vector between the two vertices
        vec2 tr = normalize(vec2(v.y,-v.x))*(thickness + soft_size); // tangent

        // now we 'emit' one of four vertices to make a quad. We do it by adding
        // or subtracting the tangent to the line segment , depending of the
        // vertex index in the quad

        if (i == 0) {
            vtx = line_start-tr;
        } else if (i == 1) {
            vtx = line_start+tr;
        } else if (i == 2) {
            vtx = line_end+tr;
        } else {
            vtx = line_end-tr;
        }

        soft_edge = soft_size;
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
    flat in int plot_circle;

    float distToLine(vec2 pt)
    {

        if (line_start == line_end) return length(pt-line_start);

        float t = (pt.x - line_start.x)*(line_end.x - line_start.x) +
            (pt.y - line_start.y)*(line_end.y - line_start.y);

        vec2 a = pt-line_start;
        vec2 L = (line_end-line_start);
        vec2 b = normalize(L);

        float dot = (a.x*b.x + a.y*b.y);

        vec2 np = line_start + b*dot;
        return length(pt - np);

    }

    void main(void)
    {
        float r = plot_circle == 1 ? length(frag_pos-(line_start+line_end)*0.5) : distToLine(frag_pos);

        if (do_soft_edge) {
            r = smoothstep(
                    thickness + soft_edge,
                    thickness,
                    r);
        } else if (plot_circle == 1) {
            r = r < thickness ? 1.0f: 0.0f;
        } else {
            r = 1.0f;
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
    uniform vec2 top_left;
    uniform vec2 bottom_right;
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
        vertex_pos.x = vertex_pos.x*(bottom_right.x-top_left.x);
        vertex_pos.y = vertex_pos.y*(bottom_right.y-top_left.y);
        vertex_pos += top_left;
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
        if (box_type==1 && ((offset_screen_pixel.x/20) & 1) == ((offset_screen_pixel.y/20) & 1))
            FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
        else if (box_type==2 && ((offset_screen_pixel.x/4) & 1) == ((offset_screen_pixel.y/4) & 1)) {
            FragColor = vec4(0.0f, 0.0f, 0.0f, opacity);
        } else {
            FragColor = vec4(1.0f, 1.0f, 1.0f, opacity);
        }

    }

    )";

} // namespace

void AnnotationsRenderer::render_opengl_before_image(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const xstudio::media_reader::ImageBufPtr &frame) {

    std::lock_guard<std::mutex> lock(immediate_data_gate_);
    utility::BlindDataObjectPtr render_data =
        frame.plugin_blind_data(utility::Uuid("46f386a0-cb9a-4820-8e99-fb53f6c019eb"));
    const auto *data = dynamic_cast<const AnnotationRenderDataSet *>(render_data.get());
    if (data) {
        for (const auto &p : *data) {
            render_annotation_to_screen(
                p, transform_window_to_viewport_space, transform_viewport_to_image_space);
            render_text_handles_to_screen(
                p, transform_window_to_viewport_space, transform_viewport_to_image_space);
        }
        return;
    }
    if (current_edited_annotation_render_data_) {
        render_annotation_to_screen(
            current_edited_annotation_render_data_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space);
        if (true) {
            render_text_handles_to_screen(
                current_edited_annotation_render_data_,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space);
        }
    }
}

void AnnotationsRenderer::render_annotation_to_screen(
    const AnnotationRenderDataPtr render_data,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space) {

    if (!render_data)
        return;

    if (!shader_)
        init_overlay_opengl();

    // strokes are made up of partially overlapping triangles - as we
    // draw with opacity we use depth test to stop overlapping triangles

    // in the same stroke accumulating in the alpha blend
    glEnable(GL_DEPTH_TEST);
    glClearDepth(0.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // this value tells us how much we are zoomed into the image in the viewport (in
    // the x dimension). If the image is width-fitted exactly to the viewport, then this
    // value will be 1.0 (what it means is the coordinates -1.0 to 1.0 are mapped to
    // the width of the viewport)
    const float image_zoom_in_viewport = transform_viewport_to_image_space[0][0];

    // this value gives us how much of the parent window is covered by the viewport.
    // So if the xstudio window is 1000px in width, and the viewport is 500px wide
    // (with the rest of the UI taking up the remainder) then this value will be 0.5
    const float viewport_x_size_in_window =
        transform_window_to_viewport_space[0][0] / transform_window_to_viewport_space[3][3];


    // the gl viewport corresponds to the parent window size.
    std::array<int, 4> gl_viewport;
    glGetIntegerv(GL_VIEWPORT, gl_viewport.data());
    const auto viewport_width = (float)gl_viewport[2];

    // this value tells us how much a screen pixel width in the viewport is in the units
    // of viewport coordinate space
    const float viewport_du_dx =
        image_zoom_in_viewport / (viewport_width * viewport_x_size_in_window);

    utility::JsonStore shader_params;
    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["soft_dim"]        = viewport_du_dx * 4.0f;
    shader_params["plot_circle"]     = false;

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


    for (const auto &stroke_info : render_data->stroke_info_) {

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
        glDrawArrays(GL_QUADS, 0, (stroke_info.stroke_point_count_ * 2 - 1) * 4);

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
        glDrawArrays(GL_QUADS, 0, (stroke_info.stroke_point_count_ * 2 - 1) * 4);

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
                viewport_du_dx,
                caption_info.text_size,
                caption_info.opacity);
        }
    }
}

void AnnotationsRenderer::render_text_handles_to_screen(
    const AnnotationRenderDataPtr render_data,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space) {

    const float image_zoom_in_viewport = transform_viewport_to_image_space[0][0];
    const float viewport_x_size_in_window =
        transform_window_to_viewport_space[0][0] / transform_window_to_viewport_space[3][3];

    std::array<int, 4> gl_viewport;

    glGetIntegerv(GL_VIEWPORT, gl_viewport.data());
    const auto viewport_width = (float)gl_viewport[2];
    const float viewport_du_dx =
        image_zoom_in_viewport / (viewport_width * viewport_x_size_in_window);

    utility::JsonStore shader_params;

    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["du_dx"]           = viewport_du_dx;
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
        shader_params2["top_left"]     = current_caption_bdb_.min;
        shader_params2["bottom_right"] = current_caption_bdb_.max;
        shader_params2["opacity"]      = 0.6;
        shader_params2["box_type"]     = 1;

        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(vertex_array_object2_);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);

        // now we draw the 'move' handle (a small box) to the top left
        // of the caption boundart
        shader_params2["top_left"] = current_caption_bdb_.min;
        shader_params2["bottom_right"] =
            current_caption_bdb_.min - Annotation::captionHandleSize * viewport_du_dx;
        shader_params2["opacity"] =
            caption_hover_state_ == Caption::HoveredOnMoveHandle ? 0.6f : 0.3f;
        shader_params2["box_type"] = 2;

        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(vertex_array_object2_);
        glLineWidth(2.0f);
        glDrawArrays(GL_QUADS, 0, 4);

        // now we draw the 'scale' handle (a small box) to the bottom right
        // of the caption boundary
        shader_params2["top_left"] = current_caption_bdb_.max;
        shader_params2["bottom_right"] =
            current_caption_bdb_.max + Annotation::captionHandleSize * viewport_du_dx;
        shader_params2["opacity"] =
            caption_hover_state_ == Caption::HoveredOnResizeHandle ? 0.6f : 0.3f;
        shader_params2["box_type"] = 2;

        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(vertex_array_object2_);
        glLineWidth(2.0f);
        glDrawArrays(GL_QUADS, 0, 4);
    }

    if (!under_mouse_caption_bdb_.isEmpty()) {
        shader_params2["top_left"]     = under_mouse_caption_bdb_.min;
        shader_params2["bottom_right"] = under_mouse_caption_bdb_.max;
        shader_params2["opacity"]      = 0.3;
        shader_params2["box_type"]     = 1;

        text_handles_shader_->set_shader_parameters(shader_params2);

        glBindVertexArray(vertex_array_object2_);

        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    if (cursor_position_[0] != Imath::V2f() && show_text_cursor_) {
        shader_params2["top_left"]     = cursor_position_[0];
        shader_params2["bottom_right"] = cursor_position_[1];
        shader_params2["box_type"]     = 3;
        text_handles_shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(vertex_array_object2_);
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

    glGenBuffers(1, &vertex_buffer_object_);
    glGenVertexArrays(1, &vertex_array_object_);

    // here specifying pairs of coordinates: 1st vec is position normalised to
    // caption origin and width, 2nd vec is a position offset *in screen pixel
    // coordinates*.
    // 1st V2f is 'aPos' in shader, 2nd is 'bPos'.

    // So to declare a point that is 20 pixels above and to the left of the origin
    // of the text caption we would do this:
    // Imath::V2f(0.0f, 0.0f), Imath::V2f(-20.0f, -20.0f)
    //
    // To declare a point 30 pixels to the right of the right wrap boundary of the
    // caption we do this:
    // Imath::V2f(1.0f, 0.0f), Imath::V2f(30.0f, 0.0f)

    static std::array<Imath::V2f, 22> vertices = {

        // draw a line from caption left-to-right under the first line of text
        // extending 20 pixels to left and right
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(-20.0f, 0.0f),
        Imath::V2f(1.0f, 0.0f),
        Imath::V2f(20.0f, 0.0f),

        // vert line 40 pixels long at origin
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(0.0f, -20.0f),
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(0.0f, 20.0f),

        // vert line 40 pixels long at right wrap boundary
        Imath::V2f(1.0f, 0.0f),
        Imath::V2f(0.0f, -20.0f),
        Imath::V2f(1.0f, 0.0f),
        Imath::V2f(0.0f, 20.0f),

        // now a box
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(30.0f, 30.0f),
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(30.0f, 60.0f),
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(60.0f, 60.0f),
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(60.0f, 30.0f),

        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(60.0f, 30.0f)

    };

    glBindVertexArray(vertex_array_object_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
    // 3. then set our vertex module pointers
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V2f), nullptr);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V2f), (void *)sizeof(Imath::V2f));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    glGenBuffers(1, &vertex_buffer_object2_);
    glGenVertexArrays(1, &vertex_array_object2_);

    static std::array<Imath::V2f, 4> vertices2 = {

        // draw a line from caption left-to-right under the first line of text
        // extending 20 pixels to left and right
        Imath::V2f(0.0f, 0.0f),
        Imath::V2f(1.0f, 0.0f),

        // vert line 40 pixels long at origin
        Imath::V2f(1.0f, 1.0f),
        Imath::V2f(0.0f, 1.0f)

    };

    glBindVertexArray(vertex_array_object2_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object2_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2.data(), GL_STATIC_DRAW);
    // 3. then set our vertex module pointers
    glEnableVertexAttribArray(0);
    // glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Imath::V2f), nullptr);
    // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 2*sizeof(Imath::V2f), (void
    // *)sizeof(Imath::V2f));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
