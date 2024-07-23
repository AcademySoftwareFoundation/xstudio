// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <OpenColorIO/OpenColorIO.h> //NOLINT

#include "xstudio/colour_pipeline/colour_operation.hpp"
#include "grading_data.h"
#include "grading_mask_gl_renderer.h"

namespace OCIO = OCIO_NAMESPACE;

namespace xstudio::colour_pipeline {

class GradingColourOperator : public ColourOpPlugin {

  public:
    inline static const utility::Uuid PLUGIN_UUID =
        utility::Uuid("b78e2aff-4709-46a1-9db2-61260997d401");

  public:
    GradingColourOperator(caf::actor_config &cfg, const utility::JsonStore &init_settings);
    ~GradingColourOperator() override = default;

    // Colour grading

    float ordering() const override { return -100.0f; }

    ColourOperationDataPtr colour_op_graphics_data(
        utility::UuidActor &media_source,
        const utility::JsonStore &media_source_colour_metadata) override;

    utility::JsonStore update_shader_uniforms(const media_reader::ImageBufPtr &image) override;

    plugin::GPUPreDrawHookPtr make_pre_draw_gpu_hook(const int /*viewer_index*/) override;

  protected:
    caf::message_handler message_handler_extensions() override;

  private:
    void build_shader_data();

    void setup_colourop_shader();

    OCIO::ConstGpuShaderDescRcPtr
    setup_ocio_shader(const std::string &function_name, const std::string &resource_prefix);

    std::vector<ColourLUTPtr> setup_ocio_textures(OCIO::ConstGpuShaderDescRcPtr &shader);

    void update_dynamic_parameters(
        OCIO::ConstGpuShaderDescRcPtr &shader, const ui::viewport::Grade &grade) const;

    void update_all_uniforms(
        OCIO::ConstGpuShaderDescRcPtr &shader, utility::JsonStore &uniforms) const;

    ui::viewport::GPUShaderPtr gradingop_shader_;

    struct LayerShaderData {
        OCIO::ConstGpuShaderDescRcPtr shader_desc;
        std::vector<ColourLUTPtr> luts;
    };
    using GradingShaderData = std::vector<LayerShaderData>;

    GradingShaderData shader_data_;

    ColourOperationDataPtr colour_op_data_;

    bool bypass_ = {false};
};

} // namespace xstudio::colour_pipeline