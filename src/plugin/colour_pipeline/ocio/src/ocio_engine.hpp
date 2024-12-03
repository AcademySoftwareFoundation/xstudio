// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <OpenColorIO/OpenColorIO.h> //NOLINT
#include <caf/all.hpp>
#include "xstudio/colour_pipeline/colour_pipeline.hpp"

namespace OCIO = OCIO_NAMESPACE;

namespace xstudio::colour_pipeline::ocio {

/* Class providing an interface to the OCIO API providing functions required
by xstudio for colour management. */
class OCIOEngine {

  public:
    OCIOEngine() = default;

  public:
    /* Extend the PixelInfo object with information about colour space transform
    and resulting RGB transformed values for PixelInfo HUD plugin */
    void extend_pixel_info(
        media_reader::PixelInfo &pixel_info,
        const media::AVFrameID &frame_id,
        const std::string &display,
        const std::string &view,
        const float exposure,
        const float gamma,
        const float saturation);

    /* Get the default OCIO display*/
    const char *default_display(const utility::JsonStore &src_colour_mgmt_metadata) const;

    /* Get the name (filesystem path) of the OCIO config that applied to the
    given source colour management metadata*/
    std::string get_ocio_config_name(const utility::JsonStore &src_colour_mgmt_metadata) const {
        return get_ocio_config(src_colour_mgmt_metadata)->getName();
    }

    /* Pick the preferred OCIO view for the given source colour management
    metadata and a client-defined override preferred view*/
    std::string preferred_view(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string user_preferred_view) const;

    /* This function is executed just before rendering an image to screen.
    In our case user_data carries the OCIO GPU shader desc and GradingPrimary
    objects. We use the GradpingPrimary, exposure and gamma to update the
    shader descriptor. We then use get the shader descriptor to tell us the
    shader uniforms (names and values) which we then store in the 'uniforms'
    object, later attached to the image. The xSTUDIO viewport opengl renderer
    then uses the uniforms dict to set the shader uniforms at draw time.
    */
    void update_shader_uniforms(
        std::any &user_data,
        utility::JsonStore &uniforms,
        const float exposure,
        const float gamma);

    /* For given media source colour metadata we fetch the appropriate OCIO
    config and query it for possible source colour spaces, the list of available
    displays and the views per display.*/
    void get_ocio_displays_view_colourspaces(
        const utility::JsonStore &src_colour_mgmt_metadata,
        std::vector<std::string> &all_colourspaces,
        std::vector<std::string> &displays,
        std::map<std::string, std::vector<std::string>> &display_views) const;

    /* Make a unique size_t from the source colour metadata that will change
    when any aspect of the colour management of the source is expected to cahnge
    the way the source will be transformed to display space.*/
    size_t compute_hash(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &extra = std::string()) const;

    /* For given media source colour metadata determine the expected source
    colourspace.*/
    std::string detect_source_colourspace(const utility::JsonStore &src_colour_mgmt_metadata);

    /* Select the most appropriate source colour space for the provided OCIO view */
    std::string input_space_for_view(
        const utility::JsonStore &src_colour_mgmt_metadata, const std::string &view) const;

    /* For the given information about the frame returns the ColourOperationData
    object with GPU shader and LUT data required for tranforming from
    source colourspace to linear colourspace. */
    ColourOperationDataPtr linearise_op_data(
        const utility::JsonStore &src_colour_mgmt_metadata, const bool colour_bypass_);

    /* For the given information about the frame plus OCIO display and view
    return the ColourOperationData object with GPU shader and LUT data required
    for tranforming from linear colourspace to display colourspace*/
    ColourOperationDataPtr linear_to_display_op_data(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &display,
        const std::string &view,
        const bool bypass);

    /*Process an RGB float format thumbnail image from the source colourspace
    of the source media into display space*/
    thumbnail::ThumbnailBufferPtr process_thumbnail(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const thumbnail::ThumbnailBufferPtr &buf,
        const std::string &display,
        const std::string &view);

  private:
    // OCIO logic
    const char *working_space(const utility::JsonStore &src_colour_mgmt_metadata) const;

    // OCIO Transform helpers
    OCIO::TransformRcPtr
    source_transform(const utility::JsonStore &src_colour_mgmt_metadata) const;

    OCIO::ConstConfigRcPtr display_transform(
        const utility::JsonStore &src_colour_mgmt_metadata,
        OCIO::ContextRcPtr context,
        OCIO::GroupTransformRcPtr group,
        OCIO::GradingPrimary &primary,
        std::string display,
        std::string view) const;

    OCIO::TransformRcPtr display_transform(
        const std::string &source,
        const std::string &display,
        const std::string &view,
        OCIO::TransformDirection direction) const;

    OCIO::TransformRcPtr identity_transform() const;

    // OCIO setup

    OCIO::ConstConfigRcPtr
    get_ocio_config(const utility::JsonStore &src_colour_mgmt_metadata) const;


    OCIO::ContextRcPtr
    setup_ocio_context(const utility::JsonStore &src_colour_mgmt_metadata) const;

    OCIO::ConstProcessorRcPtr make_to_lin_processor(
        const utility::JsonStore &src_colour_mgmt_metadata, const bool bypass) const;

    OCIO::ConstProcessorRcPtr make_display_processor(
        const utility::JsonStore &src_colour_mgmt_metadata,
        bool is_thumbnail,
        OCIO::GradingPrimary &primary,
        const std::string &view,
        const std::string &display,
        const bool bypass = false) const;

    OCIO::ConstConfigRcPtr make_dynamic_display_processor(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const OCIO::ConstConfigRcPtr &config,
        const OCIO::ConstContextRcPtr &context,
        const OCIO::GroupTransformRcPtr &group,
        const std::string &display,
        const std::string &view,
        const std::string &look_name,
        const std::string &cdl_file_name,
        OCIO::GradingPrimary &primary) const;

    OCIO::ConstGpuShaderDescRcPtr make_shader(
        OCIO::ConstProcessorRcPtr &processor,
        const char *function_name,
        const char *resource_prefix) const;

    void setup_textures(
        OCIO::ConstGpuShaderDescRcPtr &shader_desc, ColourOperationDataPtr op_data) const;

    // OCIO dynamic properties
    void update_dynamic_parameters(
        OCIO::ConstGpuShaderDescRcPtr &shader,
        const OCIO::GradingPrimary &primary,
        const float exposure,
        const float gamma) const;

    void update_all_uniforms(
        OCIO::ConstGpuShaderDescRcPtr &shader, utility::JsonStore &uniforms) const;

    std::vector<std::string> parse_all_colourspaces(OCIO::ConstConfigRcPtr ocio_config) const;

  private:
    mutable std::map<std::string, OCIO::ConstConfigRcPtr> ocio_config_cache_;

    // Pixel probe
    size_t last_pixel_probe_source_hash_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_display_proc_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_lin_proc_;
};

/* Actor wrapper for OCIOEngine, allowing us to execute 'heavy' OCIO based
IO and computation via CAF messaging so that OCIOColourPipeline instances
can offload tasks to a worker pool. */
class OCIOEngineActor : public caf::event_based_actor {

  public:
    OCIOEngineActor(caf::actor_config &cfg);

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "OCIOEngineActor";

    caf::behavior behavior_;

    OCIOEngine m_engine_;
};

} // namespace xstudio::colour_pipeline::ocio
