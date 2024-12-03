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
    shape_renderer_.reset(new OpenGLShapeRenderer());
}

void OpenGLCanvasRenderer::render_canvas(
    const Canvas &canvas,
    const HandleState &handle_state,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const bool have_alpha_buffer,
    const float image_aspectratio) {

    if (canvas.empty())
        return;

    const auto &strokes = all_canvas_items<Stroke>(canvas);
    if (!strokes.empty()) {
        stroke_renderer_->render_strokes(
            strokes,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            have_alpha_buffer);
    }

    const auto &captions = all_canvas_items<Caption>(canvas);
    if (!captions.empty()) {
        caption_renderer_->render_captions(
            captions,
            handle_state,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel);
    }

    const auto &quads    = all_canvas_items<canvas::Quad>(canvas);
    const auto &polygons = all_canvas_items<canvas::Polygon>(canvas);
    const auto &ellipses = all_canvas_items<canvas::Ellipse>(canvas);
    if (!quads.empty() || !polygons.empty() || !ellipses.empty()) {
        shape_renderer_->render_shapes(
            quads,
            polygons,
            ellipses,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            image_aspectratio);
    }
}
