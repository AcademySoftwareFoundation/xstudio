// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "xstudio/ui/viewport/video_output_plugin.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio::plugin;

VideoOutputPlugin::VideoOutputPlugin(
    caf::actor_config &cfg,
    const utility::JsonStore &init_settings,
    const std::string &plugin_name)
    : xstudio::plugin::StandardPlugin(cfg, plugin_name, init_settings),
      init_settings_store_(init_settings) {

    message_handler_extensions_ = {
        [=](offscreen_viewport_atom, caf::actor offscreen_vp) {

        },
        [=](media_reader::ImageBufPtr incoming) { incoming_video_frame_callback(incoming); },
        [=](const utility::JsonStore &data) { receive_status_callback(data); },
        [=](caf::error &err) {}};

    // this call is essential to set-up the base class
    make_behavior();
}

void VideoOutputPlugin::on_exit() {
    exit_cleanup();
    StandardPlugin::on_exit();
}

void VideoOutputPlugin::finalise() {

    // this ensures 'Attributes' created by derived class get exposed in the UI layer
    connect_to_ui();

    // Get the 'StudioUI' which lives in the Qt context and therefore is able to
    // create offscreen viewports for us
    auto studio_ui = system().registry().template get<caf::actor>(studio_ui_registry);

    // tell the studio actor to create an offscreen viewport. It will send
    // us the resulting actor asynchronously as a message which our message
    // handler above will receive
    mail(offscreen_viewport_atom_v, Module::name() + " viewport")
        .request(studio_ui, infinite)
        .then(
            [=](caf::actor offscreen_vp) {
                // this is the offscreen renderer that we asked for below.
                offscreen_viewport_ = offscreen_vp;

                // sending this message forces the new viewport attach to the current
                // on screen playhead
                anon_mail(viewport_playhead_atom_v, true).send(offscreen_viewport_);

                // now we have an offscreen viewport to send us frame buffers
                // we can initialise the card and start output
                initialise();

                spawn_audio_output_actor(init_settings_store_);
            },
            [=](caf::error &err) mutable {
                spdlog::critical(
                    "{} in plugin {} : {}",
                    __PRETTY_FUNCTION__,
                    Module::name(),
                    to_string(err));
            });
}


void VideoOutputPlugin::start(int frame_width, int frame_height) {

    if (!offscreen_viewport_)
        return;

    mail(
        video_output_actor_atom_v,
        caf::actor_cast<caf::actor>(this),
        frame_width,
        frame_height,
        viewport::RGBA_16 // viewport::RGBA_10_10_10_2 // *see note below
        )
        .send(offscreen_viewport_);
}

void VideoOutputPlugin::stop() {

    mail(video_output_actor_atom_v, caf::actor()).send(offscreen_viewport_);
}

void VideoOutputPlugin::video_frame_consumed(const utility::time_point &frame_display_time) {

    // this informs the viewport the timepoint when video frames are going on-screen. It uses
    // this to infer the re-fresh rate of the display and therefore do an accurate 'pulldown'
    // when evaluating the playhead position for the next frame to go on-screen.
    anon_mail(ui::fps_monitor::framebuffer_swapped_atom_v, frame_display_time)
        .send(offscreen_viewport_);
}

void VideoOutputPlugin::request_video_frame(const utility::time_point &frame_display_time) {

    // Make an explicit request for the viewport to render a frame that will go on-screen at the
    // given time. The resulting frame is delivered to the subclass via
    // incoming_video_frame_callback
    anon_mail(
        ui::viewport::render_viewport_to_image_atom_v,
        frame_display_time + std::chrono::milliseconds(video_delay_millisecs_))
        .send(offscreen_viewport_);
}

void VideoOutputPlugin::send_status(const utility::JsonStore &data) {

    anon_mail(data).send(caf::actor_cast<caf::actor>(this));
}

void VideoOutputPlugin::sync_geometry_to_main_viewport(const bool sync) {

    caf::scoped_actor sys(system());
    try {

        utility::request_receive<bool>(
            *sys,
            offscreen_viewport_,
            module::change_attribute_value_atom_v,
            "Sync To Main Viewport",
            utility::JsonStore(sync),
            true);

        if (!sync) {
            anon_mail(ui::viewport::fit_mode_atom_v, previous_fit_mode_)
                .send(offscreen_viewport_);
            return;
        }

        previous_fit_mode_ = utility::request_receive<FitMode>(
            *sys, offscreen_viewport_, ui::viewport::fit_mode_atom_v);

        auto playhead_events_actor =
            system().registry().template get<caf::actor>(global_playhead_events_actor);

        auto main_viewport = utility::request_receive<caf::actor>(
            *sys, playhead_events_actor, ui::viewport::active_viewport_atom_v);

        if (!main_viewport)
            return;

        auto pan = utility::request_receive<Imath::V2f>(
            *sys, main_viewport, ui::viewport::viewport_pan_atom_v);

        auto zoom = utility::request_receive<float>(
            *sys, main_viewport, ui::viewport::viewport_scale_atom_v);

        anon_mail(ui::viewport::viewport_pan_atom_v, pan).send(offscreen_viewport_);
        anon_mail(ui::viewport::viewport_scale_atom_v, zoom).send(offscreen_viewport_);


    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void VideoOutputPlugin::display_info(
    const std::string &name,
    const std::string &model,
    const std::string &manufacturer,
    const std::string &serialNumber) {
    anon_mail(screen_info_atom_v, name, model, manufacturer, serialNumber)
        .send(offscreen_viewport_);
}
