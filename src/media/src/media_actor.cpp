// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <chrono>
#include <tuple>
#include <regex>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media;
using namespace xstudio::media_metadata;
using namespace caf;


void MediaActor::deserialise(const JsonStore &jsn) {
    if (not jsn.count("store") or jsn.at("store").is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), jsn.at("store"), std::chrono::milliseconds(50));
    }

    link_to(json_store_);
    // join out metadata handler. propagate events from it.
    join_event_group(this, json_store_);

    for (const auto &[key, value] : jsn.at("actors").items()) {
        if (value.at("base").at("container").at("type") == "MediaSource") {
            auto ukey = Uuid(key);
            try {

                media_sources_[ukey] =
                    spawn<media::MediaSourceActor>(static_cast<JsonStore>(value));
                send(
                    media_sources_[ukey],
                    utility::parent_atom_v,
                    UuidActor(base_.uuid(), this));
                link_to(media_sources_[ukey]);
                join_event_group(this, media_sources_[ukey]);
            } catch (const std::exception &e) {
                base_.remove_media_source(ukey);
                // do we need this ?
                media_sources_.erase(ukey);
                spdlog::warn(
                    "{} {} Removing source {} {} {}",
                    __PRETTY_FUNCTION__,
                    e.what(),
                    key,
                    to_string(base_.uuid()),
                    value.dump(2));
            }
        }
    }
}

MediaActor::MediaActor(caf::actor_config &cfg, const JsonStore &jsn, const bool async)
    : caf::event_based_actor(cfg), base_(JsonStore(jsn.at("base"))) {

    init();

    if (async)
        anon_send(this, module::deserialise_atom_v, jsn);
    else
        deserialise(jsn);
}

MediaActor::MediaActor(
    caf::actor_config &cfg,
    const std::string &name,
    const utility::Uuid &uuid,
    const utility::UuidActorVector &srcs)
    : caf::event_based_actor(cfg), base_(name) {

    if (not uuid.is_null())
        base_.set_uuid(uuid);

    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    link_to(json_store_);

    // join out metadata handler. propagate events from it.
    join_event_group(this, json_store_);

    init();

    utility::UuidActorVector good_sources;

    for (const auto &i : srcs) {
        // validate..
        if (i.uuid().is_null() or not i.actor()) {
            spdlog::warn(
                "{} Invalid source {} {}",
                __PRETTY_FUNCTION__,
                to_string(i.uuid()),
                to_string(i.actor()));
            continue;
        }
        good_sources.push_back(i);
    }

    anon_send(caf::actor_cast<caf::actor>(this), add_media_source_atom_v, good_sources);
}

caf::message_handler MediaActor::default_event_handler() {
    return {
        [=](utility::event_atom, current_media_source_atom, const utility::UuidActor &) {},
        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},
        [=](utility::event_atom, add_media_source_atom, const utility::UuidActorVector &) {},
        [=](utility::event_atom, media_status_atom, const MediaStatus ms) {},
        [=](utility::event_atom, media_status_atom, const MediaStatus ms) {},
        [=](json_store::update_atom,
            const JsonStore &,
            const std::string &,
            const JsonStore &) {},
        [=](json_store::update_atom, const JsonStore &) {},
        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &) {
        }};
}

caf::message_handler MediaActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // [=](bookmark::associate_bookmark_atom atom, caf::actor bookmarks) {
        //     delegate(bookmarks, atom, UuidActor(base_.uuid(), actor_cast<caf::actor>(this)));
        // },

        [=](module::deserialise_atom, const utility::JsonStore &jsn) { deserialise(jsn); },

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &metadata_filter_sets) {
            media_list_columns_config_ =
                utility::json_to_tree(metadata_filter_sets, "children");
            anon_send(this, media_display_info_atom_v);
        },

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookamrk_uuid) {
            if (std::find(bookmark_uuids_.begin(), bookmark_uuids_.end(), bookamrk_uuid) ==
                bookmark_uuids_.end()) {
                bookmark_uuids_.emplace_back(bookamrk_uuid);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    bookmark::bookmark_change_atom_v,
                    base_.uuid(),
                    bookmark_uuids_);
            }
            send(
                base_.event_group(),
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookamrk_uuid);
        },

        [=](bookmark::get_bookmarks_atom) -> utility::UuidList { return bookmark_uuids_; },

        [=](utility::event_atom,
            bookmark::remove_bookmark_atom,
            const utility::Uuid &bookmark_uuid) {
            auto p = std::find(bookmark_uuids_.begin(), bookmark_uuids_.end(), bookmark_uuid);
            if (p != bookmark_uuids_.end()) {
                bookmark_uuids_.erase(p);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    bookmark::bookmark_change_atom_v,
                    base_.uuid(),
                    bookmark_uuids_);
            }

            send(
                base_.event_group(),
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookmark_uuid);
        },

        [=](rescan_atom atom) -> result<MediaReference> {
            if (base_.empty())
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaReference>();

            try {
                rp.delegate(media_sources_.at(base_.current()), atom);
            } catch ([[maybe_unused]] const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](decompose_atom) -> result<UuidActorVector> {
            if (media_sources_.empty())
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<UuidActorVector>();

            // duplicate all sources.
            fan_out_request<policy::select_all>(
                map_value_to_vec(media_sources_), infinite, utility::duplicate_atom_v)
                .then(
                    [=](UuidActorVector uav) mutable {
                        // create media from each source.
                        UuidActorVector new_media;

                        for (const auto &i : uav) {
                            auto uuid  = utility::Uuid::generate();
                            auto media = spawn<media::MediaActor>(
                                "Decomposed Media", uuid, utility::UuidActorVector({i}));
                            new_media.emplace_back(UuidActor(uuid, media));
                        }

                        rp.deliver(new_media);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](detail_atom, const bool sources) -> caf::result<std::vector<ContainerDetail>> {
            if (base_.empty())
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::vector<ContainerDetail>>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(media_sources_), infinite, detail_atom_v)
                .then(
                    [=](std::vector<ContainerDetail> details) mutable {
                        auto result = std::vector<ContainerDetail>();
                        std::map<Uuid, int> lookup;
                        for (size_t i = 0; i < details.size(); i++)
                            lookup[details[i].uuid_] = i;

                        // order results based on base_.media_sources()
                        for (const auto &i : base_.media_sources()) {
                            if (lookup.count(i))
                                result.push_back(details[lookup[i]]);
                        }
                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](media_status_atom atom, const MediaStatus status) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();
            try {
                rp.delegate(media_sources_.at(base_.current()), atom, status);
            } catch ([[maybe_unused]] const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }
            return rp;
        },

        [=](media_status_atom atom) -> caf::result<MediaStatus> {
            auto rp = make_response_promise<MediaStatus>();
            try {
                rp.delegate(media_sources_.at(base_.current()), atom);
            } catch ([[maybe_unused]] const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](add_media_source_atom, const UuidActor &source_media) -> result<Uuid> {
            auto rp = make_response_promise<Uuid>();

            if (source_media.uuid().is_null() or not source_media.actor()) {
                spdlog::warn(
                    "{} Invalid source {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(source_media.uuid()),
                    to_string(source_media.actor()));
                rp.deliver(make_error(xstudio_error::error, "Invalid MediaSource"));
                return rp;
            }

            media_sources_[source_media.uuid()] = source_media.actor();
            link_to(source_media.actor());
            join_event_group(this, source_media.actor());
            base_.add_media_source(source_media.uuid());
            request(caf::actor_cast<caf::actor>(this), infinite, playhead::media_source_atom_v)
                .then(
                    [=](bool) mutable {
                        base_.send_changed();
                        send(
                            source_media.actor(),
                            utility::parent_atom_v,
                            UuidActor(base_.uuid(), this));

                        send(
                            base_.event_group(),
                            utility::event_atom_v,
                            add_media_source_atom_v,
                            UuidActorVector({source_media}));

                        rp.deliver(source_media.uuid());
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](add_media_source_atom atom, caf::actor source_media) -> result<Uuid> {
            auto rp = make_response_promise<Uuid>();
            request(source_media, infinite, uuid_atom_v)
                .then(
                    [=](const Uuid &uuid) mutable {
                        rp.delegate(
                            actor_cast<caf::actor>(this), atom, UuidActor(uuid, source_media));
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](add_media_source_atom, const UuidActorVector &sources) -> result<bool> {
            auto rp = make_response_promise<bool>();

            UuidActorVector good_sources;

            for (const auto &source_media : sources) {
                if (source_media.uuid().is_null() or not source_media.actor()) {
                    spdlog::warn(
                        "{} Invalid source {} {}",
                        __PRETTY_FUNCTION__,
                        to_string(source_media.uuid()),
                        to_string(source_media.actor()));
                } else {
                    good_sources.push_back(source_media);
                    media_sources_[source_media.uuid()] = source_media.actor();
                    link_to(source_media.actor());
                    join_event_group(this, source_media.actor());
                    base_.add_media_source(source_media.uuid());
                    send(
                        source_media.actor(),
                        utility::parent_atom_v,
                        UuidActor(base_.uuid(), this));
                    send(
                        base_.event_group(),
                        utility::event_atom_v,
                        add_media_source_atom_v,
                        UuidActorVector({source_media}));
                }
            }

            base_.send_changed();
            send(
                base_.event_group(),
                utility::event_atom_v,
                add_media_source_atom_v,
                good_sources);

            // select a current media source if necessary
            request(caf::actor_cast<caf::actor>(this), infinite, playhead::media_source_atom_v)
                .then(
                    [=](bool) mutable {
                        base_.send_changed();
                        rp.deliver(true);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](add_media_source_atom,
            const caf::uri &uri,
            const FrameList &frame_list,
            const utility::FrameRate &rate) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            std::string ext =
                ltrim_char(to_upper(get_path_extension(fs::path(uri_to_posix_path(uri)))), '.');
            const auto source_uuid = Uuid::generate();

            auto source =
                frame_list.empty()
                    ? spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, rate, source_uuid)
                    : spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, frame_list, rate, source_uuid);

            // anon_send(source, media_metadata::get_metadata_atom_v);

            request(
                actor_cast<caf::actor>(this),
                infinite,
                add_media_source_atom_v,
                UuidActor(source_uuid, source))
                .then(
                    [=](const Uuid &uuid) mutable { rp.deliver(UuidActor(uuid, source)); },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](add_media_source_atom,
            const std::vector<utility::MediaReference> &source_refs,
            const std::vector<std::string> &source_names,
            const std::string preferred_source_name) -> result<bool> {
            // this is fiddly ... 'source_refs' has MediaReference from media
            // sources that are already part of this media_actor, but it also
            // might have new MediaReferences. As well as adding any new
            // media sources we want the order of media_sources to match the
            // order in source_refs. We also want the names to reflect
            // source_names. Here goes ...

            request(caf::actor_cast<caf::actor>(this), infinite, media_reference_atom_v)
                .then(
                    [=](const std::vector<utility::MediaReference>
                            &existing_source_refs) mutable {
                        auto p = source_refs.begin();
                        auto q = source_names.begin();
                        while (p != source_refs.end() && q != source_names.end()) {

                            add_or_rename_media_source(
                                *p, *q, existing_source_refs, preferred_source_name == *q);
                            p++;
                            q++;
                        }
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });

            // OK f*ck it I'll do the re-ordering later. headache
            return true;
        },

        [=](colour_pipeline::get_colour_pipe_params_atom atom) -> caf::result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            try {
                rp.delegate(media_sources_.at(base_.current()), atom);
            } catch ([[maybe_unused]] const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](colour_pipeline::set_colour_pipe_params_atom atom,
            const utility::JsonStore &params) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();

            try {
                rp.delegate(media_sources_.at(base_.current()), atom, params);
            } catch ([[maybe_unused]] const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](current_media_source_atom, const bool current_names)
            -> caf::result<std::pair<UuidActor, std::pair<std::string, std::string>>> {
            auto rp = make_response_promise<
                std::pair<UuidActor, std::pair<std::string, std::string>>>();

            if (base_.empty() or not media_sources_.count(base_.current())) {
                rp.deliver(std::make_pair(
                    UuidActor(base_.uuid(), this),
                    std::make_pair(std::string(), std::string())));
            }

            auto gtype = MT_IMAGE;
            if (not media_sources_.count(base_.current(MT_IMAGE)))
                gtype = MT_AUDIO;

            request(media_sources_.at(base_.current(gtype)), infinite, name_atom_v)
                .then(
                    [=](const std::string &name) mutable {
                        if (base_.current(gtype) == base_.current(MT_AUDIO) or
                            not media_sources_.count(base_.current(MT_AUDIO))) {
                            rp.deliver(std::make_pair(
                                UuidActor(base_.uuid(), this), std::make_pair(name, name)));
                        } else {
                            request(
                                media_sources_.at(base_.current(MT_AUDIO)),
                                infinite,
                                name_atom_v)
                                .then(
                                    [=](const std::string &aname) mutable {
                                        rp.deliver(std::make_pair(
                                            UuidActor(base_.uuid(), this),
                                            std::make_pair(name, aname)));
                                    },
                                    [=](error &err) mutable {
                                        rp.deliver(std::make_pair(
                                            UuidActor(base_.uuid(), this),
                                            std::make_pair(std::string(), std::string())));
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    },
                    [=](error &err) mutable {
                        rp.deliver(std::make_pair(
                            UuidActor(base_.uuid(), this),
                            std::make_pair(std::string(), std::string())));
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });

            return rp;
        },

        // dead..
        // [=](current_media_source_atom) -> caf::result<UuidActor> {
        //     if (base_.empty() or not media_sources_.count(base_.current()))
        //         return make_error(xstudio_error::error, "No MediaSources");

        //     return UuidActor(base_.current(), media_sources_.at(base_.current()));
        // },


        [=](current_media_source_atom)
            -> caf::result<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>> {
            auto iua = UuidActor();
            auto aua = UuidActor();
            if (media_sources_.count(base_.current(MT_IMAGE)))
                iua = UuidActor(
                    base_.current(MT_IMAGE), media_sources_.at(base_.current(MT_IMAGE)));

            if (media_sources_.count(base_.current(MT_AUDIO)))
                aua = UuidActor(
                    base_.current(MT_AUDIO), media_sources_.at(base_.current(MT_AUDIO)));

            if (iua.uuid().is_null())
                iua = aua;
            else if (aua.uuid().is_null())
                aua = iua;

            return std::make_pair(UuidActor(base_.uuid(), this), std::make_pair(iua, aua));
        },

        [=](current_media_source_atom, const MediaType media_type) -> caf::result<UuidActor> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            return UuidActor(
                base_.current(media_type), media_sources_.at(base_.current(media_type)));
        },

        [=](current_media_source_atom, const Uuid &uuid, const MediaType media_type) -> bool {
            auto result = base_.set_current(uuid, media_type);
            // might need this when adding initial media source ?
            // do we need to specify which media type is changing ?
            if (result) {
                base_.send_changed();
                anon_send(this, media_display_info_atom_v);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    current_media_source_atom_v,
                    UuidActor(
                        base_.current(media_type),
                        media_sources_.at(base_.current(media_type))),
                    media_type);
            }

            return result;
        },

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            thumbnail::ThumbnailBufferPtr buf) {},

        [=](media_reader::get_thumbnail_atom,
            float position) -> result<thumbnail::ThumbnailBufferPtr> {
            if (base_.empty() or not media_sources_.count(base_.current(MT_IMAGE)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<thumbnail::ThumbnailBufferPtr>();
            request(
                media_sources_.at(base_.current(media::MT_IMAGE)),
                infinite,
                media_reader::get_thumbnail_atom_v,
                position)
                .then(
                    [=](thumbnail::ThumbnailBufferPtr &buf) mutable {
                        rp.deliver(buf);
                        send(
                            base_.event_group(),
                            utility::event_atom_v,
                            media_reader::get_thumbnail_atom_v,
                            buf);
                    },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_reader::get_thumbnail_atom,
            float position) -> result<thumbnail::ThumbnailBufferPtr> {
            if (base_.empty() or not media_sources_.count(base_.current(MT_IMAGE)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<thumbnail::ThumbnailBufferPtr>();
            request(
                media_sources_.at(base_.current(media::MT_IMAGE)),
                infinite,
                media_reader::get_thumbnail_atom_v,
                position)
                .then(
                    [=](thumbnail::ThumbnailBufferPtr &buf) mutable {
                        rp.deliver(buf);
                        send(
                            base_.event_group(),
                            utility::event_atom_v,
                            media_reader::get_thumbnail_atom_v,
                            buf);
                    },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](current_media_source_atom, const Uuid &uuid) -> bool {
            auto result = base_.set_current(uuid, MT_IMAGE);
            result |= base_.set_current(uuid, MT_AUDIO);

            // might need this when adding initial media source ?
            // do we need to specify which media type is changing ?
            if (result) {
                anon_send(this, media_display_info_atom_v);
                anon_send(this, human_readable_info_atom_v, true);
                base_.send_changed();
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    current_media_source_atom_v,
                    UuidActor(base_.current(), media_sources_.at(base_.current())),
                    MT_IMAGE);
            }

            return result;
        },
        [=](timeline::duration_atom, const timebase::flicks &new_duration) -> bool {
            return false;
        },

        [=](get_media_pointer_atom atom,
            const MediaType media_type) -> caf::result<std::vector<media::AVFrameID>> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::vector<media::AVFrameID>>();
            rp.delegate(media_sources_.at(base_.current(media_type)), atom, media_type);
            return rp;
        },

        [=](get_media_pointer_atom atom,
            const MediaType media_type,
            const int logical_frame) -> caf::result<media::AVFrameID> {

            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");
            auto rp = make_response_promise<media::AVFrameID>();
            rp.delegate(
                media_sources_.at(base_.current(media_type)), atom, media_type, logical_frame);

            return rp;
        },

        [=](get_media_pointers_atom atom,
            const MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
            auto rp = make_response_promise<media::FrameTimeMapPtr>();

            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");
            rp.delegate(
                media_sources_.at(base_.current(media_type)), atom, media_type, tsm, override_rate);
                
            return rp;
        },

        [=](get_media_pointers_atom atom,
            const MediaType media_type,
            const LogicalFrameRanges &ranges,
            const FrameRate &override_rate,
            const utility::Uuid &clip_uuid) -> caf::result<media::AVFrameIDs> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<media::AVFrameIDs>();
            // we need to ensure the media detail has been acquired before
            // we can deliver AVFrameIDs
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                acquire_media_detail_atom_v,
                override_rate)
                .then(
                    [=](bool) mutable {
                        request(
                            media_sources_.at(base_.current(media_type)),
                            infinite,
                            atom,
                            media_type,
                            ranges,
                            clip_uuid)
                            .then(
                                [=](const media::AVFrameIDs &ids) mutable { rp.deliver(ids); },
                                [=](error &err) mutable { rp.deliver(err); });
                    },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_pointers_atom atom,
            const MediaType media_type,
            const LogicalFrameRanges &ranges,
            const FrameRate &override_rate) {
            // no clip ID provided
            delegate(
                caf::actor_cast<caf::actor>(this),
                atom,
                media_type,
                ranges,
                override_rate,
                utility::Uuid());
        },

        [=](utility::rate_atom,
            const media::MediaType media_type) -> caf::result<FrameRate> {

            auto rp = make_response_promise<FrameRate>();
            request(caf::actor_cast<caf::actor>(this), infinite, media_reference_atom_v, media_type).then(
                [=](const MediaReference &ref) mutable {
                    rp.deliver(ref.rate());
                },
                [=](error &err) mutable { rp.deliver(err); });
            
            return rp;
        },

        [=](media_reference_atom atom,
            const media::MediaType media_type) -> caf::result<MediaReference> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaReference>();
            rp.delegate(
                media_sources_.at(base_.current(media_type)),
                atom);
            return rp;
        },

        [=](media_reference_atom atom,
            const media::MediaType media_type,
            const Uuid &uuid) -> caf::result<std::pair<Uuid, MediaReference>> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::pair<Uuid, MediaReference>>();
            rp.delegate(
                media_sources_.at(base_.current(media_type)),
                atom,
                (uuid.is_null() ? base_.uuid() : uuid));
            return rp;
        },

        [=](media_reference_atom) -> caf::result<std::vector<MediaReference>> {
            if (base_.empty())
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::vector<MediaReference>>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(media_sources_),
                infinite,
                media_reference_atom_v,
                utility::Uuid())
                .then(
                    [=](std::vector<std::pair<Uuid, MediaReference>> refs) mutable {
                        auto result = std::vector<MediaReference>();
                        auto l      = vpair_to_map(refs);
                        // order results based on base_.media_sources()
                        for (const auto &i : base_.media_sources()) {
                            if (l.count(i))
                                result.push_back(l[i]);
                        }
                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](media_reference_atom atom,
            const Uuid &uuid) -> caf::result<std::pair<Uuid, MediaReference>> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::pair<Uuid, MediaReference>>();
            rp.delegate(
                media_sources_.at(base_.current()),
                atom,
                (uuid.is_null() ? base_.uuid() : uuid));
            return rp;
        },

        [=](playlist::reflag_container_atom) -> std::tuple<std::string, std::string> {
            return std::make_tuple(base_.flag(), base_.flag_text());
        },

        [=](playlist::reflag_container_atom,
            const std::string &flag_colour,
            std::string &flag_text) {
            delegate(
                actor_cast<caf::actor>(this),
                playlist::reflag_container_atom_v,
                std::make_tuple(
                    std::optional<std::string>(flag_colour),
                    std::optional<std::string>(flag_text)));
        },


        [=](playlist::reflag_container_atom,
            const std::tuple<std::optional<std::string>, std::optional<std::string>>
                &flag_tuple) -> bool {
            auto [flag, text] = flag_tuple;
            auto changed      = false;
            if (flag and base_.flag() != *flag) {
                changed = true;
                base_.set_flag(*flag);
            }

            if (text and base_.flag_text() != *text) {
                changed = true;
                base_.set_flag_text(*text);
            }

            if (changed) {
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    playlist::reflag_container_atom_v,
                    base_.uuid(),
                    std::make_tuple(base_.flag(), base_.flag_text()));
                base_.send_changed();
            }
            return changed;
        },

        [=](get_media_source_atom) -> std::vector<UuidActor> {
            std::vector<UuidActor> sm;
            for (const auto &i : media_sources_)
                sm.emplace_back(i.first, i.second);
            return sm;
        },

        [=](get_media_source_atom, const Uuid &uuid) -> caf::result<caf::actor> {
            if (media_sources_.count(uuid))
                return media_sources_.at(uuid);
            return make_error(xstudio_error::error, "Invalid MediaSource Uuid");
        },

        [=](get_media_source_atom, const Uuid &uuid, bool) -> caf::result<caf::actor> {
            if (media_sources_.count(uuid))
                return media_sources_.at(uuid);
            return caf::actor();
        },

        [=](get_media_source_names_atom)
            -> caf::result<std::vector<std::pair<utility::Uuid, std::string>>> {
            if (base_.empty())
                return std::vector<std::pair<utility::Uuid, std::string>>();

            auto rp =
                make_response_promise<std::vector<std::pair<utility::Uuid, std::string>>>();

            // make a vector of our MediaSourceActor(s)
            auto sources = map_value_to_vec(media_sources_);

            fan_out_request<policy::select_all>(sources, infinite, utility::detail_atom_v)
                .then(
                    [=](std::vector<ContainerDetail> refs) mutable {
                        auto l      = std::map<utility::Uuid, std::string>();
                        auto result = std::vector<std::pair<utility::Uuid, std::string>>();

                        for (const auto &i : refs)
                            l[i.uuid_] = i.name_;

                        result.reserve(l.size());

                        // order results based on base_.media_sources()
                        for (const auto &i : base_.media_sources()) {
                            if (l.count(i))
                                result.emplace_back(std::make_pair(i, l[i]));
                        }
                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](media::checksum_atom) -> result<std::pair<std::string, uintmax_t>> {
            auto rp = make_response_promise<std::pair<std::string, uintmax_t>>();

            if (base_.empty())
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            else
                rp.delegate(media_sources_.at(base_.current(MT_IMAGE)), media::checksum_atom_v);

            return rp;
        },

        [=](media::checksum_atom,
            const media::MediaType mt) -> result<std::pair<std::string, uintmax_t>> {
            auto rp = make_response_promise<std::pair<std::string, uintmax_t>>();

            if (base_.empty())
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            else
                rp.delegate(media_sources_.at(base_.current(mt)), media::checksum_atom_v);

            return rp;
        },

        [=](current_media_stream_atom, const MediaType media_type) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            if (base_.empty())
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            else
                rp.delegate(
                    media_sources_.at(base_.current(media_type)),
                    media::current_media_stream_atom_v,
                    media_type);

            return rp;
        },

        [=](current_media_stream_atom,
            const MediaType media_type,
            const Uuid &uuid) -> result<bool> {
            auto rp = make_response_promise<bool>();
            if (base_.empty())
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            else
                rp.delegate(
                    media_sources_.at(base_.current(media_type)),
                    media::current_media_stream_atom_v,
                    media_type,
                    uuid);

            return rp;
        },

        [=](get_media_stream_atom,
            const MediaType media_type) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<UuidActor>>();
            if (base_.empty())
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            else
                rp.delegate(
                    media_sources_.at(base_.current(media_type)),
                    media::get_media_stream_atom_v,
                    media_type);

            return rp;
        },

        [=](get_media_source_names_atom, const media::MediaType mt)
            -> caf::result<std::vector<std::pair<utility::Uuid, std::string>>> {
            if (base_.empty())
                return std::vector<std::pair<utility::Uuid, std::string>>();

            auto rp =
                make_response_promise<std::vector<std::pair<utility::Uuid, std::string>>>();

            // make a vector of our MediaSourceActor(s)
            auto sources = map_value_to_vec(media_sources_);

            // here we remove sources that don't contain any streams matching 'mt'
            // i.e. mt = MT_IMAGE and a source only provides audio streams then
            // we exclude it from 'sources'
            caf::scoped_actor sys(system());
            auto p = sources.begin();
            while (p != sources.end()) {
                auto stream_details = request_receive<std::vector<ContainerDetail>>(
                    *sys, *p, utility::detail_atom_v, mt);
                if (stream_details.empty())
                    p = sources.erase(p);
                else
                    p++;
            }

            if (sources.empty()) {
                // no streams of MediaType == mt
                rp.deliver(std::vector<std::pair<utility::Uuid, std::string>>());
                return rp;
            }

            fan_out_request<policy::select_all>(sources, infinite, utility::detail_atom_v)
                .then(
                    [=](std::vector<ContainerDetail> refs) mutable {
                        auto l      = std::map<utility::Uuid, std::string>();
                        auto result = std::vector<std::pair<utility::Uuid, std::string>>();

                        for (const auto &i : refs)
                            l[i.uuid_] = i.name_;

                        result.reserve(l.size());

                        // order results based on base_.media_sources()
                        for (const auto &i : base_.media_sources()) {
                            if (l.count(i))
                                result.emplace_back(std::make_pair(i, l[i]));
                        }
                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](get_media_type_atom atom, const MediaType media_type) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current(media_type)), atom, media_type);
            return rp;
        },

        [=](get_stream_detail_atom atom,
            const MediaType media_type) -> caf::result<StreamDetail> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<StreamDetail>();
            rp.delegate(media_sources_.at(base_.current(media_type)), atom, media_type);
            return rp;
        },

        [=](json_store::get_json_atom atom,
            const utility::Uuid &uuid,
            const std::string &path,
            const bool /*fanout*/) -> caf::result<std::pair<UuidActor, JsonStore>> {
            if (uuid.is_null() or uuid == base_.uuid()) {
                auto rp = make_response_promise<std::pair<UuidActor, JsonStore>>();
                request(json_store_, infinite, atom, path)
                    .then(
                        [=](const JsonStore &jsn) mutable {
                            rp.deliver(std::make_pair(UuidActor(base_.uuid(), this), jsn));
                        },
                        [=](error &err) mutable {
                            rp.deliver(
                                std::make_pair(UuidActor(base_.uuid(), this), JsonStore()));
                        });
                return rp;
            } else if (media_sources_.count(uuid)) {
                auto rp = make_response_promise<std::pair<UuidActor, JsonStore>>();
                request(media_sources_.at(uuid), infinite, atom, path)
                    .then(
                        [=](const JsonStore &jsn) mutable {
                            rp.deliver(
                                std::make_pair(UuidActor(uuid, media_sources_.at(uuid)), jsn));
                        },
                        [=](error &err) mutable {
                            rp.deliver(std::make_pair(
                                UuidActor(uuid, media_sources_.at(uuid)), JsonStore()));
                        });
                return rp;
            }
            return make_error(xstudio_error::error, "Invalid MediaSource Uuid");
        },

        [=](json_store::get_json_atom atom,
            const std::string &path,
            bool try_source_actors) -> caf::result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            if (!try_source_actors) {
                rp.delegate(caf::actor_cast<caf::actor>(this), atom, utility::Uuid(), path);
            } else {
                request(json_store_, infinite, atom, path)
                    .then(
                        [=](const JsonStore &r) mutable { rp.deliver(r); },
                        [=](error &) mutable {
                            // our own store doesn't have data at 'path'. Try the
                            // current media source as a fallback
                            rp.delegate(media_sources_.at(base_.current()), atom, path, true);
                        });
            }
            return rp;
        },

        [=](json_store::get_json_atom atom,
            const utility::Uuid &uuid,
            const std::string &path) -> caf::result<JsonStore> {
            if (uuid.is_null() or uuid == base_.uuid()) {
                auto rp = make_response_promise<JsonStore>();
                rp.delegate(json_store_, atom, path);
                return rp;
            } else if (media_sources_.count(uuid)) {
                auto rp = make_response_promise<JsonStore>();
                rp.delegate(media_sources_.at(uuid), atom, path);
                return rp;
            }
            return make_error(xstudio_error::error, "No MediaSources");
        },

        [=](json_store::get_json_atom atom, const std::string &path) -> caf::result<JsonStore> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<JsonStore>();
            rp.delegate(json_store_, atom, path);
            return rp;
        },

        /*[=](json_store::get_json_atom atom, const std::string &path) -> caf::result<JsonStore>
        { if (base_.empty() or not media_sources_.count(base_.current())) return
        make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<JsonStore>();
            rp.delegate(media_sources_.at(base_.current()), atom, path);
            return rp;
        },*/

        [=](json_store::set_json_atom atom,
            const utility::Uuid &uuid,
            const JsonStore &json,
            const std::string &path) -> caf::result<bool> {
            if (uuid.is_null() or uuid == base_.uuid()) {
                auto rp = make_response_promise<bool>();
                rp.delegate(json_store_, atom, json, path);
                return rp;
            } else if (media_sources_.count(uuid)) {
                auto rp = make_response_promise<bool>();
                rp.delegate(media_sources_.at(uuid), atom, json, path);
                return rp;
            }
            return make_error(xstudio_error::error, "No MediaSources");
        },

        [=](json_store::set_json_atom atom,
            const JsonStore &json,
            const std::string &path) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current())) {
                return make_error(xstudio_error::error, "No MediaSources");
            }

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current()), atom, json, path);
            return rp;
        },

        [=](media::acquire_media_detail_atom atom,
            const FrameRate default_rate) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();

            // first, make sure we have a media source
            request(caf::actor_cast<caf::actor>(this), infinite, playhead::media_source_atom_v)
                .then(
                    [=](bool) mutable {
                        // ensures media sources have had their details filled in and
                        // we've set the media sources (image and audio) where possible
                        if (base_.empty() or not media_sources_.count(base_.current()))
                            return rp.deliver(
                                make_error(xstudio_error::error, "No MediaSources"));

                        rp.delegate(media_sources_.at(base_.current()), atom, default_rate);
                    },
                    [=](caf::error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(err);
                    });

            return rp;
        },

        [=](media::invalidate_cache_atom atom) -> caf::result<MediaKeyVector> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaKeyVector>();
            rp.delegate(media_sources_.at(base_.current()), atom);
            return rp;
        },

        [=](media_cache::keys_atom atom) -> caf::result<MediaKeyVector> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaKeyVector>();
            rp.delegate(media_sources_.at(base_.current()), atom);
            return rp;
        },

        [=](media_cache::keys_atom atom,
            const MediaType media_type) -> caf::result<MediaKeyVector> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaKeyVector>();
            rp.delegate(media_sources_.at(base_.current(media_type)), atom, media_type);
            return rp;
        },

        [=](media_cache::keys_atom atom,
            const MediaType media_type,
            const int logical_frame) -> caf::result<MediaKey> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaKey>();
            rp.delegate(
                media_sources_.at(base_.current(media_type)), atom, media_type, logical_frame);
            return rp;
        },

        [=](media_cache::keys_atom atom,
            const MediaType media_type,
            const std::vector<int> &logical_frames) -> caf::result<MediaKeyVector> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<MediaKeyVector>();
            rp.delegate(
                media_sources_.at(base_.current(media_type)), atom, media_type, logical_frames);
            return rp;
        },

        [=](media_hook::get_media_hook_atom atom) -> caf::result<bool> {
            if (base_.empty())
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<bool>();
            std::vector<caf::actor> sources;
            for (const auto &i : media_sources_)
                sources.push_back(i.second);

            fan_out_request<policy::select_all>(
                sources, infinite, media_hook::get_media_hook_atom_v)
                .then(
                    [=](std::vector<bool>) mutable { rp.deliver(true); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_metadata::get_metadata_atom atom) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current()), atom);
            return rp;
        },

        [=](utility::event_atom,
            media_metadata::get_metadata_atom,
            const utility::JsonStore &) {},

        [=](media_metadata::get_metadata_atom atom,
            const int sequence_frame) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current()), atom, sequence_frame);
            return rp;
        },

        [=](playhead::media_source_atom) -> result<bool> {
            // ensures media sources have had their details filled in and
            // we've set the media sources (image and audio) where possible
            auto rp = make_response_promise<bool>();
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                playhead::media_source_atom_v,
                media::MT_IMAGE)
                .then(
                    [=](bool) mutable {
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            playhead::media_source_atom_v,
                            media::MT_AUDIO)
                            .then(
                                [=](bool) mutable { rp.deliver(true); },
                                [=](caf::error &err) mutable { rp.deliver(err); });
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](playhead::media_source_atom, const media::MediaType mt) -> result<bool> {
            // ensures media sources have had their details filled in and
            // we've set the media sources (image OR audio) where possible
            auto rp = make_response_promise<bool>();
            if (base_.empty() or not media_sources_.count(base_.current(mt))) {
                auto_set_current_source(mt, rp);
            } else {
                rp.deliver(true);
            }
            return rp;
        },

        [=](playhead::media_source_atom,
            const std::string &source_name,
            const media::MediaType mt) -> result<bool> {
            // switch to the named media source. For example, source name might
            // be "EXR" or "h264" etc if we have a media item that wraps several
            // transcodes of the same video/render
            auto rp = make_response_promise<bool>();
            switch_current_source_to_named_source(rp, source_name, mt);
            return rp;
        },

        [=](playhead::media_source_atom,
            const std::string &source_name,
            const media::MediaType mt,
            const bool auto_select_source_if_failed) -> result<bool> {
            // switch to the named media source. For example, source name might
            // be "EXR" or "h264" etc if we have a media item that wraps several
            // transcodes of the same video/render
            auto rp = make_response_promise<bool>();
            switch_current_source_to_named_source(
                rp, source_name, mt, auto_select_source_if_failed);
            return rp;
        },

        [=](utility::duplicate_atom) -> result<UuidUuidActor> {
            auto rp = make_response_promise<UuidUuidActor>();
            duplicate(rp, caf::actor(), caf::actor());
            return rp;
        },

        [=](utility::duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks) -> result<UuidUuidActor> {
            auto rp = make_response_promise<UuidUuidActor>();
            duplicate(rp, src_bookmarks, dst_bookmarks);
            return rp;
        },

        [=](utility::event_atom, add_media_stream_atom, const UuidActor &) {
            base_.send_changed();
        },

        [=](utility::event_atom, media_status_atom, const MediaStatus ms) {
            // one of our sources has changed status..
            if (media_sources_.count(base_.current()) and
                media_sources_[base_.current()] == current_sender()) {
                // spdlog::warn(
                //     "media_status_atom {} {}",
                //     to_string(current_sender()),
                //     static_cast<int>(ms));
                send(base_.event_group(), utility::event_atom_v, media_status_atom_v, ms);
            }
        },

        [=](utility::event_atom, utility::change_atom, const bool) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__,
            // to_string(caf::actor_cast<caf::actor>(this)));
            pending_change_ = false;
            // update any display info about this media item used in the
            // front end
            anon_send(this, media_display_info_atom_v);
            send(base_.event_group(), utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom, utility::change_atom) {
            // propagate changes upwards
            if (not pending_change_) {
                pending_change_ = true;
                delayed_send(
                    this,
                    std::chrono::milliseconds(250),
                    utility::event_atom_v,
                    change_atom_v,
                    true);
            }
        },

        [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
            // propagate changes upwards
            send(this, utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            request(json_store_, infinite, json_store::get_json_atom_v, "")
                .then(
                    [=](const JsonStore &meta) mutable {
                        std::vector<caf::actor> clients;

                        for (const auto &i : media_sources_)
                            clients.push_back(i.second);

                        if (not clients.empty()) {
                            fan_out_request<policy::select_all>(
                                clients, infinite, serialise_atom_v)
                                .then(
                                    [=](std::vector<JsonStore> json) mutable {
                                        JsonStore jsn;
                                        jsn["base"]   = base_.serialise();
                                        jsn["store"]  = meta;
                                        jsn["actors"] = {};
                                        for (const auto &j : json)
                                            jsn["actors"][static_cast<std::string>(
                                                j["base"]["container"]["uuid"])] = j;

                                        rp.deliver(jsn);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} Dead Media Source {}",
                                            __PRETTY_FUNCTION__,
                                            to_string(err));
                                        // dump sources..
                                        for (const auto &[i, j] : media_sources_)
                                            spdlog::warn("{} {}", to_string(i), to_string(j));
                                        rp.deliver(std::move(err));
                                    });
                        } else {
                            JsonStore jsn;
                            jsn["store"]  = meta;
                            jsn["base"]   = base_.serialise();
                            jsn["actors"] = {};
                            rp.deliver(jsn);
                        }
                    },
                    [=](error &err) mutable {
                        spdlog::warn(
                            "{} Dead JsonStore {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(std::move(err));
                    });
            return rp;
        },

        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            if (current_sender() == json_store_)
                send(base_.event_group(), json_store::update_atom_v, change, path, full);
        },

        [=](json_store::update_atom, const JsonStore &full) mutable {
            if (current_sender() == json_store_)
                send(base_.event_group(), json_store::update_atom_v, full);
        },

        [=](media_display_info_atom,
            const utility::JsonStore &item_query_info) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            display_info_item(item_query_info, rp);
            return rp;
        },
        [=](human_readable_info_atom, bool update) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            update_human_readable_details(rp);
            return rp;
        },
        [=](media_display_info_atom) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();
            request(
                caf::actor_cast<caf::actor>(this), infinite, human_readable_info_atom_v, true)
                .then(
                    [=](const utility::JsonStore &human_readable) mutable {
                        build_media_list_info(rp);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        }};
}


void MediaActor::init() {
    // only parial..
    print_on_create(this, base_);
    print_on_exit(this, base_);

    // get updates from the GlobalMetadataManager instance that manages the
    // filters used to get metadata from the MediaActors to the UI layer.
    auto metadata_filter_actor = home_system().registry().template get<caf::actor>(
        global_media_metadata_manager_registry);

    // this message kicks the metadata filter actor to send us the dictionary
    // for filtering our metadata
    send(metadata_filter_actor, media::media_display_info_atom_v, true);
    join_event_group(this, metadata_filter_actor);
}

void MediaActor::add_or_rename_media_source(
    const utility::MediaReference &ref,
    const std::string &name,
    const std::vector<utility::MediaReference> &existing_source_refs,
    const bool set_as_current_source) {

    const std::list<utility::Uuid> existing_source_uuids = base_.media_sources();
    auto source_uuid                                     = existing_source_uuids.begin();

    for (const auto &existing_ref : existing_source_refs) {
        if (source_uuid == existing_source_uuids.end())
            break; // shouldn't happen!
        if (existing_ref == ref) {

            if (media_sources_.count(*source_uuid)) {
                anon_send(media_sources_.at(*source_uuid), utility::name_atom_v, name);
            }
            if (set_as_current_source) {
                anon_send(
                    caf::actor_cast<caf::actor>(this),
                    current_media_source_atom_v,
                    *source_uuid);
            }
            return;
        }
        source_uuid++;
    }

    auto uuid = utility::Uuid::generate();
    auto new_source =
        spawn<MediaSourceActor>(name, ref.uri(), ref.frame_list(), ref.rate(), uuid);

    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        add_media_source_atom_v,
        utility::UuidActor(uuid, new_source))
        .then(
            [=](const Uuid &source_uuid) {
                if (set_as_current_source) {
                    anon_send(
                        caf::actor_cast<caf::actor>(this),
                        current_media_source_atom_v,
                        source_uuid);
                }
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void MediaActor::clone_bookmarks_to(
    caf::typed_response_promise<utility::UuidUuidActor> rp,
    const utility::UuidUuidActor &uua,
    caf::actor src_bookmark,
    caf::actor dst_bookmark) {
    // use global session (should be safe.. ish)
    // DON'T USE REQUEST RECEIVE!!

    request(src_bookmark, infinite, bookmark::get_bookmarks_atom_v, base_.uuid())
        .then(
            [=](const UuidActorVector &bookmarks) mutable {
                if (bookmarks.empty())
                    rp.deliver(uua);
                else {
                    auto copied = std::make_shared<UuidActorVector>();
                    auto count  = std::make_shared<int>(bookmarks.size());
                    for (const auto &i : bookmarks) {
                        request(
                            src_bookmark,
                            infinite,
                            utility::duplicate_atom_v,
                            i.uuid(),
                            uua.second)
                            .then(
                                [=](const UuidActor &new_bookmark) mutable {
                                    copied->push_back(new_bookmark);
                                    (*count)--;
                                    if (not(*count)) {
                                        anon_send(
                                            dst_bookmark,
                                            bookmark::add_bookmark_atom_v,
                                            *copied);
                                        rp.deliver(uua);
                                    }
                                },
                                [=](const error &err) mutable {
                                    (*count)--;
                                    if (not(*count)) {
                                        anon_send(
                                            dst_bookmark,
                                            bookmark::add_bookmark_atom_v,
                                            *copied);
                                        rp.deliver(uua);
                                    }
                                });
                    }
                }
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });


    // try {
    //     scoped_actor sys{system()};
    //     try {
    //         UuidActorVector copied;
    //         // // duplicate bookmarks if any
    //         auto bookmarks = utility::request_receive<UuidActorVector>(
    //             *sys, src_bookmark, bookmark::get_bookmarks_atom_v, base_.uuid());
    //         for (const auto &i : bookmarks) {
    //             copied.push_back(utility::request_receive<UuidActor>(
    //                 *sys, src_bookmark, utility::duplicate_atom_v, i.uuid(), ua));
    //         }
    //         utility::request_receive<bool>(
    //             *sys, dst_bookmark, bookmark::add_bookmark_atom_v, copied);
    //     } catch (const std::exception &err) {
    //         spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    //     }
    // } catch (const std::exception &err) {
    //     spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    // }
}

void MediaActor::switch_current_source_to_named_source(
    caf::typed_response_promise<bool> rp,
    const std::string &source_name,
    const media::MediaType media_type,
    const bool auto_select_source_if_failed) {

    try {

        // here we check that the named source can provide a stream of typer
        // media_type

        scoped_actor sys{system()};
        auto sources     = utility::map_value_to_vec(media_sources_);
        auto source_uuid = utility::Uuid();

        const utility::Uuid current_source_id = base_.current(media_type);

        for (auto &source : sources) {
            auto source_container_detail =
                utility::request_receive<ContainerDetail>(*sys, source, detail_atom_v);
            auto source_stream_details = utility::request_receive<std::vector<ContainerDetail>>(
                *sys, source, detail_atom_v, media_type);

            if (source_container_detail.name_ == source_name &&
                !source_stream_details.empty()) {
                // name matches and the source has a stream of type media_type
                if (source_uuid.is_null()) {
                    source_uuid = source_container_detail.uuid_;
                }

                // the name of the current source id matches 'source_name'.
                // Nothing to do!
                if (current_source_id == source_container_detail.uuid_) {
                    rp.deliver(true);
                    return;
                }
            }
        }

        if (auto_select_source_if_failed && source_uuid.is_null()) {

            // we have failed to match source_name with a source that has a stream
            // of MediaType media_type. Now try a fallback. Using a map that
            // will sort the sources by name and pick the first, to give a
            // consistent selection if we do this on lots of imported media, say.

            std::map<std::string, utility::Uuid> sources_with_required_stream;
            for (auto &source : sources) {

                auto source_container_detail =
                    utility::request_receive<ContainerDetail>(*sys, source, detail_atom_v);
                auto source_stream_details =
                    utility::request_receive<std::vector<ContainerDetail>>(
                        *sys, source, detail_atom_v, media_type);

                if (!source_stream_details.empty()) {
                    sources_with_required_stream[source_container_detail.name_] =
                        source_container_detail.uuid_;
                }
            }
            if (sources_with_required_stream.size()) {
                source_uuid = sources_with_required_stream.begin()->second;
            }
        }

        if (source_uuid.is_null() and not source_name.empty() and source_name != "N/A") {
            // we only error if a specific source name was supplied
            std::stringstream ss;
            ss << "Tried to switch to named media source \"" << source_name
               << "\" but no such source was found.";
            rp.deliver(make_error(xstudio_error::error, ss.str()));
        } else if (not source_uuid.is_null()) {
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                current_media_source_atom_v,
                source_uuid,
                media_type)
                .then(
                    [=](const bool result) mutable { return rp.deliver(result); },
                    [=](error &err) mutable {
                        if (rp.pending())
                            rp.deliver(err);
                    });
        } else {
            rp.deliver(true);
        }

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


void MediaActor::auto_set_current_source(
    const media::MediaType media_type, caf::typed_response_promise<bool> rp) {

    auto set_current = [=](const media::MediaType mt, const utility::Uuid &uuid) mutable {
        if (base_.set_current(uuid, mt)) {
            anon_send(this, media_display_info_atom_v);
            send(
                base_.event_group(),
                utility::event_atom_v,
                current_media_source_atom_v,
                UuidActor(
                    base_.current(media_type), media_sources_.at(base_.current(media_type))),
                media_type);
            if (rp.pending())
                rp.deliver(true);
        } else if (rp.pending()) {
            rp.deliver(false);
        }
    };

    auto auto_set_sources_mt =
        [=](const std::set<utility::Uuid> &valid_media_sources_for_media_type) mutable {
            if (base_.current(media_type).is_null()) {

                bool changed = false;
                for (auto source_uuid : base_.media_sources()) {

                    if (valid_media_sources_for_media_type.find(source_uuid) !=
                        valid_media_sources_for_media_type.end()) {
                        set_current(media_type, source_uuid);
                        break;
                    }
                }

                if (media_type == media::MT_IMAGE && base_.current(media::MT_IMAGE).is_null() &&
                    base_.media_sources().size()) {
                    // If we have been unable to set an image source, but we do have sources
                    // (which must be audio only) we set the audio only source as the image
                    // source - the reason is that xSTUDIO always needs and image source to set
                    // frame rate and duration, even if it can't provide images.
                    set_current(media::MT_IMAGE, base_.media_sources().front());
                } else if (rp.pending()) {
                    rp.deliver(false);
                }
            } else {
                rp.deliver(false);
            }
        };

    // TODO: do these requests asynchronously, as it could be heavy and slow
    // loading of big playlists etc


    auto sources_matching_media_type = std::make_shared<std::set<utility::Uuid>>();
    auto count                       = std::make_shared<int>(media_sources_.size());

    // caf::scoped_actor sys(system());
    if (base_.media_sources().empty()) {
        rp.deliver(false);
    } else {
        for (auto source_uuid : base_.media_sources()) {
            auto source_actor = media_sources_[source_uuid];

            request(source_actor, infinite, detail_atom_v, media_type)
                .then(
                    [=](const std::vector<ContainerDetail> &stream_details) mutable {
                        if (stream_details.size())
                            sources_matching_media_type->insert(source_uuid);

                        (*count)--;
                        if (not *count)
                            auto_set_sources_mt(*sources_matching_media_type);
                    },
                    [=](caf::error err) mutable {
                        (*count)--;
                        if (not *count)
                            auto_set_sources_mt(*sources_matching_media_type);
                    });

            // try {
            //     auto stream_details = request_receive<std::vector<ContainerDetail>>(
            //         *sys, source_actor, detail_atom_v, media_type);

            //     if (stream_details.size())
            //         sources_matching_media_type.insert(source_uuid);
            // } catch (...) {
            // }
        }
    }


    // auto_set_sources_mt(sources_matching_media_type);
}

void MediaActor::update_human_readable_details(
    caf::typed_response_promise<utility::JsonStore> rp) {

    // The goal with this function is make a simple dictionary with the most
    // useful info about the media - filename, duration, frame rate, resolution
    // etc. This is then used to (partly) make the 'display_info' data that
    // is used to show info about the media in the xSTUDIO UI, or perhaps used
    // in plguins via API

    // we're making 4 async requests to other actors, so we need to store the
    // result in a shared ptr captured by the lambda and deliver the response
    // only when each request has returned a response
    auto result         = std::make_shared<utility::JsonStore>();
    auto response_count = std::make_shared<int>(0);

    auto check_deliver = [=]() mutable {
        (*response_count)++;
        if (*response_count == 4) {
            if (human_readable_info_ != *result) {
                human_readable_info_ = *result;
                // rebuild our display info data
                anon_send(this, media_display_info_atom_v);
            }
            rp.deliver(human_readable_info_);
        }
    };

    // get info about IMAGE stream
    request(
        caf::actor_cast<caf::actor>(this), infinite, get_stream_detail_atom_v, media::MT_IMAGE)
        .then(
            [=](const StreamDetail &image_stream_detail) mutable {
                auto &r = *result;

                r["Duration / seconds - MediaSource (Image)"] =
                    image_stream_detail.duration_.seconds();
                r["Duration / frames - MediaSource (Image)"] =
                    image_stream_detail.duration_.frames();
                r["Frame Rate - MediaSource (Image)"] =
                    fmt::format("{:.2f}", image_stream_detail.duration_.rate().to_fps());
                r["Name - MediaSource (Image)"]       = image_stream_detail.name_;
                r["Resolution - MediaSource (Image)"] = fmt::format(
                    "{}x{}:{}",
                    image_stream_detail.resolution_.x,
                    image_stream_detail.resolution_.y,
                    image_stream_detail.pixel_aspect_);
                r["Pixel Aspect - MediaSource (Image)"] = image_stream_detail.pixel_aspect_;
                check_deliver();
            },
            [=](caf::error err) mutable { check_deliver(); });

    // get info about AUDIO stream
    request(
        caf::actor_cast<caf::actor>(this), infinite, get_stream_detail_atom_v, media::MT_AUDIO)
        .then(
            [=](const StreamDetail &audio_stream_detail) mutable {
                auto &r = *result;
                r["Duration / seconds - MediaSource (Audio)"] =
                    audio_stream_detail.duration_.seconds();
                r["Name - MediaSource (Audio)"] = audio_stream_detail.name_;
                check_deliver();
            },
            [=](caf::error err) mutable { check_deliver(); });

    // get info about IMAGE source
    if (media_sources_.count(base_.current(MT_IMAGE))) {
        request(media_sources_[base_.current(MT_IMAGE)], infinite, media_reference_atom_v)
            .then(
                [=](const MediaReference &ref) mutable {
                    auto &r = *result;

                    // The uri for frame based formats is a bit ugly ... here replace the frame
                    // expr with some hashes
                    static std::regex re(R"(\.\{\:[0-9]+d\}\.)");
                    r["File Path - MediaSource (Image)"] =
                        std::regex_replace(uri_to_posix_path(ref.uri()), re, ".####.");
                    r["Timecode - MediaSource (Image)"]    = ref.timecode().to_string();
                    r["Frame Range - MediaSource (Image)"] = to_string(ref.frame_list());

                    check_deliver();
                },
                [=](caf::error err) mutable { check_deliver(); });
    } else {
        check_deliver();
    }

    // get info about AUDIO source
    if (media_sources_.count(base_.current(MT_AUDIO))) {
        request(media_sources_[base_.current(MT_AUDIO)], infinite, media_reference_atom_v)
            .then(
                [=](const MediaReference &ref) mutable {
                    auto &r                                = *result;
                    r["File Path - MediaSource (Audio)"]   = uri_to_posix_path(ref.uri());
                    r["Timecode - MediaSource (Audio)"]    = ref.timecode().to_string();
                    r["Frame Range - MediaSource (Image)"] = to_string(ref.frame_list());
                    check_deliver();
                },
                [=](caf::error err) mutable { check_deliver(); });
    } else {
        check_deliver();
    }
}

void MediaActor::display_info_item(
    const JsonStore item_query_info, caf::typed_response_promise<utility::JsonStore> rp) {

    // fetch specific info about the media, current media source or current
    // media stream (either a metadata value or a value from the
    // human_readable_info_ json dict).
    //
    // item_query_info is json that is a segment of the
    // /ui/qml/media_list_column_configuration preference and tells us whether to
    // fetch a metadata value or on of our entries in the human_readable_info_
    // dictionary
    //
    // We also allow the facility to to a regex replace on the resulting metadata
    // value to format it in some way convenient for the media list.
    //
    // We can also define indefinite 'fallbacks' to try if our metadata fetch
    // fails to match something in the metadata.
    //
    // In this example for "File" we show a particular shotgun metadata value,
    // but if that isn't present the we fallback onto the media path, with a
    // regex to show the filename only and not the full filesystem path.
    /*{
        "title": "File",
        "metadata_path": "/metadata/shotgun/version/attributes/code",
        "data_type": "metadata",
        "size": 360,
        "object": "Media",
        "resizable": true,
        "sortable": true,
        "position": "left",
        "fallback": {
            "info_key": "File Path - MediaSource (Image)",
            "data_type": "media_standard_details",
            "regex_match": "(.+)\/([^\/]+)$",
            "regex_format": "$2",
            "fallback": {
                "info_key": "File Path - MediaSource (Audio)",
                "data_type": "media_standard_details",
                "regex_match": "(.+)\/([^\/]+)$",
                "regex_format": "$2"
            }
        }
    }
    */

    // lambda to recurse into the 'fallback'
    auto fallback = [=]() mutable {
        if (item_query_info.contains("children") && item_query_info["children"].size() &&
            item_query_info["children"].is_array()) {
            display_info_item(item_query_info["children"][0], rp);
        } else {
            rp.deliver(JsonStore());
        }
    };

    // lambda to apply optional regex replace formatting
    auto do_regex_format = [=](const JsonStore &data) -> JsonStore {
        if (item_query_info.contains("regex_match") &&
            item_query_info.contains("regex_format")) {
            try {
                std::regex re(item_query_info.value("regex_match", ""));
                auto fonk = JsonStore(std::regex_replace(
                    data.is_string() ? data.get<std::string>() : data.dump(),
                    re,
                    item_query_info.value("regex_format", "")));
                return fonk;
            } catch (const std::regex_error &e) {
                return JsonStore(e.what());
            }
        }
        return data;
    };

    const std::string data_type = item_query_info.value("data_type", "");
    if (data_type == "metadata") {

        const std::string metadata_path = item_query_info.value("metadata_path", "");

        // "object" should be Media, MediaSource or MediaStream to tell us
        // which one we should do the metadata search on. For example, codec
        // related metadata will be found on the MediaStream. File metadata
        // might be found on the MediaSource. Pipeline metadata (version stream
        // metadata) will be attached to Media.
        const std::string object = item_query_info.value("object", "");

        auto get_metadata_value = [=](caf::actor target) mutable {
            request(target, infinite, json_store::get_json_atom_v, metadata_path)
                .then(
                    [=](const JsonStore &data) mutable { rp.deliver(do_regex_format(data)); },
                    [=](caf::error &err) mutable { fallback(); });
        };

        if (object == "MediaSource (Image)" &&
            media_sources_.count(base_.current(media::MT_IMAGE))) {
            // ask for metadata from MediaSource
            get_metadata_value(media_sources_.at(base_.current(media::MT_IMAGE)));
        } else if (
            object == "MediaSource (Audio)" &&
            media_sources_.count(base_.current(media::MT_AUDIO))) {
            // ask for metadata from MediaSource
            get_metadata_value(media_sources_.at(base_.current(media::MT_AUDIO)));
        } else if (object == "Image Stream") {
            // ask for metadata from MediaStream - as it stands, the streams
            // have no metadata, by the way! The stream metadata all lives on
            // the media source.
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                current_media_stream_atom_v,
                media::MT_IMAGE)
                .then(
                    [=](caf::actor t) mutable { get_metadata_value(t); },
                    [=](caf::error &err) mutable { fallback(); });
        } else if (object == "Audio Stream") {
            // ask for metadata from MediaStream
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                current_media_stream_atom_v,
                media::MT_AUDIO)
                .then(
                    [=](caf::actor t) mutable { get_metadata_value(t); },
                    [=](caf::error &err) mutable { fallback(); });
        } else {
            // By defaul use media level json store
            get_metadata_value(json_store_);
        }
    } else if (data_type == "media_standard_details") {
        auto key = item_query_info.value("info_key", "");
        if (human_readable_info_.contains(key)) {
            rp.deliver(do_regex_format(human_readable_info_[key]));
        } else {
            fallback();
        }
    } else {
        fallback();
    }
}

inline void dump_tree3(const utility::JsonTree &node, const int depth = 0) {
    std::cerr << fmt::format("{:>{}} {}", " ", depth * 4, node.data().dump()) << "\n";
    for (const auto &i : node.base())
        dump_tree3(i, depth + 1);
}

void MediaActor::build_media_list_info(caf::typed_response_promise<utility::JsonStore> rp) {

    // empty json array for results of the filter sets
    auto result = std::make_shared<JsonStore>(R"([])"_json);

    // because do this by firing off a bunch of asynchronous requests, we count
    // the number of results we get until it matches num_metadata_filter_results_
    // before delivering the response promise
    auto result_countdown = std::make_shared<int>(media_list_columns_config_.size());

    auto check_if_finished = [=]() mutable {
        (*result_countdown)--;
        if (*result_countdown == 0) {
            // we've received reposnses to all the requests made in the
            // loop below. Boradcast
            if (media_list_columns_info_ != *result) {
                media_list_columns_info_ = *result;

                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    media_display_info_atom_v,
                    media_list_columns_info_);
            }
            rp.deliver(*result);
        }
    };

    // empty json array for results of the filter set items
    result->push_back(R"([])"_json);

    int item_idx = 0;

    for (const auto &j : media_list_columns_config_.base()) {

        (*result)[item_idx]                      = nlohmann::json();
        const utility::JsonStore metadata_filter = utility::tree_to_json(j);
        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            media_display_info_atom_v,
            metadata_filter)
            .then(
                [=](utility::JsonStore &data) mutable {
                    (*result)[item_idx] = data;
                    check_if_finished();
                },
                [=](caf::error &err) mutable { check_if_finished(); });
        item_idx++;
    }
}

void MediaActor::duplicate(
    caf::typed_response_promise<utility::UuidUuidActor> rp,
    caf::actor src_bookmarks,
    caf::actor dst_bookmarks) {
    auto uuid  = utility::Uuid::generate();
    auto actor = spawn<MediaActor>(base_.name(), uuid);

    // don't use request receive..
    request(json_store_, infinite, json_store::get_json_atom_v)
        .then(
            [=](const JsonStore &jsn) mutable {
                anon_send(actor, json_store::set_json_atom_v, Uuid(), jsn, "");
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });

    anon_send(
        actor,
        playlist::reflag_container_atom_v,
        std::make_tuple(
            std::optional<std::string>(base_.flag()),
            std::optional<std::string>(base_.flag_text())));
    // sometimes caf makes my head bleed. see below.

    // auto source_count = std::make_shared<int>();
    // (*source_count)   = base_.media_sources().size();

    fan_out_request<policy::select_all>(
        map_value_to_vec(media_sources_), infinite, utility::duplicate_atom_v)
        .then(
            [=](const std::vector<UuidUuidActor> dmedia_srcs) mutable {
                std::map<Uuid, UuidActor> dmedia_srcs_map;

                for (const auto &i : dmedia_srcs)
                    dmedia_srcs_map[i.first] = i.second;

                UuidActorVector new_media_srcs;

                for (const auto &i : base_.media_sources())
                    new_media_srcs.push_back(dmedia_srcs_map[i]);

                // bulk add srcs.
                request(actor, infinite, add_media_source_atom_v, new_media_srcs)
                    .then(
                        [=](const bool) mutable {
                            // set current source.

                            // anon_send(
                            //     actor,
                            //     current_media_source_atom_v,
                            //     dmedia_srcs_map[base_.current()].uuid());
                            request(
                                actor,
                                infinite,
                                current_media_source_atom_v,
                                dmedia_srcs_map[base_.current()].uuid())
                                .then(
                                    [=](const bool) mutable {
                                        auto uua =
                                            UuidUuidActor(base_.uuid(), UuidActor(uuid, actor));

                                        if (src_bookmarks)
                                            clone_bookmarks_to(
                                                rp, uua, src_bookmarks, dst_bookmarks);
                                        else
                                            rp.deliver(uua);
                                    },
                                    [=](const caf::error &err) mutable { rp.deliver(err); });
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });


    // for (const Uuid i : base_.media_sources()) {
    //     request(media_sources_.at(i), infinite, utility::duplicate_atom_v)
    //         .then(
    //             [=](UuidUuidActor media_src) mutable {
    //                 request(actor, infinite, add_media_source_atom_v,
    //                 media_src.second.actor())
    //                     .then(
    //                         [=](const Uuid &) mutable {
    //                             if (i == base_.current()) {
    //                                 anon_send(
    //                                     actor,
    //                                     current_media_source_atom_v,
    //                                     media_src.second.uuid());
    //                             }
    //                             (*source_count)--;
    //                             if (!*source_count) {
    //                                 // done!
    //                                 auto ua = UuidActor(uuid, actor);
    //                                 if (src_bookmarks)
    //                                     clone_bookmarks_to(
    //                                         ua, src_bookmarks, dst_bookmarks);
    //                                 rp.deliver(UuidUuidActor(base_.uuid(), ua));
    //                             }
    //                         },
    //                         [=](const error &err) mutable { rp.deliver(err); });
    //             },
    //             [=](const error &err) mutable { rp.deliver(err); });
    // }
}
