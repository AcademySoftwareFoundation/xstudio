// SPDX-License-Identifier: Apache-2.0
#pragma once

// clang-format off
#include <Imath/ImathVec.h>
#include <Imath/ImathMatrix.h>
#include <GL/glew.h>
#include <GL/gl.h>
// clang-format on

#include "xstudio/utility/json_store.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/viewport/shader.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class OpenGLShader : public viewport::GPUShader {
          public:
            OpenGLShader(utility::Uuid id, std::string code)
                : viewport::GPUShader(id, viewport::GraphicsAPI::OpenGL),
                  shader_code_(std::move(code)) {}

            [[nodiscard]] const std::string &shader_code() const { return shader_code_; }

          private:
            const std::string shader_code_;
        };

        class GLShaderProgram {

          public:
            GLShaderProgram(
                const std::string &vertex_shader,
                const std::string &fragment_shader,
                const bool do_compile = true);

            GLShaderProgram(
                const std::string &vertex_shader,
                const std::string &pixel_unpack_shader,
                const std::vector<std::string> &colour_op_shaders,
                const bool use_ssbo);

            ~GLShaderProgram();

            void inject_colour_op_shader(const std::string &colour_op_shader);

            void compile();
            void use() const;
            void stop_using() const;
            void set_shader_parameters(const utility::JsonStore &shader_params);
            void set_shader_parameters(const media_reader::ImageBufPtr &image);

            GLuint program_ = {0};

          private:
            [[nodiscard]] bool is_colour_op_shader_source(const std::string &shader_code) const;

            void inject_colour_ops(
                const std::vector<std::string> &colour_operations_shaders,
                std::string main_display_shader);

            int get_param_location(const std::string &param_name);
            std::map<std::string, int> locations_;
            std::vector<std::string> vertex_shaders_;
            std::vector<std::string> fragment_shaders_;
            std::vector<GLuint> shaders_;
            int colour_operation_index_ = {1};
        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio
