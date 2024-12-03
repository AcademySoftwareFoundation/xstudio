// SPDX-License-Identifier: Apache-2.0

#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"

#include "shotbrowser_plugin.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::shotbrowser;
using namespace xstudio::utility;
using namespace xstudio::data_source;

void ShotBrowser::find_ivy_version(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &uuid,
    const std::string &job) {
    // find version from supplied details.

    auto version_filter =
        FilterBy().And(Text("project.Project.name").is(job), Text("sg_ivy_dnuuid").is(uuid));

    request(
        shotgun_,
        std::chrono::seconds(static_cast<int>(timeout_->value())),
        shotgun_entity_search_atom_v,
        "Version",
        JsonStore(version_filter),
        concatenate_vector(VersionFields, version_fields_),
        std::vector<std::string>(),
        1,
        1)
        .then(
            [=](const JsonStore &jsn) mutable {
                auto result = JsonStore(R"({"payload":[]})"_json);
                if (jsn.count("data") and jsn.at("data").size()) {
                    result["payload"] = jsn.at("data")[0];
                }
                rp.deliver(result);
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(JsonStore(R"({"payload":[]})"_json));
            });
}

void ShotBrowser::get_fields(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int id,
    const std::string &entity,
    const std::vector<std::string> &fields) {
    request(
        shotgun_,
        std::chrono::seconds(static_cast<int>(timeout_->value())),
        shotgun_entity_atom_v,
        entity,
        id,
        fields)
        .then(
            [=](const JsonStore &jsn) mutable { rp.deliver(jsn); },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(JsonStore(R"({"data":{}})"_json));
            });
}


void ShotBrowser::find_shot(
    caf::typed_response_promise<utility::JsonStore> rp, const int shot_id) {
    // find version from supplied details.
    if (shot_cache_.count(shot_id))
        rp.deliver(shot_cache_.at(shot_id));
    else {
        request(
            shotgun_,
            std::chrono::seconds(static_cast<int>(timeout_->value())),
            shotgun_entity_atom_v,
            "Shot",
            shot_id,
            extend_fields("Shots", ShotFields))
            .then(
                [=](const JsonStore &jsn) mutable {
                    shot_cache_[shot_id] = jsn;
                    rp.deliver(jsn);
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(JsonStore(R"({"data":{}})"_json));
                });
    }
}

void ShotBrowser::link_media(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find playlist
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist =
            request_receive<caf::actor>(*sys, session, session::get_playlist_atom_v, uuid);

        // get media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // scan media for shotgun version / ivy uuid
        if (not media.empty()) {
            fan_out_request<policy::select_all>(
                vector_to_caf_actor_vector(media),
                infinite,
                json_store::get_json_atom_v,
                utility::Uuid(),
                "",
                true)
                .then(
                    [=](std::vector<std::pair<UuidActor, JsonStore>> json) mutable {
                        // ivy uuid is stored on source not media.. balls.
                        auto left    = std::make_shared<int>(0);
                        auto invalid = std::make_shared<int>(0);
                        for (const auto &i : json) {
                            try {
                                if (i.second.is_null() or
                                    not i.second["metadata"].count("shotgun")) {
                                    // request current media source metadata..
                                    scoped_actor sys{system()};
                                    auto source_meta = request_receive<JsonStore>(
                                        *sys,
                                        i.first.actor(),
                                        json_store::get_json_atom_v,
                                        "/metadata/external/DNeg");
                                    // we has got it..
                                    auto ivy_uuid = source_meta.at("Ivy").at("dnuuid");
                                    auto job      = source_meta.at("show");
                                    auto shot     = source_meta.at("shot");
                                    (*left) += 1;
                                    // spdlog::warn("{} {} {} {}", job, shot, ivy_uuid, *left);
                                    // call back into self ?
                                    // but we need to wait for the final result..
                                    // maybe in danger of deadlocks...
                                    // now we need to query shotgun..
                                    // to try and find version from this information.
                                    // this is then used to update the media actor.
                                    auto jsre        = JsonStore(GetVersionIvyUuid);
                                    jsre["ivy_uuid"] = ivy_uuid;
                                    jsre["job"]      = job;

                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        get_data_atom_v,
                                        jsre)
                                        .then(
                                            [=](const JsonStore &ver) mutable {
                                                // got ver from uuid
                                                (*left)--;
                                                if (ver["payload"].empty()) {
                                                    (*invalid)++;
                                                } else {
                                                    // push version to media object
                                                    scoped_actor sys{system()};
                                                    try {
                                                        request_receive<bool>(
                                                            *sys,
                                                            i.first.actor(),
                                                            json_store::set_json_atom_v,
                                                            utility::Uuid(),
                                                            JsonStore(ver["payload"]),
                                                            ShotgunMetadataPath + "/version");
                                                    } catch (const std::exception &err) {
                                                        spdlog::warn(
                                                            "{} {}",
                                                            __PRETTY_FUNCTION__,
                                                            err.what());
                                                    }
                                                }

                                                if (not(*left)) {
                                                    JsonStore result(
                                                        R"({"result": {"valid":0, "invalid":0}})"_json);
                                                    result["result"]["valid"] =
                                                        json.size() - (*invalid);
                                                    result["result"]["invalid"] = (*invalid);
                                                    rp.deliver(result);
                                                }
                                            },
                                            [=](error &err) mutable {
                                                spdlog::warn(
                                                    "{} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err));
                                                (*left)--;
                                                (*invalid)++;
                                                if (not(*left)) {
                                                    JsonStore result(
                                                        R"({"result": {"valid":0, "invalid":0}})"_json);
                                                    result["result"]["valid"] =
                                                        json.size() - (*invalid);
                                                    result["result"]["invalid"] = (*invalid);
                                                    rp.deliver(result);
                                                }
                                            });
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        if (not(*left)) {
                            JsonStore result(R"({"result": {"valid":0, "invalid":0}})"_json);
                            result["result"]["valid"]   = json.size();
                            result["result"]["invalid"] = 0;
                            rp.deliver(result);
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
                    });
        } else {
            rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
        }


    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}
void get_data(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id);

void ShotBrowser::download_media(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find media
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto media =
            request_receive<caf::actor>(*sys, session, playlist::get_media_atom_v, uuid);

        // get metadata, we need version id..
        auto media_metadata = request_receive<JsonStore>(
            *sys,
            media,
            json_store::get_json_atom_v,
            utility::Uuid(),
            "/metadata/shotgun/version");

        // spdlog::warn("{}", media_metadata.dump(2));

        auto name = media_metadata.at("attributes").at("code").template get<std::string>();
        auto job =
            media_metadata.at("attributes").at("sg_project_name").template get<std::string>();
        auto shot = media_metadata.at("relationships")
                        .at("entity")
                        .at("data")
                        .at("name")
                        .template get<std::string>();
        auto filepath = download_cache_.target_string() + "/" + name + "-" + job + "-" + shot +
                        ".dneg.webm";


        // check it doesn't already exist..
        if (fs::exists(filepath)) {
            // create source and add to media
            auto uuid   = Uuid::generate();
            auto source = spawn<media::MediaSourceActor>(
                "ShotGrid Preview",
                utility::posix_path_to_uri(filepath),
                FrameList(),
                FrameRate(),
                uuid);
            request(media, infinite, media::add_media_source_atom_v, UuidActor(uuid, source))
                .then(
                    [=](const Uuid &u) mutable {
                        auto jsn          = JsonStore(R"({})"_json);
                        jsn["actor_uuid"] = uuid;
                        jsn["actor"]      = actor_to_string(system(), source);

                        rp.deliver(jsn);
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore((R"({})"_json)["error"] = to_string(err)));
                    });
        } else {
            request(
                shotgun_,
                infinite,
                shotgun_attachment_atom_v,
                "version",
                media_metadata.at("id").template get<int>(),
                "sg_uploaded_movie_webm")
                .then(
                    [=](const std::string &data) mutable {
                        if (data.size() > 1024 * 15) {
                            // write to file
                            std::ofstream o(filepath);
                            try {
                                o.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                                o << data << std::endl;
                                o.close();

                                // file written add to media as new source..
                                auto uuid   = Uuid::generate();
                                auto source = spawn<media::MediaSourceActor>(
                                    "ShotGrid Preview",
                                    utility::posix_path_to_uri(filepath),
                                    FrameList(),
                                    FrameRate(),
                                    uuid);
                                request(
                                    media,
                                    infinite,
                                    media::add_media_source_atom_v,
                                    UuidActor(uuid, source))
                                    .then(
                                        [=](const Uuid &u) mutable {
                                            auto jsn          = JsonStore(R"({})"_json);
                                            jsn["actor_uuid"] = uuid;
                                            jsn["actor"] = actor_to_string(system(), source);

                                            rp.deliver(jsn);
                                        },
                                        [=](error &err) mutable {
                                            spdlog::error(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                            rp.deliver(JsonStore(
                                                (R"({})"_json)["error"] = to_string(err)));
                                        });

                            } catch (const std::exception &) {
                                // remove failed file
                                if (o.is_open()) {
                                    o.close();
                                    fs::remove(filepath);
                                }
                                spdlog::warn("Failed to open file");
                            }
                        } else {
                            rp.deliver(
                                JsonStore((R"({})"_json)["error"] = "Failed to download"));
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore((R"({})"_json)["error"] = to_string(err)));
                    });
        }
        // "content_type": "video/webm",
        // "id": 88463162,
        // "link_type": "upload",
        // "name": "b&#39;tmp_upload_webm_0okvakz6.webm&#39;",
        // "type": "Attachment",
        // "url": "http://shotgun.dneg.com/file_serve/attachment/88463162"

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(JsonStore((R"({})"_json)["error"] = err.what()));
    }
}

void ShotBrowser::get_valid_media_count(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::Uuid &uuid) {
    try {
        // find playlist
        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        auto playlist =
            request_receive<caf::actor>(*sys, session, session::get_playlist_atom_v, uuid);

        // get media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        if (not media.empty()) {
            fan_out_request<policy::select_all>(
                vector_to_caf_actor_vector(media),
                infinite,
                json_store::get_json_atom_v,
                utility::Uuid(),
                "")
                .then(
                    [=](std::vector<JsonStore> json) mutable {
                        int count = 0;
                        for (const auto &i : json) {
                            try {
                                if (i["metadata"].count("shotgun"))
                                    count++;
                            } catch (...) {
                            }
                        }

                        JsonStore result(R"({"result": {"valid":0, "invalid":0}})"_json);
                        result["result"]["valid"]   = count;
                        result["result"]["invalid"] = json.size() - count;
                        rp.deliver(result);
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
                    });
        } else {
            rp.deliver(JsonStore(R"({"result": {"valid":0, "invalid":0}})"_json));
        }
    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::prepare_playlist_notes(
    caf::typed_response_promise<utility::JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const utility::UuidVector &media_uuids,
    const bool notify_owner,
    const std::vector<int> notify_group_ids,
    const bool combine,
    const bool add_time,
    const bool add_playlist_name,
    const bool add_type,
    const bool anno_requires_note,
    const bool skip_already_pubished,
    const std::string &default_type) {

    auto playlist_name = std::string();
    auto playlist_id   = int(0);
    auto payload       = R"({"payload":[], "valid": 0, "invalid": 0})"_json;

    try {
        scoped_actor sys{system()};

        // get session
        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        // get playlist
        auto playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        // get shotgun info from playlist..
        try {
            auto sgpl = request_receive<JsonStore>(
                *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

            playlist_name = sgpl.at("attributes").at("code").template get<std::string>();
            playlist_id   = sgpl.at("id").template get<int>();

        } catch (const std::exception &err) {
            spdlog::info("No shotgun playlist information");
        }

        // get media for playlist.
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // no media so no point..
        // nothing to publish.
        if (media.empty())
            return rp.deliver(JsonStore(payload));

        std::vector<caf::actor> media_actors;

        if (not media_uuids.empty()) {
            auto lookup = uuidactor_vect_to_map(media);
            for (const auto &i : media_uuids) {
                if (lookup.count(i))
                    media_actors.push_back(lookup[i]);
            }
        } else {
            media_actors = vector_to_caf_actor_vector(media);
        }

        // get media shotgun json..
        // we can only publish notes for media that has version information
        fan_out_request<policy::select_all>(
            media_actors,
            infinite,
            json_store::get_json_atom_v,
            utility::Uuid(),
            ShotgunMetadataPath,
            true)
            .then(
                [=](std::vector<std::pair<UuidActor, JsonStore>> version_meta) mutable {
                    auto result = JsonStore(payload);

                    scoped_actor sys{system()};

                    std::map<Uuid, std::pair<UuidActor, JsonStore>> media_map;
                    UuidVector valid_media;

                    // get valid media.
                    // get all the shotgun info we need to publish
                    for (const auto &i : version_meta) {
                        try {
                            // spdlog::warn("{}", i.second.dump(2));
                            const auto &version = i.second.at("version");
                            auto jsn            = JsonStore(PublishNoteTemplateJSON);

                            // project
                            jsn["payload"]["project"]["id"] = version.at("relationships")
                                                                  .at("project")
                                                                  .at("data")
                                                                  .at("id")
                                                                  .get<int>();


                            // playlist link
                            jsn["payload"]["note_links"][0]["id"] = playlist_id;

                            if (version.at("relationships")
                                    .at("entity")
                                    .at("data")
                                    .value("type", "") == "Sequence")
                                // shot link
                                jsn["payload"]["note_links"][1]["id"] =
                                    version.at("relationships")
                                        .at("entity")
                                        .at("data")
                                        .value("id", 0);
                            else if (
                                version.at("relationships")
                                    .at("entity")
                                    .at("data")
                                    .value("type", "") == "Shot")
                                // sequence link
                                jsn["payload"]["note_links"][2]["id"] =
                                    version.at("relationships")
                                        .at("entity")
                                        .at("data")
                                        .value("id", 0);

                            // version link
                            jsn["payload"]["note_links"][3]["id"] = version.value("id", 0);

                            if (jsn["payload"]["note_links"][3]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(3);
                            if (jsn["payload"]["note_links"][2]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(2);
                            if (jsn["payload"]["note_links"][1]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(1);
                            if (jsn["payload"]["note_links"][0]["id"].get<int>() == 0)
                                jsn["payload"]["note_links"].erase(0);

                            // we don't pass these to shotgun..
                            jsn["shot"] = version.at("relationships")
                                              .at("entity")
                                              .at("data")
                                              .at("name")
                                              .get<std::string>();
                            jsn["playlist_name"] = playlist_name;

                            if (notify_owner) // 1068
                                jsn["payload"]["addressings_to"][0]["id"] =
                                    version.at("relationships")
                                        .at("user")
                                        .at("data")
                                        .at("id")
                                        .get<int>();
                            else
                                jsn["payload"].erase("addressings_to");

                            if (not notify_group_ids.empty()) {
                                auto grp = R"({ "type": "Group", "id": null})"_json;
                                for (const auto g : notify_group_ids) {
                                    if (g <= 0)
                                        continue;

                                    grp["id"] = g;
                                    jsn["payload"]["addressings_cc"].push_back(grp);
                                }
                            }

                            if (jsn["payload"]["addressings_cc"].empty())
                                jsn["payload"].erase("addressings_cc");


                            media_map[i.first.uuid()] = std::make_pair(i.first, jsn);
                            valid_media.push_back(i.first.uuid());
                        } catch (const std::exception &err) {
                            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                    // get bookmark manager.
                    auto bookmarks = request_receive<caf::actor>(
                        *sys, session, bookmark::get_bookmark_atom_v);

                    // // collect media notes if they have shotgun metadata on the media
                    auto existing_bookmarks =
                        request_receive<std::vector<std::pair<Uuid, UuidActorVector>>>(
                            *sys, bookmarks, bookmark::get_bookmarks_atom_v, valid_media);

                    // get bookmark detail..
                    for (const auto &i : existing_bookmarks) {
                        // grouped by media item.
                        // we may want to collapse to unique note_types
                        std::map<std::string, std::map<int, std::vector<JsonStore>>>
                            notes_by_type;

                        for (const auto &j : i.second) {
                            try {
                                if (skip_already_pubished) {
                                    auto already_published = false;
                                    try {
                                        // check for shotgun metadata on note.
                                        request_receive<JsonStore>(
                                            *sys,
                                            j.actor(),
                                            json_store::get_json_atom_v,
                                            ShotgunMetadataPath + "/note");
                                        already_published = true;
                                    } catch (...) {
                                    }

                                    if (already_published)
                                        continue;
                                }

                                auto detail = request_receive<bookmark::BookmarkDetail>(
                                    *sys, j.actor(), bookmark::bookmark_detail_atom_v);
                                // skip notes with no text unless annotated and
                                // only_with_annotation is true
                                auto has_note = detail.note_ and not(*(detail.note_)).empty();
                                auto has_anno =
                                    detail.has_annotation_ and *(detail.has_annotation_);

                                // do not publish non-visible bookmarks (e.g. grades)
                                auto visible = detail.visible_ and *(detail.visible_);
                                if (not visible)
                                    continue;

                                if (not(has_note or (has_anno and not anno_requires_note)))
                                    continue;

                                auto [ua, jsn] = media_map[detail.owner_->uuid()];
                                // push to shotgun client..
                                jsn["bookmark_uuid"].push_back(j.uuid());
                                if (not jsn.count("has_annotation"))
                                    jsn["has_annotation"] = R"([])"_json;

                                if (has_anno) {
                                    auto item =
                                        R"({"media_uuid": null, "media_name": null, "media_frame": 0, "timecode_frame": 0})"_json;
                                    item["media_uuid"]  = i.first;
                                    item["media_name"]  = jsn["shot"];
                                    item["media_frame"] = detail.start_frame();
                                    item["timecode_frame"] =
                                        detail.start_timecode_tc().total_frames();
                                    // requires media actor and first frame of annotation.
                                    jsn["has_annotation"].push_back(item);
                                }
                                auto cat = detail.category_ ? *(detail.category_) : "";
                                if (not default_type.empty())
                                    cat = default_type;

                                jsn["payload"]["sg_note_type"] = cat;
                                jsn["payload"]["subject"] =
                                    detail.subject_ ? *(detail.subject_) : "";
                                // format note content
                                std::string content;

                                if (add_time)
                                    content += std::string("Frame : ") +
                                               std::to_string(
                                                   detail.start_timecode_tc().total_frames()) +
                                               " / " + detail.start_timecode() + " / " +
                                               detail.duration_timecode() + "\n";

                                content += *(detail.note_);

                                jsn["payload"]["content"] = content;

                                // yeah this is a bit convoluted.
                                if (not notes_by_type.count(cat)) {
                                    notes_by_type.insert(std::make_pair(
                                        cat,
                                        std::map<int, std::vector<JsonStore>>(
                                            {{detail.start_frame(), {{jsn}}}})));
                                } else {
                                    if (notes_by_type[cat].count(detail.start_frame())) {
                                        notes_by_type[cat][detail.start_frame()].push_back(jsn);
                                    } else {
                                        notes_by_type[cat].insert(std::make_pair(
                                            detail.start_frame(),
                                            std::vector<JsonStore>({jsn})));
                                    }
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        try {
                            auto merged = JsonStore();

                            // category
                            for (auto &k : notes_by_type) {
                                auto category = k.first;
                                // frame
                                for (const auto &j : k.second) {
                                    // entry
                                    for (const auto &notepayload : j.second) {
                                        // spdlog::warn("{}",notepayload.dump(2));

                                        if (not merged.is_null() and
                                            (not combine or
                                             merged["payload"]["sg_note_type"] !=
                                                 notepayload["payload"]["sg_note_type"])) {
                                            // spdlog::warn("{}", merged.dump(2));
                                            result["payload"].push_back(merged);
                                            merged = JsonStore();
                                        }

                                        if (merged.is_null()) {
                                            merged       = notepayload;
                                            auto content = std::string();

                                            if (add_playlist_name and
                                                not merged["playlist_name"]
                                                        .get<std::string>()
                                                        .empty())
                                                content +=
                                                    "Playlist : " +
                                                    std::string(merged["playlist_name"]) + "\n";

                                            if (add_type)
                                                content += "Note Type : " +
                                                           merged["payload"]["sg_note_type"]
                                                               .get<std::string>() +
                                                           "\n\n";
                                            else
                                                content += "\n\n";

                                            merged["payload"]["content"] =
                                                content +
                                                merged["payload"]["content"].get<std::string>();

                                            merged.erase("shot");
                                            merged.erase("playlist_name");
                                        } else {
                                            merged["bookmark_uuid"].insert(
                                                merged["bookmark_uuid"].end(),
                                                notepayload["bookmark_uuid"].begin(),
                                                notepayload["bookmark_uuid"].end());
                                            merged["payload"]["content"] =
                                                merged["payload"]["content"]
                                                    .get<std::string>() +
                                                "\n\n" +
                                                notepayload["payload"]["content"]
                                                    .get<std::string>();
                                            merged["has_annotation"].insert(
                                                merged["has_annotation"].end(),
                                                notepayload["has_annotation"].begin(),
                                                notepayload["has_annotation"].end());
                                        }
                                    }
                                }
                            }

                            if (not merged.is_null())
                                result["payload"].push_back(merged);

                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }

                    result["valid"] = result["payload"].size();

                    // spdlog::warn("{}", result.dump(2));
                    rp.deliver(result);
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    rp.deliver(JsonStore(payload));
                });

    } catch (const std::exception &err) {
        spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::execute_query(
    caf::typed_response_promise<utility::JsonStore> rp, const utility::JsonStore &action) {

    auto dispatched = utility::sysclock::now();

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        action.at("entity").get<std::string>(),
        JsonStore(action.at("query")),
        extend_fields(
            action.at("entity").get<std::string>(),
            action.at("fields").get<std::vector<std::string>>()),
        action.at("order").get<std::vector<std::string>>(),
        action.at("page").get<int>(),
        action.at("max_result").get<int>())
        .then(
            [=](const JsonStore &data) mutable {
                auto result            = action;
                result["execution_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                             utility::sysclock::now() - dispatched)
                                             .count();
                result["result"] = data;
                rp.deliver(result);
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id) {

    try {
        auto cache_key = QueryEngine::cache_name(type, project_id);
        // check it's not cached..
        auto cached = engine().get_cache(cache_key);

        if (cached) {
            rp.deliver(std::move(*cached));
        } else {
            // try and tigger request..
            if (type == "department")
                get_data_department(rp, type);
            else if (type == "project")
                get_data_project(rp, type);
            else if (type == "location")
                get_data_location(rp, type);
            else if (type == "review_location")
                get_data_review_location(rp, type);
            else if (type == "playlist_type")
                get_data_playlist_type(rp, type);
            else if (type == "shot_status")
                get_data_shot_status(rp, type);
            else if (type == "note_type")
                get_data_note_type(rp, type);
            else if (type == "production_status")
                get_data_production_status(rp, type);
            else if (type == "pipeline_status")
                get_data_pipeline_status(rp, type, "version", "Pipeline Status");
            else if (type == "sequence_status")
                get_data_pipeline_status(rp, type, "sequence", "Sequence Status");
            else if (type == "user")
                get_data_user(rp, type, project_id);
            else if (type == "shot")
                get_data_shot(rp, type, project_id);
            else if (type == "sequence_shot")
                get_data_shot_for_sequence(rp, type, project_id);
            else if (type == "pipe_step")
                get_pipe_step(rp);
            else if (type == "sequence")
                get_data_sequence(rp, type, project_id);
            else if (type == "playlist")
                get_data_playlist(rp, type, project_id);
            else if (type == "unit")
                get_data_unit(rp, type, project_id);
            else if (type == "group")
                get_data_group(rp, type, project_id);
            else if (type == "stage")
                get_data_stage(rp, type, project_id);
            else {
                rp.deliver(make_error(xstudio_error::error, "Unknown type " + type));
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::get_data_department(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    // ["sg_status_list", "is", "act"]

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
        ]
    })"_json;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "Departments",
        JsonStore(filter),
        std::vector<std::string>({"name", "id"}),
        std::vector<std::string>({"name"}),
        1,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto lookup_key = QueryEngine::cache_name("Department");

                        engine().set_cache(cache_key, data.at("data"));
                        engine().set_lookup(lookup_key, data.at("data"), engine().lookup());

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_project(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(
        shotgun_,
        infinite,
        shotgun_projects_atom_v,
        ProjectFields,
        std::vector<std::string>({"name"}))
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto lookup_key = QueryEngine::cache_name("Project");

                        engine().set_cache(cache_key, data.at("data"));
                        engine().set_lookup(lookup_key, data.at("data"), engine().lookup());

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_location(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(
        shotgun_, infinite, shotgun_schema_entity_fields_atom_v, "playlist", "sg_location", -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_cache(cache_key, field_data);
                        // engine().set_lookup(QueryEngine::cache_name("Location"), field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_review_location(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(
        shotgun_,
        infinite,
        shotgun_schema_entity_fields_atom_v,
        "playlist",
        "sg_review_location_1",
        -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_cache(cache_key, field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_playlist_type(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(shotgun_, infinite, shotgun_schema_entity_fields_atom_v, "playlist", "sg_type", -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_cache(cache_key, field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_shot_status(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(
        shotgun_, infinite, shotgun_schema_entity_fields_atom_v, "shot", "sg_status_list", -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_cache(cache_key, field_data);
                        engine().set_lookup(
                            QueryEngine::cache_name("Exclude Shot Status"),
                            field_data,
                            engine().lookup());
                        engine().set_lookup(
                            QueryEngine::cache_name("Shot Status"),
                            field_data,
                            engine().lookup());

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_note_type(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(shotgun_, infinite, shotgun_schema_entity_fields_atom_v, "note", "sg_note_type", -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_cache(cache_key, field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_production_status(
    caf::typed_response_promise<utility::JsonStore> rp, const std::string &type) {

    request(
        shotgun_,
        infinite,
        shotgun_schema_entity_fields_atom_v,
        "version",
        "sg_production_status",
        -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_lookup(
                            QueryEngine::cache_name("Production Status"),
                            field_data,
                            engine().lookup());
                        engine().set_cache(cache_key, field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_pipeline_status(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const std::string &entity,
    const std::string &cache_name) {

    request(
        shotgun_, infinite, shotgun_schema_entity_fields_atom_v, entity, "sg_status_list", -1)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key  = QueryEngine::cache_name(type);
                        auto field_data = QueryEngine::data_from_field(data.at("data"));

                        engine().set_lookup(
                            QueryEngine::cache_name(cache_name), field_data, engine().lookup());
                        engine().set_cache(cache_key, field_data);

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_pipe_step(caf::typed_response_promise<utility::JsonStore> rp) {
    auto ps = engine().get_cache(QueryEngine::cache_name("Pipeline Step"));
    if (ps)
        rp.deliver(*ps);
    else
        rp.deliver(JsonStore(R"([{"name":"None"}])"));
}


void ShotBrowser::get_data_user(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id,
    const int page,
    const JsonStore &prev_data) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["sg_status_list", "is", "act"],
            ["sg_location", "is_not", null]
        ]
    })"_json;

    if (project_id != -1) {
        auto proj_filt     = R"(["projects", "is", {"type":"Project", "id": -1}])"_json;
        proj_filt[2]["id"] = project_id;
        filter["conditions"].push_back(proj_filt);
    }

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "HumanUsers",
        JsonStore(filter),
        std::vector<std::string>({"name", "id", "login"}),
        std::vector<std::string>({"name"}),
        page,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto total = prev_data;
                        total.insert(
                            total.end(), data.at("data").begin(), data.at("data").end());

                        if (data.at("data").size() == 4999) {
                            get_data_user(rp, type, project_id, page + 1, total);
                        } else {
                            auto cache_key = QueryEngine::cache_name(type, project_id);
                            engine().set_lookup(
                                QueryEngine::cache_name("User", project_id),
                                total,
                                engine().lookup());
                            engine().set_cache(cache_key, total);

                            rp.deliver(*engine().get_cache(cache_key));
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_shot_for_sequence(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id,
    const int page,
    const JsonStore &prev_data) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}]
        ]
    })"_json;

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "Shots",
        JsonStore(filter),
        SequenceShotFields,
        std::vector<std::string>({"id"}),
        page,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto total = prev_data;
                        total.insert(
                            total.end(), data.at("data").begin(), data.at("data").end());

                        if (data.at("data").size() == 4999) {
                            get_data_shot_for_sequence(rp, type, project_id, page + 1, total);
                        } else {
                            rp.deliver(total);
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_shot(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id,
    const int page,
    const JsonStore &prev_data) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}],
            ["sg_status_list", "not_in", ["na", "del"]]
        ]
    })"_json;

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "Shots",
        JsonStore(filter),
        extend_fields("Shots", ShotFields),
        std::vector<std::string>({"code"}),
        page,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto total = prev_data;
                        total.insert(
                            total.end(), data.at("data").begin(), data.at("data").end());

                        if (data.at("data").size() == 4999) {
                            get_data_shot(rp, type, project_id, page + 1, total);
                        } else {
                            auto cache_key = QueryEngine::cache_name(type, project_id);
                            engine().set_lookup(
                                QueryEngine::cache_name("Shot", project_id),
                                total,
                                engine().lookup());
                            engine().set_cache(cache_key, total);

                            rp.deliver(*engine().get_cache(cache_key));
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_playlist(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id,
    const int page,
    const JsonStore &prev_data) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}],
            ["versions", "is_not", null]
        ]
    })"_json;

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "Playlists",
        JsonStore(filter),
        std::vector<std::string>({"id", "code"}),
        std::vector<std::string>({"-created_at"}),
        page,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto total = prev_data;
                        total.insert(
                            total.end(), data.at("data").begin(), data.at("data").end());

                        if (data.at("data").size() == 4999) {
                            get_data_playlist(rp, type, project_id, page + 1, total);
                        } else {
                            auto cache_key = QueryEngine::cache_name(type, project_id);
                            engine().set_lookup(
                                QueryEngine::cache_name("Playlist", project_id),
                                total,
                                engine().lookup());
                            engine().set_cache(cache_key, total);

                            rp.deliver(*engine().get_cache(cache_key));
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_version_artist(
    caf::typed_response_promise<utility::JsonStore> rp, const int version_id) {

    request(
        shotgun_,
        infinite,
        shotgun_entity_atom_v,
        "Versions",
        version_id,
        std::vector<std::string>({"code", "id", "user"}))
        .then(
            [=](const JsonStore &data) mutable {
                if (not data.count("data"))
                    rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                else
                    rp.deliver(JsonStore(data.at("data")));
            },
            [=](error &err) mutable { rp.deliver(err); });
}


void ShotBrowser::get_data_unit(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}],
            ["sg_shots", "is_not", null]
        ]
    })"_json;

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "CustomEntity24",
        JsonStore(filter),
        std::vector<std::string>({"code", "id"}),
        std::vector<std::string>({"code"}),
        1,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key = QueryEngine::cache_name(type, project_id);

                        engine().set_lookup(
                            QueryEngine::cache_name("Unit", project_id),
                            data.at("data"),
                            engine().lookup());
                        engine().set_cache(cache_key, data.at("data"));

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_stage(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}]
        ]
    })"_json;

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "CustomEntity34",
        JsonStore(filter),
        std::vector<std::string>({"code", "id"}),
        std::vector<std::string>({"code"}),
        1,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key = QueryEngine::cache_name(type, project_id);

                        engine().set_lookup(
                            QueryEngine::cache_name("Stage", project_id),
                            data.at("data"),
                            engine().lookup());
                        engine().set_cache(cache_key, data.at("data"));

                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_group(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id) {

    request(shotgun_, infinite, shotgun_groups_atom_v, project_id)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto cache_key = QueryEngine::cache_name(type, project_id);
                        engine().set_cache(cache_key, data.at("data"));
                        rp.deliver(*engine().get_cache(cache_key));
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::get_data_sequence(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::string &type,
    const int project_id,
    const int page,
    const JsonStore &prev_data) {

    auto filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["project", "is", {"type":"Project", "id":0}]
        ]
    })"_json;
    // ["sg_status_list", "not_in", ["na","del"]]

    filter["conditions"][0][2]["id"] = project_id;

    request(
        shotgun_,
        infinite,
        shotgun_entity_search_atom_v,
        "Sequences",
        JsonStore(filter),
        SequenceFields,
        std::vector<std::string>({"code"}),
        page,
        4999)
        .then(
            [=](const JsonStore &data) mutable {
                try {
                    if (not data.count("data"))
                        rp.deliver(make_error(xstudio_error::error, data.dump(2)));
                    else {
                        auto total = prev_data;
                        total.insert(
                            total.end(), data.at("data").begin(), data.at("data").end());

                        if (data.at("data").size() == 4999) {
                            get_data_sequence(rp, type, project_id, page + 1, total);
                        } else {
                            // request and purge deleted shots.
                            auto statusfilter                      = R"(
                        {
                            "logical_operator": "and",
                            "conditions": [
                                ["project", "is", {"type":"Project", "id": 0}]
                            ]
                        })"_json;
                            statusfilter["conditions"][0][2]["id"] = project_id;

                            // ["sg_status_list", "in", ["na","del"]]

                            auto getShotData = GetData;
                            getShotData["project_id"] = project_id;
                            getShotData["type"] = "sequence_shot";

                            request(
                                caf::actor_cast<caf::actor>(this),
                                infinite,
                                get_data_atom_v,
                                JsonStore(getShotData))
                                .then(
                                    [=](const JsonStore &statusdata) mutable {
                                        try {
                                            // build set of deleted shot id's
                                            std::map<int64_t, nlohmann::json> shot_data;
                                            for (const auto &i : statusdata)
                                                shot_data[i.at("id").get<int64_t>()] = i;

                                            if (not shot_data.empty()) {
                                                // iterate over sequence -> shots and remove
                                                // deleted
                                                for (auto &i : total) {
                                                    auto &t =
                                                        i["relationships"]["shots"]["data"];
                                                    for (auto it = t.begin(); it != t.end();
                                                         ++it) {
                                                        if (shot_data.count(it->at("id"))) {
                                                            it->update(shot_data[it->at("id")]);
                                                        }
                                                    }
                                                }
                                            }

                                            // spdlog::warn("{}", total.dump(2));

                                            auto cache_key =
                                                QueryEngine::cache_name(type, project_id);

                                            engine().set_lookup(
                                                QueryEngine::cache_name("Sequence", project_id),
                                                total,
                                                engine().lookup());
                                            engine().set_shot_sequence_lookup(
                                                QueryEngine::cache_name(
                                                    "ShotSequence", project_id),
                                                total,
                                                engine().lookup());
                                            engine().set_cache(cache_key, total);

                                            rp.deliver(*engine().get_cache(cache_key));
                                        } catch (const std::exception &err) {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, err.what());
                                            rp.deliver(
                                                make_error(xstudio_error::error, err.what()));
                                        }
                                    },
                                    [=](error &err) mutable { rp.deliver(err); });
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable { rp.deliver(err); });
}

void ShotBrowser::execute_preset(
    caf::typed_response_promise<utility::JsonStore> rp,
    const std::vector<std::string> &preset_paths,
    const int _project_id,
    const utility::JsonStore &context,
    const utility::JsonStore &metadata,
    const utility::JsonStore &env,
    const utility::JsonStore &custom_terms) {
    // is preset path valid ?
    // we want the preset and it's group.

    // spdlog::warn("{}", __PRETTY_FUNCTION__);
    auto project_id = _project_id;

    try {
        auto presets      = R"([])"_json;
        auto preset_group = R"([])"_json;
        auto group_id     = utility::Uuid();
        auto entity       = std::string();

        for (const auto &i : preset_paths) {
            auto preset = engine().user_presets().at(json::json_pointer(i));
            if (entity.empty()) {
                if (preset.value("type", "") == "group") {
                    entity       = preset.at("entity");
                    preset_group = preset.at("children").at(0).at("children");
                    group_id     = preset.value("id", utility::Uuid());
                } else {
                    auto tmp = engine().user_presets().at(json::json_pointer(i)
                                                              .parent_pointer()
                                                              .parent_pointer()
                                                              .parent_pointer()
                                                              .parent_pointer());

                    entity       = tmp.at("entity");
                    preset_group = tmp.at("children").at(0).at("children");
                    group_id     = tmp.value("id", utility::Uuid());
                }
            }

            if (preset.value("type", "") == "preset")
                presets.push_back(preset.at("children"));
        }

        // derive from metadata.

        if (not project_id)
            project_id = engine().get_project_id(metadata, engine().cache());

        // spdlog::warn("project_id {}", project_id);
        // spdlog::warn("{}", preset_group.dump(2));
        // spdlog::warn("{}", presets.dump(2));

        // we possibly need to precache lookups..
        if (not engine().precache_needed(project_id, engine().lookup()).empty()) {
            // spdlog::warn("NEEDS PREECACHE {}", project_id);
            // we need to trigger loading of lookups..
            auto req          = JsonStore(GetPrecache);
            req["project_id"] = project_id;

            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        // spdlog::warn("PRECACHED COMPLETE");
                        execute_preset(
                            rp, preset_paths, project_id, context, metadata, env, custom_terms);
                    },
                    [=](error &err) mutable { rp.deliver(err); });

        } else {
            auto request = QueryEngine::build_query(
                project_id,
                entity,
                group_id,
                preset_group,
                presets,
                custom_terms,
                context,
                metadata,
                env,
                engine().lookup());

            // spdlog::error("{}\n", request.dump(2));

            execute_query(rp, request);
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::get_precache(
    caf::typed_response_promise<utility::JsonStore> rp, const int project_id) {

    // trigger precaching of lookup.
    // we call into ourselves sigh..
    const auto &lookup         = engine().lookup();
    std::set<std::string> need = engine().precache_needed(project_id, lookup);

    if (need.empty()) {
        rp.deliver(utility::JsonStore());
    } else {
        auto count = std::make_shared<int>(need.size());

        if (need.count("Project")) {
            auto req    = JsonStore(GetData);
            req["type"] = "project";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Department")) {
            auto req    = JsonStore(GetData);
            req["type"] = "department";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("User")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "user";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Pipeline Status")) {
            auto req    = JsonStore(GetData);
            req["type"] = "pipeline_status";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        // if (need.count("Shot Pipeline Status")) {
        //     auto req    = JsonStore(GetData);
        //     req["type"] = "shot_pipeline_status";
        //     request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
        //         .then(
        //             [=](const JsonStore &) mutable {
        //                 (*count) = (*count) - 1;
        //                 if (not(*count))
        //                     rp.deliver(utility::JsonStore());
        //             },
        //             [=](error &err) mutable {
        //                 (*count) = (*count) - 1;
        //                 if (not(*count))
        //                     rp.deliver(err);
        //             });
        // }

        if (need.count("Sequence Status")) {
            auto req    = JsonStore(GetData);
            req["type"] = "sequence_status";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Production Status")) {
            auto req    = JsonStore(GetData);
            req["type"] = "production_status";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Shot Status")) {
            auto req    = JsonStore(GetData);
            req["type"] = "shot_status";
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Stage")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "stage";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Unit")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "unit";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Playlist")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "playlist";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Shot")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "shot";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }

        if (need.count("Sequence")) {
            auto req          = JsonStore(GetData);
            req["type"]       = "sequence";
            req["project_id"] = project_id;
            request(caf::actor_cast<caf::actor>(this), infinite, get_data_atom_v, req)
                .then(
                    [=](const JsonStore &) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(utility::JsonStore());
                    },
                    [=](error &err) mutable {
                        (*count) = (*count) - 1;
                        if (not(*count))
                            rp.deliver(err);
                    });
        }
    }
}
