// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_caption_renderer.hpp"
#include "xstudio/ui/opengl/opengl_stroke_renderer.hpp"
#include "xstudio/ui/opengl/opengl_shape_renderer.hpp"
#include "xstudio/ui/canvas/canvas.hpp"


namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLCanvasRenderer {

          public:
            OpenGLCanvasRenderer();

            void render_canvas(
                const xstudio::ui::canvas::Canvas &canvas,
                const xstudio::ui::canvas::HandleState &handle_state,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const bool have_alpha_buffer,
                const float image_aspectratio);

          private:
            template <typename T>
            std::vector<T> all_canvas_items(const xstudio::ui::canvas::Canvas &canvas) {
                std::vector<T> result;
                canvas.read_lock();
                for (const auto &item : canvas) {
                    if (std::holds_alternative<T>(item)) {
                        result.push_back(std::get<T>(item));
                    }
                }
                if (canvas.has_current_item_nolock<T>()) {
                    result.push_back(std::move(canvas.get_current<T>()));
                }
                canvas.read_unlock();
                return result;
            }

          private:
            std::unique_ptr<xstudio::ui::opengl::OpenGLStrokeRenderer> stroke_renderer_;
            std::unique_ptr<xstudio::ui::opengl::OpenGLCaptionRenderer> caption_renderer_;
            std::unique_ptr<xstudio::ui::opengl::OpenGLShapeRenderer> shape_renderer_;
        };

    } // end namespace opengl
} // end namespace ui
} // end namespace xstudio