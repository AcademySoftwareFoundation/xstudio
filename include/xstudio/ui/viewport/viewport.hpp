// SPDX-License-Identifier: Apache-2.0
#pragma once


// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
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
                const std::string &name = std::string());
            virtual ~Viewport();

            bool process_pointer_event(PointerEvent &);

            void set_pointer_event_viewport_coords(PointerEvent &pointer_event);

            void set_scale(const float scale);
            void set_size(const float w, const float h, const float devicePixelRatio);
            void set_pan(const float x_pan, const float y_pan);
            void set_fit_mode(const FitMode md, const bool sync = true);
            void set_mirror_mode(const MirrorMode md);
            void set_screen_infos(
                const std::string &name,
                const std::string &model,
                const std::string &manufacturer,
                const std::string &serialNumber,
                const double refresh_rate);

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
             *  an active OpenGL context). Thread safe, as required by QML integration where
             *  render thread is separate to main GUI thread.
             *
             *  prepare_render_data should be called from GUI thread before calling this 
             *  method.
             */
            void render() const;

            /**
             *  @brief Render the viewport, for a specific display time measured as a system
             *  clock timepoint.
             *
             *  @details Renders the image data into some initialised graphics resource (e.g.
             *  an active OpenGL context), allowing an adjustment to the playhead position to
             *  account for a display pipeline delay. For example, if this viewport is an
             *  offscreen viewport that delivers an image to be displayed on an SDI device
             *  like a projector, and the buffering in the SDI card results in a 250ms delay
             *  then 'when_going_on_screen' should be clock::now() + milliseconds(250)
             *
             *  Calling prepare_render_data is not required before using this method.
             */
            void render(const utility::time_point &when_going_on_screen);

            /**
             *  @brief Render the viewport with a given image
             *
             *  @details Render the image data into some initialised graphics resource (e.g.
             *  an active OpenGL context)
             *
             *  Calling prepare_render_data is not required before using this method.
             */
            void render(const media_reader::ImageBufPtr &image_buf);

            /**
             *  @brief Any pre-render interaction with Viewport state data etc. must be done
             *  in this function. All necessary data for rendering the viewport must be gatherered
             *  and finalised within this function. The data should be made thread safe, as the
             *  rendering can/will be executed in a separate thread to the main GUI thread that
             *  the Viewport lives in.                    
             */
            void prepare_render_data(const utility::time_point &when_going_on_screen = utility::time_point());

            /**
             *  @brief Initialise the viewport.
             *
             *  @details Carry out first time render one-shot initialisation of any member
             * data or other state/data initialisation of graphics resources
             */
            void init() { if (active_renderer_) active_renderer_->init(); }

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
            [[nodiscard]] const Imath::M44f &window_to_viewport_matrix() const {
                return viewport_to_canvas_;
            }
            void apply_fit_mode();

            [[nodiscard]] bool playing() const { return playing_; }
            [[nodiscard]] const std::string &pixel_info_string() const {
                return pixel_info_string_;
            }
            [[nodiscard]] caf::actor playhead() {
                return caf::actor_cast<caf::actor>(playhead_addr_);
            }
            [[nodiscard]] utility::Uuid playhead_uuid() { return playhead_uuid_; }

            [[nodiscard]] caf::actor colour_pipeline() { return colour_pipeline_; }

            [[nodiscard]] const std::string &toolbar_name() const { return toolbar_name_; }

            utility::JsonStore serialise() const override;

            /**
             *  @brief Based on a user preference to control filtering and also the current
             * pixel zoom of the image in the viewport, this function can be used to switch
             * bilinear filtering on in the display shader.
             */
            bool use_bilinear_filtering() const;

            /**
             *  @brief We only call this method if we want the viewport to always automatically
             *  connect to the playhead of the currently selected playlist/timeline/subset etc.
             */
            void auto_connect_to_global_selected_playhead();


            void deserialise(const nlohmann::json &json) override;

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
            const std::vector<Imath::Box2f> & image_bounds_in_viewport_pixels() const {
                return image_bounds_in_viewport_pixels_;
            }

            /**
             *  @brief Get the resolution of the current images
             *
             */
            const std::vector<Imath::V2i> & image_resolutions() const {
                return image_resolutions_;
            }

            /**
             *  @brief Get an ordered list of the FitMode enums and the corresponding name
             *  that would be used for the UI.
             */
            static std::list<std::pair<FitMode, std::string>> fit_modes();

            caf::message_handler message_handler() override;

            enum ChangeCallbackId { Redraw, TranslationChanged, ImageResolutionsChanged, ImageBoundsChanged, PlayheadChanged };

            typedef std::function<void(ChangeCallbackId)> ChangeCallback;

            void set_change_callback(ChangeCallback f) { event_callback_ = f; }

            void set_playhead(caf::actor playhead, const bool wait_for_refresh = false);

            void set_compare_mode(const std::string & compare_mode);

            void grid_mode_media_select(const PointerEvent &pointer_event);

            caf::actor fps_monitor() { return fps_monitor_; }

            void framebuffer_swapped(const utility::time_point swap_time);

            void reset() override;

            media_reader::ImageBufPtr get_onscreen_image(const bool force_playhead_sync);

            void set_visibility(bool is_visible);

          protected:
            void register_hotkeys() override;

            void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

            void menu_item_activated(
                const utility::JsonStore &menu_item_data,
                const std::string &user_data) override;

            /**
             *  @brief Update viewport properties like frame number, error message or
             *  other metadata at the moment when the on-screen frame is updated
             *
             *  @details Info like error message, whether the frame is out-of-range and
             *  so-on is propagated up to the UI by this class and/or its actor wrapper
             */
            void update_onscreen_frame_info(const media_reader::ImageBufDisplaySetPtr &images);

            /**
             *  @brief Get the set of images that we need to draw to the viewport.
             *  Note that the set also includes not just images to be drawn now but
             *  the images that are due on screen next (during playback) to allow
             *  for asynchronous pixel uploads etc. for playback optimisation
             *
             *  @details This function should be used by classes subclassing the Viewport in
             * their main draw function to receive the image(s) to be draw to screen.
             * Returns an empty pointer if the image does not need to be refreshed since the
             * last draw.
             */
            media_reader::ImageBufDisplaySetPtr get_frames_for_display(
                const bool force_playhead_sync                  = false,
                const utility::time_point &when_being_displayed = utility::time_point());

            void instance_renderer_plugins();

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
                float layout_aspect_     = {16.0f / 9.0f};
            } state_, interact_start_state_;

            struct FitModeStat {
                FitMode fit_mode_     = {Height};
                Imath::V3f translate_ = {0.0f, 0.0f, 0.0f};
                float scale_          = {0.0f};
            } previous_fit_zoom_state_;

            struct RenderData {
                media_reader::ImageBufDisplaySetPtr images;
                Imath::M44f projection_matrix;
                Imath::M44f window_to_viewport_matrix;
                ViewportRendererPtr renderer;
                std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> overlay_renderers;
            };
            std::shared_ptr<const RenderData> render_data_;

            Imath::M44f projection_matrix_;
            Imath::M44f inv_projection_matrix_;
            Imath::M44f interact_start_projection_matrix_;
            Imath::M44f interact_start_inv_projection_matrix_;
            Imath::M44f viewport_to_canvas_;

            float devicePixelRatio_     = {1.0};
            bool broadcast_fit_details_ = {true};

            Imath::V4f normalised_pointer_position() const;

            void update_matrix();

            void get_colour_pipeline();

            void setup_menus();

            void event_callback(const ChangeCallbackId ev);

            void quickview_media(
                std::vector<caf::actor> &media_items,
                std::string compare_mode,
                const int in_pt  = -1,
                const int out_pt = -1);

            media_reader::ImageBufDisplaySetPtr prepare_image_for_display(
                const media_reader::ImageBufPtr &image_buf) const;

            void calc_image_bounds_in_viewport_pixels();

            void update_image_resolutions();

            utility::JsonStore settings_;

            std::string window_id_;

            typedef std::function<bool(const PointerEvent &pointer_event)> PointerInteractFunc;
            std::map<Signature, PointerInteractFunc> pointer_event_handlers_;

            std::string pixel_info_string_ = {"--"};
            media_reader::ImageBufPtr on_screen_hero_frame_;
            media_reader::ImageBufPtr next_on_screen_hero_frame_;
            media_reader::ImageBufDisplaySetPtr on_screen_frames_;

            timebase::flicks screen_refresh_period_ = timebase::k_flicks_zero_seconds;
            std::string toolbar_name_;
            std::string compare_mode_;

            caf::actor display_frames_queue_actor_;
            caf::actor parent_actor_;
            caf::actor playhead_viewport_events_group_;
            caf::actor fps_monitor_;
            caf::actor keypress_monitor_;
            caf::actor viewport_events_actor_;
            caf::actor colour_pipeline_;
            caf::actor keyboard_events_actor_;
            caf::actor quickview_playhead_;
            caf::actor global_playhead_events_group_;

            caf::actor_addr playhead_addr_;
            utility::Uuid playhead_uuid_;

            void dummy_evt_callback(ChangeCallbackId) {}
            ChangeCallback event_callback_;

          protected:

            bool done_init_  = {false};
            bool playing_    = {false};
            bool is_visible_ = {false};
            size_t last_images_hash_ = {0};
            bool needs_redraw_ = {true};
            std::set<int> held_keys_;
            std::vector<Imath::Box2f> image_bounds_in_viewport_pixels_;
            std::vector<Imath::V2i> image_resolutions_;
            size_t image_bounds_hash_ = {0};
            std::map<utility::Uuid, caf::actor> overlay_plugin_instances_;
            std::map<utility::Uuid, caf::actor> hud_plugin_instances_;
            std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> viewport_overlay_renderers_;
            std::map<utility::Uuid, plugin::GPUPreDrawHookPtr> overlay_pre_render_hooks_;

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
            module::StringAttribute *frame_rate_expr_;
            module::StringAttribute *custom_cursor_name_;
            module::BooleanAttribute *sync_to_main_viewport_;

            utility::Uuid zoom_hotkey_;
            utility::Uuid pan_hotkey_;
            utility::Uuid reset_hotkey_;
            utility::Uuid fit_mode_hotkey_;
            utility::Uuid mirror_mode_hotkey_;

            utility::Uuid reset_menu_item_;

            utility::time_point t1_;

            void hotkey_pressed(
                const utility::Uuid &hotkey_uuid,
                const std::string &context,
                const std::string &window) override;
            void hotkey_released(
                const utility::Uuid &hotkey_uuid, const std::string &context) override;
            void update_attrs_from_preferences(const utility::JsonStore &) override;

            struct ViewportLayout {

                caf::actor viewport_layout_controller_;
                ViewportRendererPtr viewport_layout_renderer_;
                std::vector<std::pair<std::string, playhead::AssemblyMode>> compare_modes_;

            };
            std::vector<ViewportLayout> viewport_layouts_;

            ViewportRendererPtr active_renderer_;
        };
    } // namespace viewport
} // namespace ui
} // namespace xstudio