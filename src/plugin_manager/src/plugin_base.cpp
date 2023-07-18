// SPDX-License-Identifier: Apache-2.0
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/media_reader/image_buffer.hpp"

using namespace xstudio;
using namespace xstudio::plugin;

StandardPlugin::StandardPlugin(
    caf::actor_config &cfg, std::string name, const utility::JsonStore &init_settings)
    : caf::event_based_actor(cfg), module::Module(name) {

    scoped_actor sys{system()};
    try {

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
            return prepare_render_data(image, offscreen);
        },
        [=](ui::viewport::overlay_render_function_atom, const int viewer_index)
            -> ViewportOverlayRendererPtr { return make_overlay_renderer(viewer_index); },
        [=](bookmark::build_annotation_atom, const utility::JsonStore &data)
            -> result<std::shared_ptr<bookmark::AnnotationBase>> {
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

            check_if_onscreen_bookmarks_have_changed(playhead_logical_frame);

            playhead_logical_frame_ = playhead_logical_frame;
        },

        [=](utility::event_atom,
            bookmark::get_bookmarks_atom,
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                &bookmark_frames_ranges) {
            bookmark_frame_ranges_ = bookmark_frames_ranges;
            check_if_onscreen_bookmarks_have_changed(playhead_logical_frame_, true);
        }};
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


void StandardPlugin::check_if_onscreen_bookmarks_have_changed(
    const int media_frame, const bool force_update) {

    auto t0 = utility::clock::now();

    utility::UuidList onscreen_bookmarks;
    for (const auto &a : bookmark_frame_ranges_) {
        const int in  = std::get<2>(a);
        const int out = std::get<3>(a);

        if (in <= media_frame && out >= media_frame) {
            onscreen_bookmarks.push_back(std::get<0>(a));
        }
    }
    if (onscreen_bookmarks.empty()) {
        onscreen_bookmarks.push_back(utility::Uuid());
    }

    if (onscreen_bookmarks != onscreen_bookmarks_) {
        onscreen_bookmarks_ = onscreen_bookmarks;
    } else if (!force_update) {
        return;
    }

    auto annotations =
        std::make_shared<std::vector<std::shared_ptr<bookmark::AnnotationBase>>>();

    if (onscreen_bookmarks_.size() == 1 && (*onscreen_bookmarks_.begin()).is_null()) {

        on_screen_annotation_changed(*(annotations.get()));
        return;
    }

    if (!bookmark_manager_)
        return;
    request(bookmark_manager_, infinite, bookmark::get_bookmark_atom_v, onscreen_bookmarks_)
        .then(
            [=](std::vector<utility::UuidActor> curr_bookmarks) mutable {
                for (auto &ua : curr_bookmarks) {
                    request(ua.actor(), infinite, bookmark::get_annotation_atom_v)
                        .then(
                            [=](std::shared_ptr<bookmark::AnnotationBase> annotation) mutable {
                                annotations->push_back(annotation);
                                if (annotations->size() == curr_bookmarks.size()) {
                                    on_screen_annotation_changed(*(annotations.get()));
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

void StandardPlugin::push_annotation_to_bookmark(
    std::shared_ptr<bookmark::AnnotationBase> annotation) {
    if (!bookmark_manager_)
        return;
    scoped_actor sys{system()};
    // loop over bookmarks to clear
    try {

        auto curr_bookmark = utility::request_receive<utility::UuidActor>(
            *sys, bookmark_manager_, bookmark::get_bookmark_atom_v, annotation->bookmark_uuid_);

        utility::request_receive<bool>(
            *sys, curr_bookmark.actor(), bookmark::add_annotation_atom_v, annotation);

        // kick the playhead to rebroadcast the bookmarks for the current frame
        auto playhead = caf::actor_cast<caf::actor>(active_viewport_playhead_);
        if (playhead) {
            anon_send(playhead, bookmark::get_bookmarks_atom_v);
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void StandardPlugin::restore_annotations_and_bookmarks(
    const std::map<utility::Uuid, utility::JsonStore> &bookmarks_data) {
    if (!bookmark_manager_)
        return;

    scoped_actor sys{system()};
    for (const auto &p : bookmarks_data) {

        const utility::Uuid bookmark_uuid                    = p.first;
        const utility::JsonStore bookmark_serialisation_data = p.second;

        try {
            // this call will create a new bookmark using the serialised data,
            // unless the bookmark already exists - if it does exist it updates
            // the bookmark's annotation data with the serialisation data
            utility::request_receive<utility::UuidActor>(
                *sys,
                bookmark_manager_,
                bookmark::add_bookmark_atom_v,
                bookmark_uuid,
                bookmark_serialisation_data);
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

std::map<utility::Uuid, utility::JsonStore>
StandardPlugin::clear_annotations_and_bookmarks(std::vector<utility::Uuid> bookmark_ids) {
    std::map<utility::Uuid, utility::JsonStore> result;
    if (!bookmark_manager_)
        return result;
    scoped_actor sys{system()};
    // loop over bookmarks to clear
    for (const auto &bm_uuid : bookmark_ids) {

        try {
            // serialise so we can undo ..
            auto bookmark_serialise_data = utility::request_receive<utility::JsonStore>(
                *sys, bookmark_manager_, utility::serialise_atom_v, bm_uuid);

            // get bookmark detail
            auto bm_detail = utility::request_receive<bookmark::BookmarkDetail>(
                *sys, bookmark_manager_, bookmark::bookmark_detail_atom_v, bm_uuid);

            result[bm_uuid] = bookmark_serialise_data;

            if (!bm_detail.note_ || *(bm_detail.note_) == "") {
                // note is empty, so we want to delete the note entirely
                anon_send(bookmark_manager_, bookmark::remove_bookmark_atom_v, bm_uuid);
            } else {

                request(bookmark_manager_, infinite, bookmark::get_bookmark_atom_v, bm_uuid)
                    .then(
                        [=](utility::UuidActor curr_bookmark) {
                            // push an empty annotation
                            anon_send(
                                curr_bookmark.actor(),
                                bookmark::add_annotation_atom_v,
                                std::shared_ptr<bookmark::AnnotationBase>());
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            }

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
    return result;
}

utility::Uuid StandardPlugin::create_bookmark_on_current_frame(bookmark::BookmarkDetail bmd) {

    utility::Uuid result;
    try {
        scoped_actor sys{system()};
        auto playhead = caf::actor_cast<caf::actor>(active_viewport_playhead_);
        if (playhead) {
            auto media =
                utility::request_receive<caf::actor>(*sys, playhead, playhead::media_atom_v);
            if (media) {
                auto media_uuid =
                    utility::request_receive<utility::Uuid>(*sys, media, utility::uuid_atom_v);
                auto new_bookmark = utility::request_receive<utility::UuidActor>(
                    *sys,
                    bookmark_manager_,
                    bookmark::add_bookmark_atom_v,
                    utility::UuidActor(media_uuid, media));

                if (!bmd.category_) {

                    auto default_category = utility::request_receive<std::string>(
                        *sys, bookmark_manager_, bookmark::default_category_atom_v);
                    bmd.category_ = default_category;
                }

                utility::request_receive<bool>(
                    *sys, new_bookmark.actor(), bookmark::bookmark_detail_atom_v, bmd);

                result = new_bookmark.uuid();
                onscreen_bookmarks_.clear();
                onscreen_bookmarks_.push_back(new_bookmark.uuid());

                // sync our list of bookmarks so that it includes the new one
                // that we have just created
                auto playhead = caf::actor_cast<caf::actor>(active_viewport_playhead_);
                if (playhead) {
                    try {

                        scoped_actor sys{system()};
                        bookmark_frame_ranges_ = utility::request_receive<
                            std::vector<std::tuple<utility::Uuid, std::string, int, int>>>(
                            *sys, playhead, bookmark::get_bookmark_atom_v);

                    } catch (std::exception &e) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                    }
                }
            }
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}

std::shared_ptr<bookmark::AnnotationBase>
StandardPlugin::fetch_annotation(const utility::Uuid &bookmark_uuid) {

    std::shared_ptr<bookmark::AnnotationBase> r;
    if (bookmark_manager_) {

        scoped_actor sys{system()};
        try {
            auto bm = utility::request_receive<utility::UuidActor>(
                *sys, bookmark_manager_, bookmark::get_bookmark_atom_v, bookmark_uuid);

            r = utility::request_receive<std::shared_ptr<bookmark::AnnotationBase>>(
                *sys, bm.actor(), bookmark::get_annotation_atom_v);

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
    return r;
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
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

        // make sure we have synced the bookmarks info from the playhead
        try {
            scoped_actor sys{system()};
            playhead_logical_frame_ = utility::request_receive<int>(
                *sys, viewed_playhead, playhead::logical_frame_atom_v);
            bookmark_frame_ranges_ = utility::request_receive<
                std::vector<std::tuple<utility::Uuid, std::string, int, int>>>(
                *sys, viewed_playhead, bookmark::get_bookmark_atom_v);
            check_if_onscreen_bookmarks_have_changed(playhead_logical_frame_);
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
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