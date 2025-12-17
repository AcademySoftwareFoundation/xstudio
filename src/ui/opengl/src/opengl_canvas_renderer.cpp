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
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const float image_aspectratio,
    const bool hide_strokes,
    const std::vector<std::shared_ptr<Stroke>> &live_erase_strokes,
    const std::set<std::size_t> & skip_captions) {

    if (canvas.empty())
        return;

    if (!hide_strokes) {
        auto strokes = all_canvas_items<Stroke>(canvas);
        for (const auto &live_erase_stroke: live_erase_strokes) {
            strokes.emplace_back(*live_erase_stroke);
        }

        if (!strokes.empty()) {
            stroke_renderer_->render_strokes(
                strokes,
                transform_window_to_viewport_space,
                transform_viewport_to_image_space,
                viewport_du_dpixel);
        }
    }

    const auto captions = all_canvas_items<Caption>(canvas);
    if (!captions.empty()) {
        caption_renderer_->render_captions(
            captions,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            device_pixel_ratio,
            skip_captions);
    }

    const auto quads    = all_canvas_items<canvas::Quad>(canvas);
    const auto polygons = all_canvas_items<canvas::Polygon>(canvas);
    const auto ellipses = all_canvas_items<canvas::Ellipse>(canvas);
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

void OpenGLCanvasRenderer::render_strokes(
    const std::vector<std::shared_ptr<xstudio::ui::canvas::Stroke>> &strokes,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dx,
    const float device_pixel_ratio)
{
    stroke_renderer_->render_strokes(
        strokes,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dx);
}

void OpenGLCanvasRenderer::render_single_caption(
    const xstudio::ui::canvas::Caption &caption,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dx,
    const float device_pixel_ratio) {

    caption_renderer_->render_single_caption(
        caption,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        viewport_du_dx,
        device_pixel_ratio);

}
