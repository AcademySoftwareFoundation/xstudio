// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/utility/uuid.hpp"
#include <Imath/ImathMatrix.h>
#include <caf/actor.hpp>

namespace xstudio {
namespace ui {
    namespace opengl {

        class GLDoubleBufferedTexture;
        class GLColourLutTexture;
        class GLShaderProgram;

        typedef std::shared_ptr<GLShaderProgram> GLShaderProgramPtr;

        class ColourPipeLutCollection {
          public:
            ColourPipeLutCollection() = default;
            ColourPipeLutCollection(const ColourPipeLutCollection &o);

            void upload_luts(const std::vector<colour_pipeline::ColourLUTPtr> &luts);
            void register_texture(const std::vector<colour_pipeline::ColourTexture> &textures);

            void bind_luts(GLShaderProgramPtr shader, int &tex_idx);

            void clear() { active_luts_.clear(); }

          private:
            typedef std::shared_ptr<GLColourLutTexture> GLColourLutTexturePtr;
            std::map<std::string, GLColourLutTexturePtr> lut_textures_;
            std::vector<GLColourLutTexturePtr> active_luts_;
            std::map<std::string, colour_pipeline::ColourTexture> active_textures_;
        };

        class OpenGLViewportRenderer : public viewport::ViewportRenderer {
          public:

            OpenGLViewportRenderer(const std::string &window_id);

            virtual ~OpenGLViewportRenderer() override;

            void render(
                const media_reader::ImageBufDisplaySetPtr &images,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_matrix,
                const std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> &overlay_renderers) override;

            virtual void draw_image(
                const media_reader::ImageBufPtr &image_to_be_drawn,
                const media_reader::ImageSetLayoutDataPtr &layout_data,
                const int index,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_space,
                const float viewport_du_dx);

            utility::JsonStore default_prefs() override;

            void set_prefs(const utility::JsonStore &prefs) override;

          protected:

            void __draw_image(
                const media_reader::ImageBufDisplaySetPtr &all_images,
                const int index,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_space,
                const float viewport_du_dx,
                const std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> &overlay_renderers);

            void pre_init() override;

            bool activate_shader(
                const viewport::GPUShaderPtr &image_buffer_unpack_shader,
                const std::vector<colour_pipeline::ColourOperationDataPtr> &operations);

            void
            upload_image_and_colour_data(const media_reader::ImageBufPtr &image);
            void bind_textures(const media_reader::ImageBufPtr &image);
            void release_textures();
            void clear_viewport_area(const Imath::M44f &window_to_viewport_matrix);

            typedef std::shared_ptr<GLDoubleBufferedTexture> GLTexturePtr;

            struct SharedResources {
              std::vector<GLTexturePtr> textures_;
              std::map<std::string, GLShaderProgramPtr> programs_;
              ColourPipeLutCollection colour_pipe_textures_;
              unsigned int vbo_, vao_;
              GLShaderProgramPtr no_image_shader_program_;
              void init();
              ~SharedResources();
            };
            
            typedef std::shared_ptr<SharedResources> SharedResourcesPtr;
            std::vector<GLTexturePtr> & textures() { return resources_->textures_; }
            std::map<std::string, GLShaderProgramPtr> & shader_programs() { return resources_->programs_; }
            ColourPipeLutCollection & colour_pipe_textures() { return resources_->colour_pipe_textures_; }
            GLShaderProgramPtr & no_image_shader_program() { return resources_-> no_image_shader_program_; }
            unsigned int & vbo() { return resources_->vbo_; }
            unsigned int & vao() { return resources_->vao_; }

            static std::map<std::string, SharedResourcesPtr> s_resources_store_;

            SharedResourcesPtr resources_;
            GLShaderProgramPtr active_shader_program_;
            std::string latest_colour_pipe_data_cacheid_;
            const std::string window_id_;
            bool use_ssbo_ = {false};
            bool use_alpha_ = {false};
            std::array<int, 4> scissor_coords_;
        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio
