// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"

#include "grading.h"
#include "grading_colour_op.hpp"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::colour_pipeline;
using namespace xstudio::ui::viewport;


namespace {

// N.B. Just one layer for now. The shader will become very bloated with
// several layers. Could we reuse one grading function in each layer instead
// and convert the uniforms to an array? See commented shader below for an
// example ...
static const int NUM_LAYERS = 1;

const char *fragment_shader = R"(
#version 430 core

uniform bool grade_tool_op;

// LayerDeclarations
vec4 colour_transform_op(vec4 rgba, vec2 image_pos) {

    vec4 image_col = rgba;

    if (grade_tool_op) {
        return image_col;
    }

    // LayerInvocations
    return image_col;
}
)";

const char *layer_template = R"(
// Layer<layerID>

uniform sampler2D layer<layerID>_mask;
uniform bool layer<layerID>_mask_active;
uniform bool layer<layerID>_mask_editing;
//OCIOTransform
vec4 apply_layer<layerID>(vec4 image_col, vec2 image_pos) {

    vec4 mask_color = layer<layerID>_mask_active ? texture(layer<layerID>_mask, image_pos) : vec4(1.0);
    float mask_alpha = clamp(mask_color.a, 0.0, 1.0);

    // Output color graded pixels
    if (layer<layerID>_mask_active && !layer<layerID>_mask_editing) {
        vec4 graded_col = OCIOLayer<layerID>(image_col);
        return vec4(mix(image_col.rgb, graded_col.rgb, mask_alpha), image_col.a);
    }
    // Output mask color pixels
    else if (layer<layerID>_mask_active) {
        float mask_opacity = 0.5 * mask_alpha;
        return vec4(mix(image_col.rgb, mask_color.rgb, mask_opacity), image_col.a);
    }
    else {
        return OCIOLayer<layerID>(image_col);
    }
}
)";

const char *layer_call = R"(
    image_col = apply_layer<layerID>(image_col, image_pos);
)";

// here's how it might look with uniform arrays and a single grading function:

/*const char * temp_static_shader = R"(
#version 430 core

uniform bool grade_tool_op;

// Layer0

uniform sampler2D layer_mask[8];
uniform bool layer_mask_active[8];
uniform bool layer_mask_editing[8];

// Declaration of all variables

uniform vec3 ocio_layer_grading_primary_offset[8];
uniform vec3 ocio_layer_grading_primary_exposure[8];
uniform vec3 ocio_layer_grading_primary_contrast[8];
uniform float ocio_layer_grading_primary_pivot[8];
uniform float ocio_layer_grading_primary_clampBlack[8];
uniform float ocio_layer_grading_primary_clampWhite[8];
uniform float ocio_layer_grading_primary_saturation[8];
uniform bool ocio_layer_grading_primary_localBypass[8];

// Declaration of the OCIO shader function

vec4 OCIOLayer0(vec4 inPixel, int layer_index)
{
  vec4 outColor = inPixel;

  // Add GradingPrimary 'linear' forward processing

  {
    if (!ocio_layer_grading_primary_localBypass[layer_index])
    {
      outColor.rgb += ocio_layer_grading_primary_offset[layer_index];
      outColor.rgb *= ocio_layer_grading_primary_exposure[layer_index];
      if ( ocio_layer_grading_primary_contrast[layer_index] != vec3(1., 1., 1.) )
      {
        outColor.rgb = pow(
            abs(outColor.rgb / ocio_layer_grading_primary_pivot[layer_index]),
            ocio_layer_grading_primary_contrast[layer_index] ) * sign(outColor.rgb) *
ocio_layer_grading_primary_pivot[layer_index];
      }
      vec3 lumaWgts = vec3(0.212599993, 0.715200007, 0.0722000003);
      float luma = dot( outColor.rgb, lumaWgts );
      outColor.rgb = luma + ocio_layer_grading_primary_saturation[layer_index] * (outColor.rgb -
luma); outColor.rgb = clamp( outColor.rgb, ocio_layer_grading_primary_clampBlack[layer_index],
        ocio_layer_grading_primary_clampWhite[layer_index]
        );
    }
  }

  return outColor;
}

vec4 apply_layer(vec4 image_col, vec2 image_pos, int layer_index) {

    vec4 mask_color = layer_mask_active[layer_index] ? texture(layer_mask[layer_index],
image_pos) : vec4(1.0); float mask_alpha = clamp(mask_color.a, 0.0, 1.0);

    // Output color graded pixels
    if (layer_mask_active[layer_index] && !layer_mask_editing[layer_index]) {
        vec4 graded_col = OCIOGradeFunc(image_col, layer_index);
        return vec4(mix(image_col.rgb, graded_col.rgb, mask_alpha), image_col.a);
    }
    // Output mask color pixels
    else if (layer_mask_active[layer_index]) {
        float mask_opacity = 0.5 * mask_alpha;
        return vec4(mix(image_col.rgb, mask_color.rgb, mask_opacity), image_col.a);
    }
    else {
        return OCIOGradeFunc(image_col, layer_index);
    }
}

vec4 colour_transform_op(vec4 rgba, vec2 image_pos) {

    vec4 image_col = rgba;

    if (grade_tool_op) {
        return image_col;
    }

    // would a for loop have better performance?
    if (apply_layer[0]) image_col = apply_layer(image_col, image_pos, 0);
    if (apply_layer[1]) image_col = apply_layer(image_col, image_pos, 1);
    if (apply_layer[2]) image_col = apply_layer(image_col, image_pos, 2);
    if (apply_layer[3]) image_col = apply_layer(image_col, image_pos, 3);
    if (apply_layer[4]) image_col = apply_layer(image_col, image_pos, 4);
    if (apply_layer[5]) image_col = apply_layer(image_col, image_pos, 5);
    if (apply_layer[6]) image_col = apply_layer(image_col, image_pos, 6);
    if (apply_layer[7]) image_col = apply_layer(image_col, image_pos, 7);

    return image_col;
}
)";*/

OCIO::GradingPrimary grading_primary_from_cdl(
    std::array<double, 4> slope,
    std::array<double, 4> offset,
    std::array<double, 4> power,
    double sat) {

    OCIO::GradingPrimary gp(OCIO::GRADING_LIN);

    for (int i = 0; i < 4; ++i) {
        if (slope[i] > 0) {
            offset[i] = offset[i] / slope[i];
            slope[i]  = std::log2(slope[i]);
        } else {
            slope[i] = std::numeric_limits<float>::lowest();
        }
    }

    // Lower bound on power is 0.01
    const float power_lower = 0.01f;
    for (int i = 0; i < 4; ++i) {
        if (power[i] < power_lower) {
            power[i] = power_lower;
        }
    }

    gp.m_offset     = OCIO::GradingRGBM(offset[0], offset[1], offset[2], offset[3]);
    gp.m_exposure   = OCIO::GradingRGBM(slope[0], slope[1], slope[2], slope[3]);
    gp.m_contrast   = OCIO::GradingRGBM(power[0], power[1], power[2], power[3]);
    gp.m_saturation = sat;
    gp.m_pivot      = std::log2(1.0 / 0.18);

    return gp;
}

} // anonymous namespace

GradingColourOperator::GradingColourOperator(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourOpPlugin(cfg, "GradingColourOperator", init_settings) {

    // the shader and any LUTs needed for colour transforms is static
    // so we only build it once
    build_shader_data();

    // ask plugin manager for the instance of the GradingTool plugin
    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
    request(pm, infinite, plugin_manager::get_resident_atom_v, GradingTool::PLUGIN_UUID)
        .then(
            [=](caf::actor grading_tool) mutable {
                // ping the grading tool with a pointer to ourselves, so it can
                // send us updates on the 'bypass' attr. GradingTool of course has
                // the necessary message handler for this
                anon_send(grading_tool, "follow_bypass", caf::actor_cast<caf::actor>(this));
            },
            [=](caf::error &err) mutable {

            });
}

caf::message_handler GradingColourOperator::message_handler_extensions() {

    // here's our handler for the messages coming from the GradintTool about
    // the state of its 'bypass' attribute.
    return caf::message_handler(
               {[=](utility::event_atom, const std::string &desc, bool bypass) {
                   if (desc == "bypass") {
                       bypass_ = bypass;
                   }
               }})
        .or_else(ColourOpPlugin::message_handler_extensions());
}

ColourOperationDataPtr GradingColourOperator::colour_op_graphics_data(
    utility::UuidActor &media_source, const utility::JsonStore &media_source_colour_metadata) {

    // N.B. 'colour_op_data_' is 'static' in that it is built once when this
    // class is constructed. If it becomes dynamic such that the shader and/or
    // LUTs it contains change depending on the grading data being displayed
    // then you must create new pointer data here
    return colour_op_data_;
}

utility::JsonStore
GradingColourOperator::update_shader_uniforms(const media_reader::ImageBufPtr &image) {

    utility::JsonStore uniforms_dict;
    if (!bypass_) {
        size_t layer_id = 0;
        for (auto &bookmark : image.bookmarks()) {

            auto data = dynamic_cast<GradingData *>(bookmark->annotation_.get());
            if (data) {

                for (auto &layer : *data) {

                    uniforms_dict[fmt::format("layer{}_mask_active", layer_id)] =
                        layer.mask_active();
                    uniforms_dict[fmt::format("layer{}_mask_editing", layer_id)] =
                        layer.mask_editing();

                    update_dynamic_parameters(
                        shader_data_[layer_id].shader_desc, layer.grade());
                    update_all_uniforms(shader_data_[layer_id].shader_desc, uniforms_dict);
                    layer_id++;

                    if (layer_id == NUM_LAYERS) {
                        // have a fixed number of layers for now
                        break;
                    }
                }
                break;
            }
        }
        if (layer_id) {
            uniforms_dict["grade_tool_op"] = bypass_;
        } else {
            // no grade. Turn off!
            uniforms_dict["grade_tool_op"] = true;
        }
    } else {
        uniforms_dict["grade_tool_op"] = true;
    }
    return uniforms_dict;
}

void GradingColourOperator::build_shader_data() {

    /*if (grading_data->size() == shader_data_.size()) return;

    shader_data_.clear();*/

    size_t layer_id = 0;
    for (size_t layer_id = 0; layer_id < NUM_LAYERS; ++layer_id) {

        auto desc = setup_ocio_shader(
            fmt::format("OCIOLayer{}", layer_id), fmt::format("ocio_layer{}_", layer_id));
        auto luts = setup_ocio_textures(desc);

        shader_data_.emplace_back(LayerShaderData{desc, luts});
        layer_id++;
    }

    setup_colourop_shader();

    std::string cache_id;

    cache_id += std::to_string(shader_data_.size());

    colour_op_data_ =
        std::make_shared<ColourOperationData>(ColourOperationData(PLUGIN_UUID, "Grade OP"));

    // we allow for LUTs in the grading operation (although for colour SOP no
    // LUTs are needed)
    std::vector<ColourLUTPtr> &luts = colour_op_data_->luts_;

    layer_id = 0;
    for (auto &data : shader_data_) {
        luts.insert(luts.end(), data.luts.begin(), data.luts.end());
        layer_id++;
    }

    if (!gradingop_shader_)
        setup_colourop_shader();
    colour_op_data_->shader_ = gradingop_shader_;
    colour_op_data_->luts_   = luts;
    // TODO: Update cache later when supporting colour space conversions
    colour_op_data_->cache_id_ = cache_id;
}

plugin::GPUPreDrawHookPtr
GradingColourOperator::make_pre_draw_gpu_hook(const int /*viewer_index*/) {
    return plugin::GPUPreDrawHookPtr(
        static_cast<plugin::GPUPreDrawHook *>(new GradingMaskRenderer()));
}

void GradingColourOperator::setup_colourop_shader() {

    std::string fs_str = fragment_shader;
    size_t curr_id     = 0;

    for (auto &data : shader_data_) {
        std::string layer_str = layer_template;

        layer_str = utility::replace_once(
            layer_str, "//OCIOTransform", data.shader_desc->getShaderText());

        fs_str = utility::replace_once(
            fs_str, "// LayerDeclarations", layer_str + std::string("\n// LayerDeclarations"));

        fs_str = utility::replace_once(
            fs_str, "// LayerInvocations", layer_call + std::string("// LayerInvocations"));

        fs_str = utility::replace_all(fs_str, "<layerID>", std::to_string(curr_id));

        curr_id++;
    }

    fs_str = utility::replace_once(fs_str, "// LayerDeclarations", "");
    fs_str = utility::replace_once(fs_str, "// LayerInvocations", "");

    gradingop_shader_ = std::make_shared<ui::opengl::OpenGLShader>(PLUGIN_UUID, fs_str);
}

OCIO::ConstGpuShaderDescRcPtr GradingColourOperator::setup_ocio_shader(
    const std::string &function_name, const std::string &resource_prefix) {

    // TODO: Use actual media OCIO config here to support colour space conversion
    auto config = OCIO::GetCurrentConfig();
    auto gp     = OCIO::GradingPrimaryTransform::Create(OCIO::GRADING_LIN);
    gp->makeDynamic();

    auto desc = OCIO::GpuShaderDesc::CreateShaderDesc();
    desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
    desc->setFunctionName(function_name.c_str());
    desc->setResourcePrefix(resource_prefix.c_str());

    auto gpu = config->getProcessor(gp)->getDefaultGPUProcessor();
    gpu->extractGpuShaderInfo(desc);

    return desc;
}

std::vector<ColourLUTPtr>
GradingColourOperator::setup_ocio_textures(OCIO::ConstGpuShaderDescRcPtr &shader) {

    std::vector<ColourLUTPtr> luts;

    // Process 3D LUTs
    const unsigned max_texture_3D = shader->getNum3DTextures();
    for (unsigned idx = 0; idx < max_texture_3D; ++idx) {
        const char *textureName           = nullptr;
        const char *samplerName           = nullptr;
        unsigned edgelen                  = 0;
        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;

        shader->get3DTexture(idx, textureName, samplerName, edgelen, interpolation);
        if (!textureName || !*textureName || !samplerName || !*samplerName || edgelen == 0) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::get3DTexture - The texture data is corrupted");
        }

        const float *ocio_lut_data = nullptr;
        shader->get3DTextureValues(idx, ocio_lut_data);
        if (!ocio_lut_data) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::get3DTextureValues - The texture values are missing");
        }

        auto xs_dtype    = LUTDescriptor::FLOAT32;
        auto xs_channels = LUTDescriptor::RGB;
        auto xs_interp   = interpolation == OCIO::INTERP_LINEAR ? LUTDescriptor::LINEAR
                                                                : LUTDescriptor::NEAREST;
        auto xs_lut      = std::make_shared<ColourLUT>(
            LUTDescriptor::Create3DLUT(edgelen, xs_dtype, xs_channels, xs_interp), samplerName);

        const int channels          = 3;
        const std::size_t data_size = edgelen * edgelen * edgelen * channels * sizeof(float);
        auto *xs_lut_data           = (float *)xs_lut->writeable_data();
        std::memcpy(xs_lut_data, ocio_lut_data, data_size);

        xs_lut->update_content_hash();
        luts.push_back(xs_lut);
    }

    // Process 1D LUTs
    const unsigned max_texture_2D = shader->getNumTextures();
    for (unsigned idx = 0; idx < max_texture_2D; ++idx) {
        const char *textureName                  = nullptr;
        const char *samplerName                  = nullptr;
        unsigned width                           = 0;
        unsigned height                          = 0;
        OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        OCIO::Interpolation interpolation        = OCIO::INTERP_LINEAR;

        shader->getTexture(
            idx, textureName, samplerName, width, height, channel, interpolation);

        if (!textureName || !*textureName || !samplerName || !*samplerName || width == 0) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::getTexture - The texture data is corrupted");
        }

        const float *ocio_lut_data = nullptr;
        shader->getTextureValues(idx, ocio_lut_data);
        if (!ocio_lut_data) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::getTextureValues - The texture values are missing");
        }

        auto xs_dtype    = LUTDescriptor::FLOAT32;
        auto xs_channels = channel == OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL
                               ? LUTDescriptor::RED
                               : LUTDescriptor::RGB;
        auto xs_interp   = interpolation == OCIO::INTERP_LINEAR ? LUTDescriptor::LINEAR
                                                                : LUTDescriptor::NEAREST;
        auto xs_lut      = std::make_shared<ColourLUT>(
            height > 1
                     ? LUTDescriptor::Create2DLUT(width, height, xs_dtype, xs_channels, xs_interp)
                     : LUTDescriptor::Create1DLUT(width, xs_dtype, xs_channels, xs_interp),
            samplerName);

        const int channels = channel == OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL ? 1 : 3;
        const std::size_t data_size = width * height * channels * sizeof(float);
        auto *xs_lut_data           = (float *)xs_lut->writeable_data();
        std::memcpy(xs_lut_data, ocio_lut_data, data_size);

        xs_lut->update_content_hash();
        luts.push_back(xs_lut);
    }

    return luts;
}

void GradingColourOperator::update_dynamic_parameters(
    OCIO::ConstGpuShaderDescRcPtr &shader, const ui::viewport::Grade &grade) const {

    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY)) {

        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY);
        OCIO::DynamicPropertyGradingPrimaryRcPtr primary_prop =
            OCIO::DynamicPropertyValue::AsGradingPrimary(property);

        OCIO::GradingPrimary gp = grading_primary_from_cdl(
            std::array<double, 4>{
                grade.slope[0], grade.slope[1], grade.slope[2], std::pow(2.0, grade.slope[3])},
            std::array<double, 4>{
                grade.offset[0], grade.offset[1], grade.offset[2], grade.offset[3]},
            std::array<double, 4>{
                grade.power[0], grade.power[1], grade.power[2], grade.power[3]},
            grade.sat);

        primary_prop->setValue(gp);
    }
}

void GradingColourOperator::update_all_uniforms(
    OCIO::ConstGpuShaderDescRcPtr &shader, utility::JsonStore &uniforms) const {

    const unsigned max_uniforms = shader->getNumUniforms();

    for (unsigned idx = 0; idx < max_uniforms; ++idx) {
        OCIO::GpuShaderDesc::UniformData uniform_data;
        const char *name = shader->getUniform(idx, uniform_data);

        switch (uniform_data.m_type) {
        case OCIO::UNIFORM_DOUBLE: {
            uniforms[name] = uniform_data.m_getDouble();
            break;
        }
        case OCIO::UNIFORM_BOOL: {
            // TODO: ColSci
            // This property is buggy at the moment and might report
            // grade_tool_op even though some fields are set (eg. only saturation).
            // This can be removed when upgrading to OCIO 2.3
            if (utility::ends_with(name, "grading_primary_localBypass")) {
                uniforms[name] = false;
            } else {
                uniforms[name] = uniform_data.m_getBool();
            }
            break;
        }
        case OCIO::UNIFORM_FLOAT3: {
            uniforms[name] = {
                "vec3",
                1,
                uniform_data.m_getFloat3()[0],
                uniform_data.m_getFloat3()[1],
                uniform_data.m_getFloat3()[2]};
            break;
        }
        default:
            break;
        }
    }
}