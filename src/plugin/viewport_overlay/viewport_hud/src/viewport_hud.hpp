// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class HudData : public utility::BlindDataObject {
          public:
            HudData(const utility::JsonStore &j) : hud_params_(j) {}
            ~HudData() = default;

            const utility::JsonStore hud_params_;
        };

        class ViewportHUDRenderer : public plugin::ViewportOverlayRenderer {
          public:
            void render_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

          private:
            Imath::V2f get_transformed_point(
                Imath::V2i point,
                Imath::V2i image_dims,
                Imath::M44f transform_matrix,
                const float pixel_aspect);
            void draw_gl_box(
                Imath::V2i top_left_pt,
                Imath::V2i bottom_right_pt,
                Imath::V2i image_dims,
                Imath::M44f transform_matrix,
                const float pixel_aspect,
                Imath::V3f color);
        };

        class ViewportHUD : public plugin::StandardPlugin {
          public:
            ViewportHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~ViewportHUD();

            static void render_overlay_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const xstudio::media_reader::ImageBufPtr &frame);

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

          protected:
            utility::BlindDataObjectPtr prepare_render_data(
                const media_reader::ImageBufPtr &, const bool /*offscreen*/) const override;

            plugin::ViewportOverlayRendererPtr make_overlay_renderer(const bool) override {
                return plugin::ViewportOverlayRendererPtr(new ViewportHUDRenderer());
            }

          private:
            module::StringChoiceAttribute *hud_selection_;
            module::BooleanAttribute *bounding_box;
            module::BooleanAttribute *image_borders_box;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
