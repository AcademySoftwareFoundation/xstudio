// SPDX-License-Identifier: Apache-2.0

template <typename T>
void ShotgunDataSourceActor<T>::update_playlist_versions(
    caf::typed_response_promise<JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const int playlist_id) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.
    try {

        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        auto pl_id = playlist_id;
        if (not pl_id) {
            auto plsg = request_receive<JsonStore>(
                *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

            pl_id = plsg["id"].template get<int>();
        }

        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // foreach medai actor get it's shogtun metadata.
        auto jsn = R"({"versions":[]})"_json;
        auto ver = R"({"id": 0, "type": "Version"})"_json;

        std::map<int, int> version_order_map;
        // get media detail
        int sort_order = 1;
        for (const auto &i : media) {
            try {
                auto mjson = request_receive<JsonStore>(
                    *sys,
                    i.actor(),
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    ShotgunMetadataPath + "/version");
                auto id   = mjson["id"].template get<int>();
                ver["id"] = id;
                jsn["versions"].push_back(ver);
                version_order_map[id] = sort_order;

                sort_order++;
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        // update playlist
        request(
            shotgun_,
            infinite,
            shotgun_update_entity_atom_v,
            "Playlists",
            pl_id,
            utility::JsonStore(jsn))
            .then(
                [=](const JsonStore &result) mutable {
                    // spdlog::warn("{}", JsonStore(result["data"]).dump(2));
                    // update playorder..
                    // get PlaylistVersionConnections
                    scoped_actor sys{system()};

                    auto order_filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["playlist", "is", {"type":"Playlist", "id":0}]
                    ]
                })"_json;

                    order_filter["conditions"][0][2]["id"] = pl_id;

                    try {
                        auto order = request_receive<JsonStore>(
                            *sys,
                            shotgun_,
                            shotgun_entity_search_atom_v,
                            "PlaylistVersionConnection",
                            JsonStore(order_filter),
                            std::vector<std::string>({"sg_sort_order", "version"}),
                            std::vector<std::string>({"sg_sort_order"}),
                            1,
                            4999);
                        // update all PlaylistVersionConnection's with new sort_order.
                        for (const auto &i : order["data"]) {
                            auto version_id = i.at("relationships")
                                                  .at("version")
                                                  .at("data")
                                                  .at("id")
                                                  .get<int>();
                            auto sort_order =
                                i.at("attributes").at("sg_sort_order").is_null()
                                    ? 0
                                    : i.at("attributes").at("sg_sort_order").get<int>();
                            // spdlog::warn("{} {}", std::to_string(sort_order),
                            // std::to_string(version_order_map[version_id]));
                            if (sort_order != version_order_map[version_id]) {
                                auto so_jsn             = R"({"sg_sort_order": 0})"_json;
                                so_jsn["sg_sort_order"] = version_order_map[version_id];
                                try {
                                    request_receive<JsonStore>(
                                        *sys,
                                        shotgun_,
                                        shotgun_update_entity_atom_v,
                                        "PlaylistVersionConnection",
                                        i.at("id").get<int>(),
                                        utility::JsonStore(so_jsn),
                                        std::vector<std::string>({"id"}));
                                } catch (const std::exception &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                                }
                            }
                        }

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }


                    if (pl_id != playlist_id)
                        anon_send(
                            playlist,
                            json_store::set_json_atom_v,
                            JsonStore(result["data"]),
                            ShotgunMetadataPath + "/playlist");
                    rp.deliver(result);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });

        // need to update/add PlaylistVersionConnection's
        // on creation the sort_order will be null.
        // PlaylistVersionConnection are auto created when adding to playlist. (I think)
        // so all we need to do is update..


        // get shotgun metadata
        // get media actors.
        // get media shotgun metadata.
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}
