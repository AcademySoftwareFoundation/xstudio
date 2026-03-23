// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/viewport/video_output_plugin.hpp"
#include "xstudio/ui/viewport/viewport_gpu_post_processor.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

namespace xstudio {

namespace video_render_plugin_1_0 {

    class VideoRenderPlugin : public plugin::StandardPlugin {

      public:
        VideoRenderPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings);
        virtual ~VideoRenderPlugin();

        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("4147f82d-1006-4025-ac04-79c81cd5e7b7");

        void menu_item_activated(
            const utility::JsonStore &menu_item_data, const std::string &user_data) override;

        utility::JsonStore qml_item_callback(
            const utility::Uuid &qml_item_id, const utility::JsonStore &callback_data) override;

        void on_exit() override;

      private:
        void remove_job(const utility::Uuid &job_id);
        void update_ocio_choices(const utility::Uuid &target_render_item_id);
        void make_offscreen_viewport(caf::typed_response_promise<bool> rp);
        void playback_render_output(
            std::string path_to_render, const std::string audio_file, caf::actor session);

      protected:
        void attribute_changed(const utility::Uuid &attribute_uuid, const int role) override;

        utility::Uuid create_render_job(
            const std::string &render_item_name,
            const utility::Uuid &parent_playlist_item_id,
            const utility::Uuid &target_render_item_id,
            const std::string &output_file_path,
            const std::string &output_audio_path,
            const Imath::V2i &resolution,
            const int in_point,
            const int out_point,
            const utility::FrameRate &frame_rate,
            const std::string &video_codec_opts,
            const std::string &video_render_bit_depth,
            const std::string &audio_codec_opts,
            const std::string &video_preset_name,
            const std::string &ocio_display,
            const std::string &ocio_view,
            const bool &auto_check_output,
            const std::string &timecode);

        caf::message_handler message_handler_extensions() override;
        caf::actor offscreen_viewport_, colour_pipeline_, dummy_colour_pipeline_;
        utility ::Uuid render_menu_item_, render_menu_item2_;
        module::JsonAttribute *resolutions_;
        module::JsonAttribute *frame_rates_;
        module::JsonAttribute *video_codec_presets_;
        module::JsonAttribute *video_codec_default_presets_;
        module::StringAttribute *ocio_warnings_;
        std::map<utility::Uuid, module::JsonAttribute *> jobs_status_data_;
        module::StringAttribute *overall_status_;
        utility::UuidActorVector queued_jobs_;
        utility::UuidActor current_worker_;
        utility::JsonStore ocio_settings_;
        caf::actor event_group_;
    };

    class YUVFrameGrabber : public ui::viewport::ViewportFramePostProcessor {

      public:
        YUVFrameGrabber() {}
        ~YUVFrameGrabber();

        void viewport_capture_gl_framebuffer(
            uint32_t tex_id,
            uint32_t fbo_id,
            const int fb_width,
            const int fb_height,
            const ui::viewport::ImageFormat format,
            media_reader::ImageBufPtr &destination_image) override;

      private:
        void setupYUVImageTextureAndFrameBuffer(const int width, const int __height);

        void grabYUVFrameBuffer(
            const int width, const int height, media_reader::ImageBufPtr &destination_image);

        GLuint pixel_buffer_object_ = 0;
        int pix_buf_size_           = 0;
        GLuint texId_               = 0;
        GLuint fboId_               = 0;
        GLuint vbo_                 = 0;
        GLuint vao_                 = 0;
        int tex_width_              = 0;
        int tex_height_             = 0;
        std::unique_ptr<ui::opengl::GLShaderProgram> shader_;
    };

    class RGB10BitFrameGrabber : public ui::viewport::ViewportFramePostProcessor {

      public:
        RGB10BitFrameGrabber() {}
        ~RGB10BitFrameGrabber();

        void viewport_capture_gl_framebuffer(
            uint32_t tex_id,
            uint32_t fbo_id,
            const int fb_width,
            const int fb_height,
            const ui::viewport::ImageFormat format,
            media_reader::ImageBufPtr &destination_image) override;

      private:
        void setupImageTextureAndFrameBuffer(const int width, const int height);

        void grabRGB10bitFrameBuffer(
            const int width, const int height, media_reader::ImageBufPtr &destination_image);

        GLuint pixel_buffer_object_ = 0;
        int pix_buf_size_           = 0;
        GLuint texId_               = 0;
        GLuint fboId_               = 0;
        GLuint vbo_                 = 0;
        GLuint vao_                 = 0;
        int tex_width_              = 0;
        int tex_height_             = 0;
        std::unique_ptr<ui::opengl::GLShaderProgram> shader_;
    };

} // namespace video_render_plugin_1_0
} // namespace xstudio
