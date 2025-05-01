// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/notification_handler.hpp"

#include "shotbrowser_plugin.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::shotbrowser;
using namespace xstudio::utility;

void ShotBrowser::update_playlist_versions(
    caf::typed_response_promise<JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const bool append,
    const int playlist_id,
    const utility::Uuid &notification_uuid_) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.

    auto playlist          = caf::actor();
    auto notification_uuid = notification_uuid_;

    auto failed = [=](const caf::actor &dest, const Uuid &uuid) mutable {
        if (dest and not uuid.is_null()) {
            auto notify = Notification::WarnNotification("Update Playlist Failed");
            notify.uuid(uuid);
            anon_mail(utility::notification_atom_v, notify).send(dest);
        }
    };

    auto succeeded = [=](const caf::actor &dest, const Uuid &uuid) mutable {
        if (dest and not uuid.is_null()) {
            auto notify = Notification::InfoNotification(
                "Update Playlist Succeeded", std::chrono::seconds(5));
            notify.uuid(uuid);
            anon_mail(utility::notification_atom_v, notify).send(dest);
        }
    };

    try {

        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        if (notification_uuid.is_null()) {
            auto notify       = Notification::ProcessingNotification("Updating Playlist");
            notification_uuid = notify.uuid();
            anon_mail(utility::notification_atom_v, notify).send(playlist);
        }

        auto pl_id = playlist_id;
        if (not pl_id) {
            auto plsg = request_receive<JsonStore>(
                *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

            pl_id = plsg["id"].template get<int>();
        }

        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // foreach media actor get it's shogtun metadata.
        auto jsn = R"({"versions":[]})"_json;
        auto ver = R"({"id": 0, "type": "Version"})"_json;
        std::map<int, int> version_order_map;

        if (append) {
            auto current_vers = request_receive_wait<JsonStore>(
                                    *sys,
                                    shotgun_,
                                    infinite,
                                    shotgun_entity_atom_v,
                                    "Playlists",
                                    pl_id,
                                    std::vector<std::string>({"versions"}))
                                    .at("data")
                                    .at("relationships")
                                    .at("versions")
                                    .at("data");

            for (const auto &i : current_vers) {
                ver["id"] = i.at("id");
                jsn["versions"].push_back(ver);

                // we won't acutally use this, but will stop adding same version multiple times.
                version_order_map[i.at("id").get<int>()] = 0;
            }
            // {
            //   "id": 48906133,
            //   "name": "CG_ldev_pipe_lighting_sgco_test_L010_BEAUTY_v005",
            //   "type": "Version"
            // },
        }

        // get media detail
        int sort_order = 10;
        for (const auto &i : media) {
            try {
                auto mjson = request_receive<JsonStore>(
                    *sys,
                    i.actor(),
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    ShotgunMetadataPath + "/version");
                auto id = mjson["id"].template get<int>();

                if (not version_order_map.count(id)) {
                    ver["id"] = id;
                    jsn["versions"].push_back(ver);
                    version_order_map[id] = sort_order;

                    sort_order += 10;
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        // update playlist with versions
        mail(shotgun_update_entity_atom_v, "Playlists", pl_id, utility::JsonStore(jsn))
            .request(shotgun_, infinite)
            .then(
                [=](const JsonStore &result) mutable {
                    // spdlog::warn("{}", JsonStore(result["data"]).dump(2));
                    // update playorder..
                    // get PlaylistVersionConnections
                    if (not append) {
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
                    }

                    if (pl_id != playlist_id) {
                        anon_mail(
                            json_store::set_json_atom_v,
                            JsonStore(result["data"]),
                            ShotgunMetadataPath + "/playlist")
                            .send(playlist);

                        anon_mail(
                            json_store::set_json_atom_v,
                            JsonStore(
                                R"({"icon": "qrc:/shotbrowser_icons/shot_grid.svg", "tooltip": "ShotGrid Playlist"})"_json),
                            "/ui/decorators/shotgrid")
                            .send(playlist);
                    }
                    succeeded(playlist, notification_uuid);

                    rp.deliver(result);
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    failed(playlist, notification_uuid);
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
        failed(playlist, notification_uuid);
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}
