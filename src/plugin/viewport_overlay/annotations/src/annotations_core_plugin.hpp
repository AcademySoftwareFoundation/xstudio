// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "annotation.hpp"
#include "annotation_opengl_renderer.hpp"
#include "annotation_render_data.hpp"
#include "annotation_undo_redo.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationsCore : public plugin::StandardPlugin {
          public:
            inline static const utility::Uuid PLUGIN_UUID =
                utility::Uuid("46f386a0-cb9a-4820-8e99-fb53f6c019eb");

            AnnotationsCore(caf::actor_config &cfg, const utility::JsonStore &init_settings);
            virtual ~AnnotationsCore();

          protected:
            caf::message_handler message_handler_extensions() override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const std::string &viewport_name) override;

            bookmark::AnnotationBasePtr
            build_annotation(const utility::JsonStore &anno_data) override;

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

            virtual utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufDisplaySetPtr & /*image_set*/,
                const std::string & /*viewport_name*/,
                const utility::Uuid & /*playhead_uuid*/) const override;

            friend class CreateBookmark;
            friend class ClearAnnotation;

            void make_bookmark_for_annotations(
                const media::AVFrameID &id, const utility::Uuid &mb_id);

          private:
            void receive_annotation_data(const utility::JsonStore &payload);

            // We hold one of these structs for each user that is interacting
            // with an annotation. It holds info about the stroke or caption they
            // are creating/modifying and some mouse interaction data plus
            // some overlay graphics needed.
            struct LiveEdit {
                media_reader::ImageBufPtr annotated_image;
                std::vector<std::shared_ptr<ui::canvas::Stroke>> laser_strokes;
                std::shared_ptr<ui::canvas::Stroke> live_stroke;
                std::shared_ptr<ui::canvas::Caption> live_caption;
                ui::canvas::Canvas::ItemType item_type = {ui::canvas::Canvas::ItemType::None};
                std::string viewport_name;
                Imath::V2f start_point;
                Imath::V2f drag_start;
                utility::Uuid edited_bookmark_id;
                HandleHoverState caption_handle_over_state_ = {HandleHoverState::NotHovered};
                std::size_t skip_render_caption_id          = {0};
                utility::Uuid user_id;
            };
            typedef std::shared_ptr<LiveEdit> LiveEditData;


            Imath::Box2f under_mouse_caption_bdb_;

            // per user live edit data
            std::map<utility::Uuid, LiveEditData> live_edit_data_;

            media_reader::ImageBufPtr image_under_pointer(
                const Imath::V2f &raw_pointer_position,
                const LiveEditData &live_edit_Data,
                bool *curr_im_is_onscreen = nullptr);

            void pick_image_to_annotate(const Imath::V2f &pos, LiveEditData &live_edit_Data);

            Imath::V2f transform_pointer_to_viewport_coord(
                const Imath::V2f &pos, const LiveEditData &live_edit_Data);

            Imath::V2f transform_pointer_to_image_coord(
                const Imath::V2f &pos,
                const LiveEditData &live_edit_Data,
                const media_reader::ImageBufPtr &image);

            Imath::V2f transform_pointer_to_image_coord(
                const Imath::V2f &pos, const LiveEditData &live_edit_Data);

            ui::canvas::Caption const *caption_under_pointer(
                const Imath::V2f &viewport_coord,
                LiveEditData &user_edit_data,
                utility::Uuid &bookmark_uuid,
                std::size_t skip_caption_hash = 0);

            void start_stroke_or_shape(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void modify_stroke_or_shape(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void start_editing_existing_caption(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void caption_drag(const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void
            caption_end_drag(const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void set_caption_property(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void caption_text_entered(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void schedule_bookmark_update(LiveEditData &user_edit_data);

            Annotation *modifiable_annotation(LiveEditData &user_edit_data);

            void remove_live_caption(LiveEditData &user_edit_data);

            void clear_live_caption(LiveEditData &user_edit_data);

            void
            caption_key_press(const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void caption_mouse_pressed(
                const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void caption_hover(const utility::JsonStore &payload, LiveEditData &user_edit_data);

            void undo(LiveEditData &user_edit_data);

            void redo(LiveEditData &user_edit_data);

            void push_live_edit_to_bookmark(LiveEditData &user_edit_data);

            void broadcast_live_stroke(
                const LiveEditData &user_edit_data,
                const utility::Uuid &user_id,
                const bool stroke_completed = false);

            void start_cursor_blink();

            void fade_all_laser_strokes();

            void annotation_about_to_be_edited(
                const bookmark::AnnotationBasePtr &anno, const utility::Uuid &anno_uuid);

            void remove_bookmark(const utility::Uuid &bookmark_id) {
                plugin::StandardPlugin::remove_bookmark(bookmark_id);
            }

            void clear_annotation(LiveEditData &user_edit_data);

            LiveEditData &live_edit_data(const utility::Uuid &uuid) {
                auto p = live_edit_data_.find(uuid);
                if (p != live_edit_data_.end())
                    return p->second;
                live_edit_data_[uuid]          = std::make_shared<LiveEdit>();
                live_edit_data_[uuid]->user_id = uuid;
                return live_edit_data_[uuid];
            }

            PerUserUndoRedo undo_redo_impl_;

            template <typename T, typename... TT>
            void undoable_action(
                bool concat,
                LiveEditData &user_edit_data,
                Annotation *annotation,
                TT &&...args) {

                undo_redo_impl_.undoable_action<T>(
                    concat,
                    user_edit_data->user_id,
                    user_edit_data->edited_bookmark_id,
                    *annotation,
                    args...);
            }

            template <typename T, typename... TT>
            void undoable_action(
                LiveEditData &user_edit_data, Annotation *annotation, TT &&...args) {

                undo_redo_impl_.undoable_action<T>(
                    user_edit_data->user_id,
                    user_edit_data->edited_bookmark_id,
                    *annotation,
                    args...);
            }

            std::set<LiveEditData> bookmark_update_queue_;

            std::map<std::string, Imath::M44f> viewport_transforms_;
            std::map<std::string, media_reader::ImageBufDisplaySetPtr> viewport_current_images_;

            bool laser_stroke_animation_{false};
            bool cursor_blinking_{false};
            bool show_annotations_during_playback_{false};
            std::atomic_bool hide_all_drawings_{false};
            std::atomic_bool cursor_blink_;
            std::map<std::string, std::atomic_int *> hide_strokes_per_viewport_;
            std::map<std::string, std::atomic_bool *> hide_all_per_viewport_;
            caf::actor live_edit_event_group_;
            utility::Uuid current_edited_annotation_uuid_;

            const media_reader::ImageBufDisplaySetPtr &
            get_viewport_image_set(const std::string &viewport_name) {
                // if no entry for viewport_name a default ImageBufDisplaySetPtr is made
                return viewport_current_images_[viewport_name];
            }

            // Annotations
            utility::Uuid current_bookmark_uuid_;
            utility::Uuid next_bookmark_uuid_ = {utility::Uuid::generate()};
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio