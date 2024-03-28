// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class MaskData : public utility::BlindDataObject {
          public:
            MaskData(
                const utility::JsonStore &j, const std::string &caption, const float label_size)
                : mask_shader_params_(j), mask_caption_(caption), label_size_(label_size) {}
            ~MaskData() = default;

            const utility::JsonStore mask_shader_params_;
            const std::string mask_caption_;
            const float label_size_;
        };

        class BasicMaskRenderer : public plugin::ViewportOverlayRenderer {

          public:
            void render_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            void init_overlay_opengl();

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
            GLuint vertex_buffer_object_;
            GLuint vertex_array_object_;
            std::unique_ptr<xstudio::ui::opengl::OpenGLTextRendererSDF> text_renderer_;
            std::vector<float> precomputed_text_vertex_buffer_;
            std::string text_;
            float font_scale_ = 0.0f;
            float ma_         = 0.0f;
        };

        class BasicViewportMasking : public plugin::StandardPlugin {
          public:
            BasicViewportMasking(
                caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~BasicViewportMasking();

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

            void hotkey_pressed(
                const utility::Uuid &hotkey_uuid, const std::string &context) override;

          protected:
            void register_hotkeys() override;

            utility::BlindDataObjectPtr prepare_overlay_data(
                const media_reader::ImageBufPtr &, const bool /*offscreen*/) const override;

            plugin::ViewportOverlayRendererPtr make_overlay_renderer(const int) override {
                return plugin::ViewportOverlayRendererPtr(new BasicMaskRenderer());
            }

          private:
            module::FloatAttribute *mask_aspect_ratio_;
            module::StringChoiceAttribute *aspect_ratio_presets_;
            module::FloatAttribute *line_thickness_;
            module::FloatAttribute *line_opacity_;
            module::FloatAttribute *mask_opacity_;
            module::FloatAttribute *safety_percent_;
            module::FloatAttribute *mask_label_size_;
            module::StringChoiceAttribute *mask_render_method_;

            module::FloatAttribute *current_mask_aspect_;
            module::FloatAttribute *current_mask_safety_;

            module::BooleanAttribute *show_mask_label_;

            module::BooleanAttribute *mask_enabled_;
            module::StringChoiceAttribute *mask_selection_;

            utility::Uuid mask_hotkey_;

            bool render_opengl_ = false;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
