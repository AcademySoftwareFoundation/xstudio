// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/viewport/viewport.hpp"

namespace xstudio {
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
                std::map<utility::Uuid, caf::actor> overlay_actors,
                const int viewport_index);

          private:
            caf::behavior make_behavior() override { return behavior_; }

            caf::behavior behavior_;

            caf::actor playhead_broadcast_group_;
            caf::actor playhead_;
            utility::Uuid current_playhead_;

            /**
             *  @brief Receive frame buffer and colour pipeline data for drawing to screen.
             *
             *  @details Image data and colour pipeline data are passed to the viewport via
             *  this function. The ImageBufPtr when_to_display_ member indicates when the
             *  frame should be drawn.
             */
            virtual void queue_image_buffer_for_drawing(
                media_reader::ImageBufPtr &buf, const utility::Uuid &playhead_id);

            /**
             *  @brief Get a pointer to the framebuffer and colour pipe data that should be
             *         displayed in the next redraw. We also get the next framebuffer that
             * we'll want to draw, if there is one in the queue. Calling this removes any
             * images in the queue for display that have a display timestamp older than the
             * timestamp for the returned data
             *
             *  @details This function should be used by classes subclassing the Viewport in
             * their main draw function to receive the image(s) to be draw to screen.
             * Returns an empty pointer if the image does not need to be refreshed since the
             * last draw.
             */
            void get_frames_for_display(
                const utility::Uuid &playhead_id,
                std::vector<media_reader::ImageBufPtr> &next_images);

            /**
             *  @brief Tell the viewport that certain playheads are defunct.
             *
             *  @details The viewport can remove any images that came from these
             *  playheads that are in the display queue
             */
            void
            child_playheads_deleted(const std::vector<utility::Uuid> &child_playhead_uuids);


            void add_blind_data_to_image_in_queue(
                const media_reader::ImageBufPtr &image,
                const utility::BlindDataObjectPtr &bdata,
                const utility::Uuid &overlay_actor_uuid);

            void update_blind_data(
                const std::vector<media_reader::ImageBufPtr> bufs, const bool wait = true);

            typedef std::vector<media_reader::ImageBufPtr> OrderedImagesToDraw;

            media_reader::ImageBufPtr
            get_least_old_image_in_set(const OrderedImagesToDraw &image_set);

            void drop_old_frames(const utility::time_point out_of_date_threshold);

            void update_image_blind_data_and_deliver(
                const media_reader::ImageBufPtr &buf,
                caf::typed_response_promise<std::vector<media_reader::ImageBufPtr>> rp);

            timebase::flicks compute_video_refresh() const;

            utility::time_point
            next_video_refresh(const timebase::flicks &video_refresh_period) const;

            timebase::flicks predicted_playhead_position_at_next_video_refresh();

            double average_video_refresh_period() const;

            bool playing_          = {false};
            bool playing_forwards_ = {true};

            std::map<utility::Uuid, caf::actor> overlay_actors_;

            caf::actor colour_pipeline_;

            std::map<utility::Uuid, OrderedImagesToDraw> frames_to_draw_per_playhead_;

            struct ViewportRefreshData {
                std::deque<utility::time_point> refresh_history_;
                timebase::flicks refresh_rate_hint_ = timebase::k_flicks_zero_seconds;
                utility::time_point last_video_refresh_;
            } video_refresh_data_;

            timebase::flicks playhead_vid_sync_phase_adjust_ = timebase::k_flicks_zero_seconds;

            const int viewport_index_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
