// SPDX-License-Identifier: Apache-2.0
#include "ocio_engine.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

#include "shaders.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio::colour_pipeline::ocio;
using namespace xstudio;


namespace {
const static utility::Uuid PLUGIN_UUID{"b39d1e3d-58f8-475f-82c1-081a048df705"};

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
    const OCIO::ConstTransformRcPtr &transform,
    const OCIO::GradingPrimary &cdl_grading_primary,
    const std::string &cdl_file_transform) {
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_GROUP) {
        auto group     = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(transform);
        auto new_group = OCIO::GroupTransform::Create();

        int num_transforms = group->getNumTransforms();
        for (int i = 0; i < num_transforms; ++i) {
            auto dyn_transform = to_dynamic_transform(
                group->getTransform(i), cdl_grading_primary, cdl_file_transform);
            new_group->appendTransform(dyn_transform);
        }
        return OCIO::DynamicPtrCast<OCIO::Transform>(new_group);
    } else if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_FILE) {
        auto file = OCIO::DynamicPtrCast<const OCIO::FileTransform>(transform);
        if (std::string(file->getSrc()) == cdl_file_transform) {
            auto grading_primary = OCIO::GradingPrimaryTransform::Create(OCIO::GRADING_LIN);
            grading_primary->setValue(cdl_grading_primary);
            grading_primary->makeDynamic();
            return OCIO::DynamicPtrCast<OCIO::Transform>(grading_primary);
        }
    }

    return transform->createEditableCopy();
}

struct ShaderDescriptor {
    OCIO::ConstGpuShaderDescRcPtr shader_desc;
    OCIO::GradingPrimary primary = OCIO::GradingPrimary(OCIO::GRADING_LIN);
    std::mutex mutex;
};
typedef std::shared_ptr<ShaderDescriptor> ShaderDescriptorPtr;

} // anonymous namespace

ColourOperationDataPtr OCIOEngine::linearise_op_data(
    const utility::JsonStore &src_colour_mgmt_metadata, const bool bypass) {

    using xstudio::utility::replace_once;

    auto result = std::make_shared<ColourOperationData>("OCIO Linearise OP");

    // Construct OCIO processor, shader and extract texture(s)
    auto to_lin_proc   = make_to_lin_processor(src_colour_mgmt_metadata, bypass);
    auto to_lin_shader = make_shader(to_lin_proc, "OCIOLinearise", "to_linear_");
    setup_textures(to_lin_shader, result);

    std::string to_lin_shader_src = replace_once(
        ShaderTemplates::OCIO_linearise, "//OCIOLinearise", to_lin_shader->getShaderText());

    result->shader_ = std::make_shared<ui::opengl::OpenGLShader>(
        utility::Uuid::generate(), to_lin_shader_src);

    // Store GPUShaderDesc objects for later use during uniform binding / update.
    // Note that we need updated MediaParams object, which may include primary grading
    // data, when we update the shader uniforms at draw time. Hence we add the MediaParams
    // as part of the user_data_ blind data object.
    auto desc          = std::make_shared<ShaderDescriptor>();
    desc->shader_desc  = to_lin_shader;
    result->user_data_ = desc;
    result->set_cache_id(to_string(PLUGIN_UUID) + to_lin_proc->getCacheID());

    return result;
}

ColourOperationDataPtr OCIOEngine::linear_to_display_op_data(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string &display,
    const std::string &view,
    const bool bypass) {

    using xstudio::utility::replace_once;

    auto data = std::make_shared<ColourOperationData>(ColourOperationData("OCIO Display OP"));

    // Construct OCIO processor, shader and extract texture(s)
    OCIO::GradingPrimary primary = OCIO::GradingPrimary(OCIO::GRADING_LIN);
    auto display_proc =
        make_display_processor(src_colour_mgmt_metadata, false, primary, display, view, bypass);
    auto display_shader = make_shader(display_proc, "OCIODisplay", "to_display");
    setup_textures(display_shader, data);

    std::string display_shader_src = replace_once(
        ShaderTemplates::OCIO_display, "//OCIODisplay", display_shader->getShaderText());

    data->shader_ = std::make_shared<ui::opengl::OpenGLShader>(
        utility::Uuid::generate(), display_shader_src);

    // Store GPUShaderDesc objects for later use during uniform binding / update.
    // Note that we need updated MediaParams object, which may include primary grading
    // data, when we update the shader uniforms at draw time. Hence we add the MediaParams
    // as part of the user_data_ blind data object.

    auto desc         = std::make_shared<ShaderDescriptor>();
    desc->shader_desc = display_shader;
    desc->primary     = primary;
    data->user_data_  = desc;

    data->set_cache_id(to_string(PLUGIN_UUID) + display_proc->getCacheID());
    std::stringstream ss;
    ss << primary;
    data->set_cache_id(data->cache_id() + ss.str());

    return data;
}


thumbnail::ThumbnailBufferPtr OCIOEngine::process_thumbnail(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const thumbnail::ThumbnailBufferPtr &buf,
    const std::string &display,
    const std::string &view) {

    if (buf->format() != thumbnail::TF_RGBF96) {
        throw std::runtime_error(
            "OCIOEngine: Invalid thumbnail buffer type, expects TF_RGBF96");
    }

    // Create to_lin and to_display processors and concatenate them

    const auto &ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    auto to_lin_group =
        make_to_lin_processor(src_colour_mgmt_metadata, false)->createGroupTransform();

    OCIO::GradingPrimary _primary = OCIO::GradingPrimary(OCIO::GRADING_LIN);
    auto to_display_group =
        make_display_processor(src_colour_mgmt_metadata, true, _primary, display, view)
            ->createGroupTransform();

    OCIO::GroupTransformRcPtr concat_group = OCIO::GroupTransform::Create();
    concat_group->appendTransform(to_lin_group);
    concat_group->appendTransform(to_display_group);

    auto cpu_proc =
        ocio_config->getProcessor(concat_group)
            ->getOptimizedCPUProcessor(
                OCIO::BIT_DEPTH_F32, OCIO::BIT_DEPTH_UINT8, OCIO::OPTIMIZATION_DEFAULT);

    auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
        buf->width(), buf->height(), thumbnail::TF_RGB24);
    auto src = reinterpret_cast<float *>(buf->data().data());
    auto dst = reinterpret_cast<uint8_t *>(thumb->data().data());

    OCIO::PackedImageDesc in_img(
        src,
        buf->width(),
        buf->height(),
        buf->channels(),
        OCIO::BIT_DEPTH_F32,
        OCIO::AutoStride,
        OCIO::AutoStride,
        OCIO::AutoStride);

    OCIO::PackedImageDesc out_img(
        dst,
        thumb->width(),
        thumb->height(),
        thumb->channels(),
        OCIO::BIT_DEPTH_UINT8,
        OCIO::AutoStride,
        OCIO::AutoStride,
        OCIO::AutoStride);

    cpu_proc->apply(in_img, out_img);

    return thumb;
}

void OCIOEngine::extend_pixel_info(
    media_reader::PixelInfo &pixel_info,
    const media::AVFrameID &frame_id,
    const std::string &display,
    const std::string &view,
    const float exposure,
    const float gamma,
    const float saturation) {

    if (pixel_info.raw_channels_info().size() < 3)
        return;

    auto raw_info = pixel_info.raw_channels_info();

    if (compute_hash(frame_id.params()) != last_pixel_probe_source_hash_) {
        OCIO::GradingPrimary primary = OCIO::GradingPrimary(OCIO::GRADING_LIN);
        auto display_proc =
            make_display_processor(frame_id.params(), false, primary, display, view);
        pixel_probe_to_display_proc_ = display_proc->getDefaultCPUProcessor();
        pixel_probe_to_lin_proc_ =
            make_to_lin_processor(frame_id.params(), false)->getDefaultCPUProcessor();
        last_pixel_probe_source_hash_ = compute_hash(frame_id.params());
    }

    // Update Dynamic Properties on the CPUProcessor instance
    try {
        {
            // Exposure
            OCIO::DynamicPropertyRcPtr property =
                pixel_probe_to_display_proc_->getDynamicProperty(
                    OCIO::DYNAMIC_PROPERTY_EXPOSURE);
            OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
                OCIO::DynamicPropertyValue::AsDouble(property);
            exposure_prop->setValue(exposure);
        }
        {
            // Gamma
            OCIO::DynamicPropertyRcPtr property =
                pixel_probe_to_display_proc_->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA);
            OCIO::DynamicPropertyDoubleRcPtr gamma_prop =
                OCIO::DynamicPropertyValue::AsDouble(property);
            gamma_prop->setValue(gamma);
        }
    } catch (const OCIO::Exception &e) {
        // TODO: ColSci
        // Update when OCIO::CPUProcessor include hasDynamicProperty()
    }

    // Source

    std::string source_cs = detect_source_colourspace(frame_id.params());
    if (!source_cs.empty()) {

        pixel_info.set_raw_colourspace_name(
            std::string("Source (") + source_cs + std::string(")"));
    }

    // Working space (scene_linear)

    {
        float RGB[3] = {
            raw_info[0].pixel_value, raw_info[1].pixel_value, raw_info[2].pixel_value};

        pixel_probe_to_lin_proc_->applyRGB(RGB);

        pixel_info.add_linear_channel_info(raw_info[0].channel_name, RGB[0]);
        pixel_info.add_linear_channel_info(raw_info[1].channel_name, RGB[1]);
        pixel_info.add_linear_channel_info(raw_info[2].channel_name, RGB[2]);

        pixel_info.set_linear_colourspace_name(
            std::string("Scene Linear (") + working_space(frame_id.params()) + std::string(")"));
    }

    // Display output

    {
        float RGB[3] = {
            raw_info[0].pixel_value, raw_info[1].pixel_value, raw_info[2].pixel_value};

        pixel_probe_to_display_proc_->applyRGB(RGB);

        // TODO: ColSci
        // Saturation is not managed by OCIO currently
        if (saturation != 1.0) {
            const float W[3] = {0.2126f, 0.7152f, 0.0722f};
            const float LUMA = RGB[0] * W[0] + RGB[1] * W[1] + RGB[2] * W[2];
            RGB[0]           = LUMA + saturation * (RGB[0] - LUMA);
            RGB[1]           = LUMA + saturation * (RGB[1] - LUMA);
            RGB[2]           = LUMA + saturation * (RGB[2] - LUMA);
        }

        pixel_info.add_display_rgb_info("R", RGB[0]);
        pixel_info.add_display_rgb_info("G", RGB[1]);
        pixel_info.add_display_rgb_info("B", RGB[2]);

        pixel_info.set_display_colourspace_name(
            std::string("Display (") + view + std::string("|") + display + std::string(")"));
    }
}

OCIO::ConstConfigRcPtr
OCIOEngine::get_ocio_config(const utility::JsonStore &src_colour_mgmt_metadata) const {

    const std::string config_name =
        src_colour_mgmt_metadata.get_or("ocio_config", std::string(""));
    const std::string displays =
        src_colour_mgmt_metadata.get_or("active_displays", std::string(""));
    const std::string views  = src_colour_mgmt_metadata.get_or("active_views", std::string(""));
    const std::string concat = config_name + displays + views;

    auto it = ocio_config_cache_.find(concat);
    if (it != ocio_config_cache_.end()) {
        return it->second;
    }

    // This specific OCIO config has not been loaded yet.
    OCIO::ConstConfigRcPtr config;

    try {
        if (config_name == "__raw__") {
            config = OCIO::Config::CreateRaw();
        } else if (config_name == "__current__") {
            config = OCIO::GetCurrentConfig();
        } else if (!config_name.empty()) {
            config = OCIO::Config::CreateFromFile(config_name.c_str());
        } else {
            config = OCIO::GetCurrentConfig();
        }
    } catch (const std::exception &e) {
        spdlog::warn("OCIOEngine: Failed to load OCIO config {}: {}", config_name, e.what());
        spdlog::warn("OCIOEngine: Fallback on raw config");
        config = OCIO::Config::CreateRaw();
    }

    auto econfig = config->createEditableCopy();
    econfig->setName(concat.c_str());
    if (!displays.empty())
        econfig->setActiveDisplays(displays.c_str());
    if (!views.empty())
        econfig->setActiveViews(views.c_str());
    config                     = econfig;
    ocio_config_cache_[concat] = config;

    return config;
}


const char *
OCIOEngine::working_space(const utility::JsonStore &src_colour_mgmt_metadata) const {
    auto config = get_ocio_config(src_colour_mgmt_metadata);
    if (not config) {
        return "";
    } else if (config->hasRole(OCIO::ROLE_SCENE_LINEAR)) {
        return OCIO::ROLE_SCENE_LINEAR;
    } else {
        return OCIO::ROLE_DEFAULT;
    }
}

std::string OCIOEngine::input_space_for_view(
    const utility::JsonStore &src_colour_mgmt_metadata, const std::string &view) const {

    auto config = get_ocio_config(src_colour_mgmt_metadata);
    const std::string empty;
    std::string new_colourspace;

    auto colourspace_or = [config](const std::string &cs, const std::string &fallback) {
        const bool has_cs = bool(config->getColorSpace(cs.c_str()));
        return has_cs ? cs : fallback;
    };

    const auto is_untonemapped = view == "Un-tone-mapped";
    const auto input_space     = src_colour_mgmt_metadata.get_or("input_colorspace", empty);
    const auto untonemapped_space =
        src_colour_mgmt_metadata.get_or("untonemapped_colorspace", empty);

    new_colourspace = is_untonemapped ? untonemapped_space : input_space;

    for (const auto &cs : xstudio::utility::split(new_colourspace, ':')) {
        new_colourspace = colourspace_or(new_colourspace, cs);
    }

    // Double check the new colourspace actually exists
    new_colourspace = colourspace_or(new_colourspace, "");

    // Avoid role names, helps with the source colour space menu
    if (!new_colourspace.empty()) {
        new_colourspace = config->getCanonicalName(new_colourspace.c_str());
    }

    return new_colourspace;
}

const char *
OCIOEngine::default_display(const utility::JsonStore &src_colour_mgmt_metadata) const {

    auto config = get_ocio_config(src_colour_mgmt_metadata);
    return config->getDefaultDisplay();
}

// Return the transform to bring incoming data to scene_linear space.
OCIO::TransformRcPtr
OCIOEngine::source_transform(const utility::JsonStore &src_colour_mgmt_metadata) const {

    const std::string working_cs = working_space(src_colour_mgmt_metadata);
    auto config                  = get_ocio_config(src_colour_mgmt_metadata);

    const std::string user_input_cs =
        src_colour_mgmt_metadata.get_or("override_input_cs", std::string(""));

    const std::string filepath = src_colour_mgmt_metadata.get_or("path", std::string(""));
    const std::string display =
        src_colour_mgmt_metadata.get_or("input_display", std::string(""));
    const std::string view = src_colour_mgmt_metadata.get_or("input_view", std::string(""));

    // Expand input_colorspace to the first valid colorspace found.
    std::string input_cs = src_colour_mgmt_metadata.get_or("input_colorspace", std::string(""));

    for (const auto &cs : xstudio::utility::split(input_cs, ':')) {
        if (config->getColorSpace(cs.c_str())) {
            input_cs = cs;
            break;
        }
    }

    if (!user_input_cs.empty()) {
        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(user_input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    } else if (!input_cs.empty()) {
        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    } else if (!display.empty() and !view.empty()) {
        return display_transform(working_cs, display, view, OCIO::TRANSFORM_DIR_INVERSE);
    } else {
        std::string auto_input_cs;

        // FileRules, ignore if only matched by the default rule
        if (!config->filepathOnlyMatchesDefaultRule(filepath.c_str())) {
            auto_input_cs = config->getColorSpaceFromFilepath(filepath.c_str());
        }

        // Manual file type (int8, int16, float, etc) to role / colorspace mapping
        // => Not Implemented

        // Fallback to default rule
        auto_input_cs = config->getColorSpaceFromFilepath(filepath.c_str());

        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(auto_input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    }
}

OCIO::ConstConfigRcPtr OCIOEngine::display_transform(
    const utility::JsonStore &src_colour_mgmt_metadata,
    OCIO::ContextRcPtr context,
    OCIO::GroupTransformRcPtr group,
    OCIO::GradingPrimary &primary,
    std::string display,
    std::string view) const {

    auto ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    // Determines which OCIO display / view to use

    if (display.empty() or view.empty()) {
        display = ocio_config->getDefaultDisplay();
        view    = ocio_config->getDefaultView(display.c_str());
    }

    // Turns per shot CDLs into dynamic transform

    // Instanciate a processor and query its metadata to get the
    // underlying looks used. This is more robust than simply asking
    // the view for it looks, as a look could use another look in its
    // definition.
    auto proc = ocio_config->getProcessor(
        context, "scene_linear", display.c_str(), view.c_str(), OCIO::TRANSFORM_DIR_FORWARD);
    auto proc_meta = proc->getProcessorMetadata();
    std::vector<std::string> proc_looks;
    for (int i = 0; i < proc_meta->getNumLooks(); ++i) {
        proc_looks.push_back(proc_meta->getLook(i));
    }

    std::string dynamic_look;
    std::string dynamic_file;

    if (src_colour_mgmt_metadata.contains("dynamic_cdl")) {
        if (src_colour_mgmt_metadata["dynamic_cdl"].is_object()) {
            for (auto &item : src_colour_mgmt_metadata["dynamic_cdl"].items()) {
                if (std::find(proc_looks.begin(), proc_looks.end(), item.key()) !=
                    proc_looks.end()) {
                    dynamic_look = item.key();
                    dynamic_file = item.value();
                }
            }
        } else {
            spdlog::warn(
                "OCIOEngine: 'dynamic_cdl' should be a dictionary, got {} instead",
                src_colour_mgmt_metadata["dynamic_cdl"].dump(2));
        }
    }

    if (!dynamic_look.empty() and !dynamic_file.empty()) {
        ocio_config = make_dynamic_display_processor(
            src_colour_mgmt_metadata,
            ocio_config,
            context,
            group,
            display,
            view,
            dynamic_look,
            dynamic_file,
            primary);
    } else {
        group->appendTransform(display_transform(
            working_space(src_colour_mgmt_metadata),
            display,
            view,
            OCIO::TRANSFORM_DIR_FORWARD));
    }

    return ocio_config;
}

OCIO::TransformRcPtr OCIOEngine::display_transform(
    const std::string &source,
    const std::string &display,
    const std::string &view,
    OCIO::TransformDirection direction) const {

    OCIO::DisplayViewTransformRcPtr dt = OCIO::DisplayViewTransform::Create();
    dt->setSrc(source.c_str());
    dt->setDisplay(display.c_str());
    dt->setView(view.c_str());
    dt->setDirection(direction);

    return dt;
}

OCIO::TransformRcPtr OCIOEngine::identity_transform() const {
    return OCIO::MatrixTransform::Create();
}

OCIO::ContextRcPtr OCIOEngine::setup_ocio_context(const utility::JsonStore &metadata) const {

    const auto &ocio_config = get_ocio_config(metadata);

    OCIO::ContextRcPtr context = ocio_config->getCurrentContext()->createEditableCopy();

    // Setup the OCIO context based on incoming metadata

    if (metadata.contains("ocio_context")) {
        if (metadata["ocio_context"].is_object()) {
            for (auto &item : metadata["ocio_context"].items()) {
                context->setStringVar(item.key().c_str(), std::string(item.value()).c_str());
            }
        } else {
            spdlog::warn(
                "OCIOEngine: 'ocio_context' should be a dictionary, got {} instead",
                metadata["ocio_context"].dump(2));
        }
    }

    return context;
}

OCIO::ConstProcessorRcPtr OCIOEngine::make_to_lin_processor(
    const utility::JsonStore &src_colour_mgmt_metadata, const bool bypass) const {

    const auto &ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    try {

        if (bypass) {
            return ocio_config->getProcessor(identity_transform());
        }

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();
        group->appendTransform(source_transform(src_colour_mgmt_metadata));

        OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);
        return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);

    } catch (const std::exception &e) {
        spdlog::warn("OCIOEngine: Failed to construct OCIO lin processor: {}", e.what());
        spdlog::warn("OCIOEngine: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstProcessorRcPtr OCIOEngine::make_display_processor(
    const utility::JsonStore &src_colour_mgmt_metadata,
    bool is_thumbnail,
    OCIO::GradingPrimary &primary,
    const std::string &display,
    const std::string &view,
    const bool bypass) const {

    auto ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    try {

        OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();

        {
            auto ect = OCIO::ExposureContrastTransform::Create();
            ect->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
            ect->setDirection(OCIO::TRANSFORM_DIR_FORWARD);
            ect->makeExposureDynamic();
            ect->setExposure(1.0);
            group->appendTransform(ect);
        }

        if (!bypass) {
            // To support dynamic CDLs we currently have to edit the OCIO
            // config in place, hence the need to return the new config
            // here to later query the processor from it.

            ocio_config = display_transform(
                src_colour_mgmt_metadata, context, group, primary, display, view);
        }

        {
            auto ect = OCIO::ExposureContrastTransform::Create();
            ect->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
            ect->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
            ect->makeGammaDynamic();
            // default gamma pivot in OCIO is 0.18, avoid divide by zero too
            ect->setPivot(1.0f);
            ect->setGamma(1.0f);
            group->appendTransform(ect);
        }

        return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);

    } catch (const std::exception &e) {
        spdlog::warn("OCIOEngine: Failed to construct OCIO processor: {}", e.what());
        spdlog::warn("OCIOEngine: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstConfigRcPtr OCIOEngine::make_dynamic_display_processor(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const OCIO::ConstConfigRcPtr &config,
    const OCIO::ConstContextRcPtr &context,
    const OCIO::GroupTransformRcPtr &group,
    const std::string &display,
    const std::string &view,
    const std::string &look_name,
    const std::string &cdl_file_name,
    OCIO::GradingPrimary &primary) const {

    try {
        // Load the CDL and derive a GradingPrimary from it
        auto look_path     = context->resolveFileLocation(cdl_file_name.c_str());
        auto cdl_transform = OCIO::CDLTransform::CreateFromFile(look_path, "");

        primary = grading_primary_from_cdl(cdl_transform);

        // Create a dynamic version of the look
        auto dynamic_config = config->createEditableCopy();

        auto dynamic_look = config->getLook(look_name.c_str())->createEditableCopy();
        std::string dynamic_look_name = std::string("__xstudio__dynamic_") + look_name;
        dynamic_look->setName(dynamic_look_name.c_str());

        auto forward_transform = dynamic_look->getTransform();
        if (forward_transform) {
            dynamic_look->setTransform(
                to_dynamic_transform(forward_transform, primary, cdl_file_name));
        }
        auto inverse_transform = dynamic_look->getInverseTransform();
        if (inverse_transform) {
            dynamic_look->setInverseTransform(
                to_dynamic_transform(inverse_transform, primary, cdl_file_name));
        }

        dynamic_config->addLook(dynamic_look);

        // Add a view using the dynamic look
        std::string colorspace_name =
            dynamic_config->getDisplayViewColorSpaceName(display.c_str(), view.c_str());
        std::string view_name  = view + " Dynamic";
        std::string view_looks = config->getDisplayViewLooks(display.c_str(), view.c_str());
        view_looks = xstudio::utility::replace_once(view_looks, look_name, dynamic_look_name);
        dynamic_config->addDisplayView(
            display.c_str(), view_name.c_str(), colorspace_name.c_str(), view_looks.c_str());

        group->appendTransform(display_transform(
            working_space(src_colour_mgmt_metadata),
            display,
            view_name,
            OCIO::TRANSFORM_DIR_FORWARD));

        return dynamic_config;

    } catch (const OCIO::Exception &ex) {
        group->appendTransform(display_transform(
            working_space(src_colour_mgmt_metadata),
            display,
            view,
            OCIO::TRANSFORM_DIR_FORWARD));

        return config;
    }
}

OCIO::ConstGpuShaderDescRcPtr OCIOEngine::make_shader(
    OCIO::ConstProcessorRcPtr &processor,
    const char *function_name,
    const char *resource_prefix) const {

    OCIO::GpuShaderDescRcPtr shader_desc = OCIO::GpuShaderDesc::CreateShaderDesc();
    shader_desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
    shader_desc->setFunctionName(function_name);
    shader_desc->setResourcePrefix(resource_prefix);
    auto gpu_proc = processor->getDefaultGPUProcessor();
    gpu_proc->extractGpuShaderInfo(shader_desc);
    return shader_desc;
}

void OCIOEngine::setup_textures(
    OCIO::ConstGpuShaderDescRcPtr &shader_desc, ColourOperationDataPtr op_data) const {

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

        shader_desc->getTexture(
            idx, textureName, samplerName, width, height, channel, interpolation);

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
            height > 1
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

std::string
OCIOEngine::detect_source_colourspace(const utility::JsonStore &src_colour_mgmt_metadata) {

    // Extract the input colorspace as detected by the plugin and update the UI
    std::string detected_cs;

    OCIO::TransformRcPtr transform = source_transform(src_colour_mgmt_metadata);
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_COLORSPACE) {
        OCIO::ColorSpaceTransformRcPtr csc =
            std::static_pointer_cast<OCIO::ColorSpaceTransform>(transform);
        detected_cs = csc->getSrc();
    } else if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_DISPLAY_VIEW) {
        // TODO: ColSci
        // Note that in case the view uses looks, the colour space returned here
        // is not representing the input processing accurately and will be out
        // of sync with what the input transform actually is (inverse display view).
        OCIO::DisplayViewTransformRcPtr disp =
            std::static_pointer_cast<OCIO::DisplayViewTransform>(transform);
        auto config = get_ocio_config(src_colour_mgmt_metadata);
        const std::string view_cs =
            config->getDisplayViewColorSpaceName(disp->getDisplay(), disp->getView());
        detected_cs = view_cs;
    } else {
        spdlog::warn(
            "OCIOColourPipeline: Internal error trying to extract source colour space.");
    }

    return detected_cs;
}

std::string OCIOEngine::preferred_view(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string user_preferred_view) const {

    auto ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    // Get the default display from OCIO config
    const std::string default_display = ocio_config->getDefaultDisplay();
    const std::string default_view    = ocio_config->getDefaultView(default_display.c_str());

    std::string _preferred_view;
    if (user_preferred_view == "Default") {
        _preferred_view = default_view;
    } else if (user_preferred_view == "Automatic") {
        _preferred_view = src_colour_mgmt_metadata.get_or("automatic_view", default_view);
    } else {
        _preferred_view = user_preferred_view;
    }

    // Validate that the view is in ocio config
    std::map<std::string, std::vector<std::string>> display_views;
    std::vector<std::string> displays, cs;
    get_ocio_displays_view_colourspaces(src_colour_mgmt_metadata, cs, displays, display_views);

    for (auto it = begin(display_views); it != end(display_views); ++it) {
        for (auto view_in_ocio_config : it->second) {
            if (view_in_ocio_config == _preferred_view) {
                return _preferred_view;
            }
        }
    }
    // If view is not avaialble return the default view
    return ocio_config->getDefaultView(default_display.c_str());
}

void OCIOEngine::get_ocio_displays_view_colourspaces(
    const utility::JsonStore &src_colour_mgmt_metadata,
    std::vector<std::string> &all_colourspaces,
    std::vector<std::string> &displays,
    std::map<std::string, std::vector<std::string>> &display_views) const {

    auto ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    all_colourspaces.clear();
    displays.clear();
    display_views.clear();

    for (int i = 0; i < ocio_config->getNumDisplays(); ++i) {
        const std::string display = ocio_config->getDisplay(i);
        displays.push_back(display);

        display_views[display] = std::vector<std::string>();
        for (int j = 0; j < ocio_config->getNumViews(display.c_str()); ++j) {
            const std::string view = ocio_config->getView(display.c_str(), j);
            display_views[display].push_back(view);
        }
    }

    for (int i = 0; i < ocio_config->getNumColorSpaces(); ++i) {
        const char *cs_name = ocio_config->getColorSpaceNameByIndex(i);
        if (cs_name && *cs_name) {
            all_colourspaces.emplace_back(cs_name);
        }
    }
}

void OCIOEngine::update_shader_uniforms(
    std::any &user_data,
    utility::JsonStore &uniforms,
    const float exposure,
    const float gamma) {
    if (user_data.has_value() && user_data.type() == typeid(ShaderDescriptorPtr)) {
        try {
            auto shader = std::any_cast<ShaderDescriptorPtr>(user_data);
            if (shader && shader->shader_desc) {
                // ColourOperationDataPtr is shared among MediaSource using the same OCIO
                // shader. Hence ShaderDesc objects holding OCIO DynamicProperty are shared too.
                // DynamicProperty are used to hold the uniforms representing the transform,
                // we need to update them before querying the updated uniforms. The mutex is
                // needed to protect against multiple workers concurrently updating the
                // ShaderDesc's DynamicProperty resulting in queried uniforms not reflecting
                // values for the current shot.
                std::scoped_lock lock(shader->mutex);
                update_dynamic_parameters(
                    shader->shader_desc, shader->primary, exposure, gamma);
                update_all_uniforms(shader->shader_desc, uniforms);
            }
        } catch (const std::exception &e) {
            spdlog::warn("OCIOColourPipeline: Failed to update shader uniforms: {}", e.what());
        }
    }
}

void OCIOEngine::update_all_uniforms(
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

void OCIOEngine::update_dynamic_parameters(
    OCIO::ConstGpuShaderDescRcPtr &shader,
    const OCIO::GradingPrimary &primary,
    const float exposure,
    const float gamma) const {

    // Exposure property
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE)) {
        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE);
        OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        exposure_prop->setValue(exposure);
    }
    // Gamma property
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA)) {
        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA);
        OCIO::DynamicPropertyDoubleRcPtr gamma_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        gamma_prop->setValue(gamma);
    }
    // Shot CDL
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY)) {

        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY);
        OCIO::DynamicPropertyGradingPrimaryRcPtr primary_prop =
            OCIO::DynamicPropertyValue::AsGradingPrimary(property);
        primary_prop->setValue(primary);
    }
}

size_t OCIOEngine::compute_hash(
    const utility::JsonStore &src_colour_mgmt_metadata, const std::string &extra) const {
    size_t hash = 0;
    hash        = std::hash<std::string>{}(src_colour_mgmt_metadata.dump() + extra);
    return hash;
}

OCIOEngineActor::OCIOEngineActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    behavior_.assign(
        [=](colour_pipe_display_data_atom,
            const utility::JsonStore &media_metadata,
            const std::string &display,
            const std::string &view,
            const bool bypass) -> result<ColourOperationDataPtr> {
            // This message is sent from main OCIOColourPipeline
            try {
                return m_engine_.linear_to_display_op_data(
                    media_metadata, display, view, bypass);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](colour_pipe_linearise_data_atom,
            const utility::JsonStore &media_metadata,
            const bool bypass) -> result<ColourOperationDataPtr> {
            // This message is sent from main OCIOColourPipeline
            try {
                return m_engine_.linearise_op_data(media_metadata, bypass);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](media_reader::process_thumbnail_atom,
            const utility::JsonStore &media_metadata,
            const thumbnail::ThumbnailBufferPtr &buf,
            const std::string &display,
            const std::string &view) -> result<thumbnail::ThumbnailBufferPtr> {
            // This message is sent from main OCIOColourPipeline
            try {
                return m_engine_.process_thumbnail(media_metadata, buf, display, view);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        });
}
