// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <OpenColorIO/OpenColorIO.h> //NOLINT

#include "xstudio/colour_pipeline/colour_operation.hpp"
#include "grading_data.h"
#include "grading_mask_gl_renderer.h"

namespace OCIO = OCIO_NAMESPACE;


namespace xstudio::colour_pipeline {

class GradingTool : public plugin::StandardPlugin {
    public:
        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("5598e01e-c6bc-4cf9-80ff-74bb560df12a");

  public:
    GradingTool(caf::actor_config &cfg, const utility::JsonStore &init_settings);
    ~GradingTool() override = default;

    utility::BlindDataObjectPtr prepare_overlay_data(
        const media_reader::ImageBufPtr &, const bool offscreen) const override;

    // Annotations (grading)

    bookmark::AnnotationBasePtr build_annotation(const utility::JsonStore &data) override;

    void images_going_on_screen(
        const std::vector<media_reader::ImageBufPtr> & images,
        const std::string viewport_name,
        const bool playhead_playing
    ) override;

    // Interactions

    void attribute_changed(
        const utility::Uuid &attribute_uuid, const int role) override;

    void register_hotkeys() override;
    void hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context) override;

    bool pointer_event(const ui::PointerEvent &e) override;

    // Playhead events

  protected:

      caf::message_handler message_handler_extensions() override;

  private:
    void start_stroke(const Imath::V2f &point);
    void update_stroke(const Imath::V2f &point);
    void end_drawing();

    void undo();
    void redo();

    void clear_mask();
    void clear_cdl();
    void save_cdl(const std::string &filepath) const;

    void load_grade_layers(ui::viewport::GradingData* grading_data);
    void reset_grade_layers();
    void add_grade_layer();
    void toggle_grade_layer(size_t layer);
    void delete_grade_layer();

    ui::viewport::LayerData* current_layer();
    void refresh_current_layer_from_ui();
    void refresh_ui_from_current_layer();

    utility::Uuid current_bookmark() const;
    void create_bookmark();
    void save_bookmark();


  private:
    // General
    module::BooleanAttribute *tool_is_active_ {nullptr};
    module::BooleanAttribute *mask_is_active_ {nullptr};
    module::StringAttribute  *grading_action_ {nullptr};
    module::StringAttribute  *drawing_action_ {nullptr};

    // Grading
    enum class GradingPanel { Basic, CDLSliders, CDLWheels };
    const std::map<GradingPanel, std::string> grading_panel_names_ = {
        {GradingPanel::Basic, "Basic"},
        {GradingPanel::CDLSliders, "Sliders"},
        {GradingPanel::CDLWheels, "Wheels"}
    };

    module::StringChoiceAttribute *grading_panel_  {nullptr};
    module::StringChoiceAttribute *grading_layer_  {nullptr};
    module::BooleanAttribute      *grading_bypass_ {nullptr};
    module::StringChoiceAttribute *grading_buffer_ {nullptr};

    module::FloatAttribute *slope_red_       {nullptr};
    module::FloatAttribute *slope_green_     {nullptr};
    module::FloatAttribute *slope_blue_      {nullptr};
    module::FloatAttribute *slope_master_    {nullptr};
    module::FloatAttribute *offset_red_      {nullptr};
    module::FloatAttribute *offset_green_    {nullptr};
    module::FloatAttribute *offset_blue_     {nullptr};
    module::FloatAttribute *offset_master_   {nullptr};
    module::FloatAttribute *power_red_       {nullptr};
    module::FloatAttribute *power_green_     {nullptr};
    module::FloatAttribute *power_blue_      {nullptr};
    module::FloatAttribute *power_master_    {nullptr};

    module::FloatAttribute *basic_exposure_  {nullptr};
    module::FloatAttribute *basic_offset_    {nullptr};
    module::FloatAttribute *basic_power_     {nullptr};

    module::FloatAttribute *sat_             {nullptr};

    // Drawing Mask
    enum class DrawingTool { Draw, Erase, None };
    const std::map<DrawingTool, std::string> drawing_tool_names_ = {
        {DrawingTool::Draw, "Draw"},
        {DrawingTool::Erase, "Erase"}
    };

    module::StringChoiceAttribute *drawing_tool_ {nullptr};
    module::IntegerAttribute *draw_pen_size_     {nullptr};
    module::IntegerAttribute *erase_pen_size_    {nullptr};
    module::IntegerAttribute *pen_opacity_       {nullptr};
    module::IntegerAttribute *pen_softness_      {nullptr};
    module::ColourAttribute  *pen_colour_        {nullptr};

    enum DisplayMode { Mask, Grade };
    const std::map<DisplayMode, std::string> display_mode_names_ = {
        {DisplayMode::Grade, "Grade"},
        {DisplayMode::Mask, "Mask"}
    };
    module::StringChoiceAttribute *display_mode_attribute_  {nullptr};

    // MVP delivery phase management
    module::BooleanAttribute *mvp_1_release_ {nullptr};

    // Shortcuts
    utility::Uuid toggle_active_hotkey_;
    utility::Uuid toggle_mask_hotkey_;
    utility::Uuid undo_hotkey_;
    utility::Uuid redo_hotkey_;

    // Current media info (eg. for Bookmark creation)
    bool playhead_is_playing_ {false};

    // Grading

    ui::viewport::GradingData grading_data_;
    media_reader::ImageBufPtr current_on_screen_frame_;
    media_reader::ImageBufPtr grading_data_creation_frame_;

    inline static const size_t maximum_layers_ {8};
    size_t active_layer_ {0};

    std::vector<caf::actor> grading_colour_op_actors_;

};

} // xstudio::colour_pipeline
