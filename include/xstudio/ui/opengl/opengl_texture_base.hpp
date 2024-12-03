// SPDX-License-Identifier: Apache-2.0
#pragma once

// clang-format off
#include <GL/glew.h>
#include <GL/gl.h>
// clang-format on

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/uuid.hpp"

// #define USE_SSBO

namespace xstudio {
namespace ui {
    namespace opengl {

        class GLBlindTex {

          public:

            GLBlindTex();
            ~GLBlindTex();

            void release();

            virtual void bind(int tex_index, Imath::V2i &dims) {
                wait_on_upload_pixels();
                __bind(tex_index, dims);
            }


            [[nodiscard]] const media::MediaKey &pending_media_key() const { return pending_media_key_; }
            [[nodiscard]] const media::MediaKey &media_key() const { return media_key_; }
            [[nodiscard]] const utility::time_point &when_last_used() const {
                return when_last_used_;
            }

            void prepare_for_upload(const media_reader::ImageBufPtr &frame);

            void do_pixel_upload();

            void cancel_upload();

          protected:

            virtual uint8_t *map_buffer_for_upload(const size_t buffer_size)             = 0;
            virtual void __bind(int tex_index, Imath::V2i &dims) = 0;
            virtual size_t tex_size_bytes() const                = 0;

            void wait_on_upload_pixels();

            utility::time_point when_last_used_;

            media::MediaKey media_key_;
            media_reader::ImageBufPtr source_frame_;

            media::MediaKey pending_media_key_;
            media_reader::ImageBufPtr pending_source_frame_;

            uint8_t * gpu_mapped_mem_ = nullptr;

            std::thread upload_thread_;
            std::mutex mutex_;
            std::condition_variable cv_;
            bool pending_upload_ = {false};
            bool in_progress_ = {false};
        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio