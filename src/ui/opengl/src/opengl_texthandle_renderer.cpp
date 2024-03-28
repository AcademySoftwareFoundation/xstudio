// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_texthandle_renderer.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;


namespace {

const char *vertex_shader = R"(
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

const char *frag_shader = R"(
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

    std::vector<xstudio::utility::JsonStore> aa_nudge;

} aa_jitter_table;

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

} // anonymous namespace


OpenGLTextHandleRenderer::~OpenGLTextHandleRenderer() { cleanup_gl(); }

void OpenGLTextHandleRenderer::init_gl() {

    if (!shader_) {
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
    }

    if (!handles_vertex_buffer_obj_ && !handles_vertex_array_) {
        glGenBuffers(1, &handles_vertex_buffer_obj_);
        glGenVertexArrays(1, &handles_vertex_array_);

        glBindVertexArray(handles_vertex_array_);

        glBindBuffer(GL_ARRAY_BUFFER, handles_vertex_buffer_obj_);
        glBufferData(
            GL_ARRAY_BUFFER, sizeof(handles_vertices), handles_vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Imath::V2f), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);
    }
}

void OpenGLTextHandleRenderer::cleanup_gl() {

    if (handles_vertex_array_) {
        glDeleteVertexArrays(1, &handles_vertex_array_);
        handles_vertex_array_ = 0;
    }

    if (handles_vertex_buffer_obj_) {
        glDeleteBuffers(1, &handles_vertex_buffer_obj_);
        handles_vertex_buffer_obj_ = 0;
    }
}

void OpenGLTextHandleRenderer::render_handles(
    const HandleState &handle_state,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    if (!shader_)
        init_gl();

    utility::JsonStore shader_params;

    shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]       = transform_window_to_viewport_space;
    shader_params["du_dx"]           = viewport_du_dx;
    shader_params["box_type"]        = 0;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    shader_->use();
    shader_->set_shader_parameters(shader_params);

    utility::JsonStore shader_params2;

    if (!handle_state.current_caption_bdb.isEmpty()) {

        // draw the box around the current edited caption
        shader_params2["box_position"] = handle_state.current_caption_bdb.min;
        shader_params2["box_size"]     = handle_state.current_caption_bdb.size();
        shader_params2["opacity"]      = 0.6;
        shader_params2["box_type"]     = 1;
        shader_params2["aa_nudge"]     = Imath::V2f(0.0f, 0.0f);


        shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(handles_vertex_array_);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);

        const auto handle_size = handle_state.handle_size * viewport_du_dx;

        // Draw the three
        static const auto hndls = std::vector<HandleHoverState>(
            {HandleHoverState::HoveredOnMoveHandle,
             HandleHoverState::HoveredOnResizeHandle,
             HandleHoverState::HoveredOnDeleteHandle});

        static const auto vtx_offsets = std::vector<int>({4, 14, 24});
        static const auto vtx_counts  = std::vector<int>({20, 10, 4});

        const auto positions = std::vector<Imath::V2f>(
            {handle_state.current_caption_bdb.min - handle_size,
             handle_state.current_caption_bdb.max,
             {handle_state.current_caption_bdb.max.x,
              handle_state.current_caption_bdb.min.y - handle_size.y}});

        shader_params2["box_size"] = handle_size;

        glBindVertexArray(handles_vertex_array_);

        // draw a grey box for each handle
        shader_params2["opacity"] = 0.6f;
        for (size_t i = 0; i < hndls.size(); ++i) {
            shader_params2["box_position"] = positions[i];
            shader_params2["box_type"]     = 2;
            shader_->set_shader_parameters(shader_params2);
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
            shader_params2["box_type"]     = handle_state.hover_state == hndls[i] ? 4 : 3;

            shader_->set_shader_parameters(shader_params2);
            // plot 9 times with anti-aliasing jitter to get a better looking result
            for (const auto &aa_nudge : aa_jitter_table.aa_nudge) {
                shader_->set_shader_parameters(aa_nudge);
                glDrawArrays(GL_LINES, vtx_offsets[i], vtx_counts[i]);
            }
        }
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!handle_state.under_mouse_caption_bdb.isEmpty()) {

        shader_params2["box_position"] = handle_state.under_mouse_caption_bdb.min;
        shader_params2["box_size"]     = handle_state.under_mouse_caption_bdb.size();
        shader_params2["opacity"]      = 0.3;
        shader_params2["box_type"]     = 1;

        shader_->set_shader_parameters(shader_params2);

        glBindVertexArray(handles_vertex_array_);

        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    if (handle_state.cursor_position[0] != Imath::V2f(0.0f, 0.0f)) {

        shader_params2["opacity"]      = 0.6f;
        shader_params2["box_position"] = handle_state.cursor_position[0];
        shader_params2["box_size"] =
            handle_state.cursor_position[1] - handle_state.cursor_position[0];
        shader_params2["box_type"] = handle_state.cursor_blink_state ? 2 : 0;
        shader_->set_shader_parameters(shader_params2);
        glBindVertexArray(handles_vertex_array_);
        glLineWidth(3.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    glBindVertexArray(0);
}
