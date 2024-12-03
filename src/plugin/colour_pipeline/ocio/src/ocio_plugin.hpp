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
#include "ocio_shared_settings.hpp"
#include "ocio_engine.hpp"

namespace OCIO = OCIO_NAMESPACE;

namespace xstudio::colour_pipeline::ocio {

/*  OCIOColourPipeline provides colour management for medias.

    It is instanciated per xStudio viewport and maintains it's own list of
    settings (source space, display, view, gamma, exposure, saturation, etc).
    Settings that should be shared accross multiple viewports are sync'ed
    using a global OCIOGlobalControls actor. The latter also provides
    application wide settings related to colour management.

    Long running tasks are delegated to a worker pool of OCIOEngineActor.
    This includes linearization and display shaders generation, thumbnail
    colour rendering.
*/

class OCIOColourPipeline : public ColourPipeline {

  public:
    explicit OCIOColourPipeline(
        caf::actor_config &cfg, const utility::JsonStore &init_settings = utility::JsonStore());

    virtual ~OCIOColourPipeline() override;

    size_t fast_display_transform_hash(const media::AVFrameID &media_ptr) override;

    /* Create the ColourOperationDataPtr containing the necessary LUT and
    shader data for linearising the source colourspace RGB data from the
    given media source on the screen */
    void linearise_op_data(
        caf::typed_response_promise<ColourOperationDataPtr> &rp,
        const media::AVFrameID &media_ptr) override;

    /* Create the ColourOperationDataPtr containing the necessary LUT and
    shader data for transforming linear colour values into display space */
    void linear_to_display_op_data(
        caf::typed_response_promise<ColourOperationDataPtr> &rp,
        const media::AVFrameID &media_ptr) override;

    // Update colour pipeline shader dynamic parameters.
    utility::JsonStore update_shader_uniforms(
        const media_reader::ImageBufPtr &image, std::any &user_data) override;

    void process_thumbnail(
        caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> &rp,
        const media::AVFrameID &media_ptr,
        const thumbnail::ThumbnailBufferPtr &buf) override;

    // GUI handling

    void register_hotkeys() override;

    void hotkey_pressed(
        const utility::Uuid &hotkey_uuid,
        const std::string &context,
        const std::string &window) override;

    void hotkey_released(const utility::Uuid &hotkey_uuid, const std::string &context) override;

    bool pointer_event(const ui::PointerEvent &e) override;

    void media_source_changed(
        const utility::Uuid &source_uuid,
        const utility::JsonStore &src_colour_mgmt_metadata) override;

    void attribute_changed(const utility::Uuid &attribute_uuid, const int /*role*/) override;

    void screen_changed(
        const std::string &name,
        const std::string &model,
        const std::string &manufacturer,
        const std::string &serialNumber) override;

    void connect_to_viewport(
        const std::string &viewport_name,
        const std::string &viewport_toolbar_name,
        bool connect,
        caf::actor viewport) override;

    void extend_pixel_info(
        media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) override;

    caf::message_handler message_handler_extensions() override;

  private:
    void setup_ui();

    void populate_ui(const utility::JsonStore &src_colour_mgmt_metadata);

    void update_views(const std::string &new_display);

    void update_bypass(bool bypass);

    void reset() override;

    void update_media_metadata(
        const utility::Uuid &media_uuid, const std::string &key, const std::string &val);

    utility::JsonStore patch_media_metadata(const media::AVFrameID &media_ptr);

    void synchronize_attribute(const utility::Uuid &uuid, int role, bool ocio);

    std::string detect_display(
        const std::string &name,
        const std::string &model,
        const std::string &manufacturer,
        const std::string &serialNumber,
        const utility::JsonStore &meta);

  private:
    caf::actor global_controls_;
    caf::actor worker_pool_;

    // GUI handling
    UiText ui_text_;

    std::map<utility::Uuid, std::string> channel_hotkeys_;
    utility::Uuid exposure_hotkey_;
    utility::Uuid gamma_hotkey_;
    utility::Uuid saturation_hotkey_;
    utility::Uuid reset_hotkey_;

    // Viewport colour settings
    module::StringChoiceAttribute *source_colour_space_;
    module::StringChoiceAttribute *display_;
    module::StringChoiceAttribute *view_;
    module::StringChoiceAttribute *channel_;
    module::FloatAttribute *exposure_;
    module::FloatAttribute *gamma_;
    module::FloatAttribute *saturation_;

    // Global colour settings
    OCIOGlobalData global_settings_;

    // Holds info about the currently on screen media
    utility::Uuid current_source_uuid_;
    std::string current_config_name_;
    std::string last_update_hash_;
    utility::JsonStore current_source_colour_mgmt_metadata_;

    // Holds data on display screen option
    std::string monitor_name_;
    std::string monitor_model_;
    std::string monitor_manufacturer_;
    std::string monitor_serialNumber_;
    std::string viewport_name_;

    // The ID of the window that this instance of the colour pipeline is used
    // to display images into
    std::string window_id_;

    std::map<std::string, std::vector<std::string>> display_views_;

    // Processing
    OCIOEngine m_engine_;
};

} // namespace xstudio::colour_pipeline::ocio
