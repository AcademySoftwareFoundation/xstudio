// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "annotation_opengl_renderer.hpp"
#include "annotation_render_data.hpp"
#include "annotations_tool.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;

namespace {
const char *vertex_shader = R"(
    #version 410 core
    layout (location = 1) in vec4 pos;
    layout (location = 0) in vec4 color;

    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform vec2 v_pos;
    uniform float viewport_du_dpixel;
    out vec4 t_color;
    out vec2 nrd;

    void main()
    {
        nrd = pos.xy;
        float l = length(nrd);
        float F = 5;
        float lens_scale = 2*l - l*l/F;
        nrd = normalize(nrd)*lens_scale;
        vec4 rpos = vec4((nrd.x+4.0)*50.0*viewport_du_dpixel, -(nrd.y+4.0)*50.0*viewport_du_dpixel, 0.0, 1.0);
        rpos.x += v_pos.x;
        rpos.y += v_pos.y;
        gl_Position = rpos*to_coord_system*to_canvas;
        t_color = color;
    }
    )";

const char *frag_shader = R"(
    #version 410 core
    out vec4 FragColor;
    uniform vec3 line_colour;
    in vec4 t_color;
    in vec2 nrd;

    void main(void)
    {
        float c = length(nrd);
        FragColor = t_color*smoothstep(4.1,4.0,c);
    }

    )";
} // namespace

AnnotationsRenderer::AnnotationsRenderer(
    const xstudio::ui::canvas::Canvas &interaction_canvas,
    PixelPatch &pixel_patch,
    const std::string &viewport_name)
    : interaction_canvas_(interaction_canvas),
      pixel_patch_(pixel_patch),
      annotations_visible_(true),
      viewport_name_(viewport_name) {

    canvas_renderer_.reset(new ui::opengl::OpenGLCanvasRenderer());
}

void AnnotationsRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    // when picking colours from the image, we may want to hide all annotations
    if (pixel_patch_.skip_render_of_drawings(viewport_name_)) {
        return;
    }

    auto render_data = frame.plugin_blind_data(AnnotationsTool::PLUGIN_UUID);
    if (render_data) {
        const auto *data = dynamic_cast<const AnnotationRenderDataSet *>(render_data.get());
        if (data) {
            handle_                       = data->handle_;
            current_edited_bookmark_uuid_ = data->current_edited_bookmark_uuid_;
            laser_drawing_mode_           = data->laser_mode_;
            frame_being_annotated_        = data->interaction_frame_key_;
            annotations_visible_          = data->show_annotations_;
        }
    }

    if (!annotations_visible_)
        return;

    // the xstudio playhead takes care of attaching bookmark data to the
    // ImageBufPtr that we receive here. The bookmark data may have annotations
    // data attached which we can draw to screen....
    bool draw_interaction_canvas = false;

    for (const auto &anno : frame.bookmarks()) {

        // .. we don't draw the annotation attached to the frame if its bookmark
        // uuid is the same as the uuid of the annotation that is currently
        // being edited. The reason is that the strokes and captions of this
        // annotation are already cloned into 'interaction_canvas_' which we
        // draw below.
        if (anno->detail_.uuid_ == current_edited_bookmark_uuid_) {
            draw_interaction_canvas = true;
            continue;
        }

        const Annotation *my_annotation =
            dynamic_cast<const Annotation *>(anno->annotation_.get());
        if (my_annotation) {
            canvas_renderer_->render_canvas(
                my_annotation->canvas(),
                HandleState(),
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel,
                device_pixel_ratio,
                have_alpha_buffer,
                1.f);
        }
    }

    if (draw_interaction_canvas || (current_edited_bookmark_uuid_.is_null() &&
                                    (frame_being_annotated_ == frame.frame_id().key()))) {
        canvas_renderer_->render_canvas(
            interaction_canvas_,
            handle_,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            device_pixel_ratio,
            have_alpha_buffer,
            1.f);
    }

    // The canvas drawing routines use the depth buffer and clear it
    // for this purpose, we need to restore it to what QtQuick renderer
    // has set it to which I'm pretty sure is 1.0
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void AnnotationsRenderer::render_viewport_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_normalised_coords,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const bool have_alpha_buffer) {

    if (laser_drawing_mode_) {
        canvas_renderer_->render_canvas(
            interaction_canvas_,
            handle_,
            transform_window_to_viewport_space,
            transform_viewport_to_normalised_coords,
            viewport_du_dpixel,
            device_pixel_ratio,
            have_alpha_buffer,
            1.f);

        // The canvas drawing routines use the depth buffer and clear it
        // for this purpose, we need to restore it to what QtQuick renderer
        // has set it to which I'm pretty sure is 1.0
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);

    }

    render_pixel_picker_patch(
        transform_window_to_viewport_space,
        transform_viewport_to_normalised_coords,
        viewport_du_dpixel);
}

void AnnotationsRenderer::init_overlay_opengl() {

    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
}

void AnnotationsRenderer::render_pixel_picker_patch(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel) {

    if (pixel_patch_.skip_render(viewport_name_))
        return;

    std::lock_guard l(pixel_patch_.mutex());

    if (!shader_)
        init_overlay_opengl();

    glBindVertexArray(vao_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        pixel_patch_.patch_vertex_data().size() * sizeof(Imath::V4f),
        pixel_patch_.patch_vertex_data().data(),
        GL_STREAM_DRAW);
    // 3. then set our vertex module pointers
    Imath::V4f *ptr = 0;
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V4f), (void *)ptr);
    ptr += 1;
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(Imath::V4f), (void *)ptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    utility::JsonStore shader_params;
    shader_params["to_canvas"]          = transform_window_to_viewport_space;
    shader_params["to_coord_system"]    = transform_viewport_to_image_space.inverse();
    shader_params["v_pos"]              = pixel_patch_.position();
    shader_params["viewport_du_dpixel"] = viewport_du_dpixel;
    shader_->set_shader_parameters(shader_params);
    shader_->use();
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glDrawArrays(GL_TRIANGLES, 0, (pixel_patch_.patch_vertex_data().size()) / 2 - 4);

    // inside square highlight
    if (pixel_patch_.pressed()) {
        glLineWidth(3.0f);
    } else {
        glLineWidth(1.0f);
    }
    glDrawArrays(GL_LINE_LOOP, (pixel_patch_.patch_vertex_data().size()) / 2 - 4, 4);

    // outside square highlight
    // glDrawArrays(GL_LINE_LOOP, (pixel_patch_.patch_vertex_data().size()) / 2 - 4, 4);

    shader_->stop_using();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);
}
