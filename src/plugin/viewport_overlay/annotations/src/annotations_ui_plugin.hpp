// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "pixel_patch.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        /*
        This plugin class provides UI elements and handles all user interaction
        events (keyboard/mouse) when creating and editing on-screen annotations.

        It does not modify or modify annotation data itself, however. Instead
        it sends user interaction event messages to the AnnotationsCore plugin.
        This approach allows us to use the AnnotationsCore plugin to implement
        remote user annotations editing as well as in-app edits (from this
        plugin)
        */
        class AnnotationsUI : public plugin::StandardPlugin {
          public:
            inline static const utility::Uuid PLUGIN_UUID =
                utility::Uuid("33377e04-13f0-4b86-b062-04e00abd8feb");

            AnnotationsUI(caf::actor_config &cfg, const utility::JsonStore &init_settings);
            virtual ~AnnotationsUI();

          protected:
            caf::message_handler message_handler_extensions() override;

            void
            attribute_changed(const utility::Uuid &attribute_uuid, const int /*role*/) override;

            void update_attrs_from_preferences(const utility::JsonStore &) override;

            void register_hotkeys() override;
            void hotkey_pressed(
                const utility::Uuid &uuid,
                const std::string &context,
                const std::string &window) override;
            void hotkey_released(
                const utility::Uuid &uuid,
                const std::string &context,
                const bool due_to_focus_change) override;

            bool pointer_event(const ui::PointerEvent &e) override;
            void text_entered(const std::string &text, const std::string &context) override;
            void key_pressed(
                const int key, const std::string &context, const bool auto_repeat) override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const std::string &viewport_name) override;

            void images_going_on_screen(
                const media_reader::ImageBufDisplaySetPtr &images,
                const std::string viewport_name,
                const bool playhead_playing) override;

            utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufPtr &,
                const std::string & /*viewport_name*/,
                const utility::Uuid & /*playhead_uuid*/,
                const bool is_hero_image,
                const bool images_are_in_grid_layout) const override;

            void viewport_dockable_widget_activated(std::string &widget_name) override;

            void viewport_dockable_widget_deactivated(std::string &widget_name) override;

            void turn_off_overlay_interaction() override;

          private:
            media_reader::ImageBufPtr image_under_pointer(
                const std::string &viewport_name,
                const Imath::V2f &pointer_position,
                Imath::V2i *pixel_position = nullptr);

            float pressure_source(const ui::PointerEvent &e);

            media_reader::ImageBufPtr image_under_mouse(
                const std::string &viewport_name,
                const Imath::V2f &pos,
                const bool fallback_to_hero_image) const;

            bool check_click_on_caption(const Imath::V2f &pos, const std::string &viewport_id);

            caf::actor get_colour_pipeline_actor(const std::string &viewport_name);
            void update_colour_picker_info(const ui::PointerEvent &e);

            void send_event(const std::string &event, const utility::JsonStore &payload);
            void start_item(const ui::PointerEvent &e);
            void modify_item(const ui::PointerEvent &e);
            void end_item();
            void clear_annotation(const std::string viewport_name);
            void undo(const std::string viewport_name);
            void redo(const std::string viewport_name);

          private:
            enum Tool {
                Draw,
                Brush,
                Laser,
                Square,
                Circle,
                Arrow,
                Line,
                Text,
                Erase,
                Dropper,
                None
            };

            [[nodiscard]] Tool current_tool() const { return current_tool_; }
            [[nodiscard]] bool is_curr_tool(Tool tool) const { return current_tool_ == tool; }

            const std::map<Tool, std::string> tool_names_ = {
                {Draw, "Draw"},
                {Brush, "Brush"},
                {Laser, "Laser"},
                {Square, "Square"},
                {Circle, "Circle"},
                {Arrow, "Arrow"},
                {Line, "Line"},
                {Text, "Text"},
                {Erase, "Erase"},
                {Dropper, "Colour Picker"},
                {None, "None"}};

            const std::string &tool_name(const Tool t) const {
                auto p = tool_names_.find(t);
                if (p != tool_names_.end())
                    return p->second;
                return tool_names_.rbegin()->second;
            }

            module::StringChoiceAttribute *active_tool_{nullptr};

            module::IntegerAttribute *pen_size_{nullptr};
            module::IntegerAttribute *pen_opacity_{nullptr};
            module::ColourAttribute *pen_colour_{nullptr};

            module::IntegerAttribute *brush_softness_{nullptr};
            module::IntegerAttribute *brush_size_{nullptr};
            module::IntegerAttribute *brush_size_sensitivity_{nullptr};
            module::IntegerAttribute *brush_opacity_{nullptr};
            module::IntegerAttribute *brush_opacity_sensitivity_{nullptr};
            // module::ColourAttribute  *brush_colour_{nullptr};

            module::IntegerAttribute *shapes_width_{nullptr};

            module::IntegerAttribute *erase_size_{nullptr};

            module::IntegerAttribute *text_size_{nullptr};
            module::IntegerAttribute *text_bgr_opacity_{nullptr};
            module::ColourAttribute *text_bgr_colour_{nullptr};

            module::StringChoiceAttribute *font_choice_{nullptr};

            module::IntegerAttribute *moving_scaling_text_attr_{nullptr};

            module::StringAttribute *action_attribute_{nullptr};

            module::StringChoiceAttribute *display_mode_attribute_{nullptr};

            module::BooleanAttribute *colour_picker_take_average_{nullptr};
            module::BooleanAttribute *colour_picker_take_show_magnifier_{nullptr};
            module::BooleanAttribute *colour_picker_hide_drawings_{nullptr};

            module::Attribute *dockable_widget_attr_{nullptr};

            utility::Uuid toggle_active_hotkey_;
            utility::Uuid undo_hotkey_;
            utility::Uuid redo_hotkey_;
            utility::Uuid clear_hotkey_;
            utility::Uuid colour_picker_hotkey_;
            utility::Uuid paint_stroke_id_;

            // Annotations
            utility::Uuid current_bookmark_uuid_;

            PixelPatch pixel_patch_;

            std::string current_interaction_viewport_name_;

            utility::BlindDataObjectPtr immediate_render_data_;

            // Current media info (for Bookmark creation)

            Imath::V2f caption_drag_pointer_start_pos_;
            Imath::V2f caption_drag_caption_start_pos_;
            Imath::V2f caption_drag_width_height_;
            Imath::V2f shape_anchor_;
            Imath::V4f cumulative_picked_colour_ = Imath::V4f(0.0f, 0.0f, 0.0f, 0.0f);

            Tool current_tool_ = None;
            Tool last_tool_    = Draw;

            bool fade_looping_{false};
            std::map<std::string, media_reader::ImageBufDisplaySetPtr> viewport_current_images_;
            media_reader::ImageBufPtr image_being_annotated_;
            std::map<std::string, caf::actor> colour_pipelines_;

            caf::actor core_plugin_;
            utility::Uuid current_item_id_;
            utility::Uuid user_id_ = {utility::Uuid::generate()};
            std::map<std::string, Imath::M44f> viewport_transforms_;
            std::optional<canvas::Caption> edited_caption_;
            std::size_t focus_caption_id_ = {0};
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio