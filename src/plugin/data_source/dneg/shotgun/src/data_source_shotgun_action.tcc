// SPDX-License-Identifier: Apache-2.0

template <typename T>
void ShotgunDataSourceActor<T>::use_action(
    caf::typed_response_promise<JsonStore> rp,
    const utility::JsonStore &action) {

    try {
    	auto operation = action.value("operation", "");

        if (operation == "LoadPlaylist") {
            scoped_actor sys{system()};
            auto session = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(global_registry),
                session::session_atom_v);

            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                use_data_atom_v,
                action,
                session)
                .then(
                    [=](const UuidActor &) mutable {
                        rp.deliver(
                            JsonStore(R"({"data": {"status": "successful"}})"_json));
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(
                            JsonStore(R"({"data": {"status": "successful"}})"_json));
                    });
        } else if (operation == "RefreshPlaylist") {
            refresh_playlist_versions(rp, Uuid(action.at("playlist_uuid")));
        } else {
            rp.deliver(make_error(xstudio_error::error, "Invalid operation."));

        }
    } catch (const std::exception &err) {
        rp.deliver( make_error(
            xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::use_action(
    caf::typed_response_promise<utility::UuidActor> rp,
    const utility::JsonStore &action, const caf::actor &session
) {
    try {
    	auto operation = action.value("operation", "");

        if (operation == "LoadPlaylist") {
            load_playlist(rp, action.at("playlist_id").get<int>(), session);
        } else {
            rp.deliver(make_error(xstudio_error::error, "Invalid operation."));
        }
    } catch (const std::exception &err) {
        rp.deliver(make_error(
            xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::use_action(
    caf::typed_response_promise<utility::UuidActorVector> rp,
    const caf::uri &uri, const FrameRate &media_rate
) {
    // check protocol == shotgun..
    if (uri.scheme() != "shotgun")
        return rp.deliver(UuidActorVector());

    if (to_string(uri.authority()) == "load") {
        // need type and id
        auto query = uri.query();
        if (query.count("type") and query["type"] == "Version" and query.count("ids")) {
            auto ids = split(query["ids"], '|');
            if (ids.empty())
                rp.deliver( UuidActorVector() );

            auto count   = std::make_shared<int>(ids.size());
            auto results = std::make_shared<UuidActorVector>();

            for (const auto i : ids) {
                try {
                    auto type    = query["type"];
                    auto squery  = R"({})"_json;
                    squery["id"] = i;

                    request(
                        caf::actor_cast<caf::actor>(this),
                        std::chrono::seconds(
                            static_cast<int>(data_source_.timeout_->value())),
                        shotgun_entity_filter_atom_v,
                        "Versions",
                        JsonStore(squery),
                        VersionFields,
                        std::vector<std::string>(),
                        1,
                        4999)
                        .then(
                            [=](const JsonStore &js) mutable {
                                // load version..
                                request(
                                    caf::actor_cast<caf::actor>(this),
                                    infinite,
                                    playlist::add_media_atom_v,
                                    js,
                                    utility::Uuid(),
                                    caf::actor(),
                                    utility::Uuid())
                                    .then(
                                        [=](const UuidActorVector &uav) mutable {
                                            (*count)--;

                                            for (const auto &ua : uav)
                                                results->push_back(ua);

                                            if (not(*count))
                                                rp.deliver(*results);
                                        },
                                        [=](const caf::error &err) mutable {
                                            (*count)--;
                                            spdlog::warn(
                                                "{} {}",
                                                __PRETTY_FUNCTION__,
                                                to_string(err));
                                            if (not(*count))
                                                rp.deliver(*results);
                                        });
                            },
                            [=](const caf::error &err) mutable {
                                spdlog::warn(
                                    "{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                } catch (const std::exception &err) {
                    (*count)--;
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }
        } else if (
            query.count("type") and query["type"] == "Playlist" and
            query.count("ids")) {
            // will return an array of playlist actors..
            auto ids = split(query["ids"], '|');
            if (ids.empty())
                rp.deliver( UuidActorVector());

            auto count   = std::make_shared<int>(ids.size());
            auto results = std::make_shared<UuidActorVector>();

            for (const auto i : ids) {
                auto id           = std::atoi(i.c_str());
                auto js           = JsonStore(UseLoadPlaylist);
                js["playlist_id"] = id;
                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    use_data_atom_v,
                    js,
                    caf::actor())
                    .then(
                        [=](const UuidActor &ua) mutable {
                            // process result to build playlist..
                            (*count)--;
                            results->push_back(ua);
                            if (not(*count))
                                rp.deliver(*results);
                        },
                        [=](const caf::error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            (*count)--;
                            if (not(*count))
                                rp.deliver(*results);
                        });
            }
        } else {
            spdlog::warn(
                "Invalid shotgun action {}, requires type, id", to_string(uri));
            rp.deliver(UuidActorVector());
        }
    } else {
        spdlog::warn(
            "Invalid shotgun action {} {}", to_string(uri.authority()), to_string(uri));
        rp.deliver(UuidActorVector());
    }
}


template <typename T>
void ShotgunDataSourceActor<T>::post_action(
    caf::typed_response_promise<JsonStore> rp,
    const utility::JsonStore &action) {

    try {
    	auto operation = action.value("operation", "");

    	if(operation == "RenameTag") {
            rename_tag(rp, action.at("tag_id"), action.at("value"));
    	} else if(operation == "CreateTag") {
            create_tag(rp, action.at("value"));
    	} else if(operation == "TagEntity") {
            add_entity_tag(
                rp, action.at("entity"), action.at("entity_id"), action.at("tag_id"));
    	} else if(operation == "UnTagEntity") {
            remove_entity_tag(
                rp, action.at("entity"), action.at("entity_id"), action.at("tag_id"));
    	} else if(operation == "CreatePlaylist") {
            create_playlist(rp, action);
    	} else if(operation == "CreateNotes") {
            create_playlist_notes(rp, action.at("payload"), JsonStore(action.at("playlist_uuid")));
        } else {
            rp.deliver(make_error(xstudio_error::error, "Invalid operation."));
    	}

    } catch (const std::exception &err) {
        rp.deliver( make_error(
            xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::get_action(
    caf::typed_response_promise<JsonStore> rp,
    const utility::JsonStore &action) {

    try {
    	auto operation = action.value("operation", "");

        if (operation == "VersionIvyUuid") {
            find_ivy_version(
                rp,
                action.at("ivy_uuid").get<std::string>(),
                action.at("job").get<std::string>());
        } else if (operation == "GetShotFromId") {
            find_shot(rp, action.at("shot_id").get<int>());
        } else if (operation == "LinkMedia") {
            link_media(rp, utility::Uuid(action.at("playlist_uuid")));
        } else if (operation == "DownloadMedia") {
            download_media(rp, utility::Uuid(action.at("media_uuid")));
        } else if (operation == "MediaCount") {
            get_valid_media_count(rp, utility::Uuid(action.at("playlist_uuid")));
        } else if (operation == "PrepareNotes") {
            UuidVector media_uuids;
            for (const auto &i : action.value("media_uuids", std::vector<std::string>()))
                media_uuids.push_back(Uuid(i));

            prepare_playlist_notes(
                rp,
                utility::Uuid(action.at("playlist_uuid")),
                media_uuids,
                action.value("notify_owner", false),
                action.value("notify_group_ids", std::vector<int>()),
                action.value("combine", false),
                action.value("add_time", false),
                action.value("add_playlist_name", false),
                action.value("add_type", false),
                action.value("anno_requires_note", true),
                action.value("skip_already_published", false),
                action.value("default_type", ""));
        } else if (operation == "Query") {
            execute_query(rp, action);
        } else {
            rp.deliver(
                make_error(xstudio_error::error, std::string("Invalid operation.")));
        }
    } catch (const std::exception &err) {
        rp.deliver(make_error(
            xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
    }
}

template <typename T>
void ShotgunDataSourceActor<T>::put_action(
    caf::typed_response_promise<JsonStore> rp,
    const xstudio::utility::JsonStore &action) {

    try {
    	auto operation = action.value("operation", "");

        if (operation == "UpdatePlaylistVersions") {
            update_playlist_versions(rp, Uuid(action["playlist_uuid"]));
        } else {
            rp.deliver(make_error(xstudio_error::error, "Invalid operation."));
        }
    } catch (const std::exception &err) {
        rp.deliver(make_error(
            xstudio_error::error, std::string("Invalid operation.\n") + err.what()));
    }
}

