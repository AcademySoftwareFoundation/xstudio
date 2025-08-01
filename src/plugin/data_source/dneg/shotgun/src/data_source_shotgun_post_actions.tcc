// SPDX-License-Identifier: Apache-2.0

template <typename T>
void ShotgunDataSourceActor<T>::create_playlist_notes(
    caf::typed_response_promise<utility::JsonStore> rp,
    const utility::JsonStore &notes,
    const utility::Uuid &playlist_uuid) {

    const std::string ui(R"(
        import xStudio 1.0
        import QtQuick 2.14
        XsLabel {
            anchors.fill: parent
            font.pixelSize: XsStyle.popupControlFontSize*1.2
            verticalAlignment: Text.AlignVCenter
            font.weight: Font.Bold
            color: palette.highlight
            text: "SG"
        }
    )");

    try {
        scoped_actor sys{system()};

        // get session
        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto bookmarks =
            request_receive<caf::actor>(*sys, session, bookmark::get_bookmark_atom_v);

        auto tags = request_receive<caf::actor>(*sys, session, xstudio::tag::get_tag_atom_v);

        auto count   = std::make_shared<int>(notes.size());
        auto failed  = std::make_shared<int>(0);
        auto succeed = std::make_shared<int>(0);

        auto offscreen_renderer =
            system().registry().template get<caf::actor>(offscreen_viewport_registry);
        auto thumbnail_manager =
            system().registry().template get<caf::actor>(thumbnail_manager_registry);

        for (const auto &j : notes) {
            // need to capture result to embed in playlist and add any media..
            // spdlog::warn("{}", j["payload"].dump(2));
            request(
                shotgun_,
                infinite,
                shotgun_create_entity_atom_v,
                "notes",
                utility::JsonStore(j["payload"]))
                .then(
                    [=](const JsonStore &result) mutable {
                        (*count)--;
                        try {
                            // "errors": [
                            //   {
                            //     "status": null
                            //   }
                            // ]
                            if (not result.at("errors")[0].at("status").is_null())
                                throw std::runtime_error(result["errors"].dump(2));

                            // get new playlist id..
                            auto note_id = result.at("data").at("id").template get<int>();
                            // we have a note...
                            if (not j["has_annotation"].empty()) {
                                for (const auto &anno : j["has_annotation"]) {
                                    request(
                                        session,
                                        infinite,
                                        playlist::get_media_atom_v,
                                        utility::Uuid(anno["media_uuid"]))
                                        .then(
                                            [=](const caf::actor &media_actor) mutable {
                                                // spdlog::warn("render annotation {}",
                                                // anno["media_frame"].get<int>());
                                                request(
                                                    offscreen_renderer,
                                                    infinite,
                                                    ui::viewport::
                                                        render_viewport_to_image_atom_v,
                                                    media_actor,
                                                    anno["media_frame"].get<int>(),
                                                    thumbnail::THUMBNAIL_FORMAT::TF_RGB24,
                                                    0,
                                                    true,
                                                    true)
                                                    .then(
                                                        [=](const thumbnail::ThumbnailBufferPtr
                                                                &tnail) {
                                                            // got buffer. convert to jpg..
                                                            request(
                                                                thumbnail_manager,
                                                                infinite,
                                                                media_reader::
                                                                    get_thumbnail_atom_v,
                                                                tnail)
                                                                .then(
                                                                    [=](const std::vector<
                                                                        std::byte>
                                                                            &jpgbuf) mutable {
                                                                        // final step...
                                                                        auto title = std::
                                                                            string(fmt::format(
                                                                                "{}_{}.jpg",
                                                                                anno["media_"
                                                                                     "name"]
                                                                                    .get<
                                                                                        std::
                                                                                            string>(),
                                                                                anno["timecode_"
                                                                                     "frame"]
                                                                                    .get<
                                                                                        int>()));
                                                                        request(
                                                                            shotgun_,
                                                                            infinite,
                                                                            shotgun_upload_atom_v,
                                                                            "note",
                                                                            note_id,
                                                                            "",
                                                                            title,
                                                                            jpgbuf,
                                                                            "image/jpeg")
                                                                            .then(
                                                                                [=](const bool) {
                                                                                },
                                                                                [=](const error &
                                                                                        err) mutable {
                                                                                    spdlog::warn(
                                                                                        "{} "
                                                                                        "Failed"
                                                                                        " uploa"
                                                                                        "d of "
                                                                                        "annota"
                                                                                        "tion "
                                                                                        "{}",
                                                                                        __PRETTY_FUNCTION__,
                                                                                        to_string(
                                                                                            err));
                                                                                }

                                                                            );
                                                                    },
                                                                    [=](const error
                                                                            &err) mutable {
                                                                        spdlog::warn(
                                                                            "{} Failed jpeg "
                                                                            "conversion {}",
                                                                            __PRETTY_FUNCTION__,
                                                                            to_string(err));
                                                                    });
                                                        },
                                                        [=](const error &err) mutable {
                                                            spdlog::warn(
                                                                "{} Failed render annotation "
                                                                "{}",
                                                                __PRETTY_FUNCTION__,
                                                                to_string(err));
                                                        });
                                            },
                                            [=](const error &err) mutable {
                                                spdlog::warn(
                                                    "{} Failed get media {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                            });
                                }
                            }

                            // spdlog::warn("note {}", result.dump(2));
                            // send json to note..
                            anon_send(
                                bookmarks,
                                json_store::set_json_atom_v,
                                utility::Uuid(j["bookmark_uuid"]),
                                utility::JsonStore(result.at("data")),
                                ShotgunMetadataPath + "/note");

                            xstudio::tag::Tag t;
                            t.set_type("Decorator");
                            t.set_data(ui);
                            t.set_link(utility::Uuid(j["bookmark_uuid"]));
                            t.set_unique(to_string(t.link()) + t.type() + t.data());

                            anon_send(tags, xstudio::tag::add_tag_atom_v, t);

                            // update shotgun versions from our source playlist.
                            // return the result..
                            // update_playlist_versions(rp, playlist_uuid, playlist_id);
                            (*succeed)++;
                        } catch (const std::exception &err) {
                            (*failed)++;
                            spdlog::warn(
                                "{} {} {}", __PRETTY_FUNCTION__, err.what(), result.dump(2));
                        }

                        if (not(*count)) {
                            auto jsn = JsonStore(R"({"data": {"status": ""}})"_json);
                            jsn["data"]["status"] = std::string(fmt::format(
                                "Successfully published {} / {} notes.",
                                *succeed,
                                (*failed) + (*succeed)));
                            rp.deliver(jsn);
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn(
                            "Failed create note entity {} {}",
                            __PRETTY_FUNCTION__,
                            to_string(err));
                        (*count)--;
                        (*failed)++;

                        if (not(*count)) {
                            auto jsn = JsonStore(R"({"data": {"status": ""}})"_json);
                            jsn["data"]["status"] = std::string(fmt::format(
                                "Successfully published {} / {} notes.",
                                *succeed,
                                (*failed) + (*succeed)));
                            rp.deliver(jsn);
                        }
                    });
        }

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::create_playlist(
    caf::typed_response_promise<JsonStore> rp, const utility::JsonStore &js) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.
    try {

        scoped_actor sys{system()};

        auto playlist_uuid = Uuid(js["playlist_uuid"]);
        auto project_id    = js["project_id"].template get<int>();
        auto code          = js["code"].template get<std::string>();
        auto loc           = js["location"].template get<std::string>();
        auto playlist_type = js["playlist_type"].template get<std::string>();

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        auto jsn = R"({
            "project":{ "type": "Project", "id":null },
            "code": null,
            "sg_location": "unknown",
            "sg_type": "Dailies",
            "sg_date_and_time": null
         })"_json;

        jsn["project"]["id"]    = project_id;
        jsn["code"]             = code;
        jsn["sg_location"]      = loc;
        jsn["sg_type"]          = playlist_type;
        jsn["sg_date_and_time"] = date_time_and_zone();

        // "2021-08-18T19:00:00Z"

        // need to capture result to embed in playlist and add any media..
        request(
            shotgun_,
            infinite,
            shotgun_create_entity_atom_v,
            "playlists",
            utility::JsonStore(jsn))
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        // get new playlist id..
                        auto playlist_id = result.at("data").at("id").template get<int>();
                        // update shotgun versions from our source playlist.
                        // return the result..
                        update_playlist_versions(rp, playlist_uuid, playlist_id);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, result.dump(2));
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::rename_tag(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int tag_id,
    const std::string &value) {

    // as this is an update, we have to pull current list and then add / push it back.. (I
    // THINK)
    try {

        scoped_actor sys{system()};

        auto payload    = R"({"name": null})"_json;
        payload["name"] = value;

        // send update request..
        request(
            shotgun_,
            infinite,
            shotgun_update_entity_atom_v,
            "Tag",
            tag_id,
            utility::JsonStore(payload),
            std::vector<std::string>({"id"}))
            .then(
                [=](const JsonStore &result) mutable { rp.deliver(result); },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


template <typename T>
void ShotgunDataSourceActor<T>::remove_entity_tag(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &entity,
    const int entity_id,
    const int tag_id) {

    // as this is an update, we have to pull current list and then add / push it back.. (I
    // THINK)
    try {

        scoped_actor sys{system()};
        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            shotgun_entity_atom_v,
            entity,
            entity_id,
            std::vector<std::string>({"tags"}))
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        auto current_tags =
                            result.at("data").at("relationships").at("tags").at("data");
                        for (auto it = current_tags.begin(); it != current_tags.end(); ++it) {
                            if (it->at("id") == tag_id) {
                                current_tags.erase(it);
                                break;
                            }
                        }

                        auto payload    = R"({"tags": []})"_json;
                        payload["tags"] = current_tags;

                        // send update request..
                        request(
                            shotgun_,
                            infinite,
                            shotgun_update_entity_atom_v,
                            entity,
                            entity_id,
                            utility::JsonStore(payload),
                            std::vector<std::string>({"id", "code", "tags"}))
                            .then(
                                [=](const JsonStore &result) mutable { rp.deliver(result); },
                                [=](error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });

                    } catch (const std::exception &err) {
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::create_tag(
    caf::typed_response_promise<JsonStore> rp, const std::string &value) {

    try {
        scoped_actor sys{system()};

        auto jsn = R"({
            "name": null
         })"_json;

        jsn["name"] = value;

        request(
            shotgun_, infinite, shotgun_create_entity_atom_v, "tags", utility::JsonStore(jsn))
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        rp.deliver(result);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, result.dump(2));
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });


    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::add_entity_tag(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &entity,
    const int entity_id,
    const int tag_id) {

    // as this is an update, we have to pull current list and then add / push it back.. (I
    // THINK)
    try {

        scoped_actor sys{system()};
        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            shotgun_entity_atom_v,
            entity,
            entity_id,
            std::vector<std::string>({"tags"}))
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        auto current_tags =
                            result.at("data").at("relationships").at("tags").at("data");
                        auto new_tag = R"({"id":null, "type": "Tag"})"_json;
                        auto payload = R"({"tags": []})"_json;

                        new_tag["id"] = tag_id;
                        current_tags.push_back(new_tag);
                        payload["tags"] = current_tags;

                        // send update request..
                        request(
                            shotgun_,
                            infinite,
                            shotgun_update_entity_atom_v,
                            entity,
                            entity_id,
                            utility::JsonStore(payload),
                            std::vector<std::string>({"id", "code", "tags"}))
                            .then(
                                [=](const JsonStore &result) mutable { rp.deliver(result); },
                                [=](error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });

                    } catch (const std::exception &err) {
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}
