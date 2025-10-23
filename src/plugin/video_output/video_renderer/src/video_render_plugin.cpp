// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <chrono>

#include <caf/actor_registry.hpp>

#include "video_render_plugin.hpp"
#include "video_render_worker.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

#include <ImfRgbaFile.h>
#include <vector>

using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::video_render_plugin_1_0;

namespace {

const nlohmann::json header = nlohmann::json::parse(R"(
        {
            "uuid" : "",
            "output_file" : "Output File",
            "render_item" : "Render Item",
            "resolution" : "Resolution",
            "video_preset" : "Video Preset",
            "status" : "Job Status",
            "failure" : false,
            "ffmpeg_stdout" : "",
            "visible" : false,
            "percent_complete" : 0.0
        }
        )");
}

VideoRenderPlugin::VideoRenderPlugin(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "VideoRender", init_settings) {

    resolutions_ = add_json_attribute("resolution_options");
    resolutions_->expose_in_ui_attrs_group("video_render_attrs");
    resolutions_->set_preference_path("/plugin/video_render/resolution_options");

    frame_rates_ = add_json_attribute("frame_rates");
    frame_rates_->expose_in_ui_attrs_group("video_render_attrs");
    frame_rates_->set_preference_path("/plugin/video_render/frame_rate_options");

    video_codec_presets_ = add_json_attribute("video_codec_options");
    video_codec_presets_->expose_in_ui_attrs_group("video_render_attrs");
    video_codec_presets_->set_preference_path("/plugin/video_render/video_codec_user_presets");

    video_codec_default_presets_ = add_json_attribute("video_codec_default_options");
    video_codec_default_presets_->expose_in_ui_attrs_group("video_render_attrs");
    video_codec_default_presets_->set_preference_path(
        "/plugin/video_render/video_codec_defaults_preset");

    jobs_status_data_[utility::Uuid()] =
        add_json_attribute("jobs_status_data", "jobs_status_data", header);
    jobs_status_data_[utility::Uuid()]->expose_in_ui_attrs_group("video_render_status_attrs");

    overall_status_ = add_string_attribute("overall_status", "overall_status", "");
    overall_status_->expose_in_ui_attrs_group("video_render_attrs");

    ocio_warnings_ = add_string_attribute("ocio_warnings", "ocio_warnings", "");
    ocio_warnings_->expose_in_ui_attrs_group("video_render_attrs");


    // The following attributes are set and referenced in the UI code so that
    // the user's most recent settings will persist between xstudio sessions
    auto load_on_completion =
        add_boolean_attribute("load_on_completion", "load_on_completion", false);
    load_on_completion->expose_in_ui_attrs_group("video_render_attrs");
    load_on_completion->set_preference_path("/plugin/video_render/load_on_completion");

    auto render_audio = add_boolean_attribute("render_audio", "render_audio", true);
    render_audio->expose_in_ui_attrs_group("video_render_attrs");
    render_audio->set_preference_path("/plugin/video_render/render_audio");

    auto frame_rate = add_string_attribute("frame_rate", "frame_rate", "24.0");
    frame_rate->expose_in_ui_attrs_group("video_render_attrs");
    frame_rate->set_preference_path("/plugin/video_render/frame_rate");

    auto render_queue_visible =
        add_boolean_attribute("render_queue_visible", "render_queue_visible", false);
    render_queue_visible->expose_in_ui_attrs_group("video_render_attrs");

    make_behavior();
    connect_to_ui();

    render_menu_item_ = insert_menu_item(
        "playlist_context_menu", "Render Selected To Movie ...", "Export", 0.0f);
    render_menu_item2_ =
        insert_menu_item("main menu bar", "Render Selected To Movie ...", "File|Export", 0.0f);

    register_main_menu_bar_widget(
        R"(            
            import VideoRenderer 1.0
            VideoRendererStatusIndicator {}
        )",
        1.0f);

    // To manage the dialog that shows the render queue, we have a singleton
    // parent item that shows/hides the render queue panel based on the
    // render_queue_visible attribute. This lets us show the queue panel in
    // a couple of different places (when Render is dispatched or from the
    // VideoRendererStatusIndicator button)
    register_singleton_qml(
        R"(
            import VideoRenderer 1.0
            VideoRendererJobsQueueDialogOwner {}
        )");
}

void VideoRenderPlugin::on_exit() {

    // we need this to prevent VideoRenderPlugin and VideoRenderWorker holding
    // shared references (caf::actor) to each other and preventing cleanup/destruction
    // Note that because we called 'link_to' bloew, when VideoRenderPlugin is told
    // to exit, so too will all the VideoRenderWorker instances
    if (current_worker_) {
        send_exit(current_worker_.actor(), caf::exit_reason::user_shutdown);
    }
    current_worker_ = utility::UuidActor();
    for (auto &p : queued_jobs_) {
        send_exit(p.actor(), caf::exit_reason::user_shutdown);
    }
    queued_jobs_.clear();
    offscreen_viewport_ = caf::actor();
    colour_pipeline_    = caf::actor();
    StandardPlugin::on_exit();
}

caf::message_handler VideoRenderPlugin::message_handler_extensions() {
    return caf::message_handler(
               {[=](utility::user_start_action_atom) {
                    if (current_worker_) {
                        // cleanup the current worker, which has finished rendering
                        // (or failed)
                        send_exit(current_worker_.actor(), caf::exit_reason::user_shutdown);
                        current_worker_ = utility::UuidActor();
                    }
                    if (!queued_jobs_.empty()) {
                        // pick the next job from the front of the queue and tell
                        // it to start
                        auto p          = queued_jobs_.begin();
                        current_worker_ = *p;
                        queued_jobs_.erase(p);
                        mail(utility::user_start_action_atom_v).send(current_worker_.actor());
                    }
                    if (overall_status_->value() != "Error") {
                        if (current_worker_)
                            overall_status_->set_value("In Progress");
                        else
                            overall_status_->set_value("Done");
                    }
                },
                [=](utility::event_atom,
                    const utility::JsonStore &status,
                    const std::string &ffmpeg_output) {
                    // render workers send us these status update messages. We update
                    // our jobs_status_data_ attribute, which drives the job queue UI
                    // qml stuff

                    try {

                        utility::Uuid job_id = status.at("job_id");

                        if (status.get_or("status_num", 0) == 2) {
                            // job has failed
                            overall_status_->set_value("Error");
                        }

                        if (jobs_status_data_.find(job_id) != jobs_status_data_.end()) {

                            jobs_status_data_[job_id]->set_value(status);

                        } else {

                            // attributes within a given attrs_group must have a unique name. We
                            // use the job id to set the name of this attribute that carries the
                            // render status data for this render job. The UI iterates over the
                            // attr group to create the table of job statuses
                            jobs_status_data_[job_id] =
                                add_json_attribute(status.at("job_id"), "", status);
                            jobs_status_data_[job_id]->expose_in_ui_attrs_group(
                                "video_render_status_attrs");
                        }

                        // ffmpeg output log is put into 'user_data' role. Picked up bu
                        // VideoRenderFFMpegLog.qml
                        jobs_status_data_[job_id]->set_role_data(
                            module::Attribute::UserData, ffmpeg_output);

                    } catch (std::exception &e) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                    }
                }})
        .or_else(StandardPlugin::message_handler_extensions());
}


void VideoRenderPlugin::menu_item_activated(
    const utility::JsonStore &menu_item_data, const std::string &user_data) {
    // use the hotkey handling that's set-up in PlayheadActor

    if (menu_item_data["uuid"].get<utility::Uuid>() == render_menu_item_ ||
        menu_item_data["uuid"].get<utility::Uuid>() == render_menu_item2_) {

        create_qml_item(R"(
            import VideoRenderer 1.0
            VideoRendererDialog {}        
            )");
    }
}

void VideoRenderPlugin::update_ocio_choices(const utility::Uuid &target_render_item_id) {

    if (!dummy_colour_pipeline_) {
        // here we make a 'dummy' colour pipeline - all this is for is to provide us with
        // Display and View attributes that we can expose in the UI, and it will fill out the
        // Display and View attributes based on example media we send it in order to populate
        // the drop-down menus for colour management in the VideoRendererDialog.
        //
        // This way, when the user selected a certain playlist or timeline to render, we can
        // present the usual options for colour management that are specific to that playlist or
        // timeline. Then when the user hits render, we store the Display / View settings that
        // the user specifies ( or the defaults if the user doesn't modify them). We then use
        // those settings at render time to set-up the colour management so that the output is
        // rendered in the desired colour space
        auto colour_pipe_manager =
            system().registry().get<caf::actor>(colour_pipeline_registry);
        mail(colour_pipeline::colour_pipeline_atom_v, "dummy viewport", "dummy window", false)
            .request(colour_pipe_manager, infinite)
            .then(
                [=](caf::actor dummy_colour_pipeline) {
                    dummy_colour_pipeline_ = dummy_colour_pipeline;

                    // Turn off the feature in OCIO plugin where it dynamically picks the OCIO
                    // View per-source ... this is not desirable for rendering where we just
                    // want to render out in the user chosen View
                    anon_mail(
                        colour_pipeline::global_ocio_controls_atom_v, "force_global_view", true)
                        .send(dummy_colour_pipeline_);

                    std::vector<std::string> ui_groups({"video_renderer_ocio_opts"});
                    // Force dummy colour pipeline to expose its Display and View
                    // attributes in the UI Data model called "video_renderer_ocio_opts"
                    // See VideoRendererDialog.qml for how this is then used.
                    anon_mail(
                        module::change_attribute_value_atom_v,
                        "Display",
                        int(module::Attribute::UIDataModels),
                        true,
                        utility::JsonStore(ui_groups))
                        .send(dummy_colour_pipeline_);

                    anon_mail(
                        module::change_attribute_value_atom_v,
                        "View",
                        int(module::Attribute::UIDataModels),
                        true,
                        utility::JsonStore(ui_groups))
                        .send(dummy_colour_pipeline_);

                    update_ocio_choices(target_render_item_id);
                },
                [=](caf::error &err) mutable {
                    spdlog::critical(
                        "{} in plugin {} : {}",
                        __PRETTY_FUNCTION__,
                        Module::name(),
                        to_string(err));
                });
        return;
    }


    scoped_actor sys{system()};

    auto session = utility::request_receive<caf::actor>(
        *sys,
        system().registry().template get<caf::actor>(studio_registry),
        session::session_atom_v);

    // get the playlist/subset/contact sheet/timeline actor that the user wants to render
    mail(session::get_playlist_atom_v, target_render_item_id)
        .request(session, infinite)
        .then(
            [=](caf::actor render_target) {
                // get all the media in that container
                mail(playlist::get_media_atom_v)
                    .request(render_target, infinite)
                    .then(
                        [=](const utility::UuidActorVector &media) {
                            ocio_warnings_->set_value("");
                            auto cfg         = std::make_shared<std::string>();
                            auto options_set = std::make_shared<bool>(false);

                            // loop over all the media in the container
                            for (size_t i = 0; i < media.size(); ++i) {
                                utility::UuidActor m = media[i];
                                if (not m.actor())
                                    continue;
                                anon_mail(playhead::media_source_atom_v, m)
                                    .send(dummy_colour_pipeline_);
                                break;
                            }
                        },

                        [=](caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void VideoRenderPlugin::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    auto attr = get_attribute(attr_uuid);
    if (attr) {
        // std::cerr << attr->as_json().dump(2) << "\n";
    }
    StandardPlugin::attribute_changed(attr_uuid, role);
}

void VideoRenderPlugin::remove_job(const utility::Uuid &job_id) {

    for (auto p = queued_jobs_.begin(); p != queued_jobs_.end(); ++p) {
        if (p->uuid() == job_id) {
            send_exit(p->actor(), caf::exit_reason::user_shutdown);
            queued_jobs_.erase(p);
            break;
        }
    }
    if (current_worker_.uuid() == job_id) {
        send_exit(current_worker_.actor(), caf::exit_reason::user_shutdown);
        current_worker_ = utility::UuidActor();
        anon_mail(utility::user_start_action_atom_v).send(caf::actor_cast<caf::actor>(this));
    }

    for (auto p = jobs_status_data_.begin(); p != jobs_status_data_.end(); ++p) {
        if (p->first == job_id) {
            remove_attribute(p->second->uuid());
            jobs_status_data_.erase(p);
            break;
        }
    }

    if (jobs_status_data_.size() == 1)
        overall_status_->set_value("");
}

void VideoRenderPlugin::make_offscreen_viewport(
    const utility::Uuid qml_item_id, const utility::JsonStore callback_data) {

    // Get the 'StudioUI' which lives in the Qt context and therefore is able to
    // create offscreen viewports for us
    auto studio_ui = system().registry().template get<caf::actor>(studio_ui_registry);

    // tell the studio actor to create an offscreen viewport.
    mail(offscreen_viewport_atom_v, "vid_render_offscreen_viewport", false)
        .request(studio_ui, infinite)
        .then(
            [=](caf::actor offscreen_vp) {
                // this is the offscreen renderer that we asked for below.
                offscreen_viewport_ = offscreen_vp;

                // get the colour pipeline actor from offscreen viewport
                mail(colour_pipeline::colour_pipeline_atom_v)
                    .request(offscreen_viewport_, infinite)
                    .then(
                        [=](caf::actor colour_pipeline) {
                            colour_pipeline_ = colour_pipeline;

                            // Turn off the feature in OCIO plugin where it dynamically picks
                            // the OCIO View per-source ... this is not desirable for rendering
                            // where we just want to render out in the user chosen View
                            anon_mail(
                                colour_pipeline::global_ocio_controls_atom_v,
                                "force_global_view",
                                true)
                                .send(dummy_colour_pipeline_);

                            qml_item_callback(qml_item_id, callback_data);
                        },
                        [=](caf::error &err) mutable {
                            spdlog::critical(
                                "{} in plugin {} : {}",
                                __PRETTY_FUNCTION__,
                                Module::name(),
                                to_string(err));
                        });

                // We COULD create this post-render hook class, this will convert
                // the RGB 16 bit OpenGL framebuffer to RGB 10-10-10 buffer for
                // ffmpeg, to give us decent colour accuracy (though not 12 bit).
                //
                // But we won't, because it doesn't quite work yet. It looks like
                // ffmpeg expect the 10 bit packing to be tight... not 10,10,10
                // packed into 32 bits but into 30 bits
                // viewport::ViewportFramePostProcessorPtr frameGrabber(new
                // RGB10BitFrameGrabber()); anon_mail(frameGrabber).send(offscreen_viewport_);
            },
            [=](caf::error &err) mutable {
                spdlog::critical(
                    "{} in plugin {} : {}",
                    __PRETTY_FUNCTION__,
                    Module::name(),
                    to_string(err));
            });
}


void VideoRenderPlugin::qml_item_callback(
    const utility::Uuid &qml_item_id, const utility::JsonStore &callback_data) {

    // VideoRendererDialog.qml or VideoRendererJobsQueueDialog.qml trigger this
    // callback when they call the 'xstudio_callback' function

    if (callback_data.is_array() && callback_data.size() == 3) {

        try {
            std::string action;
            std::string path;
            bool dummy;
            unpack_json_array(callback_data, action, path, dummy);

            if (action == "quickview_output") {

                mail(session::session_atom_v)
                    .request(
                        system().registry().template get<caf::actor>(global_registry), infinite)
                    .then(
                        [=](caf::actor session) { playback_render_output(path, session); },
                        [=](caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            }

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            return;
        }
        return;

    } else if (callback_data.is_array() && callback_data.size() == 2) {

        try {
            std::string action;
            utility::Uuid uuid;
            unpack_json_array(callback_data, action, uuid);

            if (action == "remove_job") {

                remove_job(uuid);

            } else if (action == "update_ocio_choices") {

                update_ocio_choices(uuid);
            }

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            return;
        }
        return;

    } else if (callback_data.is_array() && callback_data.size() == 15) {

        if (!offscreen_viewport_) {
            make_offscreen_viewport(qml_item_id, callback_data);
            return;
        }

        try {

            // make a worker actor that executes the rendering and sends status
            // updates to the plugin actor (i.e. this class)
            utility::Uuid job_id = utility::Uuid::generate();
            auto worker          = spawn<VideoRenderWorker>(
                job_id,
                callback_data,
                offscreen_viewport_,
                colour_pipeline_,
                caf::actor_cast<caf::actor>(this));

            // link to the worker, so if this plugin exists (when xSTUDIO closes)
            // the worker will be destroyed automatically
            queued_jobs_.push_back(utility::UuidActor(job_id, worker));

            // handler for worker exit (worker self exists when its render has
            // completed or is cancelled)
            monitor(worker, [this, worker](const error &err) {
                if (current_worker_ == worker) {
                    current_worker_ = utility::UuidActor();
                    anon_mail(utility::user_start_action_atom_v)
                        .send(caf::actor_cast<caf::actor>(this));
                }
                for (auto p = queued_jobs_.begin(); p != queued_jobs_.end(); ++p) {
                    if (p->actor() == worker) {
                        queued_jobs_.erase(p);
                        break;
                    }
                }
            });

            if (!current_worker_) {
                // send a message to ourselves to start working through the job queue
                anon_mail(utility::user_start_action_atom_v)
                    .send(caf::actor_cast<caf::actor>(this));
            }


        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

    } else {

        spdlog::warn(
            "{} : callback from qml was passed data that wasn't recognised\n\n{}",
            __PRETTY_FUNCTION__,
            callback_data.dump(2));
    }
}

void VideoRenderPlugin::playback_render_output(
    const std::string path_to_render, caf::actor session) {

    // a bit awkward, because the media might already be loaded into xstudio,
    // if not we *might* need to create a playlist to load it into.

    auto do_load = [=](utility::UuidUuidActor playlist) {
        mail(
            playlist::add_media_atom_v,
            utility::posix_path_to_uri(path_to_render),
            false,
            utility::Uuid())
            .request(playlist.second.actor(), infinite)
            .then(
                [=](const utility::UuidActorVector &r) {
                    anon_mail(ui::open_quickview_window_atom_v, r, "Off").send(session);
                },
                [=](caf::error &err) {

                });
    };

    auto handle_error = [=](caf::error &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
    };

    // get or make a playlist called "Render Outputs" and add the rendered
    // movie to the playlist
    auto get_target_playlist_and_load = [=](caf::actor session) {
        mail(session::get_playlist_atom_v, "Render Outputs")
            .request(session, infinite)
            .then(
                [=](caf::actor check_playlist) {
                    if (!check_playlist) {
                        mail(session::add_playlist_atom_v, "Render Outputs")
                            .request(session, infinite)
                            .then(do_load, handle_error);
                    } else {
                        do_load(utility::UuidUuidActor(
                            utility::Uuid(),
                            utility::UuidActor(utility::Uuid(), check_playlist)));
                    }
                },
                handle_error);
    };


    // search for the media item matching 'path_to_render'
    mail(playlist::get_media_atom_v, utility::posix_path_to_uri(path_to_render))
        .request(session, infinite)
        .then(
            [=](utility::UuidActorVector media) {
                if (media.size()) {
                    utility::UuidActorVector one;
                    one.push_back(media.front());
                    anon_mail(ui::open_quickview_window_atom_v, one, "Off").send(session);
                } else {
                    get_target_playlist_and_load(session);
                }
            },
            [=](caf::error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what()); });
}

VideoRenderPlugin::~VideoRenderPlugin() {}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<VideoRenderPlugin>>(
                VideoRenderPlugin::PLUGIN_UUID,
                "Video Renderer",
                plugin_manager::PluginFlags::PF_VIDEO_OUTPUT,
                false, // not 'resident' .. the StudioUI class instances the viewport plugins
                "Ted Waine",
                "Video Renderer Plugin")}));
}
}