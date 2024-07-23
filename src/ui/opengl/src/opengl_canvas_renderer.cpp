// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;


OpenGLCanvasRenderer::OpenGLCanvasRenderer() {

    stroke_renderer_.reset(new OpenGLStrokeRenderer());
    caption_renderer_.reset(new OpenGLCaptionRenderer());
}

void OpenGLCanvasRenderer::render_canvas(
    const Canvas &canvas,
    const HandleState &handle_state,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const bool have_alpha_buffer) {

    if (canvas.empty())
        return;

    stroke_renderer_->render_strokes(
        all_canvas_items<Stroke>(canvas),
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dpixel,
        have_alpha_buffer);

    caption_renderer_->render_captions(
        all_canvas_items<Caption>(canvas),
        handle_state,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dpixel);
}
