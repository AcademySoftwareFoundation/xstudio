// SPDX-License-Identifier: Apache-2.0
#include "onion_skin_renderer.hpp"
#include "onion_skin_plugin.hpp"
#include "onion_skin_render_data.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/blind_data.hpp"

using namespace xstudio;
using namespace xstudio::ui::viewport;


void OnionSkinRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame) {

    auto blind = frame.plugin_blind_data(OnionSkinPlugin::PLUGIN_UUID);
    const auto *render_data =
        dynamic_cast<const OnionSkinRenderData *>(blind.get());
    if (!render_data || render_data->canvases.empty())
        return;

    if (!canvas_renderer_)
        canvas_renderer_ = std::make_unique<opengl::OpenGLCanvasRenderer>();

    const float img_aspect = media_reader::image_aspect(frame);

    // Opacity and tint are already baked into each canvas's items,
    // so we just render them directly — no FBO needed.
    for (const auto &canvas : render_data->canvases) {
        canvas_renderer_->render_canvas(
            canvas,
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            device_pixel_ratio,
            img_aspect,
            false); // don't hide strokes
    }
}
