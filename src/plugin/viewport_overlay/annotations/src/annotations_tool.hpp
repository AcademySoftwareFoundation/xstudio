// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
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
            virtual ~AnnotationsTool();

          protected:
            caf::message_handler message_handler_extensions() override;

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

            void update_attrs_from_preferences(const utility::JsonStore &) override;

            void register_hotkeys() override;
            void hotkey_pressed(
                const utility::Uuid &uuid,
                const std::string &context,
                const std::string &window) override;
            void
            hotkey_released(const utility::Uuid &uuid, const std::string &context) override;
            bool pointer_event(const ui::PointerEvent &e) override;
            void text_entered(const std::string &text, const std::string &context) override;
            void key_pressed(
                const int key, const std::string &context, const bool auto_repeat) override;

            utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufPtr &,
                const std::string &viewport_name) const override;

            plugin::ViewportOverlayRendererPtr make_overlay_renderer() override;

            bookmark::AnnotationBasePtr
            build_annotation(const utility::JsonStore &anno_data) override;

            void images_going_on_screen(
                const media_reader::ImageBufDisplaySetPtr &images,
                const std::string viewport_name,
                const bool playhead_playing) override;

            void viewport_dockable_widget_activated(std::string &widget_name) override;

            void viewport_dockable_widget_deactivated(std::string &widget_name) override;

            void turn_off_overlay_interaction() override;

          private:
            bool is_laser_mode() const;

            void start_editing(const std::string &viewport_name, const Imath::V2f &pointer_position = Imath::V2f(-1e6f, -1e6f));

            void start_stroke(const Imath::V2f &point);
            void update_stroke(const Imath::V2f &point);

            void start_shape(const Imath::V2f &p);
            void update_shape(const Imath::V2f &pointer_pos);

            void start_or_edit_caption(const Imath::V2f &p, float viewport_pixel_scale);
            void update_caption_action(const Imath::V2f &p);
            bool
            update_caption_hovered(const Imath::V2f &pointer_pos, float viewport_pixel_scale);
            void update_caption_handle();
            void clear_caption_handle();

            void end_drawing();

            void undo();
            void redo();

            void create_new_annotation();
            void clear_onscreen_annotations();
            void restore_onscreen_annotations();
            void clear_edited_annotation();
            void update_bookmark_annotation_data();
            Imath::V2f image_transformed_ptr_pos(const Imath::V2f &p) const;

          private:
            enum Tool { Draw, Laser, Square, Circle, Arrow, Line, Text, Erase, None };
            enum DisplayMode { OnlyWhenPaused, Always, WithDrawTool };

            const std::map<int, std::string> tool_names_ = {
                {Draw, "Draw"},
                {Laser, "Laser"},
                {Square, "Square"},
                {Circle, "Circle"},
                {Arrow, "Arrow"},
                {Line, "Line"},
                {Text, "Text"},
                {Erase, "Erase"}};

            module::StringChoiceAttribute *active_tool_{nullptr};

            module::IntegerAttribute *draw_pen_size_{nullptr};
            module::IntegerAttribute *shapes_pen_size_{nullptr};
            module::IntegerAttribute *erase_pen_size_{nullptr};
            module::IntegerAttribute *text_size_{nullptr};
            module::IntegerAttribute *pen_opacity_{nullptr};
            module::ColourAttribute *pen_colour_{nullptr};
            module::ColourAttribute *text_bgr_colour_{nullptr};
            module::IntegerAttribute *text_bgr_opacity_{nullptr};

            module::BooleanAttribute *text_cursor_blink_attr_{nullptr};
            module::StringAttribute *action_attribute_{nullptr};
            module::IntegerAttribute *moving_scaling_text_attr_{nullptr};
            module::StringChoiceAttribute *font_choice_{nullptr};

            module::StringChoiceAttribute *display_mode_attribute_{nullptr};

            module::Attribute *dockable_widget_attr_{nullptr};

            DisplayMode display_mode_{OnlyWhenPaused};
            bool playhead_is_playing_{false};

            utility::Uuid toggle_active_hotkey_;
            utility::Uuid undo_hotkey_;
            utility::Uuid redo_hotkey_;
            utility::Uuid clear_hotkey_;

            // Annotations
            utility::Uuid current_bookmark_uuid_;

            // N.B. this badboy is thread-safe. This means we can happily modify
            // and access its data both in our class methods and also in the
            // AnnotationsRenderer which has a direct access to it for on-screen
            // rendering of brush-strokes during user interaction.
            xstudio::ui::canvas::Canvas interaction_canvas_;

            std::string current_interaction_viewport_name_;

            utility::BlindDataObjectPtr immediate_render_data_;

            // Current media info (for Bookmark creation)

            xstudio::ui::canvas::HandleState handle_state_;

            Imath::V2f caption_drag_pointer_start_pos_;
            Imath::V2f caption_drag_caption_start_pos_;
            Imath::V2f caption_drag_width_height_;
            Imath::V2f shape_anchor_;

            std::string last_tool_ = {"Draw"};

            bool fade_looping_{false};
            std::map<std::string, media_reader::ImageBufDisplaySetPtr>
                viewport_current_images_;
            media_reader::ImageBufPtr image_being_annotated_;

        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio