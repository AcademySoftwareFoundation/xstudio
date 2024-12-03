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

        class GLSsboTex : public GLBlindTex {

          public:
            GLSsboTex() = default;
            virtual ~GLSsboTex();

            uint8_t *map_buffer_for_upload(const size_t buffer_size) override;
            void __bind(int /*tex_index*/, Imath::V2i & /*dims*/) override;

          private:
            void compute_size(const size_t required_size_bytes);
            void pixel_upload();
            void wait_on_upload();

            GLuint ssbo_id_         = {0};
            GLuint bytes_per_pixel_ = 4;

            [[nodiscard]] size_t tex_size_bytes() const override { return tex_data_size_; }

            size_t tex_data_size_ = {0};
        };

    } // namespace opengl
} // namespace ui
} // namespace xstudio