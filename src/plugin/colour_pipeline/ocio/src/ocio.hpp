// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cmath>
#include <cfloat>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <typeinfo>

#include <OpenColorIO/OpenColorIO.h> //NOLINT

#include "xstudio/colour_pipeline/colour_pipeline_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "ui_text.hpp"

namespace OCIO = OCIO_NAMESPACE;


namespace xstudio::colour_pipeline {

class OCIOColourPipeline : public ColourPipeline {
  private:
    struct PerConfigSettings {
        std::string display;
        std::string view;
    };

    struct MediaParams {
        utility::Uuid source_uuid;

        OCIO::ConstConfigRcPtr ocio_config;
        std::string ocio_config_name;

        utility::JsonStore metadata;
        std::string user_input_cs;
        std::string user_input_display;
        std::string user_input_view;

        OCIO::GradingPrimary primary = OCIO::GradingPrimary(OCIO::GRADING_LIN);

        std::string output_view;

        std::string compute_hash() const;
    };

    struct ShaderDescriptor {
        OCIO::ConstGpuShaderDescRcPtr shader_desc;
        MediaParams params;
        std::mutex mutex;
    };
    typedef std::shared_ptr<ShaderDescriptor> ShaderDescriptorPtr;

  public:
    explicit OCIOColourPipeline(
        caf::actor_config &cfg, const utility::JsonStore &init_settings);

    void on_exit() override;

    std::string fast_display_transform_hash(const media::AVFrameID &media_ptr) override;

    [[nodiscard]] std::string linearise_op_hash(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &media_source_colour_metadata) override;

    [[nodiscard]] std::string linear_to_display_op_hash(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &media_source_colour_metadata) override;

    /* Create the ColourOperationDataPtr containing the necessary LUT and
    shader data for linearising the source colourspace RGB data from the
    given media source on the screen */
    ColourOperationDataPtr linearise_op_data(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &media_source_colour_metadata) override;

    /* Create the ColourOperationDataPtr containing the necessary LUT and
    shader data for transforming linear colour values into display space */
    ColourOperationDataPtr linear_to_display_op_data(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &media_source_colour_metadata) override;

    // Update colour pipeline shader dynamic parameters.
    utility::JsonStore update_shader_uniforms(
        const media_reader::ImageBufPtr &image, std::any &user_data) override;

    thumbnail::ThumbnailBufferPtr process_thumbnail(
        const media::AVFrameID &media_ptr, const thumbnail::ThumbnailBufferPtr &buf) override;

    // GUI handling
    void media_source_changed(
        const utility::Uuid &source_uuid, const utility::JsonStore &colour_params) override;
    void attribute_changed(const utility::Uuid &attribute_uuid, const int /*role*/) override;
    void hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context) override;
    void hotkey_released(const utility::Uuid &hotkey_uuid, const std::string &context) override;
    bool pointer_event(const ui::PointerEvent &e) override;
    void screen_changed(
        const std::string &name,
        const std::string &model,
        const std::string &manufacturer,
        const std::string &serialNumber) override;

    void register_hotkeys() override;

    void connect_to_viewport(
        const std::string &viewport_name,
        const std::string &viewport_toolbar_name,
        bool connect) override;

    void extend_pixel_info(
        media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) override;

    // CAF details
    bool allow_workers() const override { return true; }

    caf::actor self_spawn(const utility::JsonStore &s) override {
        return spawn<OCIOColourPipeline>(s);
    }

  private:
    void init_media_params(MediaParams &media_param) const;

    MediaParams get_media_params(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &colour_params = utility::JsonStore()) const;

    void set_media_params(const MediaParams &media_param) const;

    // OCIO logic

    std::string
    input_space_for_view(const MediaParams &media_param, const std::string &view) const;

    std::string
    preferred_ocio_view(const MediaParams &media_param, const std::string &view) const;

    const char *working_space(const MediaParams &media_param) const;

    const char *
    default_display(const MediaParams &media_param, const std::string &monitor_name = "") const;

    // OCIO Transform helpers

    OCIO::TransformRcPtr source_transform(const MediaParams &media_param) const;

    OCIO::ConstConfigRcPtr display_transform(
        const MediaParams &media_param,
        OCIO::ContextRcPtr context,
        OCIO::GroupTransformRcPtr group) const;

    OCIO::TransformRcPtr display_transform(
        const std::string &source,
        const std::string &display,
        const std::string &view,
        OCIO::TransformDirection direction) const;

    OCIO::TransformRcPtr identity_transform() const;

    // OCIO setup

    OCIO::ConstConfigRcPtr load_ocio_config(const std::string &config_name) const;

    OCIO::ContextRcPtr setup_ocio_context(const MediaParams &media_param) const;

    OCIO::ConstProcessorRcPtr make_to_lin_processor(const MediaParams &media_param) const;

    OCIO::ConstProcessorRcPtr
    make_display_processor(const MediaParams &media_param, bool is_thumbnail) const;

    OCIO::ConstConfigRcPtr make_dynamic_display_processor(
        const MediaParams &media_param,
        const OCIO::ConstConfigRcPtr &config,
        const OCIO::ConstContextRcPtr &context,
        const OCIO::GroupTransformRcPtr &group,
        const std::string &display,
        const std::string &view,
        const std::string &look_name,
        const std::string &cdl_file_name) const;

    OCIO::ConstGpuShaderDescRcPtr make_shader(
        OCIO::ConstProcessorRcPtr &processor,
        const char *function_name,
        const char *resource_prefix) const;

    void setup_textures(
        OCIO::ConstGpuShaderDescRcPtr &shader_desc, ColourOperationDataPtr op_data) const;

    // OCIO dynamic properties

    void update_dynamic_parameters(
        OCIO::ConstGpuShaderDescRcPtr &shader, const MediaParams &media_param) const;

    void update_all_uniforms(
        OCIO::ConstGpuShaderDescRcPtr &shader,
        utility::JsonStore &uniforms,
        const utility::Uuid &source_uuid) const;

    // GUI handling

    void setup_ui();

    void populate_ui(const MediaParams &media_param);

    std::vector<std::string> parse_display_views(
        OCIO::ConstConfigRcPtr ocio_config,
        std::map<std::string, std::vector<std::string>> &view_map) const;

    std::vector<std::string> parse_all_colourspaces(OCIO::ConstConfigRcPtr ocio_config) const;

    void update_cs_from_view(const MediaParams &media_param, const std::string &view);

    void update_views(OCIO::ConstConfigRcPtr ocio_config);

    void update_bypass(module::StringChoiceAttribute *viewer, bool bypass);

  private:
    mutable std::map<std::string, OCIO::ConstConfigRcPtr> ocio_config_cache_;
    mutable std::map<utility::Uuid, MediaParams> media_params_;
    mutable std::map<std::string, PerConfigSettings> per_config_settings_;

    // GUI handling
    bool ui_initialized_ = false;
    UiText ui_text_;

    module::StringChoiceAttribute *channel_;
    module::StringChoiceAttribute *display_;
    module::StringChoiceAttribute *view_;
    module::FloatAttribute *exposure_;
    module::FloatAttribute *gamma_;
    module::FloatAttribute *saturation_;
    module::StringChoiceAttribute *source_colour_space_;
    module::BooleanAttribute *colour_bypass_;
    module::StringChoiceAttribute *preferred_view_;
    module::BooleanAttribute *global_view_;
    module::BooleanAttribute *adjust_source_;
    module::BooleanAttribute *enable_gamma_;
    module::BooleanAttribute *enable_saturation_;

    std::map<utility::Uuid, std::string> channel_hotkeys_;
    utility::Uuid exposure_hotkey_;
    utility::Uuid gamma_hotkey_;
    utility::Uuid saturation_hotkey_;
    utility::Uuid reset_hotkey_;

    // Holds info about the currently on screen media
    utility::Uuid current_source_uuid_;
    std::string current_config_name_;
    MediaParams current_source_media_params_;

    // Holds data on display screen option
    std::string monitor_name_;

    // Pixel probe
    std::string last_pixel_probe_source_hash_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_display_proc_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_lin_proc_;
};

} // namespace xstudio::colour_pipeline
