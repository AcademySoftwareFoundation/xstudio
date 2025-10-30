// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <chrono>
#include <regex>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace std::chrono_literals;
using namespace caf;

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media;
using namespace xstudio::media_reader;
using namespace xstudio::media_metadata;
using namespace xstudio::global_store;

#define ERR_HANDLER_FUNC                                                                       \
    [=](error &err) mutable { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); }


caf::message_handler MediaSourceActor::default_event_handler() {
    return {
        [=](utility::event_atom, media_status_atom, const MediaStatus) {},
        [=](utility::event_atom, change_atom) {},
        [=](json_store::update_atom,
            const JsonStore &,
            const std::string &,
            const JsonStore &) {},
        [=](json_store::update_atom, const JsonStore &) {},
        [=](utility::event_atom,
            media_metadata::get_metadata_atom,
            const utility::JsonStore &) {}};
}

MediaSourceActor::MediaSourceActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn["base"])), parent_() {
    if (not jsn.count("store") or jsn["store"].is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(),
            static_cast<JsonStore>(jsn["store"]),
            std::chrono::milliseconds(50));
        // What happens if media was added to the session before it was on-disk, but now it
        // is on disk? We need to re-scan for media metadata. Setting the
        // media_detadata_up_to_date_ flag here will allow for this.
        media_metadata_up_to_date_ =
            jsn["store"].contains("metadata") && jsn["store"]["metadata"].contains("media");
    }
    link_to(json_store_);
    join_event_group(this, json_store_);

    bool re_aquire_detail = false;
    for (const auto &[key, value] : jsn["actors"].items()) {
        if (value["base"]["container"]["type"] == "MediaStream") {
            try {
                media_streams_[Uuid(key)] =
                    system().spawn<media::MediaStreamActor>(static_cast<JsonStore>(value));
                link_to(media_streams_[Uuid(key)]);
                join_event_group(this, media_streams_[Uuid(key)]);

                // as of xSTUDIO v2 media detail has been extended to have
                // reoslution and pixel aspect info. If we're reading from
                // an older session file we need to update the media details
                re_aquire_detail |= !value["base"].contains("resolution");

            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }
    }

    if (re_aquire_detail) {
        update_media_detail();
    }

    init();
}

MediaSourceActor::MediaSourceActor(
    caf::actor_config &cfg,
    const std::string &name,
    const caf::uri &_uri,
    const FrameList &frame_list,
    const utility::FrameRate &rate,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, _uri, frame_list), parent_() {
    if (not uuid.is_null())
        base_.set_uuid(uuid);

    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    link_to(json_store_);
    join_event_group(this, json_store_);


    // need this on creation or other functions randomly fail, as streams aren't configured..
    anon_mail(acquire_media_detail_atom_v, rate).send(actor_cast<actor>(this));
    // acquire_detail(rate);

    init();
}

MediaSourceActor::MediaSourceActor(
    caf::actor_config &cfg,
    const std::string &name,
    const caf::uri &_uri,
    const utility::FrameRate &rate,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, _uri), parent_() {
    if (not uuid.is_null())
        base_.set_uuid(uuid);
    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    link_to(json_store_);
    join_event_group(this, json_store_);

    // need this on creation or other functions randomly fail, as streams aren't configured..
    anon_mail(acquire_media_detail_atom_v, rate).send(actor_cast<actor>(this));
    // acquire_detail(rate);

    init();
}

MediaSourceActor::MediaSourceActor(
    caf::actor_config &cfg,
    const std::string &name,
    const std::string &reader,
    const utility::MediaReference &media_reference,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, media_reference), parent_() {
    if (not uuid.is_null())
        base_.set_uuid(uuid);
    base_.set_reader(reader);
    json_store_ = spawn<json_store::JsonStoreActor>(
        utility::Uuid::generate(), utility::JsonStore(), std::chrono::milliseconds(50));
    link_to(json_store_);
    join_event_group(this, json_store_);

    // MediaReference mr = base_.media_reference();
    // mr.set_timecode_from_frames();
    // base_.set_media_reference(mr);

    // special case , when duplicating, as that'll suppy streams.
    // anon_mail(acquire_media_detail_atom_v,
    // media_reference.rate().send(actor_cast<actor>(this)));

    init();
}

#include <chrono>

void MediaSourceActor::update_media_detail() {

    // xstudio 2.0 extends 'StreamDetail' to include resolution and pixel
    // aspect data ... here we therefore rescan for StreamDetail
    try {
        auto gmra = system().registry().template get<caf::actor>(media_reader_registry);
        if (!gmra)
            throw std::runtime_error("No global media reader.");

        // pick middle frame to fetch detail from (e.g. EXR parts which are
        // 'streams' in xSTUDIO) - this is less likely to mismatch the rest
        // of the sequence than first frame which can be a slate frame made
        // by a different tool.
        int frame;
        auto _uri =
            base_.media_reference().uri(base_.media_reference().frame_count() / 2, frame);

        if (!_uri) {
            // ok, middle frame didn't work lets try the first frame
            // after all as a fallback
            _uri = base_.media_reference().uri(0, frame);
        }

        if (not _uri)
            throw std::runtime_error("Invalid frame index");

        mail(get_media_detail_atom_v, *_uri, actor_cast<actor_addr>(this))
            .request(gmra, infinite)
            .then(
                [=](const MediaDetail md) mutable {
                    for (auto strm : media_streams_) {

                        mail(get_stream_detail_atom_v)
                            .request(strm.second, infinite)
                            .then(
                                [=](const StreamDetail &old_detail) {
                                    for (const auto &stream_detail : md.streams_) {
                                        if (stream_detail.name_ == old_detail.name_ &&
                                            stream_detail.media_type_ ==
                                                old_detail.media_type_) {
                                            // update the media stream actor's details
                                            mail(stream_detail).send(strm.second);
                                        }
                                    }
                                },
                                [=](const error &err) mutable {
                                    spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                    }
                },
                [=](const error &err) mutable {
                    spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } catch (std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void MediaSourceActor::acquire_detail(
    const utility::FrameRate &rate, caf::typed_response_promise<bool> rp) {

    // is this a good idea ? We can never update the details.
    if (media_streams_.size()) {
        rp.deliver(true);
        return;
    } else if (base_.media_status() != MS_UNKNOWN && !base_.online()) {
        rp.deliver(false);
        return;
    }

    // we only want to do the 'get_media_detail' and construct media_streams
    // once ...
    pending_stream_detail_requests_.push_back(rp);

    // it's possible that a call is made to acquire_detail
    // while a previous call is incomplete (and still waiting on the
    // request call below). Check here and return if this is
    // the case
    if (pending_stream_detail_requests_.size() > 1)
        return;

    auto main_method = [=]() mutable {
        try {
            auto gmra = system().registry().template get<caf::actor>(media_reader_registry);
            if (gmra) {

                // pick middle frame to fetch detail from (e.g. EXR parts which are
                // 'streams' in xSTUDIO) - this is less likely to mismatch the rest
                // of the sequence than first frame which can be a slate frame made
                // by a different tool.
                int frame;
                auto _uri = base_.media_reference().uri(
                    base_.media_reference().frame_count() / 2, frame);

                if (!_uri) {
                    // ok, middle frame didn't work lets try the first frame
                    // after all as a fallback
                    _uri = base_.media_reference().uri(0, frame);
                }

                if (not _uri)
                    throw std::runtime_error("Invalid frame index");

                mail(get_media_detail_atom_v, *_uri, actor_cast<actor_addr>(this))
                    .request(gmra, infinite)
                    .then(
                        [=](const MediaDetail &md) mutable {
                            base_.set_reader(md.reader_);

                            bool media_ref_set = false;
                            for (auto stream_detail : md.streams_) {
                                // HACK!!!

                                auto uuid   = utility::Uuid::generate();
                                auto stream = spawn<MediaStreamActor>(stream_detail, uuid);
                                link_to(stream);
                                join_event_group(this, stream);
                                media_streams_[uuid] = stream;
                                base_.add_media_stream(stream_detail.media_type_, uuid);

                                if (!media_ref_set) {
                                    // Note - it looks like we have separate media references
                                    // for each stream, but we don't there is a single media
                                    // reference for a MediaSource - as  such, we don't support
                                    // streams with different durations and/or frame rates but
                                    // we can address that (if we have to).
                                    update_stream_media_reference(
                                        stream_detail, uuid, rate, md.timecode_);
                                    media_ref_set = true;
                                }

                                mail(
                                    utility::event_atom_v,
                                    add_media_stream_atom_v,
                                    UuidActor(uuid, stream))
                                    .send(base_.event_group());

                                spdlog::debug(
                                    "Media {} fps, {} frames {} timecode.",
                                    base_.media_reference().rate().to_fps(),
                                    base_.media_reference().frame_count(),
                                    to_string(base_.media_reference().timecode()));
                            }

                            if (md.streams_.empty()) {
                                if (base_.media_status() == MS_ONLINE) {
                                    anon_mail(media_status_atom_v, MS_MISSING).send(this);
                                }
                            }

                            mail(media_metadata::get_metadata_atom_v)
                                .request(actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](const bool) mutable {
                                        anon_mail(media_hook::get_media_hook_atom_v).send(this);
                                    },
                                    [=](const caf::error &err) mutable {
                                        spdlog::critical(
                                            "{} {} {}",
                                            __PRETTY_FUNCTION__,
                                            to_string(err),
                                            to_string(base_.media_reference().uri()));
                                        anon_mail(media_hook::get_media_hook_atom_v).send(this);
                                    });

                            base_.send_changed();
                            mail(utility::event_atom_v, change_atom_v)
                                .send(base_.event_group());

                            for (auto &_rp : pending_stream_detail_requests_) {
                                _rp.deliver(true);
                            }
                            pending_stream_detail_requests_.clear();
                        },
                        [=](const error &err) mutable {
                            // set media status..
                            // set duration to one frame. Or things get upset.
                            // base_.media_reference().set_duration(
                            //     FrameRateDuration(1,
                            //     base_.media_reference().duration().rate()));
                            spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            if (base_.error_detail() != to_string(err)) {
                                base_.send_changed();
                                mail(utility::event_atom_v, change_atom_v)
                                    .send(base_.event_group());
                                base_.set_error_detail(to_string(err));
                            }
                            for (auto &_rp : pending_stream_detail_requests_) {
                                _rp.deliver(false);
                            }
                            pending_stream_detail_requests_.clear();
                            base_.set_media_status(MS_UNREADABLE);
                        });
            } else {
                for (auto &_rp : pending_stream_detail_requests_) {
                    _rp.deliver(false);
                }
                pending_stream_detail_requests_.clear();
            }
        } catch (const std::exception &err) {
            base_.set_error_detail(err.what());
            for (auto &_rp : pending_stream_detail_requests_) {
                _rp.deliver(false);
            }
            pending_stream_detail_requests_.clear();
        }
    };

    // before we run the main acquire detail stuff, we need to check the
    // partial sequence bahviour preference
    auto prefs_actor = system().registry().template get<caf::actor>(global_store_registry);
    if (prefs_actor) {
        mail(get_json_atom_v, "/core/session/partial_sequence_behaviour/value")
            .request(prefs_actor, infinite)
            .then(
                [=](const utility::JsonStore &partial_seq_behaviour) mutable {
                    if (partial_seq_behaviour.is_string()) {
                        auto p =
                            partialSeqNameMap.find(partial_seq_behaviour.get<std::string>());
                        if (p != partialSeqNameMap.end()) {
                            base_.set_partial_seq_behaviour(p->second);
                        }
                    }
                    main_method();
                },
                [=](const error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    main_method();
                });
    } else {
        main_method();
    }
}


void MediaSourceActor::update_media_status() {
    auto scanner = system().registry().template get<caf::actor>(scanner_registry);
    if (scanner) {
        anon_mail(media_status_atom_v, base_.media_reference(), this).send(scanner);
        if (std::get<2>(base_.checksum()) == 0)
            anon_mail(checksum_atom_v, this, base_.media_reference()).send(scanner);
    }
}

caf::message_handler MediaSourceActor::message_handler() {
    auto thumbnail_manager = system().registry().get<caf::actor>(thumbnail_manager_registry);

    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](json_store::update_atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            if (current_sender() == json_store_)
                mail(json_store::update_atom_v, change, path, full).send(base_.event_group());
        },

        [=](json_store::update_atom, const JsonStore &full) mutable {
            if (current_sender() == json_store_)
                mail(json_store::update_atom_v, full).send(base_.event_group());
        },

        [=](acquire_media_detail_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();

            acquire_detail(base_.media_reference().rate(), rp);
            // why ?
            // mail(utility::event_atom_v, utility::name_atom_v,
            // base_.name()).send(base_.event_group());
            return rp;
        },

        [=](acquire_media_detail_atom, const utility::FrameRate &rate) -> result<bool> {
            auto rp = make_response_promise<bool>();
            acquire_detail(rate, rp);
            // why ?
            // mail(utility::event_atom_v, utility::name_atom_v,
            // base_.name()).send(base_.event_group());
            return rp;
        },

        [=](media_status_atom) -> MediaStatus { return base_.media_status(); },

        [=](media_status_atom, const MediaStatus status) -> bool {
            if (base_.media_status() != status) {
                base_.set_media_status(status);
                base_.send_changed();
                mail(utility::event_atom_v, media_status_atom_v, status)
                    .send(base_.event_group());
            }
            return true;
        },

        [=](add_media_stream_atom, caf::actor media_stream) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            mail(uuid_atom_v)
                .request(media_stream, infinite)
                .then(
                    [=](const Uuid &uuid) mutable {
                        mail(add_media_stream_atom_v, UuidActor(uuid, media_stream))
                            .request(actor_cast<caf::actor>(this), infinite)
                            .then(
                                [=](const UuidActor &ua) mutable { rp.deliver(ua); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](add_media_stream_atom,
            const utility::UuidActor &media_stream) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            mail(get_media_type_atom_v)
                .request(media_stream.actor(), infinite)
                .then(
                    [=](const MediaType &mt) mutable {
                        join_event_group(this, media_stream.actor());
                        link_to(media_stream.actor());
                        media_streams_[media_stream.uuid()] = media_stream.actor();
                        base_.add_media_stream(mt, media_stream.uuid());
                        base_.send_changed();
                        mail(utility::event_atom_v, add_media_stream_atom_v, media_stream)
                            .send(base_.event_group());
                        rp.deliver(media_stream);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](colour_pipeline::get_colour_pipe_params_atom) {
            return mail(json_store::get_json_atom_v, "/colour_pipeline").delegate(json_store_);
        },

        [=](colour_pipeline::set_colour_pipe_params_atom, const utility::JsonStore &params) {
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(json_store::set_json_atom_v, params, "/colour_pipeline")
                .delegate(json_store_);
        },

        [=](current_media_stream_atom, const MediaType media_type) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            mail(acquire_media_detail_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) mutable {
                        if (media_streams_.count(base_.current(media_type)))
                            rp.deliver(UuidActor(
                                base_.current(media_type),
                                media_streams_.at(base_.current(media_type))));
                        rp.deliver(make_error(xstudio_error::error, "No streams"));
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](current_media_stream_atom, const MediaType media_type, const Uuid &uuid) -> bool {
            auto result = base_.set_current(media_type, uuid);
            if (result) {
                base_.send_changed();
                mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            }
            return result;
        },

        [=](detail_atom,
            const MediaType media_type) -> caf::result<std::vector<ContainerDetail>> {
            auto rp = make_response_promise<std::vector<ContainerDetail>>();
            // call actuire_detail to make sure we have inspected streams etc. first
            mail(acquire_media_detail_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) mutable {
                        if (base_.empty()) {
                            rp.deliver(std::vector<ContainerDetail>());
                            return;
                        }

                        fan_out_request<policy::select_all>(
                            map_value_to_vec(media_streams_), infinite, detail_atom_v)
                            .then(
                                [=](std::vector<ContainerDetail> details) mutable {
                                    auto result = std::vector<ContainerDetail>();
                                    std::map<Uuid, int> lookup;
                                    for (size_t i = 0; i < details.size(); i++)
                                        lookup[details[i].uuid_] = i;

                                    // order results based on base_.media_sources()
                                    for (const auto &i : base_.streams(media_type)) {
                                        if (lookup.count(i))
                                            result.push_back(details[lookup[i]]);
                                    }
                                    rp.deliver(result);
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](current_media_stream_atom,
            const MediaType media_type,
            const std::string &stream_id) -> bool {
            caf::scoped_actor sys(system());
            for (auto &strm : media_streams_) {

                // get uuid..
                auto stream_detail =
                    request_receive<StreamDetail>(*sys, strm.second, get_stream_detail_atom_v);
                if (stream_detail.name_ == stream_id &&
                    stream_detail.media_type_ == media_type) {
                    auto result = base_.set_current(media_type, strm.first);
                    if (result) {
                        base_.send_changed();
                        mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
                    }
                    return result;
                }
            }
            return false;
        },

        [=](get_media_pointer_atom atom,
            const MediaType media_type,
            const int logical_frame) -> caf::result<media::AVFrameID> {
            auto rp = make_response_promise<media::AVFrameID>();
            mail(acquire_media_detail_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) mutable {
                        if (base_.current(media_type).is_null()) {
                            rp.deliver(make_error(xstudio_error::error, "No streams"));
                        }
                        auto dur = base_.media_reference(base_.current(media_type)).duration();
                        LogicalFrameRanges ranges;
                        ranges.emplace_back(logical_frame, logical_frame);
                        mail(get_media_pointers_atom_v, media_type, ranges)
                            .request(caf::actor_cast<caf::actor>(this), infinite)
                            .then(
                                [=](const media::AVFrameIDs &ids) mutable {
                                    if (ids.size() && ids[0]) {
                                        rp.deliver(*(ids[0]));
                                    } else {
                                        rp.deliver(
                                            make_error(xstudio_error::error, "No frame"));
                                    }
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_pointers_atom atom,
            const MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
            auto rp = make_response_promise<media::FrameTimeMapPtr>();
            mail(acquire_media_detail_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) mutable {
                        if (base_.current(media_type).is_null()) {
                            rp.deliver(make_error(xstudio_error::error, "No streams"));
                            return;
                        }
                        const auto dur =
                            base_.media_reference(base_.current(media_type)).duration();
                        LogicalFrameRanges ranges;
                        ranges.emplace_back(0, dur.frames() - 1);
                        mail(atom, media_type, ranges)
                            .request(caf::actor_cast<caf::actor>(this), infinite)
                            .then(
                                [=](const media::AVFrameIDs ids) mutable {
                                    media::FrameTimeMap *result = new media::FrameTimeMap;
                                    timebase::flicks t(0);
                                    for (const auto &fid : ids) {
                                        (*result)[t] = fid;
                                        t += dur.rate().to_flicks();
                                    }
                                    rp.deliver(media::FrameTimeMapPtr(result));
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](get_media_pointers_atom,
            const MediaType media_type,
            const LogicalFrameRanges &ranges,
            const utility::Uuid clip_uuid) -> caf::result<media::AVFrameIDs> {
            auto rp = make_response_promise<media::AVFrameIDs>();

            get_media_pointers_for_frames(media_type, ranges, rp, clip_uuid);

            return rp;
        },

        [=](get_media_pointers_atom,
            const MediaType media_type,
            const LogicalFrameRanges &ranges) {
            return mail(get_media_pointers_atom_v, media_type, ranges, utility::Uuid())
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](media_reader::get_thumbnail_atom,
            float position) -> result<thumbnail::ThumbnailBufferPtr> {
            auto rp   = make_response_promise<thumbnail::ThumbnailBufferPtr>();

            int frame = (int)round(float(base_.media_reference().frame_count()) * position);
            frame     = std::max(0, std::min(frame, base_.media_reference().frame_count() - 1));
            mail(get_media_pointer_atom_v, media::MediaType::MT_IMAGE, frame)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const media::AVFrameID &mp) mutable {
                        mail(media_reader::get_thumbnail_atom_v, mp)
                            .request(thumbnail_manager, infinite)
                            .then(
                                [=](const thumbnail::ThumbnailBufferPtr &buf) mutable {
                                    // mail(utility::event_atom_v,
                                    // media_reader::get_thumbnail_atom_v,
                                    // buf).send(base_.event_group());
                                    rp.deliver(buf);
                                },
                                [=](error &err) mutable { rp.deliver(err); });
                    },
                    [=](error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](utility::rate_atom, bool reset) -> result<bool> {
            // use this to reset to 'natural' frame rate
            if (!reset)
                return false;

            auto rp = make_response_promise<bool>();

            // we can get the origninal/natural frame rate by getting the
            // StreamDetail
            mail(get_stream_detail_atom_v, media::MT_IMAGE)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const StreamDetail &detail) mutable {
                        if (base_.media_reference().rate() != detail.duration_.rate()) {
                            auto mr = base_.media_reference();
                            mr.set_rate(detail.duration_.rate());
                            rp.delegate(
                                caf::actor_cast<caf::actor>(this), media_reference_atom_v, mr);
                        } else {
                            rp.deliver(false);
                        }
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_reference_atom) -> MediaReference { return base_.media_reference(); },

        [=](media_reference_atom, MediaReference mr) {
            return mail(media_reference_atom_v, mr, false)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](media_reference_atom, MediaReference mr, bool force_change_signal) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // XSTUDIO.api.session.playlists[0].media[0].media_sources[0].media_reference =
            // MediaReference(URI("file:///user_data/ted/DRG/O_006_tcf_6140_comp_v045_422.mov"),
            // True, FrameRate())

            if (mr != base_.media_reference() || force_change_signal) {

                base_.set_media_status(MS_UNKNOWN);

                if (mr.uri() != base_.media_reference().uri()) {

                    // URI is changing! Need a full rebuild
                    if (!mr.rate().count())
                        mr.set_rate(base_.media_reference().rate());

                    // delete streams
                    for (const auto &i : media_streams_) {
                        unlink_from(i.second);
                        send_exit(i.second, caf::exit_reason::user_shutdown);
                    }
                    base_.clear();
                    media_streams_.clear();

                    base_.set_media_reference(mr);
                    uri_status_cache_.clear();

                    // rebuild our streams etc.
                    mail(acquire_media_detail_atom_v)
                        .request(caf::actor_cast<caf::actor>(this), infinite)
                        .then(

                            [=](bool) mutable {
                                // update state..
                                update_media_status();
                                rp.deliver(true);
                            },
                            [=](caf::error &err) mutable { rp.deliver(err); });

                } else {

                    base_.set_media_reference(mr);
                    uri_status_cache_.clear();
                    // update state..
                    update_media_status();
                    base_.send_changed();
                    mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
                    rp.deliver(true);
                }

            } else {
                rp.deliver(false);
            }
            return rp;
        },

        [=](media_reference_atom, const Uuid &uuid) -> std::pair<Uuid, MediaReference> {
            if (uuid.is_null())
                return std::pair<Uuid, MediaReference>(base_.uuid(), base_.media_reference());
            return std::pair<Uuid, MediaReference>(uuid, base_.media_reference());
        },

        [=](get_media_stream_atom,
            const MediaType media_type,
            bool current) -> result<caf::actor> {
            if (media_streams_.count(base_.current(media_type))) {
                return media_streams_[base_.current(media_type)];
            }
            return make_error(xstudio_error::error, "No MediaStreams");
        },

        [=](get_media_stream_atom, const MediaType media_type) -> std::vector<UuidActor> {
            std::vector<UuidActor> sm;

            for (const auto &i : base_.streams(media_type))
                sm.emplace_back(UuidActor(i, media_streams_.at(i)));

            return sm;
        },

        [=](get_media_stream_atom, const int stream_index) -> result<caf::actor> {
            // get the stream actor for the given index
            auto rp    = make_response_promise<caf::actor>();
            auto count = std::make_shared<int>(media_streams_.size());
            for (const auto &p : media_streams_) {

                caf::actor media_stream = p.second;
                mail(get_stream_detail_atom_v)
                    .request(p.second, infinite)
                    .then(
                        [=](const StreamDetail &detail) mutable {
                            if (detail.index_ == stream_index) {
                                rp.deliver(media_stream);
                            }
                            (*count)--;
                            if (!*count && rp.pending()) {
                                rp.deliver(make_error(
                                    xstudio_error::error, "No stream matching index."));
                            }
                        },
                        [=](caf::error err) mutable {
                            (*count)--;
                            if (!*count && rp.pending()) {
                                rp.deliver(err);
                            }
                        });
            }

            return rp;
        },

        [=](get_media_stream_atom, const Uuid &uuid) -> result<caf::actor> {
            if (media_streams_.count(uuid))
                return media_streams_.at(uuid);
            return result<caf::actor>(make_error(xstudio_error::error, "Invalid stream uuid"));
        },

        [=](get_media_type_atom, const MediaType media_type) -> bool {
            return base_.has_type(media_type);
        },

        [=](get_stream_detail_atom, const MediaType media_type) -> result<StreamDetail> {
            if (media_streams_.count(base_.current(media_type))) {
                auto rp = make_response_promise<StreamDetail>();

                mail(get_stream_detail_atom_v)
                    .request(media_streams_.at(base_.current(media_type)), infinite)
                    .then(
                        [=](const StreamDetail &sd) mutable { rp.deliver(sd); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }

            // mark as bad source..
            // if (base_.media_status() == MS_ONLINE) {
            //     anon_mail(media_status_atom_v, MS_MISSING).send(this);
            // }

            return result<StreamDetail>(make_error(xstudio_error::error, "No streams"));
        },

        [=](json_store::get_json_atom,
            const std::string &path,
            std::vector<caf::actor> children_to_try) -> result<utility::JsonStore> {
            // used in message handler immediately below for recursive call to another actor to
            // get json data from children. Calling 'request' in a chain/loop in the way we
            // want is pretty ugly to do in terms of the code which is why I use this
            // recursive trick
            auto rp = make_response_promise<utility::JsonStore>();
            mail(json_store::get_json_atom_v, path)
                .request(children_to_try.front(), infinite)
                .then(
                    [=](const utility::JsonStore &j) mutable { rp.deliver(j); },
                    [=](error &err) mutable {
                        // we got an error, probably because the child we tried
                        // does not have data  at the given path. Continue trying
                        // children until exhausted
                        children_to_try.erase(children_to_try.begin());
                        if (children_to_try.size()) {
                            rp.delegate(
                                caf::actor_cast<caf::actor>(this),
                                json_store::get_json_atom_v,
                                path,
                                children_to_try);
                        } else {
                            rp.deliver(err);
                        }
                    });
            return rp;
        },

        [=](json_store::get_json_atom _get_atom,
            const std::string &path,
            bool try_children) -> result<utility::JsonStore> {
            // request for metadata. Is the metadata path actually valid for us or for one
            // of our MediaStream children? Try children in order of current iamge source
            // then current media source.

            // This message handler is used by UI layer to display metadata fields in the
            // xSTUDIO media list
            auto rp = make_response_promise<utility::JsonStore>();
            std::vector<caf::actor> children_to_try({json_store_});

            if (try_children) {
                if (media_streams_.count(base_.current(MT_IMAGE)))
                    children_to_try.push_back(media_streams_.at(base_.current(MT_IMAGE)));
                if (media_streams_.count(base_.current(MT_AUDIO)))
                    children_to_try.push_back(media_streams_.at(base_.current(MT_AUDIO)));
            }

            rp.delegate(caf::actor_cast<caf::actor>(this), _get_atom, path, children_to_try);
            return rp;
        },

        [=](json_store::get_json_atom atom) { return mail(atom).delegate(json_store_); },

        [=](json_store::get_json_atom atom,
            const std::string &path) -> result<utility::JsonStore> {
            auto rp = make_response_promise<utility::JsonStore>();

            if (path.find("/metadata/media") == 0) {
                // ensure metadata has indeed been read before passing to json store
                mail(media_metadata::get_metadata_atom_v)
                    .request(caf::actor_cast<actor>(this), infinite)
                    .then(
                        [=](bool) mutable { rp.delegate(json_store_, atom, path); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.delegate(json_store_, atom, path);
            }
            return rp;
        },

        [=](json_store::get_json_atom atom,
            const std::vector<std::string> &paths) -> caf::result<JsonStore> {
            // multi json store value request
            auto rp = make_response_promise<JsonStore>();
            if (!media_metadata_up_to_date_) {
                mail(media_metadata::get_metadata_atom_v)
                    .request(caf::actor_cast<actor>(this), infinite)
                    .then(
                        [=](bool) mutable { rp.delegate(json_store_, atom, paths); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.delegate(json_store_, atom, paths);
            }
            return rp;
        },

        [=](json_store::set_json_atom atom, const JsonStore &json) {
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json).delegate(json_store_);
        },

        [=](json_store::merge_json_atom atom, const JsonStore &json) {
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json).delegate(json_store_);
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json, path).delegate(json_store_);
        },

        [=](media::invalidate_cache_atom) -> caf::result<media::MediaKeyVector> {
            auto rp = make_response_promise<media::MediaKeyVector>();

            media::MediaKeyVector keys(all_requested_frames_.size());
            std::copy(all_requested_frames_.begin(), all_requested_frames_.end(), keys.begin());

            auto image_cache =
                system().registry().template get<caf::actor>(image_cache_registry);
            auto audio_cache =
                system().registry().template get<caf::actor>(audio_cache_registry);
            std::vector<caf::actor> caches;

            if (image_cache)
                caches.push_back(image_cache);
            if (audio_cache)
                caches.push_back(audio_cache);
            if (caches.empty()) {
                rp.deliver(media::MediaKeyVector());
                return rp;
            }

            fan_out_request<policy::select_all>(
                caches, infinite, media_cache::erase_atom_v, keys)
                .then(
                    [=](std::vector<media::MediaKeyVector> erased_keys) mutable {
                        media::MediaKeyVector result;
                        for (const auto &i : erased_keys)
                            result.insert(result.end(), i.begin(), i.end());
                        rp.deliver(result);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](media_hook::get_media_hook_atom) -> caf::result<bool> {
            auto rp      = make_response_promise<bool>();
            auto m_actor = system().registry().template get<caf::actor>(media_hook_registry);

            if (not m_actor) {
                rp.deliver(false);
            } else {
                mail(media_hook::get_media_hook_atom_v, this)
                    .request(m_actor, infinite)
                    .then(
                        [=](const bool done) mutable { rp.deliver(done); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            }

            return rp;
        },

        [=](media_metadata::get_metadata_atom) -> caf::result<bool> {
            auto rp = make_response_promise<bool>();
            if (media_metadata_up_to_date_) {
                rp.deliver(true);
                return rp;
            }

            auto m_actor =
                system().registry().template get<caf::actor>(media_metadata_registry);
            if (not m_actor) {
                rp.deliver(false);
                return rp;
            }

            try {

                if (not base_.media_reference().container()) {

                    // In the case of retrieving media metadata for a frame
                    // based source (i.e. EXRs, jpegs) we want to pick a frame
                    // from the middle of the sequence. The reason is that this
                    // is more likely to have EXR channel/part data that matches
                    // most of the other frames. We frequently see an EXR sequence
                    // where the first frame is a slate frame made by a separate
                    // tool to the rest of the sequence
                    int file_frame;
                    auto test_frame_uri = base_.media_reference().uri(
                        base_.media_reference().frame_count() / 2, file_frame);

                    if (!test_frame_uri) {
                        // ok, middle frame didn't work lets try the first frame
                        // after all as a fallback
                        test_frame_uri = base_.media_reference().uri(
                            base_.media_reference().frame_count() / 2, file_frame);
                    }
                    // #pragma message "Currently only reading metadata on first frame for image
                    // sequences"

                    // If we read metadata for every frame the whole app grinds when inspecting
                    // big or multiple sequences
                    if (test_frame_uri) {

                        mail(get_metadata_atom_v, *test_frame_uri, file_frame)
                            .request(m_actor, infinite)
                            .then(
                                [=](const std::pair<JsonStore, int> &meta) mutable {
                                    mail(
                                        json_store::set_json_atom_v,
                                        meta.first,
                                        "/metadata/media/@" + std::to_string(meta.second),
                                        true)
                                        .request(json_store_, infinite)
                                        .then(
                                            [=](const bool &done) mutable {
                                                media_metadata_up_to_date_ = true;
                                                rp.deliver(done);
                                                // notify any watchers that metadata is updated
                                                mail(
                                                    utility::event_atom_v,
                                                    get_metadata_atom_v,
                                                    meta.first)
                                                    .send(base_.event_group());
                                            },
                                            [=](error &err) mutable {
                                                rp.deliver(std::move(err));
                                            });
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    } else {
                        rp.deliver(make_error(
                            xstudio_error::error,
                            std::string("Sequence with no frames ") +
                                to_string(base_.media_reference().uri())));
                    }

                    /*for(size_t i=0;i<frames.size();i++) {
                            request(m_actor, infinite, get_metadata_atom_v, frames[i].first,
                    frames[i].second).then(
                                    [=] (const std::pair<JsonStore, int> &meta) mutable {
                                            request(json_store_, infinite,
                    json_store::set_json_atom_v, meta.first,
                    "/metadata/media/@"+std::to_string(meta.second), true).then(
                                                    [=] (const bool &done) mutable {
                                                            if(i == frames.size()-1) {
                                                                    rp.deliver(done);
                                                                    // notify any watchers that
                    metadata is updated mail(utility::event_atom_v,
                    get_metadata_atom_v, meta.first).send(base_.event_group());
                                                            }
                                                    },
                                            [=](error& err) mutable {
                                                            if(i == frames.size()-1)
                                                            rp.deliver(std::move(err));
                                            }
                                            );
                                    },
                            [=](error& err) mutable {
                                    rp.deliver(std::move(err));
                            }
                            );
                    }*/

                } else {
                    mail(get_metadata_atom_v, base_.media_reference().uri())
                        .request(m_actor, infinite)
                        .then(
                            [=](const std::pair<JsonStore, int> &meta) mutable {
                                send_stream_metadata_to_stream_actors(meta.first);

                                mail(

                                    json_store::set_json_atom_v,
                                    meta.first,
                                    "/metadata/media/@")
                                    .request(json_store_, infinite)
                                    .then(
                                        [=](const bool &done) mutable {
                                            media_metadata_up_to_date_ = true;
                                            rp.deliver(done);
                                            // notify any watchers that metadata is updated
                                            mail(
                                                utility::event_atom_v,
                                                get_metadata_atom_v,
                                                meta.first)
                                                .send(base_.event_group());
                                        },
                                        [=](error &err) mutable {
                                            rp.deliver(std::move(err));
                                        });
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                }
            } catch (const std::exception &e) {
                rp.deliver(make_error(
                    xstudio_error::error,
                    std::string("media_metadata::get_metadata_atom ") + e.what()));
            }

            return rp;
        },

        [=](media_metadata::get_metadata_atom, const int sequence_frame) -> caf::result<bool> {
            if (base_.media_reference().container())
                return make_error(xstudio_error::error, "Media has no frames");

            auto _uri = base_.media_reference().uri_from_frame(sequence_frame);
            if (not _uri)
                return make_error(xstudio_error::error, "Invalid frame index");

            auto rp = make_response_promise<bool>();
            auto m_actor =
                system().registry().template get<caf::actor>(media_metadata_registry);
            mail(get_metadata_atom_v, *_uri)
                .request(m_actor, infinite)
                .then(
                    [=](const std::pair<JsonStore, int> &meta) mutable {
                        mail(
                            json_store::set_json_atom_v,
                            meta.first,
                            "/metadata/media/@" + std::to_string(meta.second))
                            .request(json_store_, infinite)
                            .then(
                                [=](const bool &done) mutable { rp.deliver(done); },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](utility::duplicate_atom) -> result<UuidUuidActor> {
            auto rp = make_response_promise<UuidUuidActor>();
            duplicate(rp);
            return rp;
        },

        [=](utility::event_atom, utility::change_atom) {},

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::get_group_atom _get_group_atom) {
            return mail(_get_group_atom).delegate(json_store_);
        },

        [=](media::checksum_atom) -> std::tuple<std::string, std::string, uintmax_t> {
            return base_.checksum();
        },

        [=](media::checksum_atom, const std::pair<std::string, uintmax_t> &checksum) {

            // force thumbnail update on change. Might cause double update..
            auto old_size = std::get<2>(base_.checksum());
            if (base_.checksum(checksum) and old_size) {

                mail(utility::event_atom_v, media_status_atom_v, base_.media_status())
                    .send(base_.event_group());

                // trigger re-eval of reader..
                mail(get_media_pointer_atom_v, MT_IMAGE, static_cast<int>(0))
                    .request(caf::actor_cast<caf::actor>(this), infinite)
                    .then(
                        [=](const media::AVFrameID &tmp) {
                            auto global_media_reader =
                                system().registry().template get<caf::actor>(
                                    media_reader_registry);
                            anon_mail(retire_readers_atom_v, tmp).send(global_media_reader);
                        },
                        [=](const error &err) {});
            }
        },

        [=](media::pixel_aspect_atom, const double new_aspect) {
            if (media_streams_.count(base_.current(MT_IMAGE)))
                anon_mail(media::pixel_aspect_atom_v, new_aspect)
                    .send(media_streams_.at(base_.current(MT_IMAGE)));
        },

        [=](media::rescan_atom atom) -> result<MediaReference> {
            auto rp = make_response_promise<MediaReference>();
            if (!base_.media_reference().container()) {
                // before we do the rescan, we need to update the partial_sequence_behaviour
                // in case it has changed since the last scan of the frames
                auto prefs_actor =
                    system().registry().template get<caf::actor>(global_store_registry);
                if (prefs_actor) {
                    mail(get_json_atom_v, "/core/session/partial_sequence_behaviour/value")
                        .request(prefs_actor, infinite)
                        .then(
                            [=](const utility::JsonStore &partial_seq_behaviour) mutable {
                                if (partial_seq_behaviour.is_string()) {
                                    auto p = partialSeqNameMap.find(
                                        partial_seq_behaviour.get<std::string>());
                                    if (p != partialSeqNameMap.end()) {
                                        if (p->second != base_.partial_seq_behaviour()) {
                                            base_.set_partial_seq_behaviour(p->second);
                                            rp.delegate(
                                                caf::actor_cast<caf::actor>(this), atom, true);
                                        }
                                    }
                                }
                                rp.delegate(caf::actor_cast<caf::actor>(this), atom, false);
                            },
                            [=](const error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.delegate(caf::actor_cast<caf::actor>(this), atom, false);
                            });
                } else {
                    rp.delegate(caf::actor_cast<caf::actor>(this), atom, false);
                }
            } else {
                rp.delegate(caf::actor_cast<caf::actor>(this), atom, false);
            }
            return rp;
        },


        [=](media::rescan_atom atom, bool force_change_signal) -> result<MediaReference> {
            auto rp = make_response_promise<MediaReference>();

            // trigger status update
            update_media_status();

            auto scanner = system().registry().template get<caf::actor>(scanner_registry);
            if (scanner) {
                mail(atom, base_.media_reference())
                    .request(scanner, infinite)
                    .then(
                        [=](MediaReference mr) mutable {
                            if (!mr.container() && base_.partial_seq_behaviour() !=
                                                       PS_COLLAPSE_TO_ON_DISK_FRAMES) {

                                // at this point, media_reference will contain a list of
                                // numbered frames that are on disk only. If we don't have
                                // PS_COLLAPSE_TO_ON_DISK_FRAMES behaviour, we now tell the
                                // media_reference to fill in the gaps in the frame numbers
                                // (even if they are not on disk) so that we can use held frame
                                // behaviour.
                                mr.fill_partial_sequences();
                            }

                            // Note: not using 'force_change_signal' - instead, passing true in
                            // this request which will make us emit a change_atom event, forcing
                            // Playhead/Timeline etc. to rebuild the frames list. Thus, if more
                            // frames are on-disk since last time, we will check for them (when
                            // plahead requests AVFrameIDs) again.
                            mail(media_reference_atom_v, mr, true /*force_change_signal*/)
                                .request(caf::actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](const bool) mutable {
                                        // rebuild hash (file might have changed)
                                        auto scanner =
                                            system().registry().template get<caf::actor>(
                                                scanner_registry);
                                        if (scanner)
                                            anon_mail(
                                                checksum_atom_v, this, base_.media_reference())
                                                .send(scanner);

                                        anon_mail(invalidate_cache_atom_v).send(this);
                                        rp.deliver(base_.media_reference());
                                    },
                                    [=](const error &err) mutable { rp.deliver(err); });
                        },
                        [=](const error &err) mutable { rp.deliver(err); });
            } else {
                rp.deliver(base_.media_reference());
            }

            return rp;
        },

        [=](utility::parent_atom) -> caf::actor { return actor_cast<actor>(parent_); },

        [=](utility::parent_atom, const UuidActor &parent) {
            parent_uuid_ = parent.uuid();
            parent_      = actor_cast<actor_addr>(parent.actor());
            base_.send_changed();
        },

        // deprecated
        [=](utility::parent_atom, caf::actor parent) {
            mail(utility::uuid_atom_v)
                .request(parent, infinite)
                .then(
                    [=](const utility::Uuid &parent_uuid) { parent_uuid_ = parent_uuid; },
                    ERR_HANDLER_FUNC);
            parent_ = actor_cast<actor_addr>(parent);
            base_.send_changed();
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            mail(json_store::get_json_atom_v, "")
                .request(json_store_, infinite)
                .then(
                    [=](const JsonStore &meta) mutable {
                        std::vector<caf::actor> clients;

                        for (const auto &i : media_streams_)
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
                                    [=](error &err) mutable { rp.deliver(std::move(err)); });
                        } else {
                            JsonStore jsn;
                            jsn["store"]  = meta;
                            jsn["base"]   = base_.serialise();
                            jsn["actors"] = {};
                            rp.deliver(jsn);
                        }
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        }};
}


void MediaSourceActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    update_media_status();

    // set an empty dict for colour_pipeline, as we request this at various
    // times and need a placeholder or we get warnings if it's not there
    mail(json_store::get_json_atom_v, "/colour_pipeline")
        .request(json_store_, infinite)
        .then(
            [=](const JsonStore &) {},
            [=](const error &) {
                // we'll get this error if there is no dict already
                anon_mail(json_store::set_json_atom_v, utility::JsonStore(), "/colour_pipeline")
                    .send(json_store_);
            });
}

void MediaSourceActor::get_media_pointers_for_frames(
    const MediaType media_type,
    const LogicalFrameRanges &ranges,
    caf::typed_response_promise<media::AVFrameIDs> rp,
    const utility::Uuid clip_uuid) {

    // func ptr to complete the task
    auto do_get_pointers = [=]() mutable {
        // fetch colour management data
        mail(json_store::get_json_atom_v, "/colour_pipeline")
            .request(json_store_, infinite)
            .then(
                [=](const JsonStore &colour_mgmt_data) mutable {
                    // fetch media detail
                    if (!media_streams_.contains(base_.current(media_type))) {
                        rp.deliver(make_error(xstudio_error::error, "No streams"));
                        return;
                    }

                    mail(get_stream_detail_atom_v)
                        .request(media_streams_.at(base_.current(media_type)), infinite)
                        .then(
                            [=](const StreamDetail &detail) mutable {
                                get_media_pointers_for_frames(
                                    media_type,
                                    ranges,
                                    rp,
                                    clip_uuid,
                                    colour_mgmt_data,
                                    detail);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                },
                [=](error &err) mutable { rp.deliver(std::move(err)); });
    };

    // before we can deliver frame pointers to allow playback, ensure
    // that we have completed the aquisition of the media detail to
    // inspect the source, get duration, assign default Image/Audio
    // streams etc.

    mail(acquire_media_detail_atom_v)
        .request(caf::actor_cast<caf::actor>(this), infinite)
        .then(
            [=](bool) mutable { do_get_pointers(); },
            [=](const error &err) mutable {
                // we proceed on error, in order to make blank frames
                do_get_pointers();
            });
}

void MediaSourceActor::get_media_pointers_for_frames(
    const MediaType media_type,
    const LogicalFrameRanges &ranges,
    caf::typed_response_promise<media::AVFrameIDs> rp,
    const utility::Uuid clip_uuid,
    const utility::JsonStore &colour_mgmt_data,
    const StreamDetail &media_detail) {

    // make a blank frame id that nevertheless includes media source actor uuid
    // and address - we use this if we are trying to resolve a frame ID
    // that is outside the frame range of the media reference. If media is not
    // on disk, this is guaranteed to happen. We still want the frameIDs to
    // include the media uuid, media source uuid & source actor address so
    // we know stuff about the media source that is *supposed* to be onscreen.
    media::AVFrameID blank = *(media::make_blank_frame(
        media_type,
        parent_uuid_,
        base_.uuid(),
        clip_uuid,
        parent_,
        caf::actor_cast<caf::actor_addr>(this)));
    blank.set_frame_status(media::FS_NOT_ON_DISK);

    if (base_.current(media_type).is_null()) {

        // in the case where there is no source, return list of empty frames.
        // This is useful for sources that have no audio or no video, to keep
        // them compatible with the video based frame request/deliver playback
        // system
        auto blank_ptr = std::make_shared<const media::AVFrameID>(blank);
        media::AVFrameIDs result;
        for (const auto &i : ranges) {
            for (auto ii = i.first; ii <= i.second; ii++)
                result.emplace_back(blank_ptr);
        }
        rp.deliver(result);
        return;
    }

    auto blank_ptr = std::make_shared<const media::AVFrameID>(blank);

    media::AVFrameIDs result;
    media::AVFrameID base_frame_id;
    auto timecode = base_.media_reference(base_.current(media_type)).timecode();

    int prev_range_last = 0;
    for (const auto &i : ranges) {

        // We are providing frameIds for a set of logical
        // frame ranges. We need to account for the ranges
        // to increment the timecode for the 'gaps' between
        // the ranges
        if (prev_range_last < i.first) {
            timecode = timecode + (i.first - prev_range_last);
        }

        for (auto logical_frame = i.first; logical_frame <= i.second; logical_frame++) {
            // the try block catches posible 'out_of_range'
            // exception coming from MediaReference::uri()
            try {

                int frame, keyframe;
                FrameStatus frame_status;
                std::filesystem::file_time_type mod_time;

                // this call will resolve missing frames into held or dropped frames
                // and also give us the filesystem modification time for the file which
                // we use to make the cache key for the image if/when it is loaded/decoded and
                // cached
                auto _uri = uri_for_logical_frame(
                    media_type, logical_frame, frame, keyframe, frame_status, mod_time);

                if (base_frame_id.is_nil()) {
                    // TODO: less hideous creation of AVFrameID
                    base_frame_id = media::AVFrameID(
                        _uri,
                        frame,
                        *(base_.media_reference(base_.current(media_type)).frame(0)),
                        frame_status,
                        mod_time.time_since_epoch().count(),
                        media_detail.pixel_aspect_,
                        base_.media_reference(base_.current(media_type)).rate(),
                        media_detail.name_,
                        media_detail.key_format_,
                        base_.reader(),
                        parent_,
                        caf::actor_cast<caf::actor_addr>(this),
                        colour_mgmt_data,
                        base_.uuid(),
                        parent_uuid_,
                        clip_uuid,
                        media_type,
                        timecode);
                }

                result.emplace_back(new media::AVFrameID(
                    base_frame_id,
                    _uri,
                    frame,
                    keyframe,
                    media_detail.key_format_,
                    frame_status,
                    mod_time.time_since_epoch().count(),
                    timecode));

                all_requested_frames_.insert(result.back()->key());

                timecode = timecode + 1;

            } catch ([[maybe_unused]] const std::exception &e) {
                // spdlog::warn("{}", e.what());
                result.emplace_back(blank_ptr);
            }
        }
    }
    rp.deliver(result);
}

MediaSourceActor::UriStatus::UriStatus(
    const caf::uri &_uri, const FrameStatus &status, const int f)
    : uri_(_uri), status_(status), frame_(f) {
    if (status == FS_ON_DISK) {
        try {
            mod_timestamp_ = fs::last_write_time(utility::uri_to_posix_path(uri_));
        } catch (...) {
        }
    }
}


caf::uri MediaSourceActor::uri_for_logical_frame(
    const MediaType media_type,
    const int logical_frame,
    int &frame,
    int &keyframe,
    FrameStatus &frame_status,
    std::filesystem::file_time_type &mod_time) {

    const auto &media_ref = base_.media_reference(base_.current(media_type));

    auto _uri = media_ref.uri(logical_frame, frame);
    if (not _uri)
        throw std::runtime_error("Time out of range");
    keyframe     = frame;
    frame_status = FS_ON_DISK;

    if (!media_ref.container() && base_.partial_seq_behaviour() == PS_DONT_HOLD_FRAME) {

        auto p = uri_status_cache_.find(logical_frame);
        if (p != uri_status_cache_.end()) {
            frame_status = p->second.status_;
            keyframe     = p->second.frame_;
            mod_time     = p->second.mod_timestamp_;
            return p->second.uri_;
        }
        // is this uri on disk?
        const auto path = fs::path(utility::uri_to_posix_path(*_uri));
        if (fs::exists(path)) {

            // store so we don't need to check again
            uri_status_cache_[logical_frame] = UriStatus(*_uri, FS_ON_DISK, frame);
            frame_status                     = FS_ON_DISK;
        } else {
            uri_status_cache_[logical_frame] = UriStatus(*_uri, FS_NOT_ON_DISK, frame);
            frame_status                     = FS_NOT_ON_DISK;
        }
        mod_time = uri_status_cache_[logical_frame].mod_timestamp_;
        return *_uri;

    } else if (!media_ref.container() && base_.partial_seq_behaviour() == PS_HOLD_FRAME) {

        // To enable 'held frame' behaviour, we need to know if the requested
        // frame is on-disk, and if not, whether there is another frame in
        // the sequence that IS on disk that we can use as the held frame.

        // We don't want to be stat'ing the filesystem any harder than we need
        // to so as such I have added a map 'uri_status_cache_'. This stores
        // the uri against the logical frame and whether it is on-disk or
        // a held frame.

        auto p = uri_status_cache_.find(logical_frame);
        if (p != uri_status_cache_.end()) {
            frame_status = p->second.status_;
            keyframe     = p->second.frame_;
            mod_time     = p->second.mod_timestamp_;
            return p->second.uri_;
        }
        // is this uri on disk?
        const auto path = fs::path(utility::uri_to_posix_path(*_uri));
        if (fs::exists(path)) {
            // store so we don't need to check again
            uri_status_cache_[logical_frame] = UriStatus(*_uri, FS_ON_DISK, frame);
            frame_status                     = FS_ON_DISK;
            mod_time                         = uri_status_cache_[logical_frame].mod_timestamp_;
            return *_uri;
        }

        const int frame_count = media_ref.frame_list().count();

        // frame is NOT on disk. Now we enact 'held frame' behaviour by finding
        // the nearest frame that IS on disk (if any)

        // first check if parent folder is on disk ...
        auto parent_dir = path.parent_path();
        if (!fs::exists(parent_dir)) {
            // parent folder is not on disk. Let's fill out the entire status
            // cache with not-on-disk status for the entire sequence
            const std::vector<int> all_frames = media_ref.frame_list().frames();
            for (int i = 0; i < frame_count; ++i) {
                int f;
                auto _uri = media_ref.uri(i, f);
                if (_uri) {
                    uri_status_cache_[logical_frame] = UriStatus(*_uri, FS_NOT_ON_DISK, f);
                }
            }
            frame_status = FS_NOT_ON_DISK;
            return *_uri;
        }

        // for a given frame, check if it's in the cache - if not check if it's on
        // disk. If it is, return the URI
        auto held_frame_check =
            [=, &keyframe](const int search_frame) mutable -> std::optional<caf::uri> {
            auto p = uri_status_cache_.find(search_frame);
            if (p != uri_status_cache_.end() && p->second.status_ != FS_NOT_ON_DISK) {
                // we've found a frame that is either on-disk or is
                // a HELD frame. Return the uri
                keyframe = p->second.frame_;
                mod_time = p->second.mod_timestamp_;
                return p->second.uri_;
            }

            // not cached. Check if is on disk
            int f;
            auto search_uri = media_ref.uri(search_frame, f);
            const auto path = fs::path(utility::uri_to_posix_path(*search_uri));
            if (fs::exists(path)) {
                uri_status_cache_[search_frame] = UriStatus(*search_uri, FS_ON_DISK, f);
                keyframe                        = f;
                mod_time = uri_status_cache_[search_frame].mod_timestamp_;
                return search_uri;
            }
            return {};
        };

        // let's search forwards through the frame range until we find a uri
        // that IS on disk and use that

        for (int search_frame = (logical_frame - 1); search_frame >= 0; --search_frame) {

            auto r = held_frame_check(
                search_frame); // this also sets mod_time to that of the found frame
            if (r) {
                frame_status                     = FS_HELD_FRAME;
                uri_status_cache_[logical_frame] = UriStatus(*r, FS_HELD_FRAME, keyframe);
                uri_status_cache_[logical_frame].mod_timestamp_ = mod_time;
                return *r;
            }
        }

        // backwards search didn't get a result. Now search forwards:
        for (int search_frame = logical_frame; search_frame < frame_count; ++search_frame) {

            auto r = held_frame_check(
                search_frame); // this also sets mod_time to that of the found frame
            if (r) {
                frame_status                     = FS_HELD_FRAME;
                uri_status_cache_[logical_frame] = UriStatus(*r, FS_HELD_FRAME, keyframe);
                uri_status_cache_[logical_frame].mod_timestamp_ = mod_time;
                return *r;
            }
        }

        // search for an on-disk frame to hold on this (missing) frame has failed
        frame_status = FS_NOT_ON_DISK;

    } else if (media_ref.container()) {

        if (!container_file_timestamp_.time_since_epoch().count()) {
            try {
                container_file_timestamp_ =
                    fs::last_write_time(utility::uri_to_posix_path(*_uri));
            } catch (...) {
                container_file_timestamp_ = std::chrono::file_clock::now();
            }
        }
    }
    return *_uri;
}

void MediaSourceActor::update_stream_media_reference(
    StreamDetail &stream_detail,
    const utility::Uuid &stream_uuid,
    const utility::FrameRate &rate,
    const utility::Timecode &timecode) {

    MediaReference media_reference = base_.media_reference(stream_uuid);

    if (not media_reference.timecode().total_frames())
        media_reference.set_timecode(timecode);

    if (!media_reference.container() &&
        base_.partial_seq_behaviour() != PS_COLLAPSE_TO_ON_DISK_FRAMES) {

        // at this point, media_reference will contain a list of numbered frames that
        // are on disk only. If we don't have PS_COLLAPSE_TO_ON_DISK_FRAMES behaviour,
        // we now tell the media_reference to fill in the gaps in the frame numbers (even
        // if they are not on disk) so that we can use held frame behaviour.
        media_reference.fill_partial_sequences();
    }

    // we don't know duration, either movie or single frame
    if (not media_reference.duration().duration().count()) {
        // movie..
        if (stream_detail.duration_.duration().count()) {
            media_reference.set_duration(stream_detail.duration_);
            media_reference.set_frame_list(FrameList(0, stream_detail.duration_.frames() - 1));
        } else {
            if (stream_detail.duration_.rate().count()) {
                media_reference.set_duration(
                    FrameRateDuration(1, stream_detail.duration_.rate()));
                stream_detail.duration_ = FrameRateDuration(1, stream_detail.duration_.rate());
            } else {
                media_reference.set_duration(FrameRateDuration(1, rate));
                stream_detail.duration_ = FrameRateDuration(1, rate);
            }
            media_reference.set_frame_list(FrameList(0, 0));
        }
    }
    // we know duration but not rate
    else if (stream_detail.duration_.rate().count()) {
        // we know duration, so override rate.
        // effects count..
        int frames              = media_reference.duration().frames();
        stream_detail.duration_ = FrameRateDuration(frames, stream_detail.duration_.rate());

        media_reference.set_duration(FrameRateDuration(frames, stream_detail.duration_.rate()));
    } else {
        if (not media_reference.container()) {
            int frames              = media_reference.duration().frames();
            stream_detail.duration_ = FrameRateDuration(frames, rate);
        } else {
            stream_detail.duration_.set_rate(rate);
        }

        media_reference.set_rate(rate);
    }

    if (not media_reference.container() and
        (!media_reference.timecode().total_frames() || media_reference.frame_list().start())) {
        // If we have image sequence (like EXRs, say) where the frame number
        // from the filename is 1001, then we use the frame number to set
        // the timecode on this source. This means timecode == frame number
        // so we are OVERRIDING the timecode embedded in EXR header data
        // with a timecode from frame number. This is because frame number
        // is paramount in aligning media in a timeline, the embedded
        // timecode is rarely used for this purpose. Also, if the timecode
        // is unknown (or is 00:00:00:00) then we default to using frame
        // number to set the timecode.
        media_reference.set_timecode_from_frames();
    }
    base_.set_media_reference(media_reference);
    uri_status_cache_.clear();
}

void MediaSourceActor::send_stream_metadata_to_stream_actors(const utility::JsonStore &meta) {
    // the metadata object returned by ffprobe is for the whole file with the metadata for
    // each stream also included in an array. Here we search for the stream metadata and
    // pass it to the relevant MediaStreamActor to own.

    if (meta.contains("streams") and meta["streams"].is_array()) {
        const auto streams = meta["streams"];
        for (const auto &stream : streams) {
            if (stream.contains("index") and stream["index"].is_number_integer()) {
                // stream is a ref.. we need a local copy to use in our response
                // (which gets copied again by the [=])
                utility::JsonStore stream_metadata = stream;
                int idx                            = stream_metadata["index"].get<int>();
                mail(get_media_stream_atom_v, idx)
                    .request(caf::actor_cast<caf::actor>(this), infinite)
                    .then(
                        [=](caf::actor stream) {
                            anon_mail(
                                json_store::set_json_atom_v,
                                stream_metadata,
                                "/metadata/stream/@")
                                .send(stream);
                        },
                        [=](caf::error &err) {});
            }
        }
    }
}

void MediaSourceActor::duplicate(caf::typed_response_promise<utility::UuidUuidActor> rp) {
    auto uuid = utility::Uuid::generate();
    auto actor =
        spawn<MediaSourceActor>(base_.name(), base_.reader(), base_.media_reference(), uuid);

    // using a lambda to try and make this more, err, 'readable'
    auto copy_metadata = [=](UuidActor destination,
                             caf::typed_response_promise<UuidUuidActor> rp) {
        mail(json_store::get_json_atom_v)
            .request(json_store_, infinite)
            .then(
                [=](const JsonStore &meta) mutable {
                    mail(json_store::set_json_atom_v, meta)
                        .request(destination.actor(), infinite)
                        .then(
                            [=](bool) mutable {
                                rp.deliver(UuidUuidActor(base_.uuid(), destination));
                            },
                            [=](const error &err) mutable { rp.deliver(err); });
                },
                [=](const error &err) mutable { rp.deliver(err); });
    };

    // duplicate streams..
    if (media_streams_.size()) {
        auto source_count = std::make_shared<int>();
        (*source_count)   = media_streams_.size();
        for (auto p : media_streams_) {
            mail(utility::duplicate_atom_v)
                .request(p.second, infinite)
                .await(
                    [=](UuidActor stream) mutable {
                        // add the stream to the duplicated media_source_actor
                        mail(add_media_stream_atom_v, stream)
                            .request(actor, infinite)
                            .await(
                                [=](UuidActor) mutable {
                                    // set the current stream as required
                                    if (p.first == base_.current(MT_IMAGE)) {

                                        anon_mail(
                                            current_media_stream_atom_v,
                                            MT_IMAGE,
                                            stream.uuid())
                                            .send(actor);

                                    } else if (p.first == base_.current(MT_AUDIO)) {

                                        anon_mail(
                                            current_media_stream_atom_v,
                                            MT_AUDIO,
                                            stream.uuid())
                                            .send(actor);
                                    }

                                    (*source_count)--;
                                    if (!(*source_count)) {
                                        copy_metadata(UuidActor(uuid, actor), rp);
                                    }
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
        }
    } else {
        copy_metadata(UuidActor(uuid, actor), rp);
    }
}
