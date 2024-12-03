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