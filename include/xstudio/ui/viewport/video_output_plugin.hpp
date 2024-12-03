// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class VideoOutputPlugin : public plugin::StandardPlugin {

          public:
            // Note - when deriving from VideoOutputPlugin your constructor must have
            // a signature of this form:
            //
            // MyVidOutputPlugin(caf::actor_config &cfg, const utility::JsonStore
            // &init_settings)
            //
            // cgf and init_settings must be supplied to base class with a unique plugin name

            VideoOutputPlugin(
                caf::actor_config &cfg,
                const utility::JsonStore &init_settings,
                const std::string &plugin_name);
            ~VideoOutputPlugin() override = default;

            // This method should be called on successful construction of your
            // class - i.e. at the end of the constructor assuming all other set-up
            // and initialisation (e.g. finding hardware devices) was completed. It
            // will create the offscreen viewport ready for generating output video
            // frames.
            void finalise();

            // This method should be implemented to allow cleanup of any/all resources
            // relating to the video output
            virtual void exit_cleanup() = 0;

            // This method is called when a new image buffer is ready to be displayed
            virtual void incoming_video_frame_callback(media_reader::ImageBufPtr incoming) = 0;

            // Re-implement this method to receive data sent from your own thread(s)
            // via 'send_status' - this is provided as a thread safe way to
            // communicate between threads your plugin manages and the xstudio
            // thread(s) that VideoOutputPlugin would normally execute within.
            virtual void receive_status_callback(const utility::JsonStore &status_data) {}

            // Allocate your resources needed for video output, initialise hardware etc. in
            // this function
            virtual void initialise() = 0;

            void on_exit() override;

            // Call this method to intiate rendering of the xstudio viewport to an offscreen
            // surface. The resulting pixel buffers are captured and returned via the
            // 'incoming_video_frame_callback' method
            //
            // This method can be safely called from any thread
            void start(int frame_width, int frame_height);

            // Call this method to stop rendering and cease calls to the
            // incoming_video_frame_callback method
            //
            // This method can be safely called from any thread
            void stop();

            // This method MUST be called on every refresh of the video output to tell the
            // offscreen viewport to send a new video frame. The 'beat' of frame_display_time
            // is important to tell the offscreen viewport the natural refresh rate of the
            // video output device and to sync the playhead to your video output.
            //
            // frame_display_time should inform as accurately as possible when the last frame
            // delivered via incoming_video_frame_callback is actually shown on the display.
            //
            // This method can be safely called from any thread
            void video_frame_consumed(
                const utility::time_point &frame_display_time = utility::clock::now());

            // Request a new video frame to be rendered and delivered via
            // incoming_video_frame_callback. If possible, inform accurately when this video
            // frame should be displayed in the video output.
            void request_video_frame(
                const utility::time_point &frame_display_time = utility::clock::now());

            // Set a value for the video pipeline delay in milliseconds. This value is used to
            // adjust the position of the playhead (forwards or backwards) when requesting
            // frames for display. It allows you to compensate for video frames being buffered
            // for display, for example, which would otherwise cause your video output to 'lag'
            // the video output in xSTUDIO's main interface
            void video_delay_milliseconds(const int millisceconds_delay) {
                video_delay_millisecs_ = millisceconds_delay;
            }

            // Override this method to instance a class derived from AudioOutputDevice with new
            // that can receive an audio sample stream from xSTUDIO and deliver to your
            // physical/virtual output device. Ownership of this object (and it's destruction)
            // is with xSTUDIO.
            virtual audio::AudioOutputDevice *
            make_audio_output_device(const utility::JsonStore &prefs) {
                return nullptr;
            }

            // Use this method to communicate arbitrary data from your own threads (for example
            // a worker loop delivering video frames to SDI video hardware) to xstudio. The data
            // is received in the 'status_callback' virtual method
            //
            // This method can be safely called from any thread
            void send_status(const utility::JsonStore &status_data);

            // Set whether the offscreen viewport tracks the mirror mode, zoom and
            // pan andr the FitMode of the 'main' viewport in the xstudio interface
            //            //
            // This method can be safely called from any thread
            void sync_geometry_to_main_viewport(const bool sync);

            // Call this method to inform the viewport about what sort of display it is driving.
            // This information is passed onto the colour management system so that it can
            // try and pick an appropriate display (colour) transform.
            void display_info(
                const std::string &name,
                const std::string &model,
                const std::string &manufacturer,
                const std::string &serialNumber);

          private:
            void __init(const utility::JsonStore init_settings);

            void spawn_audio_output_actor(const utility::JsonStore &prefs) {
                auto audio_dev = make_audio_output_device(prefs);
                if (audio_dev) {
                    if (audio_output_) {
                        unlink_from(audio_output_);
                        send_exit(audio_output_, caf::exit_reason::user_shutdown);
                    }
                    audio_output_ = spawn<audio::AudioOutputActor>(
                        std::shared_ptr<audio::AudioOutputDevice>(audio_dev));
                    link_to(audio_output_);
                }
            }

            caf::message_handler message_handler_extensions() override {
                return message_handler_extensions_;
            }

            caf::actor audio_output_;
            caf::actor offscreen_viewport_;
            caf::actor main_viewport_;
            caf::message_handler message_handler_extensions_;
            FitMode previous_fit_mode_ = {FitMode::Best};
            int video_delay_millisecs_ = {0};
            const utility::JsonStore init_settings_store_;
        };
    } // namespace viewport
} // namespace ui
} // namespace xstudio
