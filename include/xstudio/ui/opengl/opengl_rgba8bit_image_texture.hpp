// SPDX-License-Identifier: Apache-2.0
#pragma once

// clang-format off
#include <GL/glew.h>
#include <GL/gl.h>
// clang-format on

#include "opengl_texture_base.hpp"

// #define USE_SSBO

namespace xstudio {
namespace ui {
    namespace opengl {

        class GLBlindRGBA8bitTex : public GLBlindTex {

          public:
            GLBlindRGBA8bitTex() = default;
            virtual ~GLBlindRGBA8bitTex();

            uint8_t *map_buffer_for_upload(const size_t buffer_size) override;
            void __bind(int tex_index, Imath::V2i &dims) override;

          private:
            void resize(const size_t required_size_bytes);
            void pixel_upload();

            [[nodiscard]] size_t tex_size_bytes() const override {
                return tex_width_ * tex_height_ * bytes_per_pixel_;
            }

            GLuint bytes_per_pixel_     = {0};
            GLuint tex_id_              = {0};
            GLuint pixel_buf_object_id_ = {0};

            int tex_width_  = {0};
            int tex_height_ = {0};
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio