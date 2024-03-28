// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include "xstudio/ui/opengl/shader_program_base.hpp"


namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLOffscreenRenderer {
          public:
            explicit OpenGLOffscreenRenderer(GLint color_format);
            OpenGLOffscreenRenderer(const OpenGLOffscreenRenderer &) = delete;
            OpenGLOffscreenRenderer &operator=(const OpenGLOffscreenRenderer &) = delete;
            ~OpenGLOffscreenRenderer();

            void resize(const Imath::V2f &dims);
            void begin();
            void end();

            Imath::V2f dimensions() const { return fbo_dims_; }
            unsigned int texture_handle() const { return tex_id_; }
            GLenum texture_target() const { return tex_target_; }

          private:
            void cleanup();

            GLint color_format_{0};
            Imath::V2f fbo_dims_{0.0f, 0.0f};
            GLenum tex_target_{GL_TEXTURE_2D};
            unsigned int tex_id_{0};
            unsigned int rbo_id_{0};
            unsigned int fbo_id_{0};

            std::array<int, 4> vp_state_;
        };

        using OpenGLOffscreenRendererPtr = std::unique_ptr<OpenGLOffscreenRenderer>;

    } // end namespace opengl
} // end namespace ui
} // end namespace xstudio