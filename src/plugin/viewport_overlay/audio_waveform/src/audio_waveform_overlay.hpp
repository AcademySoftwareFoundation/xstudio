// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class WaveFormData : public utility::BlindDataObject {
          public:
            WaveFormData(
                const std::vector<float> &v,
                const int _num_chans,
                const float _vscale,
                const float _chan_spacing,
                const float _v_pos,
                const utility::ColourTriplet _line_colour)
                : verts_(std::move(v)),
                  num_chans(_num_chans),
                  vscale(_vscale),
                  chan_spacing(_chan_spacing),
                  v_pos(_v_pos),
                  line_colour(_line_colour) {}
            ~WaveFormData() = default;

            const std::vector<float> verts_;
            const int num_chans;
            const float vscale;
            const float chan_spacing;
            const float v_pos;
            const utility::ColourTriplet line_colour;
        };

        class AudioWaveformOverlayRenderer : public plugin::ViewportOverlayRenderer {

          public:
            void render_image_overlay(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const float device_pixel_ratio,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            ~AudioWaveformOverlayRenderer();

            void init_overlay_opengl();

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
            GLuint vbo_ = {0};
            GLuint vao_ = {0};
        };

        class AudioWaveformOverlay : public plugin::HUDPluginBase {

          public:
            AudioWaveformOverlay(
                caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~AudioWaveformOverlay();

          protected:
            utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufPtr &,
                const std::string & /*viewport_name*/,
                const utility::Uuid &playhead_uuid,
                const bool is_hero_image,
                const bool images_are_in_grid_layout) const override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const std::string &viewport_name) override {
                return plugin::ViewportOverlayRendererPtr(new AudioWaveformOverlayRenderer());
            }

            caf::message_handler message_handler_extensions() override {
                return message_handler_ext_.or_else(
                    plugin::HUDPluginBase::message_handler_extensions());
            }

            void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

          private:
            module::FloatAttribute *vertical_scale_;
            module::FloatAttribute *horizontal_scale_;
            module::FloatAttribute *chan_position_spacing_;
            module::FloatAttribute *vertical_position_;
            module::BooleanAttribute *separate_channels_;
            module::ColourAttribute *line_colour_;

            utility::Uuid mask_hotkey_;
            caf::message_handler message_handler_ext_;

            std::unordered_map<utility::Uuid, std::vector<media_reader::AudioBufPtr>>
                latest_audio_buffers_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
