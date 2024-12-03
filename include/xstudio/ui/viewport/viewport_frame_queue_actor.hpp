// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/viewport/viewport.hpp"

namespace xstudio {

namespace media_reader {
    class ImageBufferSet;
}

namespace ui {
    namespace viewport {

        /**
         *  @brief ViewportFrameQueueActor class.
         *
         *  @details
         *   This class receives frame buffers from the playhead ready for display in
         *   the viewport. It allows frames to be received at almost any rate and at
         *   any time which is important for accurate syncing of what is on screen with
         *   the playhead position. Meanwhile, the viewport, whose caf::messages
         *   are generally handled (for Qt apps) within the main UI event loop, will
         *   ask this class to tell it which frame buffers it should be uploading/displaying
         *   whenever the viewport is drawing itself. The reason for doing it this way
         *   is that caf messages processed by Qt mixin actors can get out of order or
         *   arrive late if the Qt event loop is heavily loaded (for example doing expensive
         *   ui redraws). Using this actor we can preserve the integrity of the
         * instantaneous queue of frame buffers that are ready to be drawn to the screen.
         */
        class ViewportFrameQueueActor : public caf::event_based_actor {
          public:
            ViewportFrameQueueActor(
                caf::actor_config &cfg,
                caf::actor viewport,
                std::map<utility::Uuid, caf::actor> overlay_actors,
                const std::string &viewport_name, 
                caf::actor colour_pipeline);

            ~ViewportFrameQueueActor() override;

            void on_exit() override;

          private:
            caf::behavior make_behavior() override { return behavior_; }

            caf::behavior behavior_;

            caf::actor playhead_broadcast_group_;
            caf::actor playhead_;
            utility::Uuid current_key_sub_playhead_id_, previous_key_sub_playhead_id_;
            /**
             *  @brief Receive frame buffer and colour pipeline data for drawing to screen.
             *
             *  @details Image data and colour pipeline data are passed to the viewport via
             *  this function. The ImageBufPtr when_to_display_ member indicates when the
             *  frame should be drawn.
             */
            virtual void queue_image_buffer_for_drawing(
                media_reader::ImageBufPtr &buf, const utility::Uuid &playhead_id);


            void get_frames_for_display_sync(
                caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp);

            void get_frames_for_display(
                caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
                const utility::time_point &when_going_on_screen = utility::time_point());

            void clear_images_from_old_playheads();

            void append_overlays_data(
                caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
                media_reader::ImageBufDisplaySet * result);

            void append_overlays_data(
                caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
                const int img_idx,
                media_reader::ImageBufDisplaySet * result,
                std::shared_ptr<int> response_count
                );

            typedef std::map<timebase::flicks, media_reader::ImageBufPtr> OrderedImagesToDraw;

            media_reader::ImageBufPtr
            get_least_old_image_in_set(const OrderedImagesToDraw &image_set);

            void drop_old_frames(const utility::time_point out_of_date_threshold);

            timebase::flicks compute_video_refresh() const;

            utility::time_point
            next_video_refresh(const timebase::flicks &video_refresh_period) const;

            timebase::flicks predicted_playhead_position_at_next_video_refresh();

            timebase::flicks predicted_playhead_position(const utility::time_point &when);

            double average_video_refresh_period() const;

            caf::typed_response_promise<bool> set_playhead(caf::actor playhead, const bool prefetch_inital_image);

            bool playing_          = {false};
            bool playing_forwards_ = {true};

            std::map<utility::Uuid, caf::actor> viewport_overlay_plugins_;

            const std::string viewport_name_;
            caf::actor colour_pipeline_;

            std::map<utility::Uuid, OrderedImagesToDraw> frames_to_draw_per_playhead_;

            struct ViewportRefreshData {
                std::deque<utility::time_point> refresh_history_;
                timebase::flicks refresh_rate_hint_ = timebase::k_flicks_zero_seconds;
                utility::time_point last_video_refresh_;
            } video_refresh_data_;

            timebase::flicks playhead_vid_sync_phase_adjust_ = timebase::k_flicks_zero_seconds;
            utility::time_point last_playhead_switch_tp_;
            utility::time_point last_playhead_set_tp_;
            std::set<utility::Uuid> deleted_playheads_;
            utility::UuidVector sub_playhead_ids_;

            playhead::AssemblyMode playhead_mode_;
            caf::actor viewport_layout_manager_;
            caf::actor viewport_;
            std::string viewport_layout_mode_name_;

            double playhead_velocity_ = {1.0};

        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
