// SPDX-License-Identifier: Apache-2.0
#include "ocio.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "dneg.hpp"
#include "shaders.hpp"

using namespace xstudio::colour_pipeline;
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

} // anonymous namespace

std::string OCIOColourPipeline::MediaParams::compute_hash() const {
    std::string hash;
    hash += metadata.dump(2);
    hash += user_input_cs;
    hash += user_input_display;
    hash += user_input_view;
    return hash;
}

OCIOColourPipeline::OCIOColourPipeline(const utility::JsonStore &s) : ColourPipeline(s) {
    setup_ui();
}

std::string OCIOColourPipeline::name() { return "OCIOColourPipeline"; }

const utility::Uuid &OCIOColourPipeline::class_uuid() const { return PLUGIN_UUID; }

ColourPipelineDataPtr OCIOColourPipeline::make_empty_data() const {
    return ColourPipelineDataPtr{utility::JsonStore(), std::make_shared<ColourPipelineData>()};
}

std::string OCIOColourPipeline::compute_hash(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {

    const MediaParams media_param = get_media_params(source_uuid, colour_params);

    // While OCIO processor creation is cached, we still have slight
    // gain maintaining a cache here due to all the steps involved in
    // the OCIO processor creation and because this function is called a lot.
    std::string cache_id;
    cache_id += to_string(source_uuid);
    cache_id += media_param.compute_hash();

    {
        std::scoped_lock lock(pipeline_cache_mutex_);
        if (pipeline_cache_.count(cache_id) > 0) {
            return pipeline_cache_[cache_id];
        }
    }

    std::string result_id = to_string(class_uuid());

    try {
        auto main_proc = make_processor(media_param, true, false);
        result_id += main_proc->getCacheID();
        auto popout_proc = make_processor(media_param, false, false);
        result_id += popout_proc->getCacheID();
    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute hash: {}", e.what());
    }

    {
        std::scoped_lock lock(pipeline_cache_mutex_);
        pipeline_cache_[cache_id] = result_id;
    }

    return result_id;
}

void OCIOColourPipeline::setup_shader(
    ColourPipelineData &pipe_data,
    const utility::Uuid &source_uuid,
    const utility::JsonStore &colour_params) {
    using xstudio::utility::replace_once;

    try {

        const MediaParams media_param = get_media_params(source_uuid, colour_params);

        pipe_data.colour_pipe_id_ = class_uuid();
        pipe_data.cache_id_       = compute_hash(source_uuid, colour_params);

        // Construct OCIO processor, shader and extract texture(s)
        auto main_proc   = make_processor(media_param, true, false);
        auto main_shader = make_shader(main_proc, true);
        setup_textures(main_shader, pipe_data, true);

        auto popout_proc   = make_processor(media_param, false, false);
        auto popout_shader = make_shader(popout_proc, false);
        setup_textures(popout_shader, pipe_data, false);

        // Construct and store fragment shader source code.
        std::string shader_src = ShaderTemplates::OCIO;
        std::string main_shader_src =
            replace_once(shader_src, "//OCIODisplay", main_shader->getShaderText());
        std::string popout_shader_src =
            replace_once(shader_src, "//OCIODisplay", popout_shader->getShaderText());

        // GradingPrimary implement the power function with mirrored behaviour for negatives
        // (absolute value before pow then multiply by sign). We update the shader here to
        // match ASC CDL clamping [0, 1] behaviour.
        std::regex pattern(
            R"((\w+)\.rgb = pow\( abs\(\w+\.rgb / (\w+_grading_primary_pivot)\), (\w+_grading_primary_contrast) \) \* sign\(\w+\.rgb\) \* \w+_grading_primary_pivot;)");

        main_shader_src = std::regex_replace(
            main_shader_src,
            pattern,
            "outColor.rgb = pow( clamp($1.rgb, 0.0, 1.0) / $2, $3 ) * $2;");

        popout_shader_src = std::regex_replace(
            popout_shader_src,
            pattern,
            "outColor.rgb = pow( clamp($1.rgb, 0.0, 1.0) / $2, $3 ) * $2;");

        pipe_data.main_viewport_shader_ = std::make_shared<ui::viewport::GPUShader>(
            utility::Uuid::generate(), main_shader_src);
        pipe_data.popout_viewport_shader_ = std::make_shared<ui::viewport::GPUShader>(
            utility::Uuid::generate(), popout_shader_src);

        // Store GPUShaderDesc objects for later use during uniform binding / update.
        auto shaders                       = std::make_shared<ShaderDescriptors>();
        shaders->main_viewer_shader_desc   = main_shader;
        shaders->popout_viewer_shader_desc = popout_shader;
        pipe_data.user_data_               = shaders;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to setup shader: {}", e.what());
    }
}

void OCIOColourPipeline::update_shader_uniforms(
    ColourPipelineDataPtr &pipe_data, const utility::Uuid &source_uuid) {
    if (channel_->value() == "Red") {
        pipe_data.shader_parameters_["show_chan"] = 1;
    } else if (channel_->value() == "Green") {
        pipe_data.shader_parameters_["show_chan"] = 2;
    } else if (channel_->value() == "Blue") {
        pipe_data.shader_parameters_["show_chan"] = 3;
    } else if (channel_->value() == "Alpha") {
        pipe_data.shader_parameters_["show_chan"] = 4;
    } else if (channel_->value() == "Luminance") {
        pipe_data.shader_parameters_["show_chan"] = 5;
    } else {
        pipe_data.shader_parameters_["show_chan"] = 0;
    }

    if (pipe_data->user_data_.has_value() &&
        pipe_data->user_data_.type() == typeid(ShaderDescriptorsPtr)) {
        try {
            auto shaders = std::any_cast<ShaderDescriptorsPtr>(pipe_data->user_data_);
            if (shaders && shaders->main_viewer_shader_desc &&
                shaders->popout_viewer_shader_desc) {
                // ColourPipelineData is shared among MediaSource using the same OCIO shader.
                // Hence ShaderDesc objects holding OCIO DynamicProperty are shared too.
                // DynamicProperty are used to hold the uniforms representing the transform,
                // we need to update them before querying the updated uniforms. The mutex is
                // needed to protect against multiple workers concurrently updating the
                // ShaderDesc's DynamicProperty resulting in queried uniforms not reflecting
                // values for the current shot.
                std::scoped_lock lock(shaders->mutex);

                update_dynamic_parameters(shaders->main_viewer_shader_desc, source_uuid);
                update_dynamic_parameters(shaders->popout_viewer_shader_desc, source_uuid);

                update_all_uniforms(shaders->main_viewer_shader_desc, pipe_data, source_uuid);
                update_all_uniforms(shaders->popout_viewer_shader_desc, pipe_data, source_uuid);
            }
        } catch (const std::exception &e) {
            spdlog::warn("OCIOColourPipeline: Failed to update shader uniforms: {}", e.what());
        }
    }
}

thumbnail::ThumbnailBufferPtr OCIOColourPipeline::process_thumbnail(
    const media::AVFrameID &media_ptr, const thumbnail::ThumbnailBufferPtr &buf) {

    if (buf->format() != thumbnail::TF_RGBF96) {
        spdlog::warn("OCIOColourPipeline: Invalid thumbnail buffer type, expects TF_RGBF96");
        return buf;
    }

    try {
        const MediaParams media_param =
            get_media_params(media_ptr.source_uuid_, media_ptr.params_);

        auto proc     = make_processor(media_param, true, true);
        auto cpu_proc = proc->getOptimizedCPUProcessor(
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
    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute thumbnail: {}", e.what());
    }

    return buf;
}


std::string OCIOColourPipeline::fast_display_transform_hash(const media::AVFrameID &media_ptr) {
    return get_media_params(media_ptr.source_uuid_, media_ptr.params_).compute_hash() +
           display_->value() + view_->value();
}

void OCIOColourPipeline::extend_pixel_info(
    media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) {

    try {

        const MediaParams media_param =
            get_media_params(frame_id.source_uuid_, frame_id.params_);

        std::string lin_name("linear");
        if (media_param.compute_hash() != last_pixel_probe_source_hash_) {
            auto proc =
                make_processor(media_param, true, false, pixel_probe_exposure_transform_);
            pixel_probe_proc_             = proc->getDefaultCPUProcessor();
            last_pixel_probe_source_hash_ = media_param.compute_hash();

            try {
                proc                     = make_to_lin_processor(media_param);
                pixel_probe_to_lin_proc_ = proc->getDefaultCPUProcessor();
            } catch (...) {
                pixel_probe_to_lin_proc_.reset();
            }

            lin_name = working_space(media_param);
        }

        if (pixel_probe_proc_) {
            OCIO::DynamicPropertyRcPtr property =
                pixel_probe_proc_->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE);
            OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
                OCIO::DynamicPropertyValue::AsDouble(property);
            exposure_prop->setValue(exposure_->value());

            if (!source_colour_space_->value().empty()) {
                pixel_info.set_raw_colourspace_name(
                    std::string("Source (") + source_colour_space_->value() + std::string(")"));
            }
        }


        if (pixel_probe_proc_ && pixel_info.raw_channels_info().size() >= 3) {
            float RGB[3];
            RGB[0] = pixel_info.raw_channels_info()[0].pixel_value;
            RGB[1] = pixel_info.raw_channels_info()[1].pixel_value;
            RGB[2] = pixel_info.raw_channels_info()[2].pixel_value;
            pixel_probe_proc_->applyRGB(RGB);
            pixel_info.add_display_rgb_info("R", RGB[0]);
            pixel_info.add_display_rgb_info("G", RGB[1]);
            pixel_info.add_display_rgb_info("B", RGB[2]);

            if (!source_colour_space_->value().empty()) {
                pixel_info.set_display_colourspace_name(
                    std::string("Display (") + view_->value() + std::string("|") +
                    display_->value() + std::string(")"));
            }
        }

        if (pixel_probe_to_lin_proc_ && pixel_info.raw_channels_info().size() >= 3) {
            float RGB[3];
            RGB[0] = pixel_info.raw_channels_info()[0].pixel_value;
            RGB[1] = pixel_info.raw_channels_info()[1].pixel_value;
            RGB[2] = pixel_info.raw_channels_info()[2].pixel_value;
            pixel_probe_to_lin_proc_->applyRGB(RGB);
            pixel_info.add_linear_channel_info(
                pixel_info.raw_channels_info()[0].channel_name, RGB[0]);
            pixel_info.add_linear_channel_info(
                pixel_info.raw_channels_info()[1].channel_name, RGB[1]);
            pixel_info.add_linear_channel_info(
                pixel_info.raw_channels_info()[2].channel_name, RGB[2]);

            pixel_info.set_linear_colourspace_name(
                std::string("Scene Linear (") + lin_name + std::string(")"));
        }

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute thumbnail: {}", e.what());
    }
}

OCIOColourPipeline::MediaParams OCIOColourPipeline::get_media_params(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) const {
    std::scoped_lock lock(media_params_mutex_);

    // Create an entry if empty and initialize the OCIO config.
    if (media_params_.find(source_uuid) == media_params_.end()) {
        const std::string new_config_name =
            colour_params.get_or("ocio_config", std::string(""));
        MediaParams media_param;
        media_param.source_uuid      = source_uuid;
        media_param.metadata         = colour_params;
        media_param.ocio_config      = load_ocio_config(new_config_name);
        media_param.ocio_config_name = new_config_name;
        media_params_[source_uuid]   = media_param;
    }
    // Update and reload OCIO config if source metadata have changed.
    else {
        MediaParams &media_param = media_params_[source_uuid];
        if (not colour_params.is_null() and media_param.metadata != colour_params) {
            const std::string new_config_name =
                colour_params.get_or("ocio_config", std::string(""));
            media_param.metadata         = colour_params;
            media_param.ocio_config      = load_ocio_config(new_config_name);
            media_param.ocio_config_name = new_config_name;
        }
    }

    return media_params_[source_uuid];
}

void OCIOColourPipeline::set_media_params(
    const utility::Uuid &source_uuid, const MediaParams &new_media_param) const {
    std::scoped_lock lock(media_params_mutex_);
    media_params_[source_uuid] = new_media_param;
}

OCIO::ConstConfigRcPtr
OCIOColourPipeline::load_ocio_config(const std::string &config_name) const {
    std::scoped_lock lock(ocio_config_cache_mutex_);

    auto it = ocio_config_cache_.find(config_name);
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
        spdlog::warn(
            "OCIOColourPipeline: Failed to load OCIO config {}: {}", config_name, e.what());
        spdlog::warn("OCIOColourPipeline: Fallback on raw config");
        config = OCIO::Config::CreateRaw();
    }

    ocio_config_cache_[config_name] = config;

    return config;
}

const char *OCIOColourPipeline::working_space(const MediaParams &media_param) const {
    if (not media_param.ocio_config) {
        return "";
    } else if (media_param.ocio_config->hasRole(OCIO::ROLE_SCENE_LINEAR)) {
        return OCIO::ROLE_SCENE_LINEAR;
    } else {
        return OCIO::ROLE_DEFAULT;
    }
}

// Return the transform to bring incoming data to scene_linear space.
OCIO::TransformRcPtr
OCIOColourPipeline::source_transform(const MediaParams &media_param) const {
    const std::string working_cs = working_space(media_param);

    const std::string user_input_cs = media_param.user_input_cs;
    const std::string user_display  = media_param.user_input_display;
    const std::string user_view     = media_param.user_input_view;

    const std::string filepath = media_param.metadata.get_or("path", std::string(""));
    const std::string display  = media_param.metadata.get_or("input_display", std::string(""));
    const std::string view     = media_param.metadata.get_or("input_view", std::string(""));

    // Expand input_colorspace to the first valid colorspace found.
    std::string input_cs = media_param.metadata.get_or("input_colorspace", std::string(""));
    for (const auto &cs : xstudio::utility::split(input_cs, ':')) {
        if (media_param.ocio_config->getColorSpace(cs.c_str())) {
            input_cs = cs;
            break;
        }
    }

    if (!user_input_cs.empty()) {
        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(user_input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    } else if (!user_display.empty() and !user_view.empty()) {
        return display_transform(
            working_cs, user_display, user_view, OCIO::TRANSFORM_DIR_INVERSE);
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
        if (!media_param.ocio_config->filepathOnlyMatchesDefaultRule(filepath.c_str())) {
            auto_input_cs =
                media_param.ocio_config->getColorSpaceFromFilepath(filepath.c_str());
        }

        // Manual file type (int8, int16, float, etc) to role / colorspace mapping
        // => Not Implemented

        // Fallback to default rule
        auto_input_cs = media_param.ocio_config->getColorSpaceFromFilepath(filepath.c_str());

        OCIO::ColorSpaceTransformRcPtr csc = OCIO::ColorSpaceTransform::Create();
        csc->setSrc(auto_input_cs.c_str());
        csc->setDst(working_cs.c_str());
        return csc;
    }
}

const char *OCIOColourPipeline::default_display(
    const MediaParams &media_param, const std::string &monitor_name) const {
    if (media_param.metadata.get_or("viewing_rules", false)) {
        return dneg_ocio_default_display(media_param.ocio_config, monitor_name).c_str();
    } else {
        return media_param.ocio_config->getDefaultDisplay();
    }
}

// Return the transform from scene_linear to display space.
OCIO::TransformRcPtr OCIOColourPipeline::display_transform(
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

OCIO::TransformRcPtr OCIOColourPipeline::identity_transform() const {
    return OCIO::MatrixTransform::Create();
}

OCIO::ConstProcessorRcPtr
OCIOColourPipeline::make_to_lin_processor(const MediaParams &media_param) const {
    const auto &metadata         = media_param.metadata;
    const auto &ocio_config      = media_param.ocio_config;
    const auto &ocio_config_name = media_param.ocio_config_name;

    try {
        // Setup the OCIO context based on incoming metadata

        OCIO::ContextRcPtr context = ocio_config->getCurrentContext()->createEditableCopy();
        if (metadata.contains("ocio_context")) {
            if (metadata["ocio_context"].is_object()) {
                for (auto &item : metadata["ocio_context"].items()) {
                    context->setStringVar(
                        item.key().c_str(), std::string(item.value()).c_str());
                }
            } else {
                spdlog::warn(
                    "OCIOColourPipeline: 'ocio_context' should be a dictionary, got {} instead",
                    metadata["ocio_context"].dump(2));
            }
        }

        // Construct an OCIO processor for the whole colour pipeline

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();

        if (colour_bypass_->value()) {
            return ocio_config->getProcessor(identity_transform());
        }

        group->appendTransform(source_transform(media_param));
        return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to construct OCIO processor: {}", e.what());
        spdlog::warn("OCIOColourPipeline: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstProcessorRcPtr OCIOColourPipeline::make_processor(
    const MediaParams &media_param, bool is_main_viewer, bool is_thumbnail) const {
    // A bit clumsy perhaps - we only need to get at the exposure/contrast transform
    // for the pixel inspector so use this method elsewhere
    OCIO::ExposureContrastTransformRcPtr dummy;
    return make_processor(media_param, is_main_viewer, is_thumbnail, dummy);
}

OCIO::ConstProcessorRcPtr OCIOColourPipeline::make_processor(
    const MediaParams &media_param,
    bool is_main_viewer,
    bool is_thumbnail,
    OCIO::ExposureContrastTransformRcPtr &ect) const {
    const auto &metadata         = media_param.metadata;
    const auto &ocio_config      = media_param.ocio_config;
    const auto &ocio_config_name = media_param.ocio_config_name;

    try {
        // Setup the OCIO context based on incoming metadata

        OCIO::ContextRcPtr context = ocio_config->getCurrentContext()->createEditableCopy();
        if (metadata.contains("ocio_context")) {
            if (metadata["ocio_context"].is_object()) {
                for (auto &item : metadata["ocio_context"].items()) {
                    context->setStringVar(
                        item.key().c_str(), std::string(item.value()).c_str());
                }
            } else {
                spdlog::warn(
                    "OCIOColourPipeline: 'ocio_context' should be a dictionary, got {} instead",
                    metadata["ocio_context"].dump(2));
            }
        }

        // Construct an OCIO processor for the whole colour pipeline

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();

        if (colour_bypass_->value()) {
            return ocio_config->getProcessor(identity_transform());
        }

        group->appendTransform(source_transform(media_param));

        if (!is_thumbnail) {
            ect = OCIO::ExposureContrastTransform::Create();
            ect->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
            ect->setDirection(OCIO::TRANSFORM_DIR_FORWARD);
            ect->makeExposureDynamic();
            ect->setExposure(exposure_->value());

            group->appendTransform(ect);
        }

        // Determines which OCIO display / view to use

        std::string display;
        std::string view;

        if (ocio_config_name == current_config_name_) {
            display = is_main_viewer ? display_->value() : popout_viewer_display_->value();
            view    = view_->value();
        } else {
            std::scoped_lock l(per_config_settings_mutex_);

            auto it = per_config_settings_.find(ocio_config_name);
            if (it != per_config_settings_.end()) {
                display =
                    is_main_viewer ? it->second.display : it->second.popout_viewer_display;
                view = it->second.view;
            } else {
                display = ocio_config->getDefaultDisplay();
                view    = ocio_config->getDefaultView(display.c_str());
            }
        }

        if (display.empty() or view.empty()) {
            display = ocio_config->getDefaultDisplay();
            view    = ocio_config->getDefaultView(display.c_str());
        }

        // Turns per shot CDLs into dynamic transform
        std::string view_looks =
            ocio_config->getDisplayViewLooks(display.c_str(), view.c_str());
        std::string dynamic_look;
        std::string dynamic_file;

        if (metadata.contains("dynamic_cdl")) {
            if (metadata["dynamic_cdl"].is_object()) {
                for (auto &item : metadata["dynamic_cdl"].items()) {
                    if (view_looks.find(item.key()) != std::string::npos) {
                        dynamic_look = item.key();
                        dynamic_file = item.value();
                    }
                }
            } else {
                spdlog::warn(
                    "OCIOColourPipeline: 'dynamic_cdl' should be a dictionary, got {} instead",
                    metadata["dynamic_cdl"].dump(2));
            }
        }

        if (!dynamic_look.empty() and !dynamic_file.empty()) {
            return make_dynamic_display_processor(
                media_param,
                ocio_config,
                context,
                group,
                display,
                view,
                dynamic_look,
                dynamic_file);
        } else {
            group->appendTransform(display_transform(
                working_space(media_param), display, view, OCIO::TRANSFORM_DIR_FORWARD));

            return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);
        }
    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to construct OCIO processor: {}", e.what());
        spdlog::warn("OCIOColourPipeline: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}


OCIO::ConstProcessorRcPtr OCIOColourPipeline::make_dynamic_display_processor(
    const MediaParams &media_param,
    const OCIO::ConstConfigRcPtr &config,
    const OCIO::ConstContextRcPtr &context,
    const OCIO::GroupTransformRcPtr &group,
    const std::string &display,
    const std::string &view,
    const std::string &look_name,
    const std::string &cdl_file_name) const {
    try {
        // Load the CDL and derive a GradingPrimary from it
        auto look_path     = context->resolveFileLocation(cdl_file_name.c_str());
        auto cdl_transform = OCIO::CDLTransform::CreateFromFile(look_path, "");

        // Update the MediaParams here so that each shots gets to know it's
        // own GradingPrimary value to be used as uniforms. The pipeline data
        // will otherwise be shared.
        MediaParams updated_media_params = media_param;
        auto primary                     = grading_primary_from_cdl(cdl_transform);
        updated_media_params.primary     = primary;
        set_media_params(media_param.source_uuid, updated_media_params);

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
            working_space(media_param), display, view_name, OCIO::TRANSFORM_DIR_FORWARD));

        return dynamic_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);
    } catch (const OCIO::Exception &ex) {
        group->appendTransform(display_transform(
            working_space(media_param), display, view, OCIO::TRANSFORM_DIR_FORWARD));

        return config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);
    }
}

OCIO::ConstGpuShaderDescRcPtr OCIOColourPipeline::make_shader(
    OCIO::ConstProcessorRcPtr &processor, bool is_main_viewer) const {
    OCIO::GpuShaderDescRcPtr shader_desc = OCIO::GpuShaderDesc::CreateShaderDesc();
    shader_desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);

    shader_desc->setFunctionName("OCIODisplay");
    if (is_main_viewer) {
        shader_desc->setResourcePrefix("main_");
    } else {
        shader_desc->setResourcePrefix("popout_");
    }

    auto gpu_proc = processor->getDefaultGPUProcessor();
    gpu_proc->extractGpuShaderInfo(shader_desc);
    return shader_desc;
}

void OCIOColourPipeline::setup_textures(
    OCIO::ConstGpuShaderDescRcPtr &shader_desc,
    ColourPipelineData &pipe_data,
    bool is_main_viewer) const {
    auto target_viewer = is_main_viewer ? ColourLUT::MAIN_VIEWER : ColourLUT::POPOUT_VIEWER;

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
        xs_lut->set_target_viewer(target_viewer);
        pipe_data.luts_.push_back(xs_lut);
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
        xs_lut->set_target_viewer(target_viewer);
        pipe_data.luts_.push_back(xs_lut);
    }
}

void OCIOColourPipeline::update_dynamic_parameters(
    OCIO::ConstGpuShaderDescRcPtr &shader, const utility::Uuid &source_uuid) const {
    // Exposure property
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE)) {
        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE);
        OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        exposure_prop->setValue(exposure_->value());
    }
    // Shot CDL
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY)) {
        MediaParams media_param = get_media_params(source_uuid);

        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY);
        OCIO::DynamicPropertyGradingPrimaryRcPtr primary_prop =
            OCIO::DynamicPropertyValue::AsGradingPrimary(property);
        OCIO::GradingPrimary primary = media_param.primary;
        primary_prop->setValue(primary);
    }
}

void OCIOColourPipeline::update_all_uniforms(
    OCIO::ConstGpuShaderDescRcPtr &shader,
    ColourPipelineDataPtr &pipe_data,
    const utility::Uuid &source_uuid) const {
    const unsigned max_uniforms = shader->getNumUniforms();
    for (unsigned idx = 0; idx < max_uniforms; ++idx) {
        OCIO::GpuShaderDesc::UniformData uniform_data;
        const char *name = shader->getUniform(idx, uniform_data);

        switch (uniform_data.m_type) {
        case OCIO::UNIFORM_DOUBLE: {
            pipe_data.shader_parameters_[name] = uniform_data.m_getDouble();
            break;
        }
        case OCIO::UNIFORM_BOOL: {
            pipe_data.shader_parameters_[name] = uniform_data.m_getBool();
            break;
        }
        case OCIO::UNIFORM_FLOAT3: {
            pipe_data.shader_parameters_[name] = {
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
            spdlog::warn("OCIOColourPipeline: Unknown uniform type for dynamic property");
            break;
        }
    }
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<ColourPipelinePlugin<ColourPipelineActor<OCIOColourPipeline>>>(
                PLUGIN_UUID,
                "OCIOColourPipeline",
                "xStudio",
                "OCIO (v2) Colour Pipeline",
                semver::version("1.0.0"))}));
}
}
