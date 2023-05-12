// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <chrono>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
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


MediaActor::MediaActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn["base"])) {

    if (not jsn.count("store") or jsn["store"].is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            static_cast<JsonStore>(jsn["store"]),
            std::chrono::milliseconds(50));
    }

    link_to(json_store_);

    init();

    for (const auto &[key, value] : jsn["actors"].items()) {
        if (value["base"]["container"]["type"] == "MediaSource") {
            auto ukey = Uuid(key);
            try {

                media_sources_[ukey] =
                    system().spawn<media::MediaSourceActor>(static_cast<JsonStore>(value));
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
                    "{} {} Removing source {} {}",
                    __PRETTY_FUNCTION__,
                    e.what(),
                    key,
                    to_string(base_.uuid()));
            }
        }
    }
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

    init();

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

        media_sources_[i.uuid()] = i.actor();
        link_to(i.actor());
        join_event_group(this, i.actor());
        base_.add_media_source(i.uuid());
        send(i.actor(), utility::parent_atom_v, UuidActor(base_.uuid(), this));
    }
}

caf::message_handler MediaActor::default_event_handler() {
    return {
        [=](utility::event_atom, current_media_source_atom, const utility::UuidActor &) {},
        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},
        [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::Uuid &) {}};
}

void MediaActor::init() {
    // only parial..
    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    print_on_create(this, base_);
    print_on_exit(this, base_);

    behavior_.assign(
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // [=](bookmark::associate_bookmark_atom atom, caf::actor bookmarks) {
        //     delegate(bookmarks, atom, UuidActor(base_.uuid(), actor_cast<caf::actor>(this)));
        // },

        [=](utility::event_atom atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookamrk_uuid) {
            send(
                event_group_,
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookamrk_uuid);
        },

        [=](utility::event_atom atom,
            bookmark::remove_bookmark_atom,
            const utility::Uuid &bookamrk_uuid) {
            send(
                event_group_,
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookamrk_uuid);
        },

        [=](media_status_atom atom, const MediaStatus status) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();
            try {
                rp.delegate(media_sources_.at(base_.current()), atom, status);
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }
            return rp;
        },

        [=](media_status_atom atom) -> caf::result<MediaStatus> {
            auto rp = make_response_promise<MediaStatus>();

            try {
                rp.delegate(media_sources_.at(base_.current()), atom);
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](add_media_source_atom, const UuidActor &source_media) -> result<Uuid> {
            if (source_media.uuid().is_null() or not source_media.actor()) {
                spdlog::warn(
                    "{} Invalid source {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(source_media.uuid()),
                    to_string(source_media.actor()));
                return make_error(xstudio_error::error, "Invalid MediaSource");
            }

            media_sources_[source_media.uuid()] = source_media.actor();
            link_to(source_media.actor());
            join_event_group(this, source_media.actor());
            base_.send_changed(event_group_, this);
            send(source_media.actor(), utility::parent_atom_v, UuidActor(base_.uuid(), this));
            base_.add_media_source(source_media.uuid());
            return source_media.uuid();
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

        [=](add_media_source_atom, const std::vector<UuidActor> &sources) -> result<bool> {
            for (const auto &source_media : sources) {
                if (source_media.uuid().is_null() or not source_media.actor()) {
                    spdlog::warn(
                        "{} Invalid source {} {}",
                        __PRETTY_FUNCTION__,
                        to_string(source_media.uuid()),
                        to_string(source_media.actor()));
                    continue;
                }

                media_sources_[source_media.uuid()] = source_media.actor();
                link_to(source_media.actor());
                join_event_group(this, source_media.actor());
                send(
                    source_media.actor(),
                    utility::parent_atom_v,
                    UuidActor(base_.uuid(), this));
                base_.add_media_source(source_media.uuid());
            }
            base_.send_changed(event_group_, this);

            if (media_sources_.count(base_.current())) {
                send(
                    event_group_,
                    utility::event_atom_v,
                    current_media_source_atom_v,
                    utility::UuidActor(base_.current(), media_sources_.at(base_.current())),
                    MT_IMAGE);
            }
            return true;
        },

        [=](add_media_source_atom,
            const caf::uri &uri,
            const FrameList &frame_list,
            const utility::FrameRate &rate) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            std::string ext =
                ltrim_char(to_upper(fs::path(uri_to_posix_path(uri)).extension()), '.');
            const auto source_uuid = Uuid::generate();

            auto source =
                frame_list.empty()
                    ? spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, rate, source_uuid)
                    : spawn<media::MediaSourceActor>(
                          (ext.empty() ? "UNKNOWN" : ext), uri, frame_list, rate, source_uuid);

            anon_send(source, media_metadata::get_metadata_atom_v);
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
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](colour_pipeline::set_colour_pipe_params_atom atom,
            const utility::JsonStore &params) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();

            try {
                rp.delegate(media_sources_.at(base_.current()), atom, params);
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, "No MediaSources"));
            }

            return rp;
        },

        [=](current_media_source_atom) -> caf::result<UuidActor> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            return UuidActor(base_.current(), media_sources_.at(base_.current()));
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
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    current_media_source_atom_v,
                    UuidActor(
                        base_.current(media_type),
                        media_sources_.at(base_.current(media_type))),
                    media_type);
            }

            return result;
        },

        [=](current_media_source_atom atom, const Uuid &uuid) {
            auto result = base_.set_current(uuid, MT_IMAGE);
            result |= base_.set_current(uuid, MT_AUDIO);

            // might need this when adding initial media source ?
            // do we need to specify which media type is changing ?
            if (result) {
                base_.send_changed(event_group_, this);
                send(
                    event_group_,
                    utility::event_atom_v,
                    current_media_source_atom_v,
                    UuidActor(base_.current(), media_sources_.at(base_.current())),
                    MT_IMAGE);
            }

            return result;
        },

        [=](get_edit_list_atom atom, const Uuid &uuid) -> caf::result<utility::EditList> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<utility::EditList>();
            rp.delegate(
                media_sources_.at(base_.current()),
                atom,
                (uuid.is_null() ? base_.uuid() : uuid));
            return rp;
        },

        [=](get_media_pointer_atom atom) -> caf::result<std::vector<media::AVFrameID>> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<std::vector<media::AVFrameID>>();
            rp.delegate(media_sources_.at(base_.current()), atom);
            return rp;
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
            const int logical_frame) -> caf::result<media::AVFrameID> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<media::AVFrameID>();
            rp.delegate(media_sources_.at(base_.current()), atom, logical_frame);
            return rp;
        },

        [=](get_media_pointer_atom atom,
            const std::vector<std::pair<int, utility::time_point>> &logical_frames) {
            delegate(caf::actor_cast<actor>(this), atom, MT_IMAGE, logical_frames);
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
        // const int num_frames,
        // const int start_frame,

        [=](get_media_pointers_atom atom,
            const MediaType media_type,
            const LogicalFrameRanges &ranges,
            const FrameRate &override_rate) -> caf::result<media::AVFrameIDs> {
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
                        rp.delegate(
                            media_sources_.at(base_.current(media_type)),
                            atom,
                            media_type,
                            ranges);
                    },
                    [=](error &err) mutable { rp.deliver(err); });
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
                    event_group_,
                    utility::event_atom_v,
                    playlist::reflag_container_atom_v,
                    base_.uuid(),
                    std::make_tuple(base_.flag(), base_.flag_text()));
                base_.send_changed(event_group_, this);
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

        [=](get_media_source_names_atom)
            -> caf::result<std::vector<std::pair<utility::Uuid, std::string>>> {
            if (base_.empty())
                return std::vector<std::pair<utility::Uuid, std::string>>();

            auto rp =
                make_response_promise<std::vector<std::pair<utility::Uuid, std::string>>>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(media_sources_), infinite, utility::detail_atom_v)
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
            rp.delegate(media_sources_.at(base_.current()), atom, path);
            return rp;
        },

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
            const FrameRate &default_rate) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            // auto m_hook_actor = system().registry().template
            // get<caf::actor>(media_hook_registry); if (m_hook_actor) {
            //     anon_send(
            //         m_hook_actor,
            //         media_hook::gather_media_sources_atom_v,
            //         caf::actor_cast<caf::actor>(this)
            //         );
            // }

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current()), atom, default_rate);
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

        [=](media_metadata::get_metadata_atom atom,
            const int sequence_frame) -> caf::result<bool> {
            if (base_.empty() or not media_sources_.count(base_.current()))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<bool>();
            rp.delegate(media_sources_.at(base_.current()), atom, sequence_frame);
            return rp;
        },

        [=](playhead::media_source_atom, const std::string &source_name) -> result<bool> {
            // switch to the named media source. For example, source name might
            // be "EXR" or "h264" etc if we have a media item that wraps several
            // transcodes of the same video/render
            auto rp = make_response_promise<bool>();
            switch_current_source_to_named_source(rp, source_name, source_name);
            return rp;
        },

        [=](playhead::media_source_atom,
            const std::string &visual_source_name,
            const std::string &audio_source_name) -> result<bool> {
            // switch to the named media source. For example, source name might
            // be "EXR" or "h264" etc if we have a media item that wraps several
            // transcodes of the same video/render
            auto rp = make_response_promise<bool>();
            switch_current_source_to_named_source(rp, visual_source_name, audio_source_name);
            return rp;
        },

        [=](utility::duplicate_atom atom) {
            delegate(caf::actor_cast<caf::actor>(this), atom, caf::actor(), caf::actor());
        },
        [=](utility::duplicate_atom,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks) -> result<UuidUuidActor> {
            auto rp = make_response_promise<UuidUuidActor>();

            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<MediaActor>(base_.name(), uuid);

            caf::scoped_actor sys(system());
            auto json =
                request_receive<JsonStore>(*sys, json_store_, json_store::get_json_atom_v);
            anon_send(actor, json_store::set_json_atom_v, Uuid(), json, "");
            anon_send(
                actor,
                playlist::reflag_container_atom_v,
                std::make_tuple(
                    std::optional<std::string>(base_.flag()),
                    std::optional<std::string>(base_.flag_text())));
            // sometimes caf makes my head bleed. see below.

            auto source_count = std::make_shared<int>();
            (*source_count)   = base_.media_sources().size();
            for (const Uuid i : base_.media_sources()) {

                request(media_sources_.at(i), infinite, utility::duplicate_atom_v)
                    .then(
                        [=](UuidActor media_src) mutable {
                            request(actor, infinite, add_media_source_atom_v, media_src.actor())
                                .then(
                                    [=](const Uuid &) mutable {
                                        if (i == base_.current()) {
                                            anon_send(
                                                actor,
                                                current_media_source_atom_v,
                                                media_src.uuid());
                                        }
                                        (*source_count)--;
                                        if (!*source_count) {
                                            // done!
                                            auto ua = UuidActor(uuid, actor);
                                            if (src_bookmarks)
                                                clone_bookmarks_to(
                                                    ua, src_bookmarks, dst_bookmarks);
                                            rp.deliver(UuidUuidActor(base_.uuid(), ua));
                                        }
                                    },
                                    [=](const error &err) mutable { rp.deliver(err); });
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
            }


            return rp;
        },

        [=](utility::event_atom, add_media_stream_atom, const UuidActor &) {
            base_.send_changed(event_group_, this);
        },

        [=](utility::event_atom,
            media_metadata::get_metadata_atom,
            const utility::JsonStore &) {},

        [=](utility::event_atom, utility::change_atom, const bool) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__,
            // to_string(caf::actor_cast<caf::actor>(this)));
            pending_change_ = false;
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
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
        });
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
    const utility::UuidActor &ua, caf::actor src_bookmark, caf::actor dst_bookmark) {
    // use global session (should be safe.. ish)
    try {
        scoped_actor sys{system()};
        try {
            UuidActorVector copied;
            // // duplicate bookmarks if any
            auto bookmarks = utility::request_receive<UuidActorVector>(
                *sys, src_bookmark, bookmark::get_bookmarks_atom_v, base_.uuid());
            for (const auto &i : bookmarks) {
                copied.push_back(utility::request_receive<UuidActor>(
                    *sys, src_bookmark, utility::duplicate_atom_v, i.uuid(), ua));
            }
            utility::request_receive<bool>(
                *sys, dst_bookmark, bookmark::add_bookmark_atom_v, copied);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void MediaActor::switch_current_source_to_named_source(
    caf::typed_response_promise<bool> rp,
    const std::string &visual_source_name,
    const std::string &audio_source_name) {

    fan_out_request<policy::select_all>(
        utility::map_value_to_vec(media_sources_), infinite, detail_atom_v)
        .then(
            [=](std::vector<ContainerDetail> details) mutable {
                auto audio_uuid  = utility::Uuid();
                auto visual_uuid = utility::Uuid();

                for (const auto &i : details) {
                    if (not visual_source_name.empty() and visual_uuid.is_null() and
                        i.name_ == visual_source_name)
                        visual_uuid = i.uuid_;

                    if (not audio_source_name.empty() and audio_uuid.is_null() and
                        i.name_ == audio_source_name)
                        audio_uuid = i.uuid_;
                    // spdlog::warn("{}",  i.name_);
                }

                if (visual_uuid.is_null() and not visual_source_name.empty()) {
                    std::stringstream ss;
                    ss << "Tried to switch to named media source \"" << visual_source_name
                       << "\" but no such source was found.";
                    return rp.deliver(make_error(xstudio_error::error, ss.str()));
                }

                if (audio_uuid.is_null() and not audio_source_name.empty()) {
                    std::stringstream ss;
                    ss << "Tried to switch to named media source \"" << audio_source_name
                       << "\" but no such source was found.";
                    return rp.deliver(make_error(xstudio_error::error, ss.str()));
                }

                // now have the two uuids..
                if (not visual_uuid.is_null()) {
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        current_media_source_atom_v,
                        visual_uuid,
                        MT_IMAGE)
                        .then(
                            [=](const bool result) mutable {
                                if (not audio_uuid.is_null()) {
                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        current_media_source_atom_v,
                                        audio_uuid,
                                        MT_AUDIO)
                                        .then(
                                            [=](const bool result) mutable {
                                                return rp.deliver(result);
                                            },
                                            [=](error &err) mutable {
                                                if (rp.pending())
                                                    rp.deliver(err);
                                            });

                                    // set audio as well
                                } else {
                                    return rp.deliver(result);
                                }
                            },
                            [=](error &err) mutable {
                                if (rp.pending())
                                    rp.deliver(err);
                            });
                } else if (not audio_uuid.is_null()) {
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        current_media_source_atom_v,
                        audio_uuid,
                        MT_AUDIO)
                        .then(
                            [=](const bool result) mutable { return rp.deliver(result); },
                            [=](error &err) mutable {
                                if (rp.pending())
                                    rp.deliver(err);
                            });
                } else {
                    rp.deliver(true);
                }
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });

    // std::map<utility::Uuid, caf::actor> media_sources_


    // auto n_responses = std::make_shared<int>(0);
    // const std::list<utility::Uuid> source_uuids = base_.media_sources();
    // auto source_count = std::make_shared<int>(source_uuids.size());

    // for (auto source_uuid: source_uuids) {

    //     request(
    //         media_sources_[source_uuid],
    //         infinite,
    //         utility::name_atom_v).await(
    //             [=](const std::string & name) mutable {

    //                 if (name != source_name) {
    //                     (*source_count)--;
    //                     if (!(*source_count) && rp.pending()) {
    //                         std::stringstream ss;
    //                         ss << "Tried to switch to named media source \"" << source_name
    //                         << "\" but no such source was found.";
    //                         rp.deliver(make_error(xstudio_error::error, ss.str()));
    //                     }
    //                     return;
    //                 }
    //                 request(
    //                     caf::actor_cast<caf::actor>(this),
    //                     infinite,
    //                     current_media_source_atom_v,
    //                     source_uuid).then(
    //                         [=](bool r) mutable {
    //                             if (rp.pending())
    //                                 rp.deliver(r);
    //                         },
    //                         [=](error &err) mutable {
    //                             if (rp.pending())
    //                                 rp.deliver(err);
    //                         });

    //             },
    //             [=](error &err) mutable {
    //                 if (rp.pending())
    //                     rp.deliver(err);
    //             }
    //         );
    // }
}
