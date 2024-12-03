// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>

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

    utility::BlindDataObjectPtr onscreen_render_data(
        const media_reader::ImageBufPtr &, const std::string & /*viewport_name*/) const override;

    // Annotations (grading)

    bookmark::AnnotationBasePtr build_annotation(const utility::JsonStore &data) override;

    void images_going_on_screen(
        const media_reader::ImageBufDisplaySetPtr &images,
        const std::string viewport_name,
        const bool playhead_playing
    ) override;

    void on_screen_media_changed(
        caf::actor,
        const utility::MediaReference &,
        const utility::JsonStore &
    ) override;

    // Interactions

    void attribute_changed(
        const utility::Uuid &attribute_uuid, const int role) override;

    void register_hotkeys() override;
    void hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) override;

    bool pointer_event(const ui::PointerEvent &e) override;

    void turn_off_overlay_interaction() override;

  protected:
      caf::message_handler message_handler_extensions() override;

  private:
    bool grading_tools_active() const;

    void start_stroke(const Imath::V2f &point);
    void update_stroke(const Imath::V2f &point);

    void start_quad(const std::vector<Imath::V2f> &corners);
    void start_polygon(const std::vector<Imath::V2f> &points);
    void start_ellipse(const Imath::V2f &center, const Imath::V2f &radius, float angle);

    void remove_shape(int id);

    void end_drawing();

    void undo();
    void redo();

    void clear_mask();
    void clear_shapes();
    void clear_grade();
    void save_cdl(const std::string &filepath) const;

    utility::Uuid current_bookmark() const;
    utility::UuidList current_clip_bookmarks();
    void create_bookmark_if_empty();
    void create_bookmark();
    void select_bookmark(const utility::Uuid &uuid);
    void update_boomark_shape(const utility::Uuid &uuid);
    void save_bookmark();
    void remove_bookmark();

    void refresh_current_grade_from_ui();
    void refresh_ui_from_current_grade();

  private:
    // General
    module::IntegerAttribute *tool_opened_count_    {nullptr};
    module::StringAttribute  *grading_action_       {nullptr};
    module::BooleanAttribute *grading_bypass_       {nullptr};
    module::StringAttribute  *drawing_action_       {nullptr};
    module::BooleanAttribute *media_colour_managed_ {nullptr};

    // Grading
    module::StringAttribute       *grading_bookmark_ {nullptr};
    module::StringAttribute       *working_space_    {nullptr};
    module::StringChoiceAttribute *colour_space_     {nullptr};
    module::IntegerAttribute      *grade_in_         {nullptr};
    module::IntegerAttribute      *grade_out_        {nullptr};

    module::FloatVectorAttribute  *slope_      {nullptr};
    module::FloatVectorAttribute  *offset_     {nullptr};
    module::FloatVectorAttribute  *power_      {nullptr};
    module::FloatAttribute        *saturation_ {nullptr};
    module::FloatAttribute        *exposure_   {nullptr};
    module::FloatAttribute        *contrast_   {nullptr};

    // Drawing Mask
    enum class DrawingTool { Draw, Erase, Shape, None };
    const std::map<DrawingTool, std::string> drawing_tool_names_ = {
        {DrawingTool::Shape, "Shape"},
        {DrawingTool::None, "None"},
        {DrawingTool::Draw, "Draw"},
        {DrawingTool::Erase, "Erase"}
    };

    module::StringChoiceAttribute *drawing_tool_      {nullptr};
    module::IntegerAttribute      *draw_pen_size_     {nullptr};
    module::IntegerAttribute      *erase_pen_size_    {nullptr};
    module::IntegerAttribute      *pen_opacity_       {nullptr};
    module::IntegerAttribute      *pen_softness_      {nullptr};
    module::ColourAttribute       *pen_colour_        {nullptr};
    module::BooleanAttribute      *shape_invert_      {nullptr};
    module::BooleanAttribute      *polygon_init_      {nullptr};

    module::IntegerAttribute *mask_selected_shape_{nullptr};
    module::BooleanAttribute *mask_shapes_visible_{nullptr};
    std::vector<module::Attribute *> mask_shapes_;

    enum DisplayMode { Mask, Grade };
    const std::map<DisplayMode, std::string> display_mode_names_ = {
        {DisplayMode::Grade, "Grade"},
        {DisplayMode::Mask, "Mask"}
    };
    module::StringChoiceAttribute *display_mode_attribute_  {nullptr};

    // Shortcuts
    utility::Uuid bypass_hotkey_;
    utility::Uuid undo_hotkey_;
    utility::Uuid redo_hotkey_;

    // Current media info (eg. for Bookmark creation)
    bool playhead_is_playing_ {false};
    int playhead_media_frame_ = {0};

    media_reader::ImageBufDisplaySetPtr viewport_current_images_;

    // Grading
    ui::viewport::GradingData grading_data_;

    std::vector<caf::actor> grading_colour_op_actors_;
};

} // xstudio::colour_pipeline
