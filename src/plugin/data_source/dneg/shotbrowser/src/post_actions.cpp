// SPDX-License-Identifier: Apache-2.0
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/notification_handler.hpp"

#include "shotbrowser_plugin.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::shotbrowser;
using namespace xstudio::utility;


void ShotBrowser::publish_note_annotations(
    caf::typed_response_promise<utility::JsonStore> rp,
    const caf::actor &session,
    const int note_id,
    const utility::JsonStore &annotations) {

    auto offscreen_renderer =
        system().registry().template get<caf::actor>(offscreen_viewport_registry);
    auto thumbnail_manager =
        system().registry().template get<caf::actor>(thumbnail_manager_registry);

    for (const auto &anno : annotations) {
        request(
            session, infinite, playlist::get_media_atom_v, utility::Uuid(anno.at("media_uuid")))
            .then(
                [=](const caf::actor &media_actor) mutable {
                    // spdlog::warn("render annotation {}",
                    // anno["media_frame"].get<int>());
                    request(
                        offscreen_renderer,
                        infinite,
                        ui::viewport::render_viewport_to_image_atom_v,
                        media_actor,
                        anno.at("media_frame").get<int>(),
                        thumbnail::THUMBNAIL_FORMAT::TF_RGB24,
                        2560, // forcing QHD resolution for details
                        false,
                        true)
                        .then(
                            [=](const thumbnail::ThumbnailBufferPtr &tnail) {
                                // got buffer. convert to jpg..
                                request(
                                    thumbnail_manager,
                                    infinite,
                                    media_reader::get_thumbnail_atom_v,
                                    tnail,
                                    100 // jpeg quality = 100
                                    )
                                    .then(
                                        [=](const std::vector<std::byte> &jpgbuf) mutable {
                                            // final step...
                                            auto title = std::string(fmt::format(
                                                "{}_{}.jpg",
                                                anno.at("media_name").get<std::string>(),
                                                anno.at("timecode_frame").get<int>()));
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
                                                    [=](const bool) {},
                                                    [=](const error &err) mutable {
                                                        spdlog::warn(
                                                            "{} "
                                                            "Failed"
                                                            " uploa"
                                                            "d of "
                                                            "annota"
                                                            "tion "
                                                            "{}",
                                                            __PRETTY_FUNCTION__,
                                                            to_string(err));
                                                    }

                                                );
                                        },
                                        [=](const error &err) mutable {
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
                    spdlog::warn("{} Failed get media {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}


void ShotBrowser::create_playlist_notes(
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

        auto count   = notes.size();
        auto failed  = std::make_shared<size_t>(0);
        auto succeed = std::make_shared<size_t>(0);
        auto results = std::make_shared<std::vector<JsonStore>>(count);

        auto index = 0;

        for (const auto &j : notes) {

            // need to capture result to embed in playlist and add any media..
            // spdlog::warn("{}", j["payload"].dump(2));
            request(
                shotgun_,
                infinite,
                shotgun_create_entity_atom_v,
                "notes",
                utility::JsonStore(j.at("payload")))
                .then(
                    [=](const JsonStore &result) mutable {
                        (*results)[index] = result;
                        try {
                            if (result.count("errors"))
                                (*failed)++;
                            else {

                                // get new playlist id..
                                auto note_id = result.at("data").at("id").template get<int>();
                                // we have a note...
                                if (not j.at("has_annotation").empty()) {
                                    publish_note_annotations(
                                        rp, session, note_id, j.at("has_annotation"));
                                }

                                // spdlog::warn("note {}", result.dump(2));
                                // send json to note..

                                for (const auto &ju : j.at("bookmark_uuid")) {
                                    anon_send(
                                        bookmarks,
                                        json_store::set_json_atom_v,
                                        ju.get<Uuid>(),
                                        utility::JsonStore(result.at("data")),
                                        ShotgunMetadataPath + "/note");

                                    // add shotgun decorator to note.
                                    anon_send(
                                        bookmarks,
                                        json_store::set_json_atom_v,
                                        ju.get<Uuid>(),
                                        JsonStore(
                                            R"({"icon": "qrc:/shotbrowser_icons/shot_grid.svg", "tooltip": "Published to ShotGrid"})"_json),
                                        "/ui/decorators/shotgrid");
                                }

                                (*succeed)++;
                            }
                        } catch (const std::exception &err) {
                            (*failed)++;
                            spdlog::warn(
                                "{} {} {}", __PRETTY_FUNCTION__, err.what(), result.dump(2));
                        }

                        if (count == (*failed) + (*succeed)) {
                            auto jsn = JsonStore(
                                R"({"data": {"status": ""}, "failed": [], "succeed": [], "succeed_title": [], "failed_title": []})"_json);
                            for (const auto &r : (*results)) {
                                if (r.count("errors")) {
                                    jsn["failed"].push_back(r);
                                    jsn["failed_title"].push_back(
                                        notes.at(index).at("payload").at("subject"));
                                } else {
                                    jsn["succeed"].push_back(r);
                                    jsn["succeed_title"].push_back(
                                        r.at("data").at("attributes").at("subject"));
                                }
                            }

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
                        (*failed)++;

                        if (count == (*failed) + (*succeed)) {
                            auto jsn = JsonStore(
                                R"({"data": {"status": ""}, "failed": [], "succeed": [], "succeed_title": [],"failed_title": []})"_json);
                            auto index = 0;
                            for (const auto &r : (*results)) {
                                if (r.count("errors")) {
                                    jsn["failed"].push_back(r);
                                    jsn["failed_title"].push_back(
                                        notes.at(index).at("payload").at("subject"));
                                } else {
                                    jsn["succeed"].push_back(r);
                                    jsn["succeed_title"].push_back(
                                        r.at("data").at("attributes").at("subject"));
                                }
                                index++;
                            }

                            jsn["data"]["status"] = std::string(fmt::format(
                                "Successfully published {} / {} notes.",
                                *succeed,
                                (*failed) + (*succeed)));
                            rp.deliver(jsn);
                        }
                    });
            index++;
        }

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::create_playlist(
    caf::typed_response_promise<JsonStore> rp, const utility::JsonStore &js) {
    // src should be a playlist actor..
    // and we want to update it..
    // retrieve shotgun metadata from playlist, and media items.
    auto notification_uuid = Uuid();
    auto playlist          = caf::actor();

    auto failed = [=](const caf::actor &dest, const Uuid &uuid) mutable {
        if (dest and not uuid.is_null()) {
            auto notify = Notification::WarnNotification("Publish Playlist Failed");
            notify.uuid(uuid);
            anon_send(dest, utility::notification_atom_v, notify);
        }
    };

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

        playlist = request_receive<caf::actor>(
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

        auto notify       = Notification::ProcessingNotification("Publishing Playlist");
        notification_uuid = notify.uuid();
        anon_send(playlist, utility::notification_atom_v, notify);


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
                        update_playlist_versions(
                            rp, playlist_uuid, playlist_id, notification_uuid);

                    } catch (const std::exception &err) {
                        failed(playlist, notification_uuid);
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, result.dump(2));
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    failed(playlist, notification_uuid);
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(err);
                });

    } catch (const std::exception &err) {
        failed(playlist, notification_uuid);
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::rename_tag(
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


void ShotBrowser::remove_entity_tag(
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

void ShotBrowser::create_tag(
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

void ShotBrowser::add_entity_tag(
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
