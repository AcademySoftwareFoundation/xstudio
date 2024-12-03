// SPDX-License-Identifier: Apache-2.0

#include "worker.hpp"
#include "definitions.hpp"

#include "xstudio/atoms.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;

void MediaWorker::add_media_step_1(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const FrameRate &media_rate) {
    request(
        actor_cast<caf::actor>(this),
        infinite,
        media::add_media_source_atom_v,
        jsn,
        media_rate,
        true)
        .then(
            [=](const UuidActor &movie_source) mutable {
                add_media_step_2(rp, media, jsn, media_rate, movie_source);
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}

void MediaWorker::add_media_step_2(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const FrameRate &media_rate,
    const UuidActor &movie_source) {
    // now get image..
    request(
        actor_cast<caf::actor>(this), infinite, media::add_media_source_atom_v, jsn, media_rate)
        .then(
            [=](const UuidActor &image_source) mutable {
                // check to see if what we've got..
                // failed...
                if (movie_source.uuid().is_null() and image_source.uuid().is_null()) {
                    // spdlog::warn("{} No valid sources", __PRETTY_FUNCTION__);
                    add_media_step_3(rp, media, jsn, UuidActorVector());
                } else {
                    try {
                        UuidActorVector srcs;

                        if (not movie_source.uuid().is_null())
                            srcs.push_back(movie_source);
                        if (not image_source.uuid().is_null())
                            srcs.push_back(image_source);


                        add_media_step_3(rp, media, jsn, srcs);

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}

void MediaWorker::add_media_step_3(
    caf::typed_response_promise<bool> rp,
    caf::actor media,
    const JsonStore &jsn,
    const UuidActorVector &srcs) {
    request(media, infinite, media::add_media_source_atom_v, srcs)
        .then(
            [=](const bool) mutable {
                rp.deliver(true);
                // push metadata to media actor.
                anon_send(
                    media,
                    json_store::set_json_atom_v,
                    utility::Uuid(),
                    jsn,
                    ShotgunMetadataPath + "/version");

                anon_send(
                    media,
                    json_store::set_json_atom_v,
                    utility::Uuid(),
                    JsonStore(
                        R"({"icon": "qrc:/shotbrowser_icons/shot_grid.svg", "tooltip": "ShotGrid Version"})"_json),
                    "/ui/decorators/shotgrid");

                // dispatch delayed shot data.
                try {
                    auto shotreq = JsonStore(GetShotFromId);
                    shotreq["shot_id"] =
                        jsn.at("relationships").at("entity").at("data").value("id", 0);

                    request(
                        caf::actor_cast<caf::actor>(data_source_),
                        infinite,
                        data_source::get_data_atom_v,
                        shotreq)
                        .then(
                            [=](const JsonStore &jsn) mutable {
                                try {
                                    if (jsn.count("data"))
                                        anon_send(
                                            media,
                                            json_store::set_json_atom_v,
                                            utility::Uuid(),
                                            JsonStore(jsn.at("data")),
                                            ShotgunMetadataPath + "/shot");
                                } catch (const std::exception &err) {
                                    spdlog::warn("A {} {}", __PRETTY_FUNCTION__, err.what());
                                }
                            },
                            [=](const error &err) mutable {
                                spdlog::warn("B {} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                } catch (const std::exception &err) {
                    spdlog::warn("C {} {}", __PRETTY_FUNCTION__, err.what());
                }
            },
            [=](error &err) mutable {
                spdlog::warn("D {} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}


MediaWorker::MediaWorker(caf::actor_config &cfg, const caf::actor_addr source)
    : data_source_(std::move(source)), caf::event_based_actor(cfg) {

    // for each input we spawn one media item with upto two media sources.


    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        // movie
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate,
            const bool /*movie*/) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            try {
                if (not jsn.at("attributes").at("sg_path_to_movie").is_null()) {
                    // spdlog::info("{}", i["attributes"]["sg_path_to_movie"]);
                    // prescan movie for duration..
                    // it may contain slate, which needs trimming..
                    // SLOW do we want to be offsetting the movie ?
                    // if we keep this code is needs threading..
                    auto uri = posix_path_to_uri(jsn["attributes"]["sg_path_to_movie"]);
                    const auto source_uuid = Uuid::generate();
                    auto source            = spawn<media::MediaSourceActor>(
                        "SG Movie", uri, media_rate, source_uuid);

                    request(source, infinite, media::acquire_media_detail_atom_v, media_rate)
                        .then(
                            [=](bool) mutable { rp.deliver(UuidActor(source_uuid, source)); },
                            [=](error &err) mutable {
                                // even though there is an error, we want the broken media
                                // source added so the user can see it in the UI (and its error
                                // state)
                                rp.deliver(UuidActor(source_uuid, source));
                            });

                } else {
                    rp.deliver(UuidActor());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                rp.deliver(UuidActor());
            }
            return rp;
        },

        // frames
        [=](media::add_media_source_atom,
            const JsonStore &jsn,
            const FrameRate &media_rate) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            try {
                if (not jsn.at("attributes").at("sg_path_to_frames").is_null()) {
                    FrameList frame_list;
                    caf::uri uri;

                    if (jsn.at("attributes").at("frame_range").is_null()) {
                        // no frame range specified..
                        // try and aquire from filesystem..
                        uri = parse_cli_posix_path(
                            jsn.at("attributes").at("sg_path_to_frames"), frame_list, true);
                    } else {
                        frame_list = FrameList(
                            jsn.at("attributes").at("frame_range").template get<std::string>());
                        uri = parse_cli_posix_path(
                            jsn.at("attributes").at("sg_path_to_frames"), frame_list, false);
                    }

                    const auto source_uuid = Uuid::generate();
                    auto source =
                        frame_list.empty()
                            ? spawn<media::MediaSourceActor>(
                                  "SG Frames", uri, media_rate, source_uuid)
                            : spawn<media::MediaSourceActor>(
                                  "SG Frames", uri, frame_list, media_rate, source_uuid);

                    request(source, infinite, media::acquire_media_detail_atom_v, media_rate)
                        .then(
                            [=](bool) mutable { rp.deliver(UuidActor(source_uuid, source)); },
                            [=](error &err) mutable {
                                // even though there is an error, we want the broken media
                                // source added so the user can see it in the UI (and its error
                                // state)
                                rp.deliver(UuidActor(source_uuid, source));
                            });
                } else {
                    rp.deliver(UuidActor());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
                rp.deliver(UuidActor());
            }

            return rp;
        },

        [=](playlist::add_media_atom,
            caf::actor media,
            JsonStore jsn,
            const FrameRate &media_rate) -> result<bool> {
            auto rp = make_response_promise<bool>();

            try {
                // do stupid stuff, because data integrity is for losers.
                // if we've got a movie in the sg_frames property, swap them over.
                if (jsn.at("attributes").at("sg_path_to_movie").is_null() and
                    not jsn.at("attributes").at("sg_path_to_frames").is_null() and
                    jsn.at("attributes")
                            .at("sg_path_to_frames")
                            .template get<std::string>()
                            .find_first_of('#') == std::string::npos) {
                    // movie in image sequence..
                    jsn["attributes"]["sg_path_to_movie"] =
                        jsn.at("attributes").at("sg_path_to_frames");
                    jsn["attributes"]["sg_path_to_frames"] = nullptr;
                }

                // request movie .. THESE MUST NOT RETURN error on fail.
                add_media_step_1(rp, media, jsn, media_rate);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }
            return rp;
        });
}
