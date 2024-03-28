// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/viewport/hud_plugin.hpp"
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

        class PixelProbeHUD : public HUDPluginBase {
          public:
            PixelProbeHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~PixelProbeHUD();

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

            void images_going_on_screen(
                const std::vector<media_reader::ImageBufPtr> & /*images*/,
                const std::string viewport_name,
                const bool playhead_playing) override;

          protected:
            bool pointer_event(const ui::PointerEvent &e) override;

          private:
            void update_onscreen_info(const std::string &viewport_name = std::string());
            caf::actor get_colour_pipeline_actor(const std::string &viewport_name);
            void make_pixel_info_onscreen_text(const media_reader::PixelInfo &pixel_info);

            media_reader::ImageBufPtr current_onscreen_image_;
            std::map<std::string, media_reader::ImageBufPtr> current_onscreen_images_;
            std::map<std::string, caf::actor> colour_pipelines_;
            module::StringAttribute *pixel_info_text_;
            module::StringAttribute *pixel_info_title_;
            module::BooleanAttribute *show_code_values_;
            module::BooleanAttribute *show_raw_pixel_values_;
            module::BooleanAttribute *show_linear_pixel_values_;
            module::BooleanAttribute *show_final_screen_rgb_pixel_values_;
            module::IntegerAttribute *value_precision_;

            caf::actor colour_pipeline_;
            Imath::V2f last_pointer_pos_;
            bool is_enabled_ = {false};
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
