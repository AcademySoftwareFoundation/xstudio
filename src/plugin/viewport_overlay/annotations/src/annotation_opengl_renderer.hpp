// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/opengl_canvas_renderer.hpp"
#include "annotation.hpp"


namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsRenderer : public plugin::ViewportOverlayRenderer {

          public:
            AnnotationsRenderer();

            void render_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            RenderPass preferred_render_pass() const override { return BeforeImage; }

          private:
            std::unique_ptr<xstudio::ui::opengl::OpenGLCanvasRenderer> canvas_renderer_;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio