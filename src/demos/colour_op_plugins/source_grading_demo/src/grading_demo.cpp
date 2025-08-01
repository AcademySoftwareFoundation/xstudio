// SPDX-License-Identifier: Apache-2.0

#include "xstudio/colour_pipeline/colour_operation.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio;

namespace {

const static utility::Uuid PLUGIN_UUID{"5598e01e-c6bc-4cf9-80ff-74bb560df12a"};

const char *glsl_shader_code = R"(
#version 430 core
uniform vec3 rgb_factor;

vec4 colour_transform_op(vec4 rgba) {
    return vec4(
        rgba.r*rgb_factor.r,
        rgba.g*rgb_factor.g,
        rgba.b*rgb_factor.b,
        rgba.a
        );
}
)";


class GradingDemoColourOp : public ColourOpPlugin {
  public:
    GradingDemoColourOp(caf::actor_config &cfg, const utility::JsonStore &init_settings);

    ~GradingDemoColourOp() override = default;

    float ordering() const override { return -100.0f; }

    ColourOperationDataPtr colour_op_graphics_data(
        utility::UuidActor &media_source,
        const utility::JsonStore &media_source_colour_metadata) override {
        return op_data_;
    }

    utility::JsonStore update_shader_uniforms(const media_reader::ImageBufPtr &image) override;

    void attribute_changed(
        const utility::Uuid &attribute_uuid, const int /*role*/
        ) override;

    void onscreen_media_source_changed(
        const utility::UuidActor &media_source,
        const utility::JsonStore &colour_params) override;

    module::FloatAttribute *red_   = {nullptr};
    module::FloatAttribute *green_ = {nullptr};
    module::FloatAttribute *blue_  = {nullptr};
    module::BooleanAttribute *tool_is_active_;

    ColourOperationDataPtr op_data_;

    utility::UuidActor on_screen_source_;
};

GradingDemoColourOp::GradingDemoColourOp(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourOpPlugin(cfg, "GradingDemoColourOp", init_settings) {

    // Here we add an attribute that carries custom QML code that instantiates
    // the toolbar button that launches the Dialog for applying grade values
    module::QmlCodeAttribute *button = add_qml_code_attribute(
        "MyCode",
        R"(
    import GradingDemo 1.0
    GradingDemoButton {
        anchors.fill: parent
    }
    )");

    // By adding our attribute to the 'media_tools_buttons' attribute group,
    // the xSTUDIO UI code will add the button to the media tools for us.
    button->expose_in_ui_attrs_group("media_tools_buttons");
    // The toolbar position is used to order the button among the others (left
    // to right) in the media tools button tray
    button->set_role_data(module::Attribute::ToolbarPosition, 500.0);

    // Now we define
    red_ = add_float_attribute("Red", "Red", 0.5f, 0.0f, 6.0f, 0.005f);
    red_->set_redraw_viewport_on_change(true);
    red_->set_role_data(module::Attribute::ToolTip, "Red channel multiplier");
    red_->expose_in_ui_attrs_group("grading_demo_controls");
    red_->set_role_data(module::Attribute::Colour, utility::ColourTriplet(1.0f, 0.0f, 0.0f));


    green_ = add_float_attribute("Green", "Green", 1.0f, 0.0f, 6.0f, 0.005f);
    green_->set_redraw_viewport_on_change(true);
    green_->set_role_data(module::Attribute::ToolTip, "Green channel multiplier");
    green_->expose_in_ui_attrs_group("grading_demo_controls");
    green_->set_role_data(module::Attribute::Colour, utility::ColourTriplet(0.0f, 1.0f, 0.0f));

    blue_ = add_float_attribute("Blue", "Blue", 0.0f, 0.0f, 6.0f, 0.005f);
    blue_->set_redraw_viewport_on_change(true);
    blue_->set_role_data(module::Attribute::ToolTip, "Blue channel multiplier");
    blue_->expose_in_ui_attrs_group("grading_demo_controls");
    blue_->set_role_data(module::Attribute::Colour, utility::ColourTriplet(0.0f, 0.0f, 1.0f));

    ColourOperationData *d = new ColourOperationData(PLUGIN_UUID, "Grade Demo OP");
    d->shader_.reset(new ui::opengl::OpenGLShader(PLUGIN_UUID, glsl_shader_code));
    op_data_.reset(d);


    tool_is_active_ =
        add_boolean_attribute("grading_tool_active", "grading_tool_active", false);

    tool_is_active_->expose_in_ui_attrs_group("grading_demo_settings");
    tool_is_active_->set_role_data(
        module::Attribute::MenuPaths,
        std::vector<std::string>({"panels_main_menu_items|Grading Tool Demo"}));
}

void GradingDemoColourOp::update_shader_uniforms(
    utility::JsonStore &uniforms_dict,
    const utility::Uuid & /*source_uuid*/,
    const utility::JsonStore & /*media_source_colour_metadata*/
) {}

utility::JsonStore
GradingDemoColourOp::update_shader_uniforms(const media_reader::ImageBufPtr &image)

    // for this simple plugin, the effect is global so we don't depend on
    // the media
    utility::JsonStore uniforms_dict;
// for this simple plugin, the effect is global so we don't depend on
// the media
if (tool_is_active_->value())
    uniforms_dict["rgb_factor"] = Imath::V3f(red_->value(), green_->value(), blue_->value());
else
    uniforms_dict["rgb_factor"] = Imath::V3f(1.0f, 1.0f, 1.0f);
return uniforms_dict;
}

void GradingDemoColourOp::attribute_changed(
    const utility::Uuid &attribute_uuid, const int role) {
    // Slight flaw here ... attribute_changed can be called during construction, before all
    // attrs have been created
    if ((red_ && green_ && blue_) &&
        (attribute_uuid == red_->uuid() || attribute_uuid == green_->uuid() ||
         attribute_uuid == blue_->uuid())) {

        // We construct a dictionary (json) object to carry our data
        Imath::V3f mult_vals(red_->value(), green_->value(), blue_->value());
        utility::JsonStore extra_colour_metadata;
        extra_colour_metadata["demo_grading_plugin_data"]["RGB_MULTIPLIER"] = mult_vals;

        // Get the extra metadata to be merged into
        onscreen_source_colour_metadata_merge(extra_colour_metadata);
    }
}

void GradingDemoColourOp::onscreen_media_source_changed(
    const utility::UuidActor &media_source, const utility::JsonStore &colour_params) {

    auto rgb_mult = colour_params.contains("demo_grading_plugin_data")
                        ? colour_params["demo_grading_plugin_data"].value(
                              "RGB_MULTIPLIER", Imath::V3f(1.0f, 1.0f, 1.0f))
                        : Imath::V3f(1.0f, 1.0f, 1.0f);

    // the 'false' arg stops attribute_changed from being called
    red_->set_value(rgb_mult.x, false);
    green_->set_value(rgb_mult.y, false);
    blue_->set_value(rgb_mult.z, false);
}


} // namespace
extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<GradingDemoColourOp>>(
                PLUGIN_UUID,
                "GradingDemoColourOp",
                plugin_manager::PluginType::PT_COLOUR_OPERATION,
                false,
                "Ted Waine",
                "Demo Colour Op that applies a per-source RGB multiplier.")}));
}
}
