// SPDX-License-Identifier: Apache-2.0
#pragma once

#pragma once

#include "colour_pipeline.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

namespace xstudio {

namespace colour_pipeline {

    // Base class for implementing colour operation plugins. A colour operation
    // transforms an RGBA value by defining glsl fragment shader that
    // includes a function with the following signature (exactly):
    // vec4 colour_transform_op(vec4 rgba, vec2 image_pos);
    //
    // The function operates on the linear RGBA fragment colour before it is
    // passed to the display shader that applies the display LUT, for example.
    //
    // By adding appropriate attributes to your class you can create UI elements
    // that will modify the attributes. By naming attributes the same as
    // uniforms in your shader, xSTUDIO will ensure that the uniform value is
    // updated at draw time to match the correspondin attribute value.
    class ColourOpPlugin : public plugin::StandardPlugin {

      protected:
      public:
        ColourOpPlugin(
            caf::actor_config &cfg, std::string name, const utility::JsonStore &init_settings)
            : StandardPlugin(cfg, name, init_settings) {}

        ~ColourOpPlugin() override = default;

        /* This float value determines the order that the colour operation is
        applied within a chain of operations, if there are multiple ColourOpPlugin
        plugins active. For example, if you are implementing an exposure function
        that should be applied *After* all other colour operations, return FLT_MAX
        or appropriate high number. */
        [[nodiscard]] virtual float ordering() const = 0;

        /* For the given image, return the colour operation data to be applied
        to the linearised RGB pixel values of that image.
        The which data would include (for OpenGL viewport) a required glsl
        shader which implements the colour transform maths of your colour
        operator, with optional LUT data that may also be used to apply a
        colour operation.
        Typically this result will be static for all sources but there is the
        possibility to have data that depends on properties (like metadata) of
        the media_source if required. It is up to the plugin write to make this
        call efficient and have cacheing of shader data where that might
        be appropriate.*/
        virtual ColourOperationDataPtr colour_op_graphics_data(
            utility::UuidActor &media_source,
            const utility::JsonStore &media_source_colour_metadata) = 0;

        /* This function is called when the on-screen media source changes.
        Reimplement to update your plugin attributes in response, as required.
        For example, if your plugin has sliders that apply grades, you might want
        to read grade data already written to the media source metadata in order
        to update the attributes connected to the corresponding sliders.*/
        virtual void onscreen_media_source_changed(
            const utility::UuidActor &media_source, const utility::JsonStore &colour_params) {}

        /* For the given image build a dictionary of shader uniform names and
        their corresponding values to be used to set the uniform values in your
        shader at draw-time - keys should match the names of uniforms in your
        shader and values should match the type of your uniform.
        For vec3 types etc us Imath::V3f for example.*/
        virtual utility::JsonStore
        update_shader_uniforms(const media_reader::ImageBufPtr &image) = 0;

        /* Call this function with custom metadata to be merged into the colour
        metadata of the current on screen source */
        void onscreen_source_colour_metadata_merge(
            const utility::JsonStore &additional_colour_params);

      protected:
        caf::message_handler message_handler_extensions() override;

        utility::UuidActor on_screen_source_;
    };

} // namespace colour_pipeline
} // namespace xstudio
