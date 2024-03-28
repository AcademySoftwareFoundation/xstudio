// SPDX-License-Identifier: Apache-2.0
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/media_reader/image_buffer.hpp"

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
            const bool offscreen) -> utility::BlindDataObjectPtr {
            return prepare_overlay_data(image, offscreen);
        },

        [=](ui::viewport::prepare_overlay_render_data_atom,
            const media_reader::ImageBufPtr &image,
            const std::string &viewport_name) -> utility::BlindDataObjectPtr {
            return onscreen_render_data(image, viewport_name);
        },

        [=](playhead::show_atom,
            const std::vector<media_reader::ImageBufPtr> &images,
            const std::string &viewport_name,
            const bool playing) { images_going_on_screen(images, viewport_name, playing); },

        [=](ui::viewport::overlay_render_function_atom, const int viewer_index)
            -> ViewportOverlayRendererPtr { return make_overlay_renderer(viewer_index); },

        [=](ui::viewport::pre_render_gpu_hook_atom, const int viewer_index)
            -> GPUPreDrawHookPtr { return make_pre_draw_gpu_hook(viewer_index); },

        [=](bookmark::build_annotation_atom,
            const utility::JsonStore &data) -> result<AnnotationBasePtr> {
            try {
                return build_annotation(data);
                ;
            } catch (std::exception &e) {
                return caf::make_error(xstudio_error::error, e.what());
            }
        },

        [=](module::current_viewport_playhead_atom, caf::actor_addr live_playhead) -> bool {
            current_viewed_playhead_changed(live_playhead);
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
            on_screen_frame_changed(
                playhead_position,
                playhead_logical_frame,
                media_frame,
                media_logical_frame,
                timecode);

            playhead_logical_frame_ = playhead_logical_frame;
        },

        [=](utility::event_atom,
            bookmark::get_bookmarks_atom,
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>> &) {},
        [=](utility::event_atom, utility::event_atom) { join_studio_events(); }};
}

void StandardPlugin::on_screen_media_changed(caf::actor media) {

    if (media) {
        request(media, infinite, utility::name_atom_v)
            .then(
                [=](const std::string name) mutable {
                    request(
                        media, infinite, media::current_media_source_atom_v, media::MT_IMAGE)
                        .then(

                            [=](utility::UuidActor source) mutable {
                                request(source.actor(), infinite, media::media_reference_atom_v)
                                    .then(

                                        [=](const utility::MediaReference &media_ref) mutable {
                                            on_screen_media_changed(media, media_ref, name);
                                        },
                                        [=](error &err) mutable {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        });
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
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

        // fetch the current viewed playhead from the viewport so we can 'listen' to it
        // for position changes, current media changes etc.
        auto playhead_events_actor =
            system().registry().template get<caf::actor>(global_playhead_events_actor);
        if (playhead_events_actor) {
            request(playhead_events_actor, infinite, ui::viewport::viewport_playhead_atom_v)
                .then(
                    [=](caf::actor playhead) {
                        current_viewed_playhead_changed(
                            caf::actor_cast<caf::actor_addr>(playhead));
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


void StandardPlugin::current_viewed_playhead_changed(caf::actor_addr viewed_playhead_addr) {

    // here we join the playhead events group of the new playhead that is
    // attached to the viewport

    auto viewed_playhead      = caf::actor_cast<caf::actor>(viewed_playhead_addr);
    active_viewport_playhead_ = viewed_playhead_addr;

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

        // make sure we have synced the bookmarks info from the playhead
        try {
            scoped_actor sys{system()};
            playhead_logical_frame_ = utility::request_receive<int>(
                *sys, viewed_playhead, playhead::logical_frame_atom_v);
        } catch (std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void StandardPlugin::qml_viewport_overlay_code(const std::string &code) {
    if (!viewport_overlay_qml_code_) {
        viewport_overlay_qml_code_ = add_qml_code_attribute("OverlayCode", code);
        viewport_overlay_qml_code_->expose_in_ui_attrs_group("viewport_overlay_plugins");
    } else {
        viewport_overlay_qml_code_->set_value(code);
    }
}

utility::Uuid StandardPlugin::create_bookmark_on_current_media(
    const std::string &viewport_name,
    const std::string &bookmark_subject,
    const bookmark::BookmarkDetail &detail,
    const bool bookmark_entire_duration) {

    utility::Uuid result;

    scoped_actor sys{system()};
    auto ph_events = system().registry().template get<caf::actor>(global_playhead_events_actor);
    try {
        auto vp = utility::request_receive<caf::actor>(
            *sys, ph_events, ui::viewport::viewport_atom_v, viewport_name);
        auto playhead_addr = utility::request_receive<caf::actor_addr>(
            *sys, vp, ui::viewport::viewport_playhead_atom_v);
        auto playhead = caf::actor_cast<caf::actor>(playhead_addr);

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

void StandardPlugin::update_bookmark_annotation(
    const utility::Uuid bookmark_id,
    std::shared_ptr<bookmark::AnnotationBase> annotation_data,
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
    request(bookmark_manager_, infinite, bookmark::get_bookmark_atom_v, bookmark_id)
        .then(
            [=](utility::UuidActor &bm) {
                anon_send(bm.actor(), bookmark::bookmark_detail_atom_v, bmd);
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}
