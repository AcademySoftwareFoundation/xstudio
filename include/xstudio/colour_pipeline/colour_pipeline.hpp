// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "colour_lut.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/ui/viewport/shader.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xstudio {

namespace thumbnail {
    class ThumbnailBuffer;
    typedef std::shared_ptr<ThumbnailBuffer> ThumbnailBufferPtr;
} // namespace thumbnail

namespace colour_pipeline {

    struct ColourPipelineData {
        utility::Uuid colour_pipe_id_;
        std::string cache_id_;
        std::vector<ColourLUTPtr> luts_;
        ui::viewport::GPUShaderPtr main_viewport_shader_;
        ui::viewport::GPUShaderPtr popout_viewport_shader_;

        // user_data can be used by colour pipeline plugin to add any data specific
        // to a media source that the colour pipeline wants xstudio to cache and
        // pass back into the colour pipeline when re-evaluating shader, LUTs and
        // uniforms for final video display
        std::any user_data_;
        [[nodiscard]] size_t size() const;
    };

    struct ColourPipelineDataPtr {
        // Shader uniforms are moved away from ColourPipelineData to avoid
        // a mutex in viewport_renderer which would be needed because the
        // OCIO plugin can update those during the frame rendering.
        utility::JsonStore shader_parameters_;
        std::shared_ptr<ColourPipelineData> data_ptr_;

        ColourPipelineData *operator->() { return data_ptr_.get(); }
        ColourPipelineData &operator*() { return *data_ptr_; }
        explicit operator bool() const { return (bool)data_ptr_; }
        bool operator==(const ColourPipelineDataPtr &rhs) const {
            return shader_parameters_ == rhs.shader_parameters_ &&
                   data_ptr_.get() == rhs.data_ptr_.get();
        }
    };

    class ColourPipeline;
    typedef std::shared_ptr<ColourPipeline> ColourPipelinePtr;


    // xStudio module implementing OpenColorIO driven colour management.
    // Note that the methods below must be thread safe.
    class ColourPipeline : public module::Module {

      protected:
      public:
        enum Pipeline { OpenColorIO };

        ColourPipeline(
            const utility::JsonStore &,
            const utility::Uuid &pipe_uuid = utility::Uuid::generate())
            : Module("ColourPipeline"), uuid_(pipe_uuid) {}

        ~ColourPipeline() override = default;

        [[nodiscard]] virtual const utility::Uuid &class_uuid() const = 0;

        virtual ColourPipelineDataPtr make_empty_data() const = 0;

        [[nodiscard]] virtual std::string compute_hash(
            const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) = 0;

        virtual void setup_shader(
            ColourPipelineData &pipe_data,
            const utility::Uuid &source_uuid,
            const utility::JsonStore &colour_params) = 0;

        virtual void update_shader_uniforms(
            ColourPipelineDataPtr &pipe_data, const utility::Uuid &source_uuid) = 0;

        virtual void media_source_changed(
            const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) = 0;

        virtual void screen_changed(
            const bool &is_primary_viewer,
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber) = 0;

        virtual thumbnail::ThumbnailBufferPtr process_thumbnail(
            const media::AVFrameID &media_ptr, const thumbnail::ThumbnailBufferPtr &buf) = 0;

        virtual std::string fast_display_transform_hash(const media::AVFrameID &media_ptr) = 0;

      protected:
        utility::Uuid uuid_;
    };

} // namespace colour_pipeline
} // namespace xstudio
