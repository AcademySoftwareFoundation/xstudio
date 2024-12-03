// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        /*class PixelProbeHUDRenderer : public plugin::ViewportOverlayRenderer {
          public:
            void render_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            void set_mouse_pointer_position(const Imath::V2f p);

          private:
            Imath::V2f mouse_pointer_position_;
            std::mutex mutex_;
            std::unique_ptr<xstudio::ui::opengl::OpenGLTextRendererSDF> text_renderer_;
            std::vector<float> precomputed_text_vertex_buffer_;
            std::string text_;

        };*/

        class PixelProbeHUD : public plugin::HUDPluginBase {
          public:
            PixelProbeHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~PixelProbeHUD();

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

            void images_going_on_screen(
                const media_reader::ImageBufDisplaySetPtr & /*images*/,
                const std::string viewport_name,
                const bool playhead_playing) override;

          protected:
            bool pointer_event(const ui::PointerEvent &e) override;

          private:
            void update_onscreen_info(const std::string &viewport_name = std::string(), const Imath::V2f &pointer = Imath::V2f(-1,-1));
            caf::actor get_colour_pipeline_actor(const std::string &viewport_name);
            void make_pixel_info_onscreen_text(const media_reader::PixelInfo &pixel_info);

            std::map<std::string, media_reader::ImageBufDisplaySetPtr> current_onscreen_images_;
            std::map<std::string, caf::actor> colour_pipelines_;
            module::StringAttribute *pixel_info_text_;
            module::StringAttribute *pixel_info_title_;
            module::StringAttribute *pixel_info_current_viewport_;
            module::BooleanAttribute *show_code_values_;
            module::BooleanAttribute *show_raw_pixel_values_;
            module::BooleanAttribute *show_linear_pixel_values_;
            module::BooleanAttribute *show_final_screen_rgb_pixel_values_;
            module::IntegerAttribute *value_precision_;

            bool is_enabled_ = {false};
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
