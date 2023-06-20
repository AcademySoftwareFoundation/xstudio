// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "annotation.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsRenderer : public plugin::ViewportOverlayRenderer {

          public:
            void render_opengl(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const xstudio::media_reader::ImageBufPtr &frame,
                const bool have_alpha_buffer) override;

            RenderPass preferred_render_pass() const { return BeforeImage; }

            void set_edited_annotation_render_data(
                AnnotationRenderDataPtr data,
                const bool show_text_handles  = false,
                const Imath::V2f &pointer_pos = Imath::V2f(0.0f, 0.0f)) {
                current_edited_annotation_render_data_ = data;
                show_text_handles_                     = show_text_handles;
            }

            void set_caption_hover_state(const Caption::HoverState state) {
                caption_hover_state_ = state;
            }

            void set_under_mouse_caption_bdb(const Imath::Box2f &bdb) {
                under_mouse_caption_bdb_ = bdb;
            }

            void set_current_edited_caption_bdb(const Imath::Box2f &bdb) {
                current_caption_bdb_ = bdb;
            }

            void set_cursor_position(const Imath::V2f top, const Imath::V2f bottom) {
                cursor_position_[0] = top;
                cursor_position_[1] = bottom;
            }

            void blink_text_cursor(const bool show_cursor) {
                text_cursor_blink_state_ = show_cursor;
            }

            void lock() { immediate_data_gate_.lock(); }
            void unlock() { immediate_data_gate_.unlock(); }

          private:
            void render_annotation_to_screen(
                const AnnotationRenderDataPtr render_data,
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel,
                const bool do_erase_strokes_first);

            void render_text_handles_to_screen(
                const Imath::M44f &transform_window_to_viewport_space,
                const Imath::M44f &transform_viewport_to_image_space,
                const float viewport_du_dpixel);

            void init_overlay_opengl();
            void init_caption_handles_graphics();

            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_, shader2_;
            std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> text_handles_shader_;

            typedef std::shared_ptr<xstudio::ui::opengl::OpenGLTextRendererSDF> FontRenderer;
            std::map<std::string, FontRenderer> text_renderers_;

            GLuint ssbo_id_;
            GLuint ssbo_size_ = {0};

            std::mutex immediate_data_gate_;
            utility::BlindDataObjectPtr immediate_data_;
            AnnotationRenderDataPtr current_edited_annotation_render_data_;
            const void *last_data_ = {nullptr};
            Imath::Box2f under_mouse_caption_bdb_, current_caption_bdb_;
            Imath::V2f cursor_position_[2];
            Caption::HoverState caption_hover_state_ = {Caption::NotHovered};
            bool show_text_handles_                  = {false};
            GLuint handles_vertex_buffer_obj_;
            GLuint handles_vertex_array_;

            bool text_cursor_blink_state_ = {false};
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio