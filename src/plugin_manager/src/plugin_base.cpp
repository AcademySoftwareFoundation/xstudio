// SPDX-License-Identifier: Apache-2.0
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::plugin;

StandardPlugin::StandardPlugin(
    caf::actor_config &cfg, std::string name, const utility::JsonStore &init_settings)
    : caf::event_based_actor(cfg), module::Module(name) {

    utility::print_on_exit(this, name);

    join_studio_events();

    message_handler_ = {

        [=](utility::event_atom, session::session_atom, caf::actor session) {
            session_changed(session);
        },

        [=](utility::event_atom,
            session::session_request_atom,
            const std::string &path,
            const utility::JsonStore &js) {},

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &addr) {
            if (addr == playhead_media_events_group_) {
                playhead_media_events_group_ = caf::actor_addr();
            }
        },

        [=](ui::viewport::prepare_overlay_render_data_atom,
            const media_reader::ImageBufPtr &image,
            const std::string &viewport_name) -> utility::BlindDataObjectPtr {
            return onscreen_render_data(image, viewport_name);
        },

        [=](playhead::show_atom,
            const media_reader::ImageBufDisplaySetPtr &image_set,
            const std::string &viewport_name,
            const bool playing) { 
                __images_going_on_screen(image_set, viewport_name, playing); 
                images_going_on_screen(image_set, viewport_name, playing); 
            },

        [=](ui::viewport::overlay_render_function_atom) -> ViewportOverlayRendererPtr {
            return make_overlay_renderer();
        },

        [=](ui::viewport::pre_render_gpu_hook_atom) -> GPUPreDrawHookPtr {
            return make_pre_draw_gpu_hook();
        },

        [=](bookmark::build_annotation_atom,
            const utility::JsonStore &data) -> result<AnnotationBasePtr> {
            try {
                return build_annotation(data);
                ;
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },

        [=](module::current_viewport_playhead_atom,
            const std::string &viewport_name,
            caf::actor live_playhead) -> bool {
            // this comes from a viewport, if we are an overlay actor
            return true;
        },

        [=](utility::event_atom, playhead::media_atom, caf::actor media_actor) {
            on_screen_media_changed(media_actor);
        },

        [=](utility::event_atom, playhead::play_atom, bool playing) {
            on_playhead_playing_changed(playing);
        },

        [=](utility::event_atom,
            playhead::position_atom,
            const timebase::flicks playhead_position,
            const int playhead_logical_frame,
            const int media_frame,
            const int media_logical_frame,
            const utility::Timecode &timecode) {
        },

        [=](utility::event_atom,
            bookmark::get_bookmarks_atom,
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>> &) {},
        [=](utility::event_atom, utility::event_atom) { join_studio_events(); },
        [=](utility::event_atom,
            ui::viewport::viewport_playhead_atom,
            const std::string &viewport_name,
            caf::actor playhead) {
            // the playhead of the given viewport has changed
        },
        [=](utility::event_atom,
            ui::viewport::viewport_playhead_atom,
            const std::string &viewport_name,
            caf::actor playhead) {
            // the playhead of the given viewport has changed
        },

        [=](utility::event_atom,
            ui::viewport::viewport_playhead_atom,
            caf::actor live_playhead) {
            // this comes from 'global_playhead_events_actor' when the main
            // playhead driving viewports changes
            current_viewed_playhead_changed(live_playhead);
        },

        [=](utility::event_atom,
            playhead::show_atom,
            caf::actor media,
            caf::actor media_source,
            const std::string &viewport_name) {
            // the onscreen media for the given viewport has changed
            on_screen_media_changed(media);
        },
        [=](ui::viewport::turn_off_overlay_interaction_atom, const utility::Uuid requester_id) {
            if (requester_id != uuid()) {
                turn_off_overlay_interaction();
            }
        }};
}

void StandardPlugin::on_exit() {
    playhead_events_actor_ = caf::actor();
}

void StandardPlugin::on_screen_media_changed(caf::actor media) {

    if (!media)
        return;
    request(media, infinite, media::current_media_source_atom_v, media::MT_IMAGE)
        .then(

            [=](utility::UuidActor source) mutable {
                request(source.actor(), infinite, media::media_reference_atom_v)
                    .then(
                        [=](const utility::MediaReference &media_ref) mutable {
                            request(
                                source.actor(),
                                infinite,
                                colour_pipeline::get_colour_pipe_params_atom_v)
                                .then(
                                    [=](utility::JsonStore params) {
                                        on_screen_media_changed(media, media_ref, params);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::debug(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        },
                        [=](error &err) mutable {
                            spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](error &err) mutable {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void StandardPlugin::session_changed(caf::actor session) {
    request(session, infinite, bookmark::get_bookmark_atom_v)
        .then(
            [=](caf::actor bookmark_manager) { bookmark_manager_ = bookmark_manager; },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void StandardPlugin::join_studio_events() {

    scoped_actor sys{system()};
    try {

        if (!system().registry().template get<caf::actor>(studio_registry)) {
            // studio not created yet. Retry in 100ms
            delayed_anon_send(
                caf::actor_cast<caf::actor>(this),
                std::chrono::milliseconds(100),
                utility::event_atom_v,
                utility::event_atom_v);
            return;
        }

        playhead_events_actor_ =
            system().registry().template get<caf::actor>(global_playhead_events_actor);

        // join studio events, so we know when a new session has been created
        auto grp = utility::request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(studio_registry),
            utility::get_event_group_atom_v);

        utility::request_receive<bool>(
            *sys, grp, broadcast::join_broadcast_atom_v, caf::actor_cast<caf::actor>(this));

        session_changed(utility::request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(studio_registry),
            session::session_atom_v));

        anon_send(
            playhead_events_actor_,
            broadcast::join_broadcast_atom_v,
            caf::actor_cast<caf::actor>(this));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void StandardPlugin::__images_going_on_screen(
    const media_reader::ImageBufDisplaySetPtr & image_set,
    const std::string viewport_name,
    const bool playhead_playing
) {

    // skip viewports whose name doesn't start with 'viewport' - this lets us
    // ignore offscreen and quickview viewports
    if (viewport_name.find("viewport") != 0) return;

    if (image_set && image_set->hero_image().frame_id().source_uuid() != last_source_uuid_[viewport_name]) {

        last_source_uuid_[viewport_name] = image_set->hero_image().frame_id().source_uuid();
        auto media_source = caf::actor_cast<caf::actor>(image_set->hero_image().frame_id().actor_addr());
        request(media_source, infinite, utility::parent_atom_v).then(
            [=](caf::actor media_actor) {
                on_screen_media_changed(media_actor);
            },
            [=](caf::error &err) {});
    }
}


void StandardPlugin::listen_to_playhead_events(const bool listen) {

    // fetch the current viewed playhead from the viewport so we can 'listen' to it
    // for position changes, current media changes etc.

    // join the global playhead events group - this tells us when the playhead that should
    // be on screen changes, among other things
    joined_playhead_events_ = listen;
    if (listen) {

        anon_send(
            playhead_events_actor_,
            broadcast::join_broadcast_atom_v,
            caf::actor_cast<caf::actor>(this));

        // this call means we get event messages when the on-screen media
        // changes
        request(playhead_events_actor_, infinite, ui::viewport::viewport_playhead_atom_v)
            .then(
                [=](caf::actor ph) { current_viewed_playhead_changed(ph); },
                [=](caf::error &err) {

                });
    } else {

        anon_send(
            playhead_events_actor_,
            broadcast::leave_broadcast_atom_v,
            caf::actor_cast<caf::actor>(this));
        joined_playhead_events_ = false;
        current_viewed_playhead_changed(caf::actor());
    }
}


void StandardPlugin::start_stop_playback(const std::string viewport_name, bool play) {

    scoped_actor sys{system()};
    try {

        auto playhead = utility::request_receive<caf::actor>(
            *sys,
            playhead_events_actor_,
            ui::viewport::viewport_playhead_atom_v,
            viewport_name);
        if (playhead) {
            utility::request_receive<bool>(*sys, playhead, playhead::play_atom_v, play);
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void StandardPlugin::set_viewport_cursor(const std::string cursor_name) {

    anon_send(playhead_events_actor_, ui::viewport::viewport_cursor_atom_v, cursor_name);
}


void StandardPlugin::current_viewed_playhead_changed(caf::actor viewed_playhead) {

    // here we join the playhead events group of the new playhead that is
    // attached to the viewport

    auto self = caf::actor_cast<caf::actor>(this);
    if (caf::actor_cast<caf::actor>(playhead_media_events_group_)) {
        utility::leave_broadcast(
            this, caf::actor_cast<caf::actor>(playhead_media_events_group_));
    }

    if (viewed_playhead) {

        request(viewed_playhead, infinite, playhead::media_events_group_atom_v)
            .then(
                [=](caf::actor playhead_media_events_broadcast_group) {
                    utility::join_broadcast(this, playhead_media_events_broadcast_group);
                    playhead_media_events_group_ =
                        caf::actor_cast<caf::actor_addr>(playhead_media_events_broadcast_group);

                    // this kicks the playhead into re-broadcasting us an updated list of
                    // bookmarks for this new playhead
                    anon_send(viewed_playhead, bookmark::get_bookmarks_atom_v);

                    // this kicks the playhead to re-broadcast its position, media frame and
                    // so-on
                    anon_send(viewed_playhead, playhead::redraw_viewport_atom_v);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

        request(viewed_playhead, infinite, playhead::media_atom_v)
            .then(
                [=](caf::actor current_media_actor) {
                    on_screen_media_changed(current_media_actor);
                },
                [=](error &err) mutable {
                    // spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

    }
}

void StandardPlugin::qml_viewport_overlay_code(const std::string &code) {
    if (!viewport_overlay_qml_code_) {
        viewport_overlay_qml_code_ = add_boolean_attribute(
            Module::name() + " Overlay", Module::name() + " Overlay", true);
        viewport_overlay_qml_code_->set_role_data(module::Attribute::QmlCode, code);
        viewport_overlay_qml_code_->expose_in_ui_attrs_group("viewport_overlay_plugins");
    } else {
        viewport_overlay_qml_code_->set_role_data(module::Attribute::QmlCode, code);
    }
}

utility::Uuid StandardPlugin::create_bookmark_on_frame(
    const media::AVFrameID &frame_details,
    const std::string &bookmark_subject,
    const bookmark::BookmarkDetail &detail,
    const bool bookmark_entire_duration) {

    utility::Uuid result;

    scoped_actor sys{system()};
    try {

        auto media_source = caf::actor_cast<caf::actor>(frame_details.actor_addr());
        auto media = utility::request_receive<caf::actor>(*sys, media_source, utility::parent_atom_v);

        if (media) {

            auto media_uuid = frame_details.media_uuid();

            auto media_ref =
                utility::request_receive<std::pair<utility::Uuid, utility::MediaReference>>(
                    *sys, media, media::media_reference_atom_v, media_uuid)
                    .second;

            auto new_bookmark = utility::request_receive<utility::UuidActor>(
                *sys,
                bookmark_manager_,
                bookmark::add_bookmark_atom_v,
                utility::UuidActor(media_uuid, media));

            bookmark::BookmarkDetail bmd = detail;

            if (bookmark_entire_duration) {

                bmd.start_    = timebase::k_flicks_low;
                bmd.duration_ = timebase::k_flicks_max;

            } else {

                // this will make a bookmark of single frame duration on the current frame
                bmd.start_    = (frame_details.frame()-frame_details.first_frame())*media_ref.rate().to_flicks();
                bmd.duration_ = timebase::flicks(0);
            }

            bmd.subject_ = bookmark_subject;

            if (!bmd.category_) {

                auto default_category = utility::request_receive<std::string>(
                    *sys, bookmark_manager_, bookmark::default_category_atom_v);
                bmd.category_ = default_category;
            }

            utility::request_receive<bool>(
                *sys, new_bookmark.actor(), bookmark::bookmark_detail_atom_v, bmd);

            result = new_bookmark.uuid();
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


utility::Uuid StandardPlugin::create_bookmark_on_current_media(
    const std::string &viewport_name,
    const std::string &bookmark_subject,
    const bookmark::BookmarkDetail &detail,
    const bool bookmark_entire_duration) {

    utility::Uuid result;

    scoped_actor sys{system()};
    try {


        caf::actor playhead;
        try {
            auto vp = utility::request_receive<caf::actor>(
                *sys, playhead_events_actor_, ui::viewport::viewport_atom_v, viewport_name);
            playhead = utility::request_receive<caf::actor>(
                *sys, vp, ui::viewport::viewport_playhead_atom_v);
        } catch (...) {
            // couldn't find the specified viewport or its playhead. Instead
            // get the 'global active' playhead from the global_playhead_events_actor
            playhead = utility::request_receive<caf::actor>(
                *sys, playhead_events_actor_, ui::viewport::viewport_playhead_atom_v);
        }

        if (!playhead) {
            throw std::runtime_error("Couldn't find viewport/playhead.");
        }

        auto media =
            utility::request_receive<caf::actor>(*sys, playhead, playhead::media_atom_v);

        if (media) {

            auto media_uuid =
                utility::request_receive<utility::Uuid>(*sys, media, utility::uuid_atom_v);

            auto media_ref =
                utility::request_receive<std::pair<utility::Uuid, utility::MediaReference>>(
                    *sys, media, media::media_reference_atom_v, media_uuid)
                    .second;

            auto new_bookmark = utility::request_receive<utility::UuidActor>(
                *sys,
                bookmark_manager_,
                bookmark::add_bookmark_atom_v,
                utility::UuidActor(media_uuid, media));

            bookmark::BookmarkDetail bmd = detail;

            if (bookmark_entire_duration) {

                bmd.start_    = timebase::k_flicks_low;
                bmd.duration_ = timebase::k_flicks_max;

            } else {
                auto media_frame =
                    utility::request_receive<int>(*sys, playhead, playhead::media_frame_atom_v);

                // this will make a bookmark of single frame duration on the current frame
                bmd.start_    = (media_frame)*media_ref.rate().to_flicks();
                bmd.duration_ = timebase::flicks(0);
            }

            bmd.subject_ = bookmark_subject;

            if (!bmd.category_) {

                auto default_category = utility::request_receive<std::string>(
                    *sys, bookmark_manager_, bookmark::default_category_atom_v);
                bmd.category_ = default_category;
            }

            utility::request_receive<bool>(
                *sys, new_bookmark.actor(), bookmark::bookmark_detail_atom_v, bmd);

            result = new_bookmark.uuid();
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

void StandardPlugin::cancel_other_drawing_tools() {

    // get all viewports
    request(playhead_events_actor_, infinite, ui::viewport::viewport_atom_v)
        .then(
            [=](const std::vector<caf::actor> &viewports) {
                for (auto &vp : viewports) {
                    send(vp, ui::viewport::turn_off_overlay_interaction_atom_v, uuid());
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

utility::UuidList
StandardPlugin::get_bookmarks_on_current_media(const std::string &viewport_name) {

    utility::UuidList result;

    scoped_actor sys{system()};
    try {
        auto vp = utility::request_receive<caf::actor>(
            *sys, playhead_events_actor_, ui::viewport::viewport_atom_v, viewport_name);
        auto playhead = utility::request_receive<caf::actor>(
            *sys, vp, ui::viewport::viewport_playhead_atom_v);
        auto media_actor =
            utility::request_receive<caf::actor>(*sys, playhead, playhead::media_atom_v);

        result = utility::request_receive<utility::UuidList>(
            *sys, media_actor, bookmark::get_bookmarks_atom_v);
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}

bookmark::BookmarkDetail StandardPlugin::get_bookmark_detail(const utility::Uuid &bookmark_id) {

    bookmark::BookmarkDetail result;

    scoped_actor sys{system()};
    try {
        auto uuid_actor = utility::request_receive<utility::UuidActor>(
            *sys, bookmark_manager_, bookmark::get_bookmark_atom_v, bookmark_id);
        result = utility::request_receive<bookmark::BookmarkDetail>(
            *sys, uuid_actor.actor(), bookmark::bookmark_detail_atom_v);
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}

AnnotationBasePtr StandardPlugin::get_bookmark_annotation(const utility::Uuid &bookmark_id) {

    AnnotationBasePtr result;

    scoped_actor sys{system()};
    try {
        auto uuid_actor = utility::request_receive<utility::UuidActor>(
            *sys, bookmark_manager_, bookmark::get_bookmark_atom_v, bookmark_id);
        result = utility::request_receive<AnnotationBasePtr>(
            *sys, uuid_actor.actor(), bookmark::get_annotation_atom_v);
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}

void StandardPlugin::update_bookmark_annotation(
    const utility::Uuid bookmark_id,
    AnnotationBasePtr annotation_data,
    const bool annotation_is_empty) {

    request(bookmark_manager_, infinite, bookmark::get_bookmark_atom_v, bookmark_id)
        .then(
            [=](utility::UuidActor &bm) {
                if (!annotation_is_empty) {

                    anon_send(bm.actor(), bookmark::add_annotation_atom_v, annotation_data);

                } else {

                    request(bm.actor(), infinite, bookmark::bookmark_detail_atom_v)
                        .then(
                            [=](const bookmark::BookmarkDetail &detail) {
                                if (!detail.note_ || *(detail.note_) == "") {
                                    // bookmark has no note, and the annotation is empty. Delete
                                    // the bookmark altogether
                                    request(
                                        bookmark_manager_,
                                        infinite,
                                        bookmark::remove_bookmark_atom_v,
                                        bookmark_id);

                                } else {
                                    anon_send(
                                        bm.actor(),
                                        bookmark::add_annotation_atom_v,
                                        annotation_data);
                                }
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void StandardPlugin::update_bookmark_detail(
    const utility::Uuid bookmark_id, const bookmark::BookmarkDetail &bmd) {

    scoped_actor sys{system()};
    try {
        auto uuid_actor = utility::request_receive<utility::UuidActor>(
            *sys, bookmark_manager_, bookmark::get_bookmark_atom_v, bookmark_id);
        auto result = utility::request_receive<bool>(
            *sys, uuid_actor.actor(), bookmark::bookmark_detail_atom_v, bmd);
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void StandardPlugin::remove_bookmark(const utility::Uuid &bookmark_id) {

    request(bookmark_manager_, infinite, bookmark::remove_bookmark_atom_v, bookmark_id);
}
