// SPDX-License-Identifier: Apache-2.0
#pragma once

// clang-format off
#include <GL/glew.h>
#include <GL/gl.h>
// clang-format on

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/uuid.hpp"

//#define USE_SSBO

namespace xstudio {
namespace ui {
    namespace opengl {

        class GLBlindTex {

          public:
            GLBlindTex() = default;
            ~GLBlindTex();

            void release();

            virtual void map_buffer_for_upload(media_reader::ImageBufPtr &frame) = 0;
            virtual void start_pixel_upload()                                    = 0;
            virtual void bind(int tex_index, Imath::V2i &dims)                   = 0;

            [[nodiscard]] media_reader::ImageBufPtr current_frame() const {
                return current_source_frame_;
            }
            [[nodiscard]] const media::MediaKey &media_key() const { return media_key_; }
            [[nodiscard]] const utility::time_point &when_last_used() const {
                return when_last_used_;
            }

          protected:
            media::MediaKey media_key_;

            media_reader::ImageBufPtr new_source_frame_;
            media_reader::ImageBufPtr current_source_frame_;

            uint8_t *buffer_io_ptr_ = {nullptr};
            std::thread upload_thread_;
            std::mutex mutex_;
            utility::time_point when_last_used_;
        };

        class GLSsboTex : public GLBlindTex {

          public:
            GLSsboTex();
            virtual ~GLSsboTex();

            void map_buffer_for_upload(media_reader::ImageBufPtr &frame) override;
            void start_pixel_upload() override;
            void bind(int /*tex_index*/, Imath::V2i & /*dims*/) override { wait_on_upload(); }

          private:
            void compute_size(const size_t required_size_bytes);
            void pixel_upload();
            void wait_on_upload();

            GLuint ssbo_id_         = {0};
            GLuint bytes_per_pixel_ = 4;

            [[nodiscard]] size_t tex_size_bytes() const { return tex_data_size_; }

            size_t tex_data_size_ = {0};
        };

        class GLBlindRGBA8bitTex : public GLBlindTex {

          public:
            GLBlindRGBA8bitTex() = default;
            virtual ~GLBlindRGBA8bitTex();

            void map_buffer_for_upload(media_reader::ImageBufPtr &frame) override;
            void start_pixel_upload() override;
            void bind(int tex_index, Imath::V2i &dims) override;

          private:
            void resize(const size_t required_size_bytes);
            void pixel_upload();

            [[nodiscard]] size_t tex_size_bytes() const {
                return tex_width_ * tex_height_ * bytes_per_pixel_;
            }

            GLuint bytes_per_pixel_     = {0};
            GLuint tex_id_              = {0};
            GLuint pixel_buf_object_id_ = {0};

            int tex_width_  = {0};
            int tex_height_ = {0};
        };


        class GLDoubleBufferedTexture {

          public:
            GLDoubleBufferedTexture();
            virtual ~GLDoubleBufferedTexture() = default;

            void bind(int &tex_index, Imath::V2i &dims);
            void release();

            void upload_next(std::vector<media_reader::ImageBufPtr>);

            void set_use_ssbo(const bool using_ssbo);

            /*[[nodiscard]] int width() const ;
            [[nodiscard]] int height() const;*/
            [[nodiscard]] media_reader::ImageBufPtr current_frame() const {
                return current_ ? current_->current_frame() : media_reader::ImageBufPtr();
            }

          private:
            typedef std::shared_ptr<GLBlindTex> GLBlindTexturePtr;
            GLBlindTexturePtr current_;
            std::vector<GLBlindTexturePtr> textures_;
            media::MediaKey active_media_key_;
            bool using_ssbo_ = {false};
        };


        class GLColourLutTexture {

          public:
            GLColourLutTexture(
                const colour_pipeline::LUTDescriptor desc, const std::string texture_name);
            virtual ~GLColourLutTexture();

            void bind(int tex_index);
            void release();
            void upload_texture_data(const colour_pipeline::ColourLUTPtr &lut);

            [[nodiscard]] GLuint texture_id() const { return tex_id_; }
            [[nodiscard]] GLenum target() const;
            [[nodiscard]] const std::string &texture_name() const { return texture_name_; }

          private:
            GLint interpolation();
            GLint internal_format();
            GLint data_type();
            GLint format();

            GLuint tex_id_ = {0};
            GLuint pbo_    = {0};
            const colour_pipeline::LUTDescriptor descriptor_;
            std::size_t lut_cache_id_;
            const std::string texture_name_;
        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio