// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_caption_renderer.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;

namespace {

const char *flat_color_vertex_shader = R"(
    #version 430 core
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    layout (location = 0) in vec2 aPos;

    void main()
    {
        // as simple as it gets. Do I actually need to do this to draw a 
        // filled triangle?? OpenGL 3.3+
        vec2 vertex_pos = aPos.xy;
        gl_Position = vec4(vertex_pos,0.0,1.0)*to_coord_system*to_canvas;
    }
)";

const char *flat_color_frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 brush_colour;
    uniform float opacity;
    void main(void)
    {
        FragColor = vec4(
            brush_colour*opacity,
            opacity
            );
    }
)";

} // anonymous namespace


OpenGLCaptionRenderer::~OpenGLCaptionRenderer() { cleanup_gl(); }

void OpenGLCaptionRenderer::init_gl() {

    auto font_files = Fonts::available_fonts();
    for (const auto &f : font_files) {
        try {
            auto font = new ui::opengl::OpenGLTextRendererSDF(f.second, 96);
            text_renderers_[f.first].reset(font);
        } catch (std::exception &e) {
            spdlog::warn("Failed to load font: {}.", e.what());
        }
    }

    texthandle_renderer_.reset(new ui::opengl::OpenGLTextHandleRenderer());

    bg_shader_ = std::make_unique<ui::opengl::GLShaderProgram>(
        flat_color_vertex_shader, flat_color_frag_shader);

    if (!bg_vertex_buffer_) {
        glGenBuffers(1, &bg_vertex_buffer_);
        glGenVertexArrays(1, &bg_vertex_array_);
    }
}

void OpenGLCaptionRenderer::cleanup_gl() {

    if (bg_vertex_array_) {
        glDeleteVertexArrays(1, &bg_vertex_array_);
        bg_vertex_array_ = 0;
    }

    if (bg_vertex_buffer_) {
        glDeleteBuffers(1, &bg_vertex_buffer_);
        bg_vertex_buffer_ = 0;
    }
}

void OpenGLCaptionRenderer::render_captions(
    const std::vector<Caption> &captions,
    const HandleState &handle_state,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx) {

    if (!texthandle_renderer_) {
        init_gl();
    }

    if (text_renderers_.empty()) {
        return;
    }

    for (const auto &caption : captions) {

        auto it = text_renderers_.find(caption.font_name);
        auto text_renderer =
            it == text_renderers_.end() ? text_renderers_.begin()->second : it->second;

        render_background(
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dx,
            caption.background_colour,
            caption.background_opacity,
            caption.bounding_box());

        text_renderer->render_text(
            caption.vertices(),
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            caption.colour,
            viewport_du_dx,
            caption.font_size,
            caption.opacity);
    }

    texthandle_renderer_->render_handles(
        handle_state,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dx);
}

void OpenGLCaptionRenderer::render_background(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const utility::ColourTriplet &background_colour,
    const float background_opacity,
    const Imath::Box2f &bounding_box) {

    if (!bounding_box.isEmpty() && background_opacity > 0.f) {

        // TBH I preferred glBegin(GL_TRIANGLES) !!

        std::array<Imath::V2f, 6> bg_verts = {

            Imath::V2f(bounding_box.min.x, bounding_box.min.y),
            Imath::V2f(bounding_box.max.x, bounding_box.min.y),
            Imath::V2f(bounding_box.max.x, bounding_box.max.y),

            Imath::V2f(bounding_box.max.x, bounding_box.max.y),
            Imath::V2f(bounding_box.min.x, bounding_box.max.y),
            Imath::V2f(bounding_box.min.x, bounding_box.min.y)};

        glBindVertexArray(bg_vertex_array_);
        glBindBuffer(GL_ARRAY_BUFFER, bg_vertex_buffer_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(bg_verts), bg_verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Imath::V2f), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        utility::JsonStore shader_params;

        shader_params["to_coord_system"] = transform_viewport_to_image_space.inverse();
        shader_params["to_canvas"]       = transform_window_to_viewport_space;
        shader_params["brush_colour"]    = background_colour;
        shader_params["opacity"]         = background_opacity;

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);

        bg_shader_->use();
        bg_shader_->set_shader_parameters(shader_params);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}