// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/opengl_offscreen_renderer.hpp"
#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"


namespace xstudio {
namespace ui {
namespace viewport {

    /*
    The pre_viewport_draw_gpu_hook is called with the GL context of the
    viewport in an active state. We draw the strokes of the grading mask
    into a GL texture, and set the texture ID on the colour pipeline data
    of the image that is passed in. When the image is drawn to the screen
    our shader can sample the texture to mask the grade.
    */

    class GradingMaskRenderer : public plugin::GPUPreDrawHook {

        struct RenderLayer {
            opengl::OpenGLOffscreenRendererPtr offscreen_renderer;
            utility::clock::time_point last_canvas_change_time;
            utility::Uuid last_canvas_uuid;
        };

      public:

        GradingMaskRenderer();

        void pre_viewport_draw_gpu_hook(
            const Imath::M44f &transform_window_to_viewport_space,
            const Imath::M44f &transform_viewport_to_image_space,
            const float viewport_du_dpixel,
            xstudio::media_reader::ImageBufPtr &image) override;

      private:

        size_t layer_count() const;
        void add_layer();

        void renderGradingDataMasks(
            const GradingData *,
            xstudio::media_reader::ImageBufPtr &image);

        void render_layer(
            const LayerData& data,
            RenderLayer& layer,
            const xstudio::media_reader::ImageBufPtr &frame,
            const bool have_alpha_buffer);

        std::mutex immediate_data_gate_;
        utility::BlindDataObjectPtr immediate_data_;

        std::vector<RenderLayer> render_layers_;
        std::unique_ptr<opengl::OpenGLCanvasRenderer> canvas_renderer_;
    };

    using GradingMaskRendererSPtr = std::shared_ptr<GradingMaskRenderer>;

} // end namespace viewport
} // end namespace ui
} // end namespace xstudio