// SPDX-License-Identifier: Apache-2.0

#include "xstudio/colour_pipeline/colour_operation.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio;

namespace {

const static utility::Uuid PLUGIN_UUID{"300bd462-d30c-4d24-a854-439c9ae94495"};

const char *glsl_shader_code = R"(
#version 430 core
uniform float solarise;

vec4 colour_transform_op(vec4 rgba) {
    return vec4(
        rgba.r >= solarise ? rgba.r : 1.0f - rgba.r,
        rgba.g >= solarise ? rgba.g : 1.0f - rgba.g,
        rgba.b >= solarise ? rgba.b : 1.0f - rgba.b,
        rgba.a
        );
}
)";


class SolariseOp : public ColourOpPlugin {
  public:
    SolariseOp(caf::actor_config &cfg, const utility::JsonStore &init_settings);

    ~SolariseOp() override = default;

    float ordering() const override { return 100.0f; }

    ColourOperationDataPtr colour_op_graphics_data(
        utility::UuidActor &media_source,
        const utility::JsonStore &media_source_colour_metadata) override {
        return op_data_;
    }

    utility::JsonStore update_shader_uniforms(const media_reader::ImageBufPtr &image) override;

    module::FloatAttribute *gamma_;
    ColourOperationDataPtr op_data_;
};

SolariseOp::SolariseOp(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourOpPlugin(cfg, "SolariseOp", init_settings) {

    gamma_ = add_float_attribute("Solarise", "Solarise", 0.0f, 0.0f, 6.0f, 0.005f);
    gamma_->set_redraw_viewport_on_change(true);
    gamma_->set_role_data(module::Attribute::ToolbarPosition, 4.5f);
    gamma_->set_role_data(module::Attribute::UIDataModels, nlohmann::json{"any_toolbar"});
    gamma_->set_role_data(module::Attribute::Activated, false);
    gamma_->set_role_data(module::Attribute::DefaultValue, 1.0f);
    gamma_->set_role_data(module::Attribute::ToolTip, "Set the viewport gamma");

    ColourOperationData *d = new ColourOperationData(PLUGIN_UUID, "Grade and Saturation OP");
    d->shader_.reset(new ui::opengl::OpenGLShader(PLUGIN_UUID, glsl_shader_code));
    op_data_.reset(d);
}

utility::JsonStore SolariseOp::update_shader_uniforms(const media_reader::ImageBufPtr &image)

    // for this simple plugin, the effect is global so we don't depend on
    // the media
    utility::JsonStore rt;
rt["solarise"] = gamma_->value();
return rt;
}

} // namespace
extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<SolariseOp>>(
                PLUGIN_UUID,
                "SolariseOp",
                plugin_manager::PluginType::PT_COLOUR_OPERATION,
                false,
                "Ted Waine",
                "Demo Colour Op that applies a solarise effect on the viewport.")}));
}
}
