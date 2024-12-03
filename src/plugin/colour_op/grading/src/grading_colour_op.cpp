// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"

#include "grading.h"
#include "grading_colour_op.hpp"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_common.h"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::colour_pipeline;
using namespace xstudio::ui::viewport;


namespace {


const char *fragment_shader = R"(
#version 430 core

#define SCENE_LINEAR 0
#define COMPOSITING_LOG 1

struct Grade {
    int color_space;
    bool grade_active;

    // Masking
    bool mask_active;
    bool mask_editing;

    // CDL
    vec3 slope;
    vec3 offset;
    vec3 power;
    float sat;

    float exposure;
    float contrast;

    // Can be extended with more grading operations
};

uniform bool      tool_active;
uniform int       grade_count;
uniform sampler2D masks[32];
uniform Grade     grades[32];

vec4 apply_grade(vec4 rgba, Grade grade)
{
    vec4 outColor = rgba;

    if (!grade.grade_active)
    {
        return outColor;
    }


    // Exposure

    if (grade.exposure != 0.)
    {
        outColor.rgb *= pow(2.0, grade.exposure);
    }

    // Contrast

    if (grade.contrast != 1.)
    {
        outColor.rgb = pow( abs(outColor.rgb / 0.18), vec3(grade.contrast)) * sign(outColor.rgb) * 0.18;
    }

    // CDL SOP

    if (grade.slope != vec3(1., 1., 1.)
     || grade.offset != vec3(0., 0., 0.)
     || grade.power != vec3(1., 1., 1.))
    {
        outColor.rgb *= grade.slope;
        outColor.rgb += grade.offset;
        if (grade.power != vec3(1., 1., 1.))
        {
            // Strict CDL specs do not match Nuke, using OCIO mirrored style here.
            // outColor.rgb = pow(clamp(outColor.rgb, 0.0, 1.0), grade.power);
            outColor.rgb = pow(abs(outColor.rgb), grade.power) * sign(outColor.rgb);
        }
    }

    // CDL Sat

    if (grade.sat != 1.)
    {
        vec3 lumaWgts = vec3(0.212599993, 0.715200007, 0.0722000003);
        float luma = dot(outColor.rgb, lumaWgts);
        outColor.rgb = luma + grade.sat * (outColor.rgb - luma);
    }

    // Can be extended with more grading operations

    return outColor;
}

vec4 apply_layer(vec4 rgba, vec2 image_pos, int layer_index)
{
    Grade grade = grades[layer_index];

    vec4 mask_color = grade.mask_active ? texture(masks[layer_index], image_pos) : vec4(1.0);
    float mask_alpha = clamp(mask_color.a, 0.0, 1.0);

    if (grade.mask_active && !grade.mask_editing)
    {
        vec4 graded_col = apply_grade(rgba, grade);
        return vec4(mix(rgba.rgb, graded_col.rgb, mask_alpha), rgba.a);
    }
    else if (grade.mask_active)
    {
        float mask_opacity = 0.5 * mask_alpha;
        return vec4(mix(rgba.rgb, mask_color.rgb, mask_opacity), rgba.a);
    }
    else
    {
        return apply_grade(rgba, grade);
    }
}

//INJECT_LIN_TO_LOG
//INJECT_LOG_TO_LIN

vec4 apply_color_conv(vec4 rgba, int source_space, int dest_space)
{
    if (source_space != dest_space)
    {
        if (source_space == SCENE_LINEAR)
        {
            rgba = OCIOLinToLog(rgba);
        }
        else
        {
            rgba = OCIOLogToLin(rgba);
        }
    }

    return rgba;
}

vec4 colour_transform_op(vec4 rgba, vec2 image_pos)
{
    vec4 image_col = rgba;

    if (tool_active)
    {
        // xStudio guarantee conversion to scene_linear
        int current_space = SCENE_LINEAR;

        for (int i = 0; i < grade_count; ++i)
        {
            int grade_space = grades[i].color_space;
            if (grade_space != current_space)
            {
                image_col = apply_color_conv(image_col, current_space, grade_space);
                current_space = grade_space;
            }
            image_col = apply_layer(image_col, image_pos, i);
        }

        if (current_space != SCENE_LINEAR)
        {
            image_col = apply_color_conv(image_col, current_space, SCENE_LINEAR);
        }
    }

    return image_col;
}
)";

} // anonymous namespace

GradingColourOperator::GradingColourOperator(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourOpPlugin(cfg, "GradingColourOperator", init_settings) {

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

    // here's our handler for the messages coming from the GradingTool about
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

    // The result of this call will depend on the ocio config, as the grading op
    // depends on the lin to lon transform which generally varies per ocio config.

    // It's also possible that the result will depend on the ocio_context because
    // for some jobs the log to lin transform varies PER SHOT. To Be extra safe
    // we make a hash from the whole context data dictionary

    std::string hash_data;
    if (media_source_colour_metadata.contains("ocio_config")) {
        hash_data += media_source_colour_metadata.get_or("ocio_config", std::string(""));
    }
    if (media_source_colour_metadata.contains("ocio_context")) {
        hash_data += media_source_colour_metadata["ocio_context"].dump();
    }

    size_t hash = std::hash<std::string>{}(hash_data);
    auto p      = colour_op_data_cache_.find(hash);
    if (p != colour_op_data_cache_.end()) {
        return p->second;
    }

    colour_op_data_cache_[hash] = setup_shader_data(media_source_colour_metadata);
    return colour_op_data_cache_[hash];
}

utility::JsonStore
GradingColourOperator::update_shader_uniforms(const media_reader::ImageBufPtr &image) {

    utility::JsonStore uniforms_dict;
    uniforms_dict["grade_count"] = 0;
    uniforms_dict["tool_active"] = false;

    if (bypass_) {
        return uniforms_dict;
    }

    size_t grade_count = 0;
    auto active_grades = get_active_grades(image);
    for (const auto grade_info : active_grades) {

        auto grade_data = grade_info.data;

        std::string grade_str = fmt::format("grades[{}].", grade_count);

        uniforms_dict[grade_str + "grade_active"] = grade_info.isActive;

        // We only support compositing_log as colour space conversion for now.
        // All other values will be treated as being the current colour space.
        uniforms_dict[grade_str + "color_space"] =
            grade_data->colour_space() == "compositing_log" ? 1 : 0;
        uniforms_dict[grade_str + "mask_active"]  = !grade_data->mask().empty();
        uniforms_dict[grade_str + "mask_editing"] = grade_data->mask_editing();
        uniforms_dict[grade_str + "slope"]        = {
            "vec3",
            1,
            grade_data->grade().slope[0] * grade_data->grade().slope[3],
            grade_data->grade().slope[1] * grade_data->grade().slope[3],
            grade_data->grade().slope[2] * grade_data->grade().slope[3]};
        uniforms_dict[grade_str + "offset"] = {
            "vec3",
            1,
            grade_data->grade().offset[0] + grade_data->grade().offset[3],
            grade_data->grade().offset[1] + grade_data->grade().offset[3],
            grade_data->grade().offset[2] + grade_data->grade().offset[3]};
        uniforms_dict[grade_str + "power"] = {
            "vec3",
            1,
            grade_data->grade().power[0] * grade_data->grade().power[3],
            grade_data->grade().power[1] * grade_data->grade().power[3],
            grade_data->grade().power[2] * grade_data->grade().power[3]};
        uniforms_dict[grade_str + "sat"]      = grade_data->grade().sat;
        uniforms_dict[grade_str + "exposure"] = grade_data->grade().exposure;
        uniforms_dict[grade_str + "contrast"] = grade_data->grade().contrast;

        grade_count++;
    }

    if (grade_count) {
        uniforms_dict["grade_count"] = grade_count;
        uniforms_dict["tool_active"] = true;
    }

    return uniforms_dict;
}

std::shared_ptr<ColourOperationData> GradingColourOperator::setup_shader_data(
    const utility::JsonStore &media_source_colour_metadata) {

    auto colour_op_data =
        std::make_shared<ColourOperationData>(ColourOperationData(PLUGIN_UUID, "Grade OP"));

    std::string fs_str                 = fragment_shader;
    std::vector<ColourLUTPtr> &fs_luts = colour_op_data->luts_;

    // Inject colour transforms
    {
        auto desc = setup_ocio_shader(
            "OCIOLinToLog",
            "ocio_lin_to_log",
            media_source_colour_metadata,
            "scene_linear",
            "compositing_log");
        fs_str    = utility::replace_once(fs_str, "//INJECT_LIN_TO_LOG", desc->getShaderText());
        auto luts = setup_ocio_textures(desc);
        fs_luts.insert(fs_luts.end(), luts.begin(), luts.end());
    }
    {
        auto desc = setup_ocio_shader(
            "OCIOLogToLin",
            "ocio_log_to_lin",
            media_source_colour_metadata,
            "compositing_log",
            "scene_linear");
        fs_str    = utility::replace_once(fs_str, "//INJECT_LOG_TO_LIN", desc->getShaderText());
        auto luts = setup_ocio_textures(desc);
        fs_luts.insert(fs_luts.end(), luts.begin(), luts.end());
    }

    gradingop_shader_         = std::make_shared<ui::opengl::OpenGLShader>(PLUGIN_UUID, fs_str);
    colour_op_data->shader_   = gradingop_shader_;
    colour_op_data->set_cache_id(fmt::format("{}", std::hash<std::string_view>{}(fragment_shader)));
    for (const auto &lut : fs_luts) {
        colour_op_data->set_cache_id(colour_op_data->cache_id() + fmt::format("{}",lut->cache_id()));
    }
    return colour_op_data;
}

plugin::GPUPreDrawHookPtr GradingColourOperator::make_pre_draw_gpu_hook() {
    return std::make_shared<GradingMaskRenderer>();
}

OCIO::ConstGpuShaderDescRcPtr GradingColourOperator::setup_ocio_shader(
    const std::string &function_name,
    const std::string &resource_prefix,
    const utility::JsonStore &metadata,
    const std::string &src,
    const std::string &dst) {

    auto desc = OCIO::GpuShaderDesc::CreateShaderDesc();
    desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
    desc->setFunctionName(function_name.c_str());
    desc->setResourcePrefix(resource_prefix.c_str());

    try {
        const std::string config_name = metadata.get_or("ocio_config", std::string(""));
        auto config                   = OCIO::Config::CreateFromFile(config_name.c_str());

        auto context = config->getCurrentContext()->createEditableCopy();
        if (metadata.contains("ocio_context")) {
            if (metadata["ocio_context"].is_object()) {
                for (auto &item : metadata["ocio_context"].items()) {
                    context->setStringVar(
                        item.key().c_str(), std::string(item.value()).c_str());
                }
            }
        }

        auto gpu =
            config->getProcessor(context, src.c_str(), dst.c_str())->getDefaultGPUProcessor();
        gpu->extractGpuShaderInfo(desc);
        return desc;
    } catch (const OCIO::Exception &ex) {
        auto config = OCIO::Config::CreateRaw();
        auto gpu    = config->getProcessor("raw", "raw")->getDefaultGPUProcessor();
        gpu->extractGpuShaderInfo(desc);
        return desc;
    }
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
