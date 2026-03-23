#include "ocio_shader.hpp"

#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio::colour_pipeline::ocio;
using namespace xstudio;


namespace {

OCIO::GradingPrimary grading_primary_from_cdl(const OCIO::ConstCDLTransformRcPtr &cdl) {

    OCIO::GradingPrimary gp(OCIO::GRADING_LIN);

    std::array<double, 3> slope;
    std::array<double, 3> offset;
    std::array<double, 3> power;
    double sat;

    cdl->getSlope(slope.data());
    cdl->getOffset(offset.data());
    cdl->getPower(power.data());
    sat = cdl->getSat();

    // TODO: ColSci
    // GradingPrimary with only saturation is currently considered identity
    // Temporary workaround until OCIO fix the issue by adding a clamp to
    // an arbitrary low value.
    if (slope == decltype(slope){1.0, 1.0, 1.0} && offset == decltype(offset){0.0, 0.0, 0.0} &&
        power == decltype(power){1.0, 1.0, 1.0} && sat != 1.0) {
        gp.m_clampBlack = -1e6;
    }

    for (int i = 0; i < 3; ++i) {
        offset[i] = offset[i] / slope[i];
        slope[i]  = std::log2(slope[i]);
    }

    gp.m_offset     = OCIO::GradingRGBM(offset[0], offset[1], offset[2], 0.0);
    gp.m_exposure   = OCIO::GradingRGBM(slope[0], slope[1], slope[2], 0.0);
    gp.m_contrast   = OCIO::GradingRGBM(power[0], power[1], power[2], 1.0);
    gp.m_saturation = sat;
    gp.m_pivot      = std::log2(1.0 / 0.18);

    return gp;
}

OCIO::TransformRcPtr to_dynamic_transform(
    const OCIO::ConstContextRcPtr &context,
    const OCIO::ConstTransformRcPtr &transform,
    DynamicCDLMap &dynamic_cdls,
    int &dynamic_cdls_start_idx) {

    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_GROUP) {
        auto group     = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(transform);
        auto new_group = OCIO::GroupTransform::Create();

        int num_transforms = group->getNumTransforms();
        for (int i = 0; i < num_transforms; ++i) {
            auto dyn_transform = to_dynamic_transform(
                context, group->getTransform(i), dynamic_cdls, dynamic_cdls_start_idx);
            new_group->appendTransform(dyn_transform);
        }
        return OCIO::DynamicPtrCast<OCIO::Transform>(new_group);
    } else if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_FILE) {
        auto file = OCIO::DynamicPtrCast<const OCIO::FileTransform>(transform);

        for (const auto &[name, dynamic_cdl] : dynamic_cdls) {
            if (std::string(file->getSrc()) == dynamic_cdl.file_name) {

                try {
                    // Make sure the file is really a CDL
                    auto file_path = context->resolveFileLocation(
                        std::string(dynamic_cdl.file_name).c_str());
                    OCIO::CDLTransform::CreateFromFile(file_path, "" /* cccid */);
                    dynamic_cdls[name].resolved_path = file_path;

                    if (dynamic_cdl.id == -1) {
                        dynamic_cdls[name].id = dynamic_cdls_start_idx++;
                    }

                    auto cdl_transform = OCIO::CDLTransform::CreateFromFile(
                        dynamic_cdl.resolved_path.c_str(), "");
                    dynamic_cdls[name].grading_primary =
                        grading_primary_from_cdl(cdl_transform);
                    ;
                    dynamic_cdls[name].in_use = true;

                    OCIO::GradingPrimary cdl_gp(OCIO::GRADING_LIN);
                    int cdl_id      = dynamic_cdls[name].id;
                    cdl_gp.m_offset = OCIO::GradingRGBM(cdl_id, cdl_id, cdl_id, 0.0);

                    auto gp = OCIO::GradingPrimaryTransform::Create(OCIO::GRADING_LIN);
                    gp->setValue(cdl_gp);

                    return OCIO::DynamicPtrCast<OCIO::Transform>(gp);
                } catch (OCIO::Exception &e) {
                    // We could not resolve the file path, ignore
                }
            }
        }
    }

    return transform->createEditableCopy();
}

OCIO::TransformRcPtr to_dynamic_cdl_transform(
    const OCIO::ConstCDLTransformRcPtr &transform,
    DynamicCDLMap &dynamic_cdls,
    int &dynamic_cdls_start_idx) {

    auto cdl_name = transform->getFormatMetadata().getAttributeValue("name");

    for (const auto &[name, dynamic_cdl] : dynamic_cdls) {
        if (cdl_name == dynamic_cdl.look_name) {
            if (dynamic_cdl.id == -1) {
                dynamic_cdls[name].id = dynamic_cdls_start_idx++;
            }
            dynamic_cdls[name].grading_primary = grading_primary_from_cdl(transform);
            ;
            dynamic_cdls[name].in_use = true;

            OCIO::GradingPrimary cdl_gp(OCIO::GRADING_LIN);
            int cdl_id      = dynamic_cdls[name].id;
            cdl_gp.m_offset = OCIO::GradingRGBM(cdl_id, cdl_id, cdl_id, 0.0);

            auto gp = OCIO::GradingPrimaryTransform::Create(OCIO::GRADING_LIN);
            gp->setValue(cdl_gp);

            return OCIO::DynamicPtrCast<OCIO::Transform>(gp);
        }
    }

    return transform->createEditableCopy();
}

OCIO::ConfigRcPtr to_dynamic_config(
    const OCIO::ConstConfigRcPtr &config,
    const OCIO::ConstContextRcPtr &context,
    DynamicCDLMap &dynamic_cdls) {

    // Need at least v2 for GradingPrimaryTransform
    auto dynamic_config = config->createEditableCopy();
    if (dynamic_config->getMajorVersion() < 2) {
        dynamic_config->setMajorVersion(2);
        dynamic_config->setMinorVersion(0);
    }

    // Turn every dynamic CDL FileTransform into GradingPrimary

    int dynamic_cdls_start_idx = 4096;

    // Grab all transforms from the ColorSpaces.
    OCIO::SearchReferenceSpaceType cs_type = OCIO::SEARCH_REFERENCE_SPACE_ALL;
    OCIO::ColorSpaceVisibility cs_vis      = OCIO::COLORSPACE_ALL;
    for (int i = 0; i < dynamic_config->getNumColorSpaces(cs_type, cs_vis); ++i) {

        const auto cs_name = dynamic_config->getColorSpaceNameByIndex(cs_type, cs_vis, i);
        auto cs            = dynamic_config->getColorSpace(cs_name);
        auto cs_edit       = cs->createEditableCopy();

        auto to_ref = cs->getTransform(OCIO::COLORSPACE_DIR_TO_REFERENCE);
        if (to_ref) {
            auto new_tr =
                to_dynamic_transform(context, to_ref, dynamic_cdls, dynamic_cdls_start_idx);
            cs_edit->setTransform(new_tr, OCIO::COLORSPACE_DIR_TO_REFERENCE);
        }

        auto from_ref = cs->getTransform(OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        if (from_ref) {
            auto new_tr =
                to_dynamic_transform(context, from_ref, dynamic_cdls, dynamic_cdls_start_idx);
            cs_edit->setTransform(new_tr, OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        }

        dynamic_config->addColorSpace(cs_edit);
    }

    // Grab all transforms from the Looks.
    for (int i = 0; i < dynamic_config->getNumLooks(); ++i) {

        auto look      = dynamic_config->getLook(dynamic_config->getLookNameByIndex(i));
        auto look_edit = look->createEditableCopy();

        auto fwd_tr = look->getTransform();
        if (fwd_tr) {
            auto new_tr =
                to_dynamic_transform(context, fwd_tr, dynamic_cdls, dynamic_cdls_start_idx);
            look_edit->setTransform(new_tr);
        }

        auto inv_tr = look->getInverseTransform();
        if (inv_tr) {
            auto new_tr =
                to_dynamic_transform(context, inv_tr, dynamic_cdls, dynamic_cdls_start_idx);
            look_edit->setInverseTransform(new_tr);
        }

        dynamic_config->addLook(look_edit);
    }

    // Grab all transforms from the view transforms.
    for (int i = 0; i < dynamic_config->getNumViewTransforms(); ++i) {

        const auto vt_name = dynamic_config->getViewTransformNameByIndex(i);
        auto vt            = dynamic_config->getViewTransform(vt_name);
        auto vt_edit       = vt->createEditableCopy();

        auto to_ref = vt->getTransform(OCIO::VIEWTRANSFORM_DIR_TO_REFERENCE);
        if (to_ref) {
            auto new_tr =
                to_dynamic_transform(context, to_ref, dynamic_cdls, dynamic_cdls_start_idx);
            vt_edit->setTransform(new_tr, OCIO::VIEWTRANSFORM_DIR_TO_REFERENCE);
        }

        auto from_ref = vt->getTransform(OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
        if (from_ref) {
            auto new_tr =
                to_dynamic_transform(context, from_ref, dynamic_cdls, dynamic_cdls_start_idx);
            vt_edit->setTransform(new_tr, OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
        }

        dynamic_config->addViewTransform(vt_edit);
    }

    // Grab all transforms from the named transforms.
    for (int i = 0; i < dynamic_config->getNumNamedTransforms(); ++i) {

        const auto nt_name = dynamic_config->getNamedTransformNameByIndex(i);
        auto nt            = dynamic_config->getNamedTransform(nt_name);
        auto nt_edit       = nt->createEditableCopy();

        auto fwd_tr = nt->getTransform(OCIO::TRANSFORM_DIR_FORWARD);
        if (fwd_tr) {
            auto new_tr =
                to_dynamic_transform(context, fwd_tr, dynamic_cdls, dynamic_cdls_start_idx);
            nt_edit->setTransform(new_tr, OCIO::TRANSFORM_DIR_FORWARD);
        }

        auto inv_tr = nt->getTransform(OCIO::TRANSFORM_DIR_INVERSE);
        if (inv_tr) {
            auto new_tr =
                to_dynamic_transform(context, inv_tr, dynamic_cdls, dynamic_cdls_start_idx);
            nt_edit->setTransform(new_tr, OCIO::TRANSFORM_DIR_INVERSE);
        }

        dynamic_config->addNamedTransform(nt_edit);
    }

    return dynamic_config;
}

OCIO::ConstProcessorRcPtr to_dynamic_processor(
    const OCIO::ConstConfigRcPtr &config,
    const OCIO::ConstContextRcPtr &context,
    OCIO::ConstProcessorRcPtr &processor,
    DynamicCDLMap &dynamic_cdls) {

    // Need at least v2 for GradingPrimaryTransform
    auto dynamic_config = config->createEditableCopy();
    if (dynamic_config->getMajorVersion() < 2) {
        dynamic_config->setMajorVersion(2);
        dynamic_config->setMinorVersion(0);
    }

    // Turn every dynamic CDLTransform into GradingPrimary

    int dynamic_cdls_start_idx = 4096;

    auto group         = processor->createGroupTransform();
    auto dynamic_group = OCIO::GroupTransform::Create();

    for (int i = 0; i < group->getNumTransforms(); ++i) {
        if (group->getTransform(i)->getTransformType() == OCIO::TRANSFORM_TYPE_CDL) {
            OCIO::CDLTransformRcPtr cdl =
                OCIO::DynamicPtrCast<OCIO::CDLTransform>(group->getTransform(i));
            dynamic_group->appendTransform(
                to_dynamic_cdl_transform(cdl, dynamic_cdls, dynamic_cdls_start_idx));
        } else {
            dynamic_group->appendTransform(group->getTransform(i));
        }
    }

    return dynamic_config->getProcessor(context, dynamic_group, OCIO::TRANSFORM_DIR_FORWARD);
}

std::string to_dynamic_shader(const std::string &shader_text, DynamicCDLMap &dynamic_cdls) {

    std::vector<std::string> new_shader_lines;

    auto lines = utility::split(shader_text, '\n');
    for (int i = 0; i < lines.size(); ++i) {
        if (utility::starts_with(lines[i], "  // Add GradingPrimary")) {
            for (auto &[name, dynamic_cdl] : dynamic_cdls) {
                if (lines[i + 3].find(std::to_string(dynamic_cdl.id)) != std::string::npos) {

                    std::string new_code =
                        R"(  // Add GradingPrimary 'linear' forward processing for __name__

{
    if (!__name___localBypass)
    {
    outColor.rgb += __name___offset;
    outColor.rgb *= __name___exposure;
    if ( __name___contrast != vec3(1., 1., 1.) )
    {
        outColor.rgb = pow( abs(outColor.rgb / __name___pivot), __name___contrast ) * sign(outColor.rgb) * __name___pivot;
    }
    vec3 lumaWgts = vec3(0.212599993, 0.715200007, 0.0722000003);
    float luma = dot( outColor.rgb, lumaWgts );
    outColor.rgb = luma + __name___saturation * (outColor.rgb - luma);
    outColor.rgb = clamp( outColor.rgb, __name___clampBlack, __name___clampWhite );
    }
}
)";
                    new_code            = utility::replace_all(new_code, "__name__", name);
                    auto new_code_lines = utility::split(new_code, '\n');
                    new_shader_lines.insert(
                        new_shader_lines.end(), new_code_lines.begin(), new_code_lines.end());

                    std::string new_declarations =
                        R"(
uniform vec3 __name___offset;
uniform vec3 __name___exposure;
uniform vec3 __name___contrast;
uniform float __name___pivot;
uniform float __name___clampBlack;
uniform float __name___clampWhite;
uniform float __name___saturation;
uniform bool __name___localBypass;)";

                    new_declarations = utility::replace_all(new_declarations, "__name__", name);
                    new_code_lines   = utility::split(new_declarations, '\n');
                    new_shader_lines.insert(
                        new_shader_lines.begin(), new_code_lines.begin(), new_code_lines.end());

                    // Find the offset to the closing scope and skip lines
                    int offset_to_end = 0;
                    bool offset_found = false;
                    while (!offset_found) {
                        if (lines[i + offset_to_end] == std::string("  }")) {
                            offset_found = true;
                        } else {
                            offset_to_end++;
                        }
                    }
                    i = i + offset_to_end;

                    break;
                }
            }
        } else {
            new_shader_lines.push_back(lines[i]);
        }
    }

    auto shader = utility::join_as_string(new_shader_lines, "\n");

    return shader;
}

} // anonymous namespace


ShaderBuilder &ShaderBuilder::setDynamicCDLs(const DynamicCDLMap &cdls) {
    _dynamic_cdls = cdls;
    return *this;
}

ShaderBuilder &ShaderBuilder::setDynamicCDLMode(DynamicCDLMode mode) {
    _dynamic_cdl_mode = mode;
    return *this;
}

ShaderBuilder &ShaderBuilder::setConfig(const OCIO::ConstConfigRcPtr &config) {
    _config = config;
    return *this;
}

ShaderBuilder &ShaderBuilder::setContext(const OCIO::ConstContextRcPtr &context) {
    _context = context;
    return *this;
}

ShaderBuilder &ShaderBuilder::setTransform(const OCIO::ConstTransformRcPtr &transform) {
    _transform = transform;
    return *this;
}

ShaderBuilder &ShaderBuilder::setLanguage(OCIO::GpuLanguage lang) {
    _lang = lang;
    return *this;
}

ShaderBuilder &ShaderBuilder::setFunctionName(const std::string &func) {
    _func_name = func;
    return *this;
}

ShaderBuilder &ShaderBuilder::setResourcePrefix(const std::string &prefix) {
    _resource_prefix = prefix;
    return *this;
}

std::string ShaderBuilder::getShaderText() {

    auto shader_desc = getShaderDesc();

    if (!_dynamic_cdls.empty()) {
        return to_dynamic_shader(shader_desc->getShaderText(), _dynamic_cdls);
    } else {
        return shader_desc->getShaderText();
    }
}

std::string ShaderBuilder::getCacheString() const {

    std::ostringstream oss;

    oss << _dynamic_processor->getCacheID();
    for (const auto &[name, dynamic_cdl] : _dynamic_cdls) {
        oss << dynamic_cdl.grading_primary;
    }

    return oss.str();
}

void ShaderBuilder::setupTextures(ColourOperationDataPtr &op_data) const {

    auto shader_desc = _dynamic_shader_desc;

    // Process 3D LUTs
    const unsigned max_texture_3D = shader_desc->getNum3DTextures();
    for (unsigned idx = 0; idx < max_texture_3D; ++idx) {
        const char *textureName           = nullptr;
        const char *samplerName           = nullptr;
        unsigned edgelen                  = 0;
        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;

        shader_desc->get3DTexture(idx, textureName, samplerName, edgelen, interpolation);
        if (!textureName || !*textureName || !samplerName || !*samplerName || edgelen == 0) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::get3DTexture - The texture data is corrupted");
        }

        const float *ocio_lut_data = nullptr;
        shader_desc->get3DTextureValues(idx, ocio_lut_data);
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
        // TODO:
        // Texture data already stored in ShaderDesc?
        // Do we need to copy it once again here or can't we just keep the ShaderDesc object
        // around?
        std::memcpy(xs_lut_data, ocio_lut_data, data_size);

        xs_lut->update_content_hash();
        op_data->luts_.push_back(xs_lut);
    }

    // Process 1D LUTs
    const unsigned max_texture_2D = shader_desc->getNumTextures();
    for (unsigned idx = 0; idx < max_texture_2D; ++idx) {
        const char *textureName                  = nullptr;
        const char *samplerName                  = nullptr;
        unsigned width                           = 0;
        unsigned height                          = 0;
        OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        OCIO::Interpolation interpolation        = OCIO::INTERP_LINEAR;
        bool is2DTexture                         = false;

#if OCIO_VERSION_HEX >= 0x02030000
        OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_1D;
        shader_desc->getTexture(
            idx, textureName, samplerName, width, height, channel, dimensions, interpolation);
        is2DTexture = dimensions == OCIO::GpuShaderDesc::TEXTURE_2D;
#else
        shader_desc->getTexture(
            idx, textureName, samplerName, width, height, channel, interpolation);
        is2DTexture = height > 1;
#endif

        if (!textureName || !*textureName || !samplerName || !*samplerName || width == 0) {
            throw std::runtime_error(
                "OCIO::ShaderDesc::getTexture - The texture data is corrupted");
        }

        const float *ocio_lut_data = nullptr;
        shader_desc->getTextureValues(idx, ocio_lut_data);
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
            is2DTexture
                     ? LUTDescriptor::Create2DLUT(width, height, xs_dtype, xs_channels, xs_interp)
                     : LUTDescriptor::Create1DLUT(width, xs_dtype, xs_channels, xs_interp),
            samplerName);

        const int channels = channel == OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL ? 1 : 3;
        const std::size_t data_size = width * height * channels * sizeof(float);
        auto *xs_lut_data           = (float *)xs_lut->writeable_data();
        std::memcpy(xs_lut_data, ocio_lut_data, data_size);

        xs_lut->update_content_hash();
        op_data->luts_.push_back(xs_lut);
    }
}

void ShaderBuilder::updateUniforms(utility::JsonStore &uniforms, float exposure, float gamma) {

    updateDynamicProperties(exposure, gamma);
    updateDynamicPropertiesUniforms(uniforms);

    if (!_dynamic_cdls.empty()) {
        updateDynamicCDLs(uniforms);
    }
}

OCIO::ConstConfigRcPtr ShaderBuilder::getDynamicConfig() {

    if (_dynamic_config) {
        return _dynamic_config;
    }

    if (_dynamic_cdl_mode == DynamicCDLMode::Config) {
        _dynamic_config = to_dynamic_config(_config, _context, _dynamic_cdls);
    } else {
        _dynamic_config = _config;
    }

    return _dynamic_config;
}

OCIO::ConstProcessorRcPtr ShaderBuilder::getProcessor() {

    if (_dynamic_processor) {
        return _dynamic_processor;
    }

    auto dynamic_config = getDynamicConfig();

    try {
        auto proc =
            dynamic_config->getProcessor(_context, _transform, OCIO::TRANSFORM_DIR_FORWARD);

        if (_dynamic_cdl_mode == DynamicCDLMode::Processor) {
            _dynamic_processor = to_dynamic_processor(_config, _context, proc, _dynamic_cdls);
        } else {
            _dynamic_processor = proc;
        }
    } catch (const std::exception &e) {
        spdlog::warn("ShaderBuilder: Failed to construct processor: {}", e.what());
        spdlog::warn("ShaderBuilder: Defaulting to no-op processor");
        _dynamic_processor = dynamic_config->getProcessor(OCIO::MatrixTransform::Create());
    }

    return _dynamic_processor;
}

OCIO::ConstGpuShaderDescRcPtr ShaderBuilder::getShaderDesc() {

    if (_dynamic_shader_desc) {
        return _dynamic_shader_desc;
    }

    OCIO::GpuShaderDescRcPtr shader_desc = OCIO::GpuShaderDesc::CreateShaderDesc();
    shader_desc->setLanguage(_lang);
    shader_desc->setFunctionName(_func_name.c_str());
    shader_desc->setResourcePrefix(_resource_prefix.c_str());

    auto proc     = getProcessor();
    auto gpu_proc = proc->getDefaultGPUProcessor();
    gpu_proc->extractGpuShaderInfo(shader_desc);

    _dynamic_shader_desc = shader_desc;
    return _dynamic_shader_desc;
}

void ShaderBuilder::updateDynamicProperties(float exposure, float gamma) {

    auto shader_desc = getShaderDesc();

    // Exposure property
    if (shader_desc->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE)) {
        OCIO::DynamicPropertyRcPtr property =
            shader_desc->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE);
        OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        exposure_prop->setValue(exposure);
    }
    // Gamma property
    if (shader_desc->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA)) {
        OCIO::DynamicPropertyRcPtr property =
            shader_desc->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA);
        OCIO::DynamicPropertyDoubleRcPtr gamma_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        gamma_prop->setValue(gamma);
    }
}

void ShaderBuilder::updateDynamicPropertiesUniforms(utility::JsonStore &uniforms) {

    auto shader_desc            = getShaderDesc();
    const unsigned max_uniforms = shader_desc->getNumUniforms();

    for (unsigned idx = 0; idx < max_uniforms; ++idx) {
        OCIO::GpuShaderDesc::UniformData uniform_data;
        const char *name = shader_desc->getUniform(idx, uniform_data);

        switch (uniform_data.m_type) {
        case OCIO::UNIFORM_DOUBLE: {
            uniforms[name] = uniform_data.m_getDouble();
            break;
        }
        case OCIO::UNIFORM_BOOL: {
            uniforms[name] = uniform_data.m_getBool();
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
        // Note, the below has not been tested and might require adjustments.
        // This can show up if we start using more advanced dynamic grading.
        // case OCIO::UNIFORM_VECTOR_FLOAT:{
        //     std::vector<float> vector_float;
        //     for (int i = 0; i < uniform_data.m_vectorFloat.m_getSize(); i++){
        //         vector_float.push_back(uniform_data.m_vectorFloat.m_getVector()[i]);
        //     }
        //     pipe_data.shader_parameters_[name] = vector_float;
        //     break;
        // }
        // case OCIO::UNIFORM_VECTOR_INT:{
        //     std::vector<int> vector_int;
        //     for (int i = 0; i < uniform_data.m_vectorInt.m_getSize(); i++){
        //         vector_int.push_back(uniform_data.m_vectorInt.m_getVector()[i]);
        //     }
        //     pipe_data.shader_parameters_[name] = vector_int;
        //     break;
        // }
        default:
            spdlog::warn("OCIOEngine: Unknown uniform type for dynamic property");
            break;
        }
    }
}

void ShaderBuilder::updateDynamicCDLs(utility::JsonStore &uniforms) {

    for (auto &[name, dynamic_cdl] : _dynamic_cdls) {
        if (dynamic_cdl.in_use) {
            auto gp = dynamic_cdl.grading_primary;
            // spdlog::warn("update: {} slope {} {} {}", dynamic_cdl.look_name,
            // gp.m_exposure.m_red, gp.m_exposure.m_green, gp.m_exposure.m_blue);

            uniforms[name + "_offset"] = {
                "vec3", 1, gp.m_offset.m_red, gp.m_offset.m_green, gp.m_offset.m_blue};
            uniforms[name + "_exposure"] = {
                "vec3",
                1,
                powf(2.f, gp.m_exposure.m_red),
                powf(2.f, gp.m_exposure.m_green),
                powf(2.f, gp.m_exposure.m_blue)};
            uniforms[name + "_contrast"] = {
                "vec3", 1, gp.m_contrast.m_red, gp.m_contrast.m_green, gp.m_contrast.m_blue};
            uniforms[name + "_pivot"]       = 0.18 * std::pow(2., gp.m_pivot);
            uniforms[name + "_clampBlack"]  = gp.m_clampBlack;
            uniforms[name + "_clampWhite"]  = gp.m_clampWhite;
            uniforms[name + "_saturation"]  = gp.m_saturation;
            uniforms[name + "_localBypass"] = false;
        }
    }
}