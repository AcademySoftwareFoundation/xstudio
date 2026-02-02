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

    /* Extend the PixelInfo object with information about colour space transform
    and resulting RGB transformed values for PixelInfo HUD plugin */
    void extend_pixel_info(
        media_reader::PixelInfo &pixel_info,
        const media::AVFrameID &frame_id,
        const std::string &display,
        const std::string &view,
        const bool untonemapped_mode,
        const bool apply_saturation_after_lut,
        const float exposure,
        const float gamma,
        const float saturation);

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

    /* Make a unique size_t from the source colour metadata that will change
    when any aspect of the colour management of the source is expected to cahnge
    the way the source will be transformed to display space.*/
    size_t compute_hash(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &extra = std::string()) const;

    /* For given media source colour metadata determine the expected source
    colourspace.*/
    std::string detect_source_colourspace(
        const utility::JsonStore &src_colour_mgmt_metadata, const bool untonemapped_mode);

    /* For the given information about the frame returns the ColourOperationData
    object with GPU shader and LUT data required for tranforming from
    source colourspace to linear colourspace. */
    ColourOperationDataPtr linearise_op_data(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const bool untonemapped_mode,
        const bool colour_bypass_);

    /* For the given information about the frame plus OCIO display and view
    return the ColourOperationData object with GPU shader and LUT data required
    for tranforming from linear colourspace to display colourspace*/
    ColourOperationDataPtr linear_to_display_op_data(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &display,
        const std::string &view,
        const bool bypass);

    /* Process an RGB float format thumbnail image from the source colourspace
    of the source media into display space*/
    thumbnail::ThumbnailBufferPtr process_thumbnail(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const thumbnail::ThumbnailBufferPtr &buf,
        const std::string &display,
        const std::string &view,
        const bool untonemapped_mode);

    /* When no ocio_config metadata is provided from a MediaHook plugin,
    this OCIO config will be used.*/
    void set_default_config(const std::string &default_config) {
        default_config_ = default_config;
    }

    /* When multiple config versions are available (from a MediaHook plugin),
    this can be used to set the preferred one. This can be useful in case
    newer configs use more advanced shaders and the workstation GPU can
    not keep up (eg. combined with 4K SDI output).*/
    void set_preferred_config_version(const std::string &version) {
        preferred_config_version_ = version;
    }

    /* Get the name of the OCIO config that applied to the given source colour management
     metadata, this can be used to track config changes accros medias.*/
    const char *ocio_config_name(const utility::JsonStore &src_colour_mgmt_metadata) const;

    /* Get the default OCIO display.*/
    const char *default_display(const utility::JsonStore &src_colour_mgmt_metadata) const;

    /* Get the default OCIO view.*/
    const char *default_view(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &display = std::string()) const;

    /* Return true if display is available.*/
    bool has_display(
        const utility::JsonStore &src_colour_mgmt_metadata, const std::string &display) const;

    /* Return true if display (or default display) has the given view.*/
    bool has_view(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &view,
        const std::string &display = std::string()) const;

    /* Pick the most appropriate OCIO view for the given source colour management
    metadata using rules from the media hook or OCIO v2 Viewing Rules.*/
    std::string automatic_view(const utility::JsonStore &src_colour_mgmt_metadata) const;

    /* For given media source colour metadata we fetch the appropriate OCIO
    config and query it for possible source colour spaces, the list of available
    displays and the views per display.*/
    void get_ocio_displays_view_colourspaces(
        const utility::JsonStore &src_colour_mgmt_metadata,
        std::vector<std::string> &all_colourspaces,
        std::vector<std::string> &displays,
        std::map<std::string, std::vector<std::string>> &display_views) const;

  private:
    // OCIO logic
    std::string working_space(const utility::JsonStore &src_colour_mgmt_metadata) const;

    // OCIO Transform helpers
    OCIO::TransformRcPtr source_transform(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const bool untonemapped_mode,
        const bool bypass) const;

    OCIO::TransformRcPtr display_transform(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &display,
        const std::string &view,
        const bool bypass) const;

    OCIO::TransformRcPtr identity_transform() const;

    // OCIO setup

    OCIO::ConstConfigRcPtr
    get_ocio_config(const utility::JsonStore &src_colour_mgmt_metadata) const;

    void get_ocio_config_version_override(
        const utility::JsonStore &src_colour_mgmt_metadata, std::string &config_name) const;

    OCIO::ContextRcPtr
    setup_ocio_context(const utility::JsonStore &src_colour_mgmt_metadata) const;

    OCIO::ConstProcessorRcPtr make_to_lin_processor(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &view,
        const bool untonemapped_mode,
        const bool bypass) const;

    OCIO::ConstProcessorRcPtr make_display_processor(
        const utility::JsonStore &src_colour_mgmt_metadata,
        const std::string &view,
        const std::string &display,
        const bool bypass = false) const;

  private:
    mutable std::map<std::string, OCIO::ConstConfigRcPtr> ocio_config_cache_;

    // Pixel probe
    size_t last_pixel_probe_source_hash_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_display_proc_;
    OCIO::ConstCPUProcessorRcPtr pixel_probe_to_lin_proc_;
    std::string default_config_;
    std::string preferred_config_version_;
};

/* Actor wrapper for OCIOEngine, allowing us to execute 'heavy' OCIO based
IO and computation via CAF messaging so that OCIOColourPipeline instances
can offload tasks to a worker pool. */
class OCIOEngineActor : public caf::event_based_actor, OCIOEngine {

  public:
    OCIOEngineActor(caf::actor_config &cfg);

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "OCIOEngineActor";

    caf::behavior behavior_;
};

} // namespace xstudio::colour_pipeline::ocio
