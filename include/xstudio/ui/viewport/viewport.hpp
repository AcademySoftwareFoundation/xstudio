// SPDX-License-Identifier: Apache-2.0
#pragma once


// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/viewport/enums.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "viewport_helpers.hpp"
#include "viewport_renderer_base.hpp"

#define NO_FRAME INT_MIN


namespace xstudio {
namespace ui {
    namespace viewport {

        /**
         *  @brief Viewport class.
         *
         *  @details
         *   This class maintains the viewport state, such as pan, zoom as well as
         *   receiving images for display from the playhead and passing to the
         *   ViewportRenderer which is responsible for displaying images and overlays
         *   on the screen.
         */

        class Viewport : public module::Module {

          public:
            Viewport(
                const utility::JsonStore &state_data,
                caf::actor parent_actor,
                const int viewport_index,
                ViewportRendererPtr the_renderer,
                const std::string & name = std::string());
            virtual ~Viewport();

            bool process_pointer_event(PointerEvent &);

            void set_pointer_event_viewport_coords(PointerEvent &pointer_event);

            void set_scale(const float scale);
            void set_size(const float w, const float h, const float devicePixelRatio);
            void set_pan(const float x_pan, const float y_pan);
            void set_fit_mode(const FitMode md);
            void set_mirror_mode(const MirrorMode md);
            void set_pixel_zoom(const float zoom);
            void set_screen_infos(
                const std::string &name,
                const std::string &model,
                const std::string &manufacturer,
                const std::string &serialNumber,
                const double refresh_rate);

            /**
             *  @brief Link to another viewport so the zoom, scale and colour
             *  management settings are shared between the two viewports
             *
             *  @details This allows the pop-out viewer to track the primary
             *  viewer in the main interface, for example
             */
            void link_to_viewport(caf::actor other_viewport);

            /**
             *  @brief Switch the fit mode and zoom to it's previous state (usually before
             * some user interaction)
             *
             *  @details This allows for the double click behaviour on the fit and zoom
             * buttons to toggle the fit/zoom back to what it was before the last
             * interactino started.
             */
            void revert_fit_zoom_to_previous(const bool synced = false);

            /**
             *  @brief Switch the mirror mode to Flop/Off
             *
             *  @details This allows setting Flop (mirror along Y axis)
             */
            void switch_mirror_mode();

            /**
             *  @brief Render the viewport.
             *
             *  @details Render the image data into some initialised graphics resource (e.g.
             *  an active OpenGL context)
             */
            void render() {
                std::vector<media_reader::ImageBufPtr> next_images;
                get_frames_for_display(next_images);
                if (!next_images.empty())
                    update_onscreen_frame_info(next_images[0]);
                the_renderer_->render(
                    next_images, to_scene_matrix(), projection_matrix(), fit_mode_matrix());
            }

            /**
             *  @brief Render the viewport with a given image
             *
             *  @details Render the image data into some initialised graphics resource (e.g.
             *  an active OpenGL context)
             */
            void render(const media_reader::ImageBufPtr &image_buf) {
                update_onscreen_frame_info(image_buf);
                the_renderer_->render(
                    std::vector<media_reader::ImageBufPtr>({image_buf}),
                    to_scene_matrix(),
                    projection_matrix(),
                    fit_mode_matrix());
            }


            /**
             *  @brief Initialise the viewport.
             *
             *  @details Carry out first time render one-shot initialisation of any member
             * data or other state/data initialisation of graphics resources
             */
            void init() { the_renderer_->init(); }

            /**
             *  @brief Inform the viewport of how its coordinate system maps to the
             *  coordinates of the canvas/scene into which it is drawn.
             *
             *  @details The viewport may be drawn into a quad within some larger canvas.
             *  Here we can tell the viewport the coordinates of this quad in the canvas
             *  coordinates. This is important for the QQuickItem wrapping of the viewport,
             *  as in order to draw the viewport correctly into the QQuickItem bounds we
             *  need to know how the QQuickItem is transformed into the parent QQuickWindow
             */
            bool set_scene_coordinates(
                const Imath::V2f topleft,
                const Imath::V2f topright,
                const Imath::V2f bottomright,
                const Imath::V2f bottomleft,
                const Imath::V2i scene_size,                
                const float devicePixelRatio);

            /**
             *  @brief Inform the viewport of the size of the image currently on screen to
             *  update the fit mode matrix.
             *
             *  @details The matrix translating & scaling the image so that the active 'fit
             * mode' works as required is dependent on the resolution of the image being
             * displaye. This function receives the resolution of the current image being
             * displayed and updates the fit mode matrix as required.
             */
            void update_fit_mode_matrix(
                const int image_width    = 0,
                const int image_height   = 0,
                const float pixel_aspect = 1.0f);

            [[nodiscard]] float scale() const { return state_.scale_; }
            [[nodiscard]] float pixel_zoom() const;
            [[nodiscard]] Imath::V2f size() const { return state_.size_; }
            [[nodiscard]] Imath::V2f pan() const {
                return Imath::V2f(state_.translate_.x, state_.translate_.y);
            }
            [[nodiscard]] Imath::V2i on_screen_image_dims() const { return state_.image_size_; }
            [[nodiscard]] FitMode fit_mode() const { return state_.fit_mode_; }
            [[nodiscard]] const Imath::M44f &projection_matrix() const {
                return projection_matrix_;
            }
            [[nodiscard]] const Imath::M44f &inv_projection_matrix() const {
                return inv_projection_matrix_;
            }
            [[nodiscard]] const Imath::M44f &to_scene_matrix() const {
                return viewport_to_canvas_;
            }
            [[nodiscard]] const Imath::M44f &fit_mode_matrix() const {
                return fit_mode_matrix_;
            }
            [[nodiscard]] const std::string &frame_rate_expression() const {
                return frame_rate_expr_;
            }
            [[nodiscard]] bool frame_out_of_range() const { return frame_out_of_range_; }
            [[nodiscard]] bool no_alpha_channel() const { return no_alpha_channel_; }
            [[nodiscard]] int on_screen_frame() const { return on_screen_frame_; }
            [[nodiscard]] bool playing() const { return playing_; }
            [[nodiscard]] const std::string &pixel_info_string() const {
                return pixel_info_string_;
            }
            [[nodiscard]] caf::actor playhead() {
                return caf::actor_cast<caf::actor>(playhead_addr_);
            }
            [[nodiscard]] const std::string &toolbar_name() const { return toolbar_name_; }

            [[nodiscard]] caf::actor colour_pipeline() { return colour_pipeline_; }

            utility::JsonStore serialise() const override;

            /**
             *  @brief Based on a user preference to control filtering and also the current
             * pixel zoom of the image in the viewport, this function can be used to switch
             * bilinear filtering on in the display shader.
             */
            bool use_bilinear_filtering() const;


            void deserialise(const nlohmann::json &json) override;

            /**
             *  @brief Given image coordinate in pixels, returns the position in the
                       viewport coordinate system.
             */
            Imath::V2f image_coordinate_to_viewport_coordinate(const int x, const int y) const;


            /**
             *  @brief Return current pointer position transformed into the viewport
             *         coordinate system.
             */
            Imath::V2f pointer_position() const;

            /**
             *  @brief Return current pointer position as a pixel coordinate in the
             *         viewport window.
             */
            Imath::V2i raw_pointer_position() const;

            /**
             *  @brief Get the bounding box, in the viewport area, of the current image
             *
             */
            Imath::Box2f image_bounds_in_viewport_pixels() const;

            /**
             *  @brief Get the resolution of the current image
             *
             */
            Imath::V2i image_resolution() const;

            /**
             *  @brief Get an ordered list of the FitMode enums and the corresponding name
             *  that would be used for the UI.
             */
            static std::list<std::pair<FitMode, std::string>> fit_modes();

            caf::message_handler message_handler() override;

            enum ChangeCallbackId {
                Redraw,
                ZoomChanged,
                ScaleChanged,
                FitModeChanged,
                MirrorModeChanged,
                FrameRateChanged,
                OutOfRangeChanged,
                NoAlphaChannelChanged,
                OnScreenFrameChanged,
                ExposureChanged,
                TranslationChanged,
                PlayheadChanged
            };

            typedef std::function<void(ChangeCallbackId)> ChangeCallback;

            /**
             *  @brief Set whether a viewport will automatically show the
             *  'active' session playlist/subset/timeline
             *
             *  @details When a viewport is set to auto-connect to the playhead,
             *  this means that when the 'active' playlist/subset/timeline at
             *  the session level changes (e.g. if the user double cliks on a
             *  playlist in the playlist panel interface) then the viewport
             *  will automatically connect to the playhead for that playlist/
             *  subset/timeline such that it shows the select media therein.
             *
             *  Then auto-connect is not set, the viewport remains connected
             *  to the playhead that was set by calling the 'set_playhead
             *  function.
             */
            void auto_connect_to_playhead(bool auto_connect);

            void set_change_callback(ChangeCallback f) { event_callback_ = f; }

            void set_playhead(caf::actor playhead, const bool wait_for_refresh = false);

            caf::actor fps_monitor() { return fps_monitor_; }

            void framebuffer_swapped(const utility::time_point swap_time);

            media_reader::ImageBufPtr get_image_from_playhead(caf::actor playhead);

            media_reader::ImageBufPtr get_onscreen_image();

            void set_aux_shader_uniforms(const utility::JsonStore & j, const bool clear_and_overwrite = false);

          protected:
            void register_hotkeys() override;

            void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

            /**
             *  @brief Update viewport properties like frame number, error message or
             *  other metadata at the moment when the on-screen frame is updated
             *
             *  @details Info like error message, whether the frame is out-of-range and
             *  so-on is propagated up to the UI by this class and/or its actor wrapper
             */
            void update_onscreen_frame_info(const media_reader::ImageBufPtr &frame);

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
            void get_frames_for_display(std::vector<media_reader::ImageBufPtr> &next_images);

            void instance_overlay_plugins();


          private:
            struct ViewportState {
                Imath::V3f translate_            = {0.0f, 0.0f, 0.0f};
                float scale_                     = {1.0f};
                Imath::V2i image_size_           = {1920, 1080};
                Imath::V2f size_                 = {};
                Imath::V2i raw_pointer_position_ = {};
                Imath::V4f pointer_position_;
                FitMode fit_mode_       = {Height};
                MirrorMode mirror_mode_ = {Off};
                float image_aspect_     = {16.0f / 9.0f};
                float fit_mode_zoom_    = {1.0};
            } state_, interact_start_state_;

            struct FitModeStat {
                FitMode fit_mode_     = {Height};
                Imath::V3f translate_ = {0.0f, 0.0f, 0.0f};
                float scale_          = {0.0f};
            } previous_fit_zoom_state_;

            Imath::M44f projection_matrix_;
            Imath::M44f inv_projection_matrix_;
            Imath::M44f interact_start_projection_matrix_;
            Imath::M44f interact_start_inv_projection_matrix_;
            Imath::M44f viewport_to_canvas_;
            Imath::M44f fit_mode_matrix_;
            float devicePixelRatio_ = {1.0};

            Imath::V4f normalised_pointer_position() const;

            void update_matrix();

            void get_colour_pipeline();

            void
            quickview_media(std::vector<caf::actor> &media_items, std::string compare_mode);

            utility::JsonStore settings_;

            typedef std::function<bool(const PointerEvent &pointer_event)> PointerInteractFunc;
            std::map<Signature, PointerInteractFunc> pointer_event_handlers_;

            bool frame_out_of_range_ = {false};
            bool no_alpha_channel_   = {false};
            int on_screen_frame_;
            std::string frame_rate_expr_   = {"--/--"};
            std::string pixel_info_string_ = {"--"};
            media_reader::ImageBufPtr on_screen_frame_buffer_;
            media_reader::ImageBufPtr about_to_go_on_screen_frame_buffer_;
            timebase::flicks screen_refresh_period_ = timebase::k_flicks_zero_seconds;
            std::string toolbar_name_;

            caf::actor display_frames_queue_actor_;
            caf::actor parent_actor_;
            caf::actor playhead_viewport_events_group_;
            caf::actor fps_monitor_;
            caf::actor keypress_monitor_;
            caf::actor viewport_events_actor_;
            std::vector<caf::actor> other_viewports_;
            caf::actor colour_pipeline_;
            caf::actor keyboard_events_actor_;
            caf::actor quickview_playhead_;

            caf::actor_addr playhead_addr_;

            void dummy_evt_callback(ChangeCallbackId) {}
            ChangeCallback event_callback_;

          protected:
            utility::Uuid current_playhead_, new_playhead_;
            bool done_init_       = {false};
            int viewport_index_   = {0};
            bool playing_         = {false};
            bool playhead_pinned_ = {false};
            std::set<int> held_keys_;

            utility::JsonStore aux_shader_uniforms_;

            std::map<utility::Uuid, caf::actor> overlay_plugin_instances_;
            std::map<utility::Uuid, caf::actor> hud_plugin_instances_;

            module::BooleanAttribute *zoom_mode_toggle_;
            module::BooleanAttribute *pan_mode_toggle_;
            module::StringChoiceAttribute *fit_mode_;
            module::StringChoiceAttribute *mirror_mode_;
            module::StringAttribute *frame_error_message_;
            module::StringChoiceAttribute *filter_mode_preference_;
            module::StringChoiceAttribute *texture_mode_preference_;
            module::StringChoiceAttribute *mouse_wheel_behaviour_;
            module::BooleanAttribute *hud_toggle_;
            module::StringChoiceAttribute *hud_elements_;

            utility::Uuid zoom_hotkey_;
            utility::Uuid pan_hotkey_;
            utility::Uuid reset_hotkey_;
            utility::Uuid fit_mode_hotkey_;
            utility::Uuid mirror_mode_hotkey_;

            utility::time_point t1_;

            void hotkey_pressed(
                const utility::Uuid &hotkey_uuid, const std::string &context) override;
            void hotkey_released(
                const utility::Uuid &hotkey_uuid, const std::string &context) override;
            void update_attrs_from_preferences(const utility::JsonStore &) override;


            ViewportRendererPtr the_renderer_;
        };
    } // namespace viewport
} // namespace ui
} // namespace xstudio