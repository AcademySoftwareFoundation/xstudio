// SPDX-License-Identifier: Apache-2.0
#include "ocio_engine.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

#include "shaders.hpp"
#include "ocio_shader.hpp"

using namespace xstudio::colour_pipeline;
using namespace xstudio::colour_pipeline::ocio;
using namespace xstudio;


namespace {
const static utility::Uuid PLUGIN_UUID{"b39d1e3d-58f8-475f-82c1-081a048df705"};


struct ShaderDescriptor {
    ShaderBuilder shader_builder;
    std::mutex mutex;
};
typedef std::shared_ptr<ShaderDescriptor> ShaderDescriptorPtr;

} // anonymous namespace

ColourOperationDataPtr OCIOEngine::linearise_op_data(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const bool bypass,
    const bool auto_adjust_source,
    const std::string &view) {

    auto data = std::make_shared<ColourOperationData>("OCIO Linearise OP");

    OCIO::ConstConfigRcPtr config = get_ocio_config(src_colour_mgmt_metadata);
    OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);
    OCIO::TransformRcPtr transform = source_transform(src_colour_mgmt_metadata, auto_adjust_source, view, bypass);

    auto shader_builder = ShaderBuilder()
        .setConfig(config)
        .setContext(context)
        .setTransform(transform)
        .setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0)
        .setFunctionName("OCIOLinearise")
        .setResourcePrefix("to_linear_");

    std::string shader_text =
        xstudio::utility::replace_once(
            ShaderTemplates::OCIO_linearise,
            "//OCIOLinearise",
            shader_builder.getShaderText()
        );

    shader_builder.setupTextures(data);

    data->shader_ = std::make_shared<ui::opengl::OpenGLShader>(
        utility::Uuid::generate(), shader_text);

    data->set_cache_id(to_string(PLUGIN_UUID) + shader_builder.getCacheString());

    return data;
}

ColourOperationDataPtr OCIOEngine::linear_to_display_op_data(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string &display,
    const std::string &view,
    const bool bypass) {

    auto data = std::make_shared<ColourOperationData>(ColourOperationData("OCIO Display OP"));

    OCIO::ConstConfigRcPtr config = get_ocio_config(src_colour_mgmt_metadata);
    OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);
    OCIO::TransformRcPtr transform = display_transform(src_colour_mgmt_metadata, display, view, bypass);

    // CDLs will be turned into uniform baked transforms in the generated shader.
    DynamicCDLMap cdls;
    if (src_colour_mgmt_metadata.contains("dynamic_cdl")) {
        if (src_colour_mgmt_metadata["dynamic_cdl"].is_object()) {
            for (auto &item : src_colour_mgmt_metadata["dynamic_cdl"].items()) {
                    const std::string xstudio_name = fmt::format("xstudio_cdl_{}", item.key());
                    cdls[xstudio_name].look_name = item.key();
                    cdls[xstudio_name].file_name = item.value();
            }
        } else {
            spdlog::warn(
                "OCIOEngine: 'dynamic_cdl' should be a dictionary, got {} instead",
                src_colour_mgmt_metadata["dynamic_cdl"].dump(2));
        }
    }

    // CDL dynamic patching can use different methods.
    const std::string dynamic_cdl_mode = src_colour_mgmt_metadata.get_or(
        "dynamic_cdl_mode", std::string("config"));
    ShaderBuilder::DynamicCDLMode cdl_mode = dynamic_cdl_mode == "config" ?
        ShaderBuilder::DynamicCDLMode::Config : ShaderBuilder::DynamicCDLMode::Processor;

    auto shader_builder = ShaderBuilder()
        .setConfig(config)
        .setContext(context)
        .setDynamicCDLs(cdls)
        .setDynamicCDLMode(cdl_mode)
        .setTransform(transform)
        .setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0)
        .setFunctionName("OCIODisplay")
        .setResourcePrefix("to_display_");

    std::string shader_text =
        xstudio::utility::replace_once(
            ShaderTemplates::OCIO_display,
            "//OCIODisplay",
            shader_builder.getShaderText()
        );

    shader_builder.setupTextures(data);

    data->shader_ = std::make_shared<ui::opengl::OpenGLShader>(
        utility::Uuid::generate(), shader_text);

    // Store ShaderBuilder for later use during uniform binding / update.
    auto desc = std::make_shared<ShaderDescriptor>();
    desc->shader_builder = shader_builder;
    data->user_data_   = desc;

    // Cache ID needs to contain the shot specific grades as the shader
    // will be identical otherwise and individual shots won't get proper
    // grades applied.
    data->set_cache_id(to_string(PLUGIN_UUID) + shader_builder.getCacheString());

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

    std::string _display = display;
    std::string _view = view;
    if (display.empty() or view.empty()) {
        _display = ocio_config->getDefaultDisplay();
        _view    = ocio_config->getDefaultView(_display.c_str());
    }

    auto to_lin_group = make_to_lin_processor(src_colour_mgmt_metadata, view, false, false)
                            ->createGroupTransform();
    auto to_display_group =
        make_display_processor(src_colour_mgmt_metadata, _display, _view, false)
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
    const bool auto_adjust_source,
    const float exposure,
    const float gamma,
    const float saturation) {

    if (pixel_info.raw_channels_info().size() < 3)
        return;

    auto raw_info = pixel_info.raw_channels_info();

    const auto hash = compute_hash(
        frame_id.params(), display + view + (auto_adjust_source ? "auto_adjust" : ""));

    if (hash != last_pixel_probe_source_hash_) {

        auto colour_mgmt_params = frame_id.params();

        auto display_proc =
            make_display_processor(colour_mgmt_params, display, view, false);
        pixel_probe_to_display_proc_ = display_proc->getDefaultCPUProcessor();
        pixel_probe_to_lin_proc_ =
            make_to_lin_processor(colour_mgmt_params, view, auto_adjust_source, false)
                ->getDefaultCPUProcessor();
        last_pixel_probe_source_hash_ = hash;
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

    std::string source_cs =
        detect_source_colourspace(frame_id.params(), auto_adjust_source, view);

    if (!source_cs.empty()) {

        pixel_info.set_raw_colourspace_name(
            std::string("Source (") + source_cs + std::string(")"));
    }

    // Working space (scene_linear)

    {
        float RGB[3] = {
            raw_info[0].channel_value, raw_info[1].channel_value, raw_info[2].channel_value};

        pixel_probe_to_lin_proc_->applyRGB(RGB);

        pixel_info.add_linear_channel_info(raw_info[0].channel_name, RGB[0]);
        pixel_info.add_linear_channel_info(raw_info[1].channel_name, RGB[1]);
        pixel_info.add_linear_channel_info(raw_info[2].channel_name, RGB[2]);

        pixel_info.set_linear_colourspace_name(
            std::string("Scene Linear (") + working_space(frame_id.params()) +
            std::string(")"));
    }

    // Display output

    {
        float RGB[3] = {
            raw_info[0].channel_value, raw_info[1].channel_value, raw_info[2].channel_value};

        pixel_probe_to_lin_proc_->applyRGB(RGB);

        // TODO: ColSci
        // Saturation is not managed by OCIO currently
        if (saturation != 1.0) {
            const float W[3] = {0.2126f, 0.7152f, 0.0722f};
            const float LUMA = RGB[0] * W[0] + RGB[1] * W[1] + RGB[2] * W[2];
            RGB[0]           = LUMA + saturation * (RGB[0] - LUMA);
            RGB[1]           = LUMA + saturation * (RGB[1] - LUMA);
            RGB[2]           = LUMA + saturation * (RGB[2] - LUMA);
        }

        pixel_probe_to_display_proc_->applyRGB(RGB);

        pixel_info.add_display_rgb_info("R", RGB[0]);
        pixel_info.add_display_rgb_info("G", RGB[1]);
        pixel_info.add_display_rgb_info("B", RGB[2]);

        pixel_info.set_display_colourspace_name(
            std::string("Display (") + view + std::string("|") + display + std::string(")"));

        for (const auto &extra_pix : pixel_info.extra_pixel_raw_rgba_values()) {
            // Transform from source colourspace to display, any 'extra' pixel
            // rgb values stored in pixel_info
            float RGB[3] = {extra_pix.x, extra_pix.y, extra_pix.z};
            pixel_probe_to_lin_proc_->applyRGB(RGB);

            // TODO: ColSci
            // Saturation is not managed by OCIO currently
            if (saturation != 1.0) {
                const float W[3] = {0.2126f, 0.7152f, 0.0722f};
                const float LUMA = RGB[0] * W[0] + RGB[1] * W[1] + RGB[2] * W[2];
                RGB[0]           = LUMA + saturation * (RGB[0] - LUMA);
                RGB[1]           = LUMA + saturation * (RGB[1] - LUMA);
                RGB[2]           = LUMA + saturation * (RGB[2] - LUMA);
            }

            pixel_probe_to_display_proc_->applyRGB(RGB);
            pixel_info.add_extra_pixel_display_rgba(Imath::V4f(RGB[0], RGB[1], RGB[2], 1.0f));
        }
    }
}

OCIO::ConstConfigRcPtr
OCIOEngine::get_ocio_config(const utility::JsonStore &src_colour_mgmt_metadata) const {

    const std::string config_name =
        src_colour_mgmt_metadata.get_or("ocio_config", default_config_);
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

#if OCIO_VERSION_HEX >= 0x02020100
            // CreateFromBuiltinConfig introduced in OCIO v2.2
            if (fs::exists(config_name)) {
                config = OCIO::Config::CreateFromFile(config_name.c_str());
            } else {
                config = OCIO::Config::CreateFromBuiltinConfig(config_name.c_str());
            }
#else
            config = OCIO::Config::CreateFromFile(config_name.c_str());
#endif

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
    // Workaround OCIO < 2.3.1 bug
    // See https://github.com/AcademySoftwareFoundation/OpenColorIO/issues/1885
    econfig->setDefaultViewTransformName(config->getDefaultViewTransformName());
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
OCIO::TransformRcPtr OCIOEngine::source_transform(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const bool auto_adjust_source,
    const std::string &auto_adjust_view,
    const bool bypass) const {

    auto config = get_ocio_config(src_colour_mgmt_metadata);

    if (bypass) {
        return identity_transform();
    }

    // get the user source colourspace override, if it has been set
    std::string override_input_cs =
        src_colour_mgmt_metadata.get_or("override_input_cs", std::string(""));

    if (auto_adjust_source) {
        // When 'auto adjust source' global setting is on, the source colourspace
        // is overriden by our own logic
        override_input_cs =
            input_space_for_view(src_colour_mgmt_metadata, auto_adjust_view);
    }

    const std::string filepath = src_colour_mgmt_metadata.get_or("path", std::string(""));
    const std::string working_cs = working_space(src_colour_mgmt_metadata);
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

    if (!override_input_cs.empty()) {
        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(override_input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    } else if (!input_cs.empty()) {
        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    } else if (!display.empty() and !view.empty()) {
        OCIO::DisplayViewTransformRcPtr dt = OCIO::DisplayViewTransform::Create();
        dt->setSrc(working_cs.c_str());
        dt->setDisplay(display.c_str());
        dt->setView(view.c_str());
        dt->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
        return dt;
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

OCIO::TransformRcPtr OCIOEngine::display_transform(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string &display,
    const std::string &view,
    const bool bypass) const {

    const auto &ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();

    // We always include exposure / gamma transforms to generate similar processors
    // for both viewport and thumbnails / pixel probe, hoping for better caching.

    auto exp = OCIO::ExposureContrastTransform::Create();
    exp->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
    exp->setDirection(OCIO::TRANSFORM_DIR_FORWARD);
    exp->makeExposureDynamic();
    exp->setExposure(0.0);
    group->appendTransform(exp);

    if (!bypass) {
        std::string _display = display;
        std::string _view = view;
        if (display.empty() or view.empty()) {
            _display = ocio_config->getDefaultDisplay();
            _view    = ocio_config->getDefaultView(_display.c_str());
        }

        OCIO::DisplayViewTransformRcPtr dt = OCIO::DisplayViewTransform::Create();
        dt->setSrc(working_space(src_colour_mgmt_metadata));
        dt->setDisplay(_display.c_str());
        dt->setView(_view.c_str());
        dt->setDirection(OCIO::TRANSFORM_DIR_FORWARD);
        group->appendTransform(dt);
    }

    auto gam = OCIO::ExposureContrastTransform::Create();
    gam->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
    gam->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
    gam->makeGammaDynamic();
    // default gamma pivot in OCIO is 0.18, avoid divide by zero too
    gam->setPivot(1.0f);
    gam->setGamma(1.0f);
    group->appendTransform(gam);

    return group;
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
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string &view,
    const bool auto_adjust_source,
    const bool bypass) const {

    const auto &ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    try {

        OCIO::TransformRcPtr transform = source_transform(src_colour_mgmt_metadata, auto_adjust_source, view, bypass);
        OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);
        auto proc = ocio_config->getProcessor(context, transform, OCIO::TRANSFORM_DIR_FORWARD);
        return proc;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOEngine: Failed to construct OCIO lin processor: {}", e.what());
        spdlog::warn("OCIOEngine: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstProcessorRcPtr OCIOEngine::make_display_processor(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const std::string &display,
    const std::string &view,
    const bool bypass) const {

    auto ocio_config = get_ocio_config(src_colour_mgmt_metadata);

    try {

        OCIO::TransformRcPtr transform = display_transform(src_colour_mgmt_metadata, display, view, bypass);
        OCIO::ContextRcPtr context = setup_ocio_context(src_colour_mgmt_metadata);
        auto proc = ocio_config->getProcessor(context, transform, OCIO::TRANSFORM_DIR_FORWARD);
        return proc;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOEngine: Failed to construct OCIO processor: {}", e.what());
        spdlog::warn("OCIOEngine: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

std::string OCIOEngine::detect_source_colourspace(
    const utility::JsonStore &src_colour_mgmt_metadata,
    const bool auto_adjust_source,
    const std::string &view) {

    // Extract the input colorspace as detected by the plugin and update the UI
    std::string detected_cs;
    OCIO::TransformRcPtr transform =
        source_transform(src_colour_mgmt_metadata, auto_adjust_source, view, false);
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

    std::string _preferred_view = default_view;
    if (user_preferred_view == "Default") {
        _preferred_view = default_view;
    } else if (user_preferred_view == "Automatic") {
        // Metadata from the media hook
        if (src_colour_mgmt_metadata.get_or("automatic_view", std::string("")) != "") {
            _preferred_view = src_colour_mgmt_metadata.get_or("automatic_view", default_view);
        }
        // Viewing Rules from OCIO v2 config
        else if (ocio_config->getViewingRules()->getNumEntries() > 0) {
            const std::string filepath = src_colour_mgmt_metadata.get_or("path", std::string(""));
            const std::string auto_input_cs = ocio_config->getColorSpaceFromFilepath(filepath.c_str());
            const int num_cs = ocio_config->getNumViews(default_display.c_str(), auto_input_cs.c_str());
            if (num_cs > 0) {
                _preferred_view = ocio_config->getView(default_display.c_str(), auto_input_cs.c_str(), 0);
            }
        }
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
            if (shader) {
                // ColourOperationDataPtr is shared among MediaSource using the same OCIO
                // shader. Hence ShaderDesc objects holding OCIO DynamicProperty are shared too.
                // DynamicProperty are used to hold the uniforms representing the transform,
                // we need to update them before querying the updated uniforms. The mutex is
                // needed to protect against multiple workers concurrently updating the
                // ShaderDesc's DynamicProperty resulting in queried uniforms not reflecting
                // values for the current shot.
                std::scoped_lock lock(shader->mutex);
                shader->shader_builder.updateUniforms(uniforms, exposure, gamma);
            }
        } catch (const std::exception &e) {
            spdlog::warn("OCIOColourPipeline: Failed to update shader uniforms: {}", e.what());
        }
    }
}

size_t OCIOEngine::compute_hash(
    const utility::JsonStore &src_colour_mgmt_metadata, const std::string &extra) const {
    size_t hash = 0;
    hash        = std::hash<std::string>{}(src_colour_mgmt_metadata.dump() + extra);
    return hash;
}

OCIOEngineActor::OCIOEngineActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg), OCIOEngine() {

    behavior_.assign(
        [=](global_ocio_controls_atom, const utility::JsonStore &settings) {
            set_default_config(settings.value("default_config", ""));
        },
        [=](colour_pipe_display_data_atom,
            const utility::JsonStore &media_metadata,
            const std::string &display,
            const std::string &view,
            const bool bypass) -> result<ColourOperationDataPtr> {
            // This message is sent from main OCIOColourPipeline
            try {
                return linear_to_display_op_data(media_metadata, display, view, bypass);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },
        [=](colour_pipe_linearise_data_atom,
            const utility::JsonStore &media_metadata,
            const bool bypass,
            const bool auto_adjust_source_cs,
            const std::string &view) -> result<ColourOperationDataPtr> {
            // This message is sent from main OCIOColourPipeline
            try {
                return linearise_op_data(media_metadata, bypass, auto_adjust_source_cs, view);
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
                return process_thumbnail(media_metadata, buf, display, view);
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        });
}
