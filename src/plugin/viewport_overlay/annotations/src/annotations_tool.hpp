// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "annotation.hpp"
#include "annotation_opengl_renderer.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsTool : public plugin::StandardPlugin {
          public:
            inline static const utility::Uuid PLUGIN_UUID =
                utility::Uuid("46f386a0-cb9a-4820-8e99-fb53f6c019eb");

            AnnotationsTool(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~AnnotationsTool();

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

          protected:
            void register_hotkeys() override;

            void update_attrs_from_preferences(const utility::JsonStore &) override;

            caf::message_handler message_handler_extensions() override {
                return message_handler_;
            }

            utility::BlindDataObjectPtr prepare_render_data(
                const media_reader::ImageBufPtr &, const bool /*offscreen*/) const override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const int viewer_index) override;

            std::shared_ptr<bookmark::AnnotationBase>
            build_annotation(const utility::JsonStore &anno_data) override;

            void hotkey_pressed(const utility::Uuid &uuid, const std::string &context) override;
            void hotkey_released(
                const utility::Uuid &uuid, const std::string & /*context*/) override;
            bool pointer_event(const ui::PointerEvent &e) override;
            void text_entered(const std::string &text, const std::string &context) override;
            void key_pressed(
                const int key, const std::string &context, const bool auto_repeat) override;

          private:
            void load_fonts();
            bool check_pointer_hover_on_text(
                const Imath::V2f &pointer_pos, const float viewport_pixel_scale);
            void caption_drag(const Imath::V2f &p);
            void end_stroke(const Imath::V2f & = Imath::V2f());
            void interact_start();
            void interact_end();
            void start_or_edit_caption(const Imath::V2f &p, const float viewport_pixel_scale);
            void start_shape_placement(const Imath::V2f &p);
            void update_shape_placement(const Imath::V2f &pointer_pos);
            void start_freehand_pen_stroke(const Imath::V2f &point);
            void freehand_pen_stroke_point(const Imath::V2f &point);
            void undo();
            void redo();
            void clear_caption_overlays();
            void update_caption_overlay();
            void on_screen_frame_changed(
                const timebase::flicks,   // playhead position
                const int,                // playhead logical frame
                const int,                // media frame
                const int,                // media logical frame
                const utility::Timecode & // media frame timecode
                ) override;

            void on_screen_annotation_changed(
                std::vector<std::shared_ptr<bookmark::AnnotationBase>> // ptrs to annotation
                                                                       // data
                ) override;

            void on_screen_media_changed(
                caf::actor,                      // media item actor
                const utility::MediaReference &, // media reference
                const std::string) override;

            void on_playhead_playing_changed(const bool // is playing
                                             ) override;

            void create_new_annotation();
            void change_current_bookmark(const utility::Uuid &onscreen_bookmark);
            void push_edited_annotation_back_to_bookmark();
            void clear_onscreen_annotations();
            void restore_onscreen_annotations();
            void clear_edited_annotation();
            bool is_laser_mode() const {
                return active_tool_->value() == "Draw" && draw_mode_->value() == "Laser";
            }

            caf::message_handler message_handler_;

            enum Tool { Draw, Shapes, Text, Erase, None };
            enum ShapeTool { Square, Circle, Arrow, Line };
            enum DisplayMode { OnlyWhenPaused, Always, WithDrawTool };
            enum ScribbleTool { Sketch, Laser, Onion };

            const std::map<int, std::string> tool_names_ = {
                {Draw, "Draw"}, {Shapes, "Shapes"}, {Text, "Text"}, {Erase, "Erase"}};

            const std::map<int, std::string> draw_mode_names_ = {
                {Sketch, "Sketch"}, {Laser, "Laser"}, {Onion, "Onion"}};

            module::StringChoiceAttribute *active_tool_;

            module::IntegerAttribute *draw_pen_size_   = {nullptr};
            module::IntegerAttribute *shapes_pen_size_ = {nullptr};
            module::IntegerAttribute *erase_pen_size_  = {nullptr};
            module::IntegerAttribute *text_size_       = {nullptr};
            module::IntegerAttribute *pen_opacity_     = {nullptr};
            module::ColourAttribute *pen_colour_       = {nullptr};

            module::BooleanAttribute *text_cursor_blink_attr_ = {nullptr};
            module::BooleanAttribute *tool_is_active_         = {nullptr};
            module::StringAttribute *test_text_               = {nullptr};
            module::StringAttribute *action_attribute_        = {nullptr};
            module::IntegerAttribute *shape_tool_;
            module::StringChoiceAttribute *draw_mode_;
            module::IntegerAttribute *moving_scaling_text_attr_;
            module::StringChoiceAttribute *font_choice_;

            module::StringChoiceAttribute *display_mode_attribute_ = {nullptr};
            DisplayMode display_mode_                              = {OnlyWhenPaused};

            utility::Uuid toggle_active_hotkey_;
            utility::Uuid undo_hotkey_;
            utility::Uuid redo_hotkey_;

            AnnotationPtr current_edited_annotation_;
            std::map<utility::Uuid, AnnotationPtr> edited_annotation_cache_;

            std::map<utility::Uuid, AnnotationRenderDataPtr> annotations_render_data_;
            std::vector<utility::Uuid> current_viewed_annotations_;
            std::vector<std::map<utility::Uuid, utility::JsonStore>>
                cleared_annotations_serialised_data_;

            utility::Uuid current_bookmark_uuid_;
            utility::Uuid last_edited_annotation_uuid_;

            int playhead_logical_frame_ = {-1};
            int media_frame_            = {-1};
            int media_logical_frame_    = {-1};
            timebase::flicks playhead_position_;
            utility::MediaReference on_screen_media_ref_;
            std::string on_screen_media_name_;

            bool playhead_is_playing_ = {false};

            std::vector<AnnotationsRenderer *> renderers_;

            std::map<std::string, std::shared_ptr<SDFBitmapFont>> fonts_;

            std::shared_ptr<PenStroke> shape_stroke_;
            Imath::V2f shape_anchor_;

            Caption::HoverState hover_state_;
            Imath::V2f caption_drag_pointer_start_pos_;
            Imath::V2f caption_drag_caption_start_pos_;
            Imath::V2f caption_drag_width_height_;
            Imath::Box2f under_mouse_caption_bdb_;

            bool fade_looping_               = {false};
            bool interacting_with_renderers_ = {false};
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio