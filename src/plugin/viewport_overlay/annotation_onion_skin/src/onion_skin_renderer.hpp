// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class OnionSkinRenderer : public plugin::ViewportOverlayRenderer {

          public:
            OnionSkinRenderer() = default;

            void render_image_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const float device_pixel_ratio,
                const xstudio::media_reader::ImageBufPtr &frame) override;

            float stack_order() const override { return 1.5f; }

          private:
            std::unique_ptr<opengl::OpenGLCanvasRenderer> canvas_renderer_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
