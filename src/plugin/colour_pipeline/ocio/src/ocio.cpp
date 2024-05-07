// SPDX-License-Identifier: Apache-2.0
#include "ocio.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"

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
    hash += output_view;
    return hash;
}

OCIOColourPipeline::OCIOColourPipeline(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ColourPipeline(cfg, init_settings) {

    setup_ui();
}

void OCIOColourPipeline::on_exit() {
    auto main_ocio =
        system().registry().template get<caf::actor>("MAIN_VIEWPORT_OCIO_INSTANCE");
    if (main_ocio == self()) {
        system().registry().erase("MAIN_VIEWPORT_OCIO_INSTANCE");
    }
}

std::string OCIOColourPipeline::linearise_op_hash(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {

    if (colour_bypass_->value())
        return "LineariseTransformBypass";


    // By making the hash include our UUID we can make sure that any other
    // OCIO colour pipelines that construct the colour op data hash Id won't
    // clash.
    std::string result_id = to_string(PLUGIN_UUID);

    try {

        const MediaParams media_param = get_media_params(source_uuid, colour_params);
        auto main_proc                = make_to_lin_processor(media_param);
        result_id += main_proc->getCacheID();

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute linearise hash: {}", e.what());
    }

    return result_id;
}

std::string OCIOColourPipeline::linear_to_display_op_hash(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {

    if (colour_bypass_->value())
        return "DisplayTransformBypass";

    std::string result_id = to_string(PLUGIN_UUID);

    try {

        const MediaParams media_param = get_media_params(source_uuid, colour_params);
        auto main_proc                = make_display_processor(media_param, false);
        result_id += main_proc->getCacheID();
        {
            // Here we make sure the cacheID string depends on any grading primaries data
            // that may have been picked up for the source (if we are applying the primary
            // grade).
            const MediaParams media_param = get_media_params(source_uuid, colour_params);
            std::stringstream ss;
            ss << media_param.primary;
            result_id += ss.str();
        }

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute display hash: {}", e.what());
    }

    return result_id;
}

std::string OCIOColourPipeline::fast_display_transform_hash(const media::AVFrameID &media_ptr) {
    return get_media_params(media_ptr.source_uuid_, media_ptr.params_).compute_hash() +
           (colour_bypass_->value() ? "null" : display_->value() + view_->value());
}

ColourOperationDataPtr OCIOColourPipeline::linearise_op_data(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {

    using xstudio::utility::replace_once;

    auto data = std::make_shared<ColourOperationData>("OCIO Linearise OP");

    try {

        const MediaParams media_param = get_media_params(source_uuid, colour_params);

        // Construct OCIO processor, shader and extract texture(s)
        auto to_lin_proc   = make_to_lin_processor(media_param);
        auto to_lin_shader = make_shader(to_lin_proc, "OCIOLinearise", "to_linear_");
        setup_textures(to_lin_shader, data);

        std::string to_lin_shader_src = replace_once(
            ShaderTemplates::OCIO_linearise, "//OCIOLinearise", to_lin_shader->getShaderText());

        data->shader_ = std::make_shared<ui::opengl::OpenGLShader>(
            utility::Uuid::generate(), to_lin_shader_src);

        // Store GPUShaderDesc objects for later use during uniform binding / update.
        // Note that we need updated MediaParams object, which may include primary grading
        // data, when we update the shader uniforms at draw time. Hence we add the MediaParams
        // as part of the user_data_ blind data object.
        auto desc         = std::make_shared<ShaderDescriptor>();
        desc->shader_desc = to_lin_shader;
        desc->params      = get_media_params(source_uuid, colour_params);
        ;
        data->user_data_ = desc;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to setup lin shader: {}", e.what());
    }

    return data;
}

ColourOperationDataPtr OCIOColourPipeline::linear_to_display_op_data(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) {

    using xstudio::utility::replace_once;

    auto data = std::make_shared<ColourOperationData>(ColourOperationData("OCIO Display OP"));

    try {

        const MediaParams media_param = get_media_params(source_uuid, colour_params);

        // Construct OCIO processor, shader and extract texture(s)
        auto display_proc   = make_display_processor(media_param, false);
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
        desc->params      = get_media_params(source_uuid, colour_params);

        data->user_data_ = desc;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to setup display shader: {}", e.what());
    }

    return data;
}

utility::JsonStore OCIOColourPipeline::update_shader_uniforms(
    const media_reader::ImageBufPtr &image, std::any &user_data) {

    utility::JsonStore uniforms;
    if (channel_->value() == "Red") {
        uniforms["show_chan"] = 1;
    } else if (channel_->value() == "Green") {
        uniforms["show_chan"] = 2;
    } else if (channel_->value() == "Blue") {
        uniforms["show_chan"] = 3;
    } else if (channel_->value() == "Alpha") {
        uniforms["show_chan"] = 4;
    } else if (channel_->value() == "Luminance") {
        uniforms["show_chan"] = 5;
    } else {
        uniforms["show_chan"] = 0;
    }

    // TODO: ColSci
    // Saturation is not managed by OCIO currently
    uniforms["saturation"] = enable_saturation_->value() ? saturation_->value() : 1.0f;

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
                update_dynamic_parameters(shader->shader_desc, shader->params);
                update_all_uniforms(
                    shader->shader_desc, uniforms, image.frame_id().source_uuid_);
            }
        } catch (const std::exception &e) {
            spdlog::warn("OCIOColourPipeline: Failed to update shader uniforms: {}", e.what());
        }
    }
    return uniforms;
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

        // TODO: just use a single pass processor rather than two step processors.
        auto to_lin_proc         = make_to_lin_processor(media_param);
        auto lin_to_display_proc = make_display_processor(media_param, true);

        auto cpu_to_lin_proc = to_lin_proc->getOptimizedCPUProcessor(
            OCIO::BIT_DEPTH_F32, OCIO::BIT_DEPTH_F32, OCIO::OPTIMIZATION_DEFAULT);

        auto cpu_lin_to_display_proc = lin_to_display_proc->getOptimizedCPUProcessor(
            OCIO::BIT_DEPTH_F32, OCIO::BIT_DEPTH_UINT8, OCIO::OPTIMIZATION_DEFAULT);

        auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
            buf->width(), buf->height(), thumbnail::TF_RGB24);
        auto src = reinterpret_cast<float *>(buf->data().data());
        auto dst = reinterpret_cast<uint8_t *>(thumb->data().data());

        std::vector<float> intermediate(buf->width() * buf->height() * buf->channels());

        OCIO::PackedImageDesc in_img(
            src,
            buf->width(),
            buf->height(),
            buf->channels(),
            OCIO::BIT_DEPTH_F32,
            OCIO::AutoStride,
            OCIO::AutoStride,
            OCIO::AutoStride);

        OCIO::PackedImageDesc intermediate_img(
            intermediate.data(),
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

        cpu_to_lin_proc->apply(in_img, intermediate_img);
        cpu_lin_to_display_proc->apply(intermediate_img, out_img);
        return thumb;

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute thumbnail: {}", e.what());
    }

    return buf;
}

void OCIOColourPipeline::extend_pixel_info(
    media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) {

    if (pixel_info.raw_channels_info().size() < 3)
        return;

    try {

        MediaParams media_param = get_media_params(frame_id.source_uuid_, frame_id.params_);

        media_param.output_view = view_->value();

        auto raw_info = pixel_info.raw_channels_info();

        if (media_param.compute_hash() != last_pixel_probe_source_hash_) {
            pixel_probe_to_display_proc_ =
                make_display_processor(media_param, false)->getDefaultCPUProcessor();
            pixel_probe_to_lin_proc_ =
                make_to_lin_processor(media_param)->getDefaultCPUProcessor();
            last_pixel_probe_source_hash_ = media_param.compute_hash();
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
                exposure_prop->setValue(exposure_->value());
            }
            {
                // Gamma
                OCIO::DynamicPropertyRcPtr property =
                    pixel_probe_to_display_proc_->getDynamicProperty(
                        OCIO::DYNAMIC_PROPERTY_GAMMA);
                OCIO::DynamicPropertyDoubleRcPtr gamma_prop =
                    OCIO::DynamicPropertyValue::AsDouble(property);
                gamma_prop->setValue(enable_gamma_->value() ? gamma_->value() : 1.0f);
            }
        } catch ([[maybe_unused]] const OCIO::Exception &e) {
            // TODO: ColSci
            // Update when OCIO::CPUProcessor include hasDynamicProperty()
        }

        // Source

        if (!source_colour_space_->value().empty()) {
            pixel_info.set_raw_colourspace_name(
                std::string("Source (") + source_colour_space_->value() + std::string(")"));
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
                std::string("Scene Linear (") + working_space(media_param) + std::string(")"));
        }

        // Display output

        {
            float RGB[3] = {
                raw_info[0].pixel_value, raw_info[1].pixel_value, raw_info[2].pixel_value};

            pixel_probe_to_display_proc_->applyRGB(RGB);

            // TODO: ColSci
            // Saturation is not managed by OCIO currently
            if (saturation_->value() != 1.0) {
                const float W[3] = {0.2126f, 0.7152f, 0.0722f};
                const float LUMA = RGB[0] * W[0] + RGB[1] * W[1] + RGB[2] * W[2];
                RGB[0]           = LUMA + saturation_->value() * (RGB[0] - LUMA);
                RGB[1]           = LUMA + saturation_->value() * (RGB[1] - LUMA);
                RGB[2]           = LUMA + saturation_->value() * (RGB[2] - LUMA);
            }

            pixel_info.add_display_rgb_info("R", RGB[0]);
            pixel_info.add_display_rgb_info("G", RGB[1]);
            pixel_info.add_display_rgb_info("B", RGB[2]);

            pixel_info.set_display_colourspace_name(
                std::string("Display (") + media_param.output_view + std::string("|") +
                display_->value() + std::string(")"));
        }

    } catch (const std::exception &e) {
        spdlog::warn("OCIOColourPipeline: Failed to compute pixel probe: {}", e.what());
    }
}

void OCIOColourPipeline::init_media_params(MediaParams &media_param) const {

    const auto &metadata = media_param.metadata;

    const std::string config_name = metadata.get_or("ocio_config", std::string(""));

    media_param.ocio_config      = load_ocio_config(config_name);
    media_param.ocio_config_name = config_name;
    media_param.output_view      = preferred_ocio_view(media_param, preferred_view_->value());

    if (metadata.contains("active_displays")) {
        const std::string displays = metadata.get_or("active_displays", std::string(""));

        auto config = media_param.ocio_config->createEditableCopy();
        config->setActiveDisplays(displays.c_str());
        media_param.ocio_config = config;
    }
    if (metadata.contains("active_views")) {
        const std::string views = metadata.get_or("active_views", std::string(""));

        auto config = media_param.ocio_config->createEditableCopy();
        config->setActiveViews(views.c_str());
        media_param.ocio_config = config;
    }
}

OCIOColourPipeline::MediaParams OCIOColourPipeline::get_media_params(
    const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) const {

    // Create an entry if empty and initialize the OCIO config.
    if (media_params_.find(source_uuid) == media_params_.end()) {
        MediaParams media_param;
        media_param.source_uuid = source_uuid;
        media_param.metadata    = colour_params;
        init_media_params(media_param);
        media_params_[source_uuid] = media_param;
    }
    // Update and reload OCIO config if source metadata have changed.
    else {
        MediaParams &media_param = media_params_[source_uuid];
        if (not colour_params.is_null() and media_param.metadata != colour_params) {
            media_param.metadata = colour_params;
            init_media_params(media_param);
        }
    }

    return media_params_[source_uuid];
}

void OCIOColourPipeline::set_media_params(const MediaParams &new_media_param) const {
    media_params_[new_media_param.source_uuid] = new_media_param;
}

std::string OCIOColourPipeline::input_space_for_view(
    const MediaParams &media_param, const std::string &view) const {

    std::string new_colourspace;

    auto colourspace_or = [media_param](const std::string &cs, const std::string &fallback){
        const bool has_cs = bool(media_param.ocio_config->getColorSpace(cs.c_str()));
        return has_cs ? cs : fallback;
    };

    if (media_param.metadata.contains("input_category")) {
        const auto is_untonemapped = view == "Un-tone-mapped";
        const auto category = media_param.metadata["input_category"];
        if (category == "internal_movie") {
            new_colourspace = is_untonemapped ?
                "disp_Rec709-G24" : colourspace_or("DNEG_Rec709", "Film_Rec709");
        } else if (category == "edit_ref" or category == "movie_media") {
            new_colourspace = is_untonemapped ?
                "disp_Rec709-G24" : colourspace_or("Client_Rec709", "Film_Rec709");
        } else if (category == "still_media") {
            new_colourspace = is_untonemapped ?
                "disp_sRGB" : colourspace_or("DNEG_sRGB", "Film_sRGB");
        }

        // Double check the new colourspace actually exists
        new_colourspace = colourspace_or(new_colourspace, "");
    }

    return new_colourspace;
}

std::string OCIOColourPipeline::preferred_ocio_view(
    const MediaParams &media_param, const std::string &ocio_view) const {

    // Get the default display from OCIO config
    const OCIO::ConstConfigRcPtr ocio_config = media_param.ocio_config;
    const std::string default_display        = ocio_config->getDefaultDisplay();
    const std::string default_view = ocio_config->getDefaultView(default_display.c_str());

    std::string preferred_view;
    if (ocio_view == ui_text_.DEFAULT_VIEW) {
        preferred_view = default_view;
    } else if (ocio_view == ui_text_.AUTOMATIC_VIEW) {
        preferred_view = media_param.metadata.get_or("automatic_view", default_view);
    } else {
        preferred_view = ocio_view;
    }

    // Validate that the view is in ocio config
    std::map<std::string, std::vector<std::string>> display_views;
    const auto displays = parse_display_views(ocio_config, display_views);

    for (auto it = begin(display_views); it != end(display_views); ++it) {
        for (auto view_in_ocio_config : it->second) {
            if (view_in_ocio_config == preferred_view) {
                return preferred_view;
            }
        }
    }
    // If view is not avaialble return the default view
    return ocio_config->getDefaultView(default_display.c_str());
}

OCIO::ConstConfigRcPtr
OCIOColourPipeline::load_ocio_config(const std::string &config_name) const {

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

const char *OCIOColourPipeline::default_display(
    const MediaParams &media_param, const std::string &monitor_name) const {
    if (media_param.metadata.get_or("viewing_rules", false)) {
        return dneg_ocio_default_display(media_param.ocio_config, monitor_name).c_str();
    } else {
        return media_param.ocio_config->getDefaultDisplay();
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

OCIO::ConstConfigRcPtr OCIOColourPipeline::display_transform(
    const MediaParams &media_param,
    OCIO::ContextRcPtr context,
    OCIO::GroupTransformRcPtr group) const {

    const auto &metadata         = media_param.metadata;
    auto ocio_config             = media_param.ocio_config;
    const auto &ocio_config_name = media_param.ocio_config_name;

    // Determines which OCIO display / view to use

    std::string display;
    std::string view;

    if (ocio_config_name == current_config_name_ || is_worker()) {
        // if we are a worker, our view has been set by the main
        // OCIOColourPipeline, we don't need to worry about fallback
        // to defaults etc.
        display = display_->value();
        view    = global_view_->value() ? view_->value() : media_param.output_view;
    } else {
        auto it = per_config_settings_.find(ocio_config_name);
        if (it != per_config_settings_.end()) {
            display = it->second.display;
            view    = global_view_->value() ? it->second.view : media_param.output_view;
        }
    }

    if (display.empty() or view.empty()) {
        display = ocio_config->getDefaultDisplay();
        view    = ocio_config->getDefaultView(display.c_str());
    }

    // Turns per shot CDLs into dynamic transform

    std::string view_looks = ocio_config->getDisplayViewLooks(display.c_str(), view.c_str());
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
        ocio_config = make_dynamic_display_processor(
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
    }

    return ocio_config;
}

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

OCIO::ContextRcPtr
OCIOColourPipeline::setup_ocio_context(const MediaParams &media_param) const {

    const auto &metadata    = media_param.metadata;
    const auto &ocio_config = media_param.ocio_config;

    OCIO::ContextRcPtr context = ocio_config->getCurrentContext()->createEditableCopy();

    // Setup the OCIO context based on incoming metadata

    if (metadata.contains("ocio_context")) {
        if (metadata["ocio_context"].is_object()) {
            for (auto &item : metadata["ocio_context"].items()) {
                context->setStringVar(item.key().c_str(), std::string(item.value()).c_str());
            }
        } else {
            spdlog::warn(
                "OCIOColourPipeline: 'ocio_context' should be a dictionary, got {} instead",
                metadata["ocio_context"].dump(2));
        }
    }

    return context;
}

OCIO::ConstProcessorRcPtr
OCIOColourPipeline::make_to_lin_processor(const MediaParams &media_param) const {

    const auto &metadata         = media_param.metadata;
    const auto &ocio_config      = media_param.ocio_config;
    const auto &ocio_config_name = media_param.ocio_config_name;

    try {

        if (colour_bypass_->value()) {
            return ocio_config->getProcessor(identity_transform());
        }

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();
        group->appendTransform(source_transform(media_param));

        OCIO::ContextRcPtr context = setup_ocio_context(media_param);
        return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);

    } catch (const std::exception &e) {
        spdlog::warn(
            "OCIOColourPipeline: Failed to construct OCIO lin processor: {}", e.what());
        spdlog::warn("OCIOColourPipeline: Defaulting to no-op processor");
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstProcessorRcPtr OCIOColourPipeline::make_display_processor(
    const MediaParams &media_param, bool is_thumbnail) const {

    auto ocio_config = media_param.ocio_config;

    try {

        OCIO::ContextRcPtr context = setup_ocio_context(media_param);

        OCIO::GroupTransformRcPtr group = OCIO::GroupTransform::Create();

        if (!is_thumbnail) {
            auto ect = OCIO::ExposureContrastTransform::Create();
            ect->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
            ect->setDirection(OCIO::TRANSFORM_DIR_FORWARD);
            ect->makeExposureDynamic();
            ect->setExposure(exposure_->value());

            group->appendTransform(ect);
        }

        if (!colour_bypass_->value()) {
            // To support dynamic CDLs we currently have to edit the OCIO
            // config in place, hence the need to return the new config
            // here to later query the processor from it.
            ocio_config = display_transform(media_param, context, group);
        }

        if (!is_thumbnail) {
            auto ect = OCIO::ExposureContrastTransform::Create();
            ect->setStyle(OCIO::EXPOSURE_CONTRAST_LINEAR);
            ect->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
            ect->makeGammaDynamic();
            // default gamma pivot in OCIO is 0.18, avoid divide by zero too
            ect->setPivot(1.0f);
            ect->setGamma(enable_gamma_->value() ? gamma_->value() : 1.0f);

            group->appendTransform(ect);
        }

        return ocio_config->getProcessor(context, group, OCIO::TRANSFORM_DIR_FORWARD);

    } catch (const std::exception &e) {
        if (media_param.ocio_config_name != "__raw__") {
            spdlog::warn(
                "OCIOColourPipeline: Failed to construct OCIO processor: {}", e.what());
            spdlog::warn("OCIOColourPipeline: Defaulting to no-op processor");
        }
        return ocio_config->getProcessor(identity_transform());
    }
}

OCIO::ConstConfigRcPtr OCIOColourPipeline::make_dynamic_display_processor(
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

        updated_media_params.primary = primary;
        set_media_params(updated_media_params);

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

        return dynamic_config;

    } catch ([[maybe_unused]] const OCIO::Exception &ex) {
        group->appendTransform(display_transform(
            working_space(media_param), display, view, OCIO::TRANSFORM_DIR_FORWARD));

        return config;
    }
}

OCIO::ConstGpuShaderDescRcPtr OCIOColourPipeline::make_shader(
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

void OCIOColourPipeline::setup_textures(
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

void OCIOColourPipeline::update_dynamic_parameters(
    OCIO::ConstGpuShaderDescRcPtr &shader, const MediaParams &media_param) const {

    // Exposure property
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE)) {
        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE);
        OCIO::DynamicPropertyDoubleRcPtr exposure_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        exposure_prop->setValue(exposure_->value());
    }
    // Gamma property
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA)) {
        OCIO::DynamicPropertyRcPtr property =
            shader->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA);
        OCIO::DynamicPropertyDoubleRcPtr gamma_prop =
            OCIO::DynamicPropertyValue::AsDouble(property);
        gamma_prop->setValue(enable_gamma_->value() ? gamma_->value() : 1.0f);
    }
    // Shot CDL
    if (shader->hasDynamicProperty(OCIO::DYNAMIC_PROPERTY_GRADING_PRIMARY)) {

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
    utility::JsonStore &uniforms,
    const utility::Uuid &source_uuid) const {

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
            spdlog::warn("OCIOColourPipeline: Unknown uniform type for dynamic property");
            break;
        }
    }
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<OCIOColourPipeline>>(
                PLUGIN_UUID,
                "OCIOColourPipeline",
                plugin_manager::PluginFlags::PF_COLOUR_MANAGEMENT,
                false,
                "xStudio",
                "OCIO (v2) Colour Pipeline",
                semver::version("1.0.0"))}));
}
}