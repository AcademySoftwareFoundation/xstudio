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

namespace xstudio {
namespace ui {
    namespace opengl {

        class GLShaderProgram {

          public:
            GLShaderProgram(
                const std::string &vertex_shader,
                const std::string &fragment_shader,
                const bool do_compile = true);

            GLShaderProgram(
                const std::string &vertex_shader,
                const std::string &colour_transform_shader,
                const std::string &fragment_shader,
                const bool use_ssbo,
                const bool do_compile = true);

            void compile();
            void use() const;
            void stop_using() const;
            void set_shader_parameters(
                const utility::JsonStore &shader_params, int is_main_viewer = -1);
            void set_shader_parameters(
                const media_reader::ImageBufPtr &image, int is_main_viewer = -1);

            GLuint program_ = {0};

          private:
            int get_param_location(const std::string &param_name);
            std::map<std::string, int> locations_;
            std::vector<std::string> vertex_shaders_;
            std::vector<std::string> fragment_shaders_;
        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio
