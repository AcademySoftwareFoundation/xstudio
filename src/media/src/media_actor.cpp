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


void MediaActor::deserialise(const JsonStore &jsn) {
    if (not jsn.count("store") or jsn.at("store").is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), jsn.at("store"), std::chrono::milliseconds(50));
    }

    link_to(json_store_);

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
        [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::Uuid &) {}};
}

void MediaActor::init() {
    // only parial..
    event_group_ = spawn<broadcast::BroadcastActor>(this);
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

        [=](module::deserialise_atom, const utility::JsonStore &jsn) { deserialise(jsn); },

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookamrk_uuid) {
            send(
                event_group_,
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookamrk_uuid);
        },

        [=](utility::event_atom,
            bookmark::remove_bookmark_atom,
            const utility::Uuid &bookmark_uuid) {
            send(
                event_group_,
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
            } catch (const std::exception &err) {
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
            base_.add_media_source(source_media.uuid());
            auto_set_current_source(media::MT_IMAGE);
            auto_set_current_source(media::MT_AUDIO);
            base_.send_changed(event_group_, this);
            send(source_media.actor(), utility::parent_atom_v, UuidActor(base_.uuid(), this));

            send(
                event_group_,
                utility::event_atom_v,
                add_media_source_atom_v,
                UuidActorVector({source_media}));

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

        [=](add_media_source_atom, const UuidActorVector &sources) -> result<bool> {
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
                    auto_set_current_source(media::MT_IMAGE);
                    auto_set_current_source(media::MT_AUDIO);
                    send(
                        source_media.actor(),
                        utility::parent_atom_v,
                        UuidActor(base_.uuid(), this));
                }
            }

            base_.send_changed(event_group_, this);
            send(event_group_, utility::event_atom_v, add_media_source_atom_v, good_sources);

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
        [=](timeline::duration_atom, const timebase::flicks &new_duration) -> bool {
            return false;
        },
        [=](get_edit_list_atom atom,
            const MediaType media_type,
            const Uuid &uuid) -> caf::result<utility::EditList> {
            if (base_.empty() or not media_sources_.count(base_.current(media_type)))
                return make_error(xstudio_error::error, "No MediaSources");

            auto rp = make_response_promise<utility::EditList>();

            // Audio media source must have same duration as video media source.
            // We force that here.
            if (media_type == media::MT_IMAGE ||
                not media_sources_.count(base_.current(media::MT_IMAGE))) {
                rp.delegate(
                    media_sources_.at(base_.current(media_type)),
                    atom,
                    media_type,
                    (uuid.is_null() ? base_.uuid() : uuid));
            } else {

                request(
                    media_sources_.at(base_.current(media::MT_AUDIO)),
                    infinite,
                    atom,
                    media::MT_AUDIO,
                    uuid.is_null() ? base_.uuid() : uuid)
                    .then(
                        [=](utility::EditList audio_edl) mutable {
                            request(
                                media_sources_.at(base_.current(media::MT_IMAGE)),
                                infinite,
                                atom,
                                media::MT_IMAGE,
                                uuid.is_null() ? base_.uuid() : uuid)
                                .then(
                                    [=](const utility::EditList &video_edl) mutable {
                                        ClipList audio_clips = audio_edl.section_list();
                                        ClipList video_clips = video_edl.section_list();
                                        if (audio_clips.size() != video_clips.size()) {
                                            // audio doesn't match video so just return
                                            // unmodified audio EDL
                                            rp.deliver(audio_edl);
                                            return;
                                        }

                                        ClipList modified_audio_clips;
                                        for (size_t i = 0; i < audio_clips.size(); ++i) {
                                            modified_audio_clips.push_back(audio_clips[i]);
                                            modified_audio_clips.back()
                                                .frame_rate_and_duration_.set_seconds(
                                                    video_clips[i]
                                                        .frame_rate_and_duration_.seconds());
                                            modified_audio_clips.back().timecode_ =
                                                video_clips[i].timecode_;
                                        }
                                        rp.deliver(utility::EditList(modified_audio_clips));
                                    },
                                    [=](error &err) mutable { rp.deliver(err); });
                        },
                        [=](error &err) mutable { rp.deliver(err); });
            }


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
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMap> {
            auto rp = make_response_promise<media::FrameTimeMap>();

            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                get_edit_list_atom_v,
                media_type,
                utility::Uuid())
                .then(
                    [=](const utility::EditList &edl) mutable {
                        const auto clip           = edl.section_list()[0];
                        const int num_clip_frames = clip.frame_rate_and_duration_.frames(
                            tsm == TimeSourceMode::FIXED ? override_rate : FrameRate());
                        const utility::Timecode tc = clip.timecode_;

                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            atom,
                            media_type,
                            media::LogicalFrameRanges({{0, num_clip_frames - 1}}),
                            override_rate)
                            .then(
                                [=](const media::AVFrameIDs &ids) mutable {
                                    media::FrameTimeMap result;
                                    auto time_point = timebase::flicks(0);
                                    for (int f = 0; f < num_clip_frames; f++) {
                                        result[time_point] = ids[f];
                                        const_cast<media::AVFrameID *>(result[time_point].get())
                                            ->playhead_logical_frame_ = f;
                                        const_cast<media::AVFrameID *>(ids[f].get())
                                            ->timecode_ = tc + f;
                                        time_point +=
                                            tsm == TimeSourceMode::FIXED
                                                ? override_rate
                                                : clip.frame_rate_and_duration_.rate();
                                    }
                                    rp.deliver(result);
                                },
                                [=](error &err) mutable { rp.deliver(err); });
                    },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

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
                        request(
                            media_sources_.at(base_.current(media_type)),
                            infinite,
                            atom,
                            media_type,
                            ranges)
                            .then(
                                [=](const media::AVFrameIDs &ids) mutable { rp.deliver(ids); },
                                [=](error &err) mutable { rp.deliver(err); });
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

        [=](media::source_offset_frames_atom) -> int {
            // needed for SubPlayhead when playing media direct
            return 0;
        },

        [=](media::source_offset_frames_atom, const int) -> bool {
            // needed for SubPlayhead when playing media direct
            return false;
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
                            rp.delegate(media_sources_.at(base_.current()), atom, path);
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
            rp.delegate(media_sources_.at(base_.current()), atom, path);
            return rp;
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

        [=](utility::event_atom, media_status_atom, const MediaStatus ms) {
            // one of our sources has changed status..
            if (media_sources_.count(base_.current()) and
                media_sources_[base_.current()] == current_sender()) {
                // spdlog::warn(
                //     "media_status_atom {} {}",
                //     to_string(current_sender()),
                //     static_cast<int>(ms));
                send(event_group_, utility::event_atom_v, media_status_atom_v, ms);
            }
        },

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
    const std::string &source_name,
    const media::MediaType media_type,
    const bool auto_select_source_if_failed) {

    try {

        // here we check that the named source can provide a stream of typer
        // media_type

        scoped_actor sys{system()};
        auto sources     = utility::map_value_to_vec(media_sources_);
        auto source_uuid = utility::Uuid();
        for (auto &source : sources) {
            auto source_container_detail =
                utility::request_receive<ContainerDetail>(*sys, source, detail_atom_v);
            auto source_stream_details = utility::request_receive<std::vector<ContainerDetail>>(
                *sys, source, detail_atom_v, media_type);

            if (source_container_detail.name_ == source_name &&
                !source_stream_details.empty()) {
                // name matches and the source has a stream of type media_type
                source_uuid = source_container_detail.uuid_;
                break;
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

void MediaActor::auto_set_current_source(const media::MediaType media_type) {

    auto set_current = [=](const media::MediaType mt, const utility::Uuid &uuid) mutable {
        if (base_.set_current(uuid, mt)) {
            send(
                event_group_,
                utility::event_atom_v,
                current_media_source_atom_v,
                UuidActor(
                    base_.current(media_type), media_sources_.at(base_.current(media_type))),
                media_type);
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
                }
            }
        };

    // TODO: do these requests asynchronously, as it could be heavy and slow
    // loading of big playlists etc
    
    std::set<utility::Uuid> sources_matching_media_type;
    caf::scoped_actor sys(system());

    for (auto source_uuid : base_.media_sources()) {

        auto source_actor = media_sources_[source_uuid];

        try {
            auto stream_details = request_receive<std::vector<ContainerDetail>>(
                *sys,
                source_actor,
                detail_atom_v,
                media_type);

            if (stream_details.size())
                sources_matching_media_type.insert(source_uuid);
        } catch (...) {}
    }

    auto_set_sources_mt(sources_matching_media_type);

}
