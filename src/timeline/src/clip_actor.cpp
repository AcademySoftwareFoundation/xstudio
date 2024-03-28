// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

ClipActor::ClipActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);
    base_.item().set_system(&system());

    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const JsonStore &jsn, Item &pitem)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);
    base_.item().set_system(&system());

    pitem = base_.item();
    init();
}

ClipActor::ClipActor(
    caf::actor_config &cfg,
    const utility::UuidActor &media,
    const std::string &name,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg),
      // playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(name, uuid, this, media.uuid()) {
    base_.item().set_system(&system());
    base_.item().set_name(name);

    if (media.actor()) {

        media_ = caf::actor_cast<caf::actor_addr>(media.actor());

        monitor(media.actor());
        join_event_group(this, media.actor());

        try {
            caf::scoped_actor sys(system());
            auto ref = request_receive<std::vector<MediaReference>>(
                *sys, media.actor(), media::media_reference_atom_v)[0];

            if (name.empty()) {
                base_.item().set_name(
                    fs::path(uri_to_posix_path(ref.uri())).filename().string());
            }

            if (ref.frame_count())
                base_.item().set_available_range(utility::FrameRange(ref.duration()));
            else
                delayed_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::milliseconds(100),
                    media::acquire_media_detail_atom_v);

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    init();
}


void ClipActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    // we die if our media dies.
    set_down_handler([=](down_msg &msg) {
        if (msg.source == media_) {
            demonitor(caf::actor_cast<caf::actor>(media_));
            send_exit(this, caf::exit_reason::user_shutdown);
        }
    });

    // monitor changes in media..
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

        [=](link_media_atom, const UuidActorMap &media) -> bool {
            if (media.count(base_.media_uuid())) {
                auto media_actor = media.at(base_.media_uuid());
                auto addr        = caf::actor_cast<caf::actor_addr>(media_actor);

                if (media_ != addr) {
                    monitor(media_actor);
                    join_event_group(this, media_actor);
                    media_ = addr;
                }
            } else {
                media_ = caf::actor_addr();
            }
            return true;
        },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> utility::FrameRange { return base_.item().trimmed_range(); },

        [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().undo(hist);
            return true;
        },

        [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().redo(hist);
            return true;
        },

        [=](item_atom) -> Item { return base_.item(); },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            request(caf::actor_cast<caf::actor>(this), infinite, utility::serialise_atom_v)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item()));
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },
        // [=](duration_atom) -> utility::FrameRateDuration { return base_.duration(); },

        // [=](duration_atom, const utility::FrameRateDuration &duration) -> bool {
        //     base_.set_duration(duration);
        //     update_edit_list();
        //     return true;
        // },

        // [=](media::get_edit_list_atom, const Uuid &uuid) -> utility::EditList {
        //     auto edit_list = clip_edit_list_;

        //     if (not uuid.is_null())
        //         edit_list.set_uuid(uuid);
        //     else
        //         edit_list.set_uuid(base_.uuid());

        //     return edit_list;
        // },

        // [=](media::get_media_pointer_atom,
        //     const int logical_frame) -> result<media::AVFrameID> {
        //     if (base_.media_uuid().is_null())
        //         return result<media::AVFrameID>(make_error(xstudio_error::error, "No
        //         media"));

        //     auto rp = make_response_promise<media::AVFrameID>();
        //     deliver_media_pointer(logical_frame, rp);
        //     return rp;
        // },

        // [=](media::get_media_type_atom) -> media::MediaType { return base_.media_type(); },

        // [=](playlist::get_media_atom) -> UuidActor {
        //     return UuidActor(base_.media_uuid(), caf::actor_cast<actor>(media_));
        // },

        // [=](start_time_atom) -> utility::FrameRateDuration { return base_.start_time(); },

        // [=](start_time_atom, const utility::FrameRateDuration &start_time) -> bool {
        //     base_.set_start_time(start_time);
        //     update_edit_list();
        //     return true;
        // },

        // [&](utility::event_atom, utility::change_atom) {
        //     request(actor_cast<actor>(media_), infinite, media::get_edit_list_atom_v, Uuid())
        //         .then(
        //             [&](const utility::EditList &sl) {
        //                 media_edit_list_ = sl;
        //                 update_edit_list();
        //             },
        //             [=](const error &err) {
        //                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        //             });
        // },

        // [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},
        // events from media actor

        // re-evaluate media reference.., needed for lazy loading
        [=](media::acquire_media_detail_atom) {
            auto actor = caf::actor_cast<caf::actor>(media_);
            if (actor) {
                request(actor, infinite, media::media_reference_atom_v)
                    .then(
                        [=](const std::vector<MediaReference> &refs) {
                            if (not refs.empty() and refs[0].frame_count()) {
                                auto jsn = base_.item().set_available_range(
                                    utility::FrameRange(refs[0].duration()));

                                if (not jsn.is_null())
                                    send(event_group_, event_atom_v, item_atom_v, jsn, false);
                            } else {
                                // retry ?
                                delayed_send(
                                    caf::actor_cast<caf::actor>(this),
                                    std::chrono::seconds(1),
                                    media::acquire_media_detail_atom_v);
                            }
                        },
                        [=](const error &err) {});
            }
        },

        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookmark_uuid) {
            // not sure why we want this..
            // send(
            //     event_group_,
            //     utility::event_atom_v,
            //     bookmark::bookmark_change_atom_v,
            //     bookmark_uuid);
        },

        [=](utility::event_atom, bookmark::remove_bookmark_atom, const utility::Uuid &) {},
        [=](utility::event_atom, bookmark::add_bookmark_atom, const utility::UuidActor &) {},

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},
        [=](utility::event_atom,
            media::current_media_source_atom,
            const UuidActor &,
            const media::MediaType) {
            image_ptr_cache_.clear();
            audio_ptr_cache_.clear();
        },
        [=](utility::event_atom, utility::change_atom) {},
        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &) {},

        [=](playlist::get_media_atom) -> UuidActor {
            return UuidActor(base_.media_uuid(), caf::actor_cast<caf::actor>(media_));
        },

        [=](xstudio::media::current_media_source_atom atom) -> caf::result<UuidActor> {
            if (media_) {
                auto rp = make_response_promise<UuidActor>();
                rp.delegate(caf::actor_cast<caf::actor>(media_), atom);
                return rp;
            }
            return make_error(xstudio_error::error, "No media assigned.");
        },
        [=](xstudio::playlist::reflag_container_atom atom)
            -> result<std::tuple<std::string, std::string>> {
            if (media_) {
                auto rp = make_response_promise<std::tuple<std::string, std::string>>();
                rp.delegate(caf::actor_cast<caf::actor>(media_), atom);
                return rp;
            }
            return make_error(xstudio_error::error, "No media assigned.");
        },

        [=](media::get_media_pointer_atom,
            const media::MediaType media_type,
            const std::vector<FrameRate> &timepoints,
            const FrameRate &override_rate) -> result<media::AVFrameIDs> {
            if (media_) {
                auto result = std::make_shared<media::AVFrameIDs>(timepoints.size());
                auto rp     = make_response_promise<media::AVFrameIDs>();

                auto trimmed_start = base_.item().trimmed_start();
                auto active_start =
                    (base_.item().active_start() ? *base_.item().active_start()
                                                 : trimmed_start);
                auto available_start =
                    (base_.item().available_start() ? *base_.item().available_start()
                                                    : trimmed_start);

                // spdlog::warn("trs {} avs {} act {}", trimmed_start.to_seconds(),
                // available_start.to_seconds(), active_start.to_seconds());

                // build logical frame ranges.
                auto ranges = media::LogicalFrameRanges();
                auto indexs = std::vector<int>();
                auto range  = std::pair<int, int>(-1, -1);

                auto index = 0;
                for (const auto timepoint : timepoints) {
                    auto frd = FrameRateDuration(
                        FrameRate(timepoint.to_flicks() - available_start.to_flicks()),
                        override_rate);
                    auto logical_frame = frd.frames(base_.item().rate());

                    if (media_type == media::MediaType::MT_IMAGE and
                        image_ptr_cache_.count(logical_frame)) {
                        (*result)[index] = image_ptr_cache_[logical_frame];
                        if (range.first != -1) {
                            ranges.push_back(range);
                            range.first  = -1;
                            range.second = -1;
                        }
                    } else if (
                        media_type == media::MediaType::MT_AUDIO and
                        audio_ptr_cache_.count(logical_frame)) {
                        (*result)[index] = audio_ptr_cache_[logical_frame];
                        if (range.first != -1) {
                            ranges.push_back(range);
                            range.first  = -1;
                            range.second = -1;
                        }
                    } else {
                        // extend or flush rane
                        if (range.first != -1) {
                            if (logical_frame != range.second + 1) {
                                ranges.push_back(range);
                                range.first  = -1;
                                range.second = -1;
                            } else {
                                range.second = logical_frame;
                            }
                        }

                        if (range.first == -1) {
                            range.first  = logical_frame;
                            range.second = logical_frame;
                            indexs.push_back(index);
                        }
                    }

                    index++;
                }

                if (range.first != -1)
                    ranges.push_back(range);

                if (indexs.empty()) {
                    rp.deliver(*result);
                } else {
                    request(
                        caf::actor_cast<caf::actor>(media_),
                        infinite,
                        media::get_media_pointers_atom_v,
                        media_type,
                        ranges,
                        FrameRate())
                        .then(
                            [=](const media::AVFrameIDs &mps) mutable {
                                auto mp = mps.begin();
                                for (size_t i = 0; i < indexs.size(); i++) {
                                    auto ind = indexs[i];
                                    auto rng = ranges[i];
                                    for (auto ii = rng.first; ii <= rng.second; ii++, ind++) {
                                        if (media_type == media::MediaType::MT_IMAGE) {
                                            image_ptr_cache_[ii] = *mp;
                                            (*result)[ind]       = image_ptr_cache_[ii];
                                        } else if (media_type == media::MediaType::MT_AUDIO) {
                                            audio_ptr_cache_[ii] = *mp;
                                            (*result)[ind]       = audio_ptr_cache_[ii];
                                        }
                                        mp++;
                                    }
                                }
                                // spdlog::warn("deliver {}", result->size());
                                rp.deliver(*result);
                            },
                            [=](error &err) mutable {
                                for (size_t i = 0; i < indexs.size(); i++) {
                                    auto ind = indexs[i];
                                    auto rng = ranges[i];
                                    for (auto ii = rng.first; ii <= rng.second; ii++, ind++)
                                        (*result)[ind] = media::make_blank_frame(media_type);
                                }
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                // rp.deliver(std::move(err));
                                rp.deliver(*result);
                            });

                    // for(size_t i = 0; i < indexs.size(); i++) {
                    //     auto ind = indexs[i];
                    //     auto rng = ranges[i];

                    //     try {
                    //         // FrameRate() might need setting correctly..
                    //         auto mps = request_receive<media::AVFrameIDs>(
                    //             *sys, caf::actor_cast<caf::actor>(media_),
                    //             media::get_media_pointers_atom_v, media_type,
                    //             media::LogicalFrameRanges({rng}), FrameRate());

                    //         for(auto mp : mps ){
                    //             if(media_type == media::MediaType::MT_IMAGE) {
                    //                 image_ptr_cache_[ind] = mp;
                    //                 (*result)[ind] = image_ptr_cache_[ind];
                    //             } else if(media_type == media::MediaType::MT_AUDIO) {
                    //                 audio_ptr_cache_[ind] = mp;
                    //                 (*result)[ind] = audio_ptr_cache_[ind];
                    //             }
                    //             ind++;
                    //         }

                    //     } catch (const std::exception &err) {
                    //         for(auto ii = rng.first; ii <= rng.second; ii++, ind++)
                    //             (*result)[ind] = make_blank_frame(media_type);
                    //         spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    //     }
                    // }
                    // rp.deliver(*result);
                }
                return rp;
            }

            return make_error(xstudio_error::error, "No media assigned.");
        },

        // [=](media::get_media_pointer_atom, const media::MediaType media_type, const FrameRate
        // &timepoint, const FrameRate &override_rate) -> result<std::shared_ptr<const
        // media::AVFrameID>> {
        //     if(media_) {
        //         auto trimmed_start = base_.item().trimmed_start();
        //         auto active_start = (base_.item().active_start() ? *
        //         base_.item().active_start() : trimmed_start); auto available_start =
        //         (base_.item().available_start() ? * base_.item().available_start() :
        //         trimmed_start);

        //         // spdlog::warn("trs {} avs {} act {}", trimmed_start.to_seconds(),
        //         available_start.to_seconds(), active_start.to_seconds());

        //         auto frd =
        //         FrameRateDuration(FrameRate(timepoint.to_flicks()-available_start.to_flicks()),
        //         override_rate); auto logical_frame = frd.frames(base_.item().rate());

        //         if(media_type == media::MediaType::MT_IMAGE and
        //         image_ptr_cache_.count(logical_frame)) {
        //             return image_ptr_cache_[logical_frame];
        //         } else if(media_type == media::MediaType::MT_AUDIO and
        //         audio_ptr_cache_.count(logical_frame)) {
        //             return audio_ptr_cache_[logical_frame];
        //         } else {
        //             auto rp = make_response_promise<std::shared_ptr<const
        //             media::AVFrameID>>(); request(caf::actor_cast<caf::actor>(media_),
        //             infinite, media::get_media_pointer_atom_v, media_type,
        //             logical_frame).then(
        //                 [=](const media::AVFrameID &mp) mutable {
        //                     if(media_type == media::MediaType::MT_IMAGE) {
        //                         image_ptr_cache_[logical_frame] =
        //                         std::make_shared<media::AVFrameID>(std::move(mp));
        //                         rp.deliver(image_ptr_cache_[logical_frame]);
        //                     } else if(media_type == media::MediaType::MT_AUDIO) {
        //                         audio_ptr_cache_[logical_frame] =
        //                         std::make_shared<media::AVFrameID>(std::move(mp));
        //                         rp.deliver(audio_ptr_cache_[logical_frame]);
        //                     } else {
        //                         rp.deliver(make_error(xstudio_error::error, "Unknown media
        //                         type."));
        //                     }
        //                 },
        //                 [=](error &err) mutable { rp.deliver(std::move(err)); }
        //             );
        //             return rp;
        //         }
        //     }

        //     return make_error(xstudio_error::error, "No media assigned.");
        // },


        [=](xstudio::media::current_media_source_atom atom) {
            delegate(caf::actor_cast<caf::actor>(media_), atom);
        },
        [=](xstudio::playlist::reflag_container_atom atom) {
            delegate(caf::actor_cast<caf::actor>(media_), atom);
        },

        [=](utility::duplicate_atom) -> result<UuidActor> {
            JsonStore jsn;
            auto dup    = base_.duplicate();
            jsn["base"] = dup.serialise();

            auto actor = spawn<ClipActor>(jsn);
            UuidActorMap media_map;
            media_map[base_.media_uuid()] = caf::actor_cast<caf::actor>(media_);

            auto rp = make_response_promise<UuidActor>();

            request(actor, infinite, link_media_atom_v, media_map)
                .then(
                    [=](const bool) mutable { rp.deliver(UuidActor(dup.uuid(), actor)); },
                    [=](const caf::error &err) mutable {
                        send_exit(actor, caf::exit_reason::user_shutdown);
                        rp.deliver(err);
                    });

            return rp;
        },

        [=](utility::serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        }

    );
}

// void ClipActor::update_edit_list() {
//     ClipList sl = media_edit_list_.section_list();

//     sl[0].frame_rate_and_duration_ = base_.duration();
//     sl[0].timecode_                = sl[0].timecode_ + base_.start_time().frames();

//     clip_edit_list_ = EditList(sl);

//     // all we do is adjust the duration ? and the timecode?
//     // adjust edit_list based off our start_time and duration..
//     // a media object should really only return one entry, otherwise we've no idea how to
//     adjust
//     // the STL start_time we set the uuid to ourself as we're the ones that return
//     mediapointer.
//     // remapped to our start_time/duration.
//     send(event_group_, utility::event_atom_v, utility::change_atom_v);
// }

// void ClipActor::deliver_media_pointer(
//     const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp) {
//     // send request to media actor but offset the logical_frame.
//     // also send no frame error if duration exceeds our duration.
//     if (logical_frame >= base_.duration().frames())
//         rp.deliver(make_error(xstudio_error::error, "No frames left"));
//     else {
//         int request_frame = logical_frame + base_.start_time().frames();
//         request(
//             actor_cast<actor>(media_), infinite, media::get_media_pointer_atom_v,
//             media::MT_IMAGE, request_frame) .then(
//                 [=](const media::AVFrameID &mptr) mutable { rp.deliver(mptr); },
//                 [=](error &err) mutable { rp.deliver(std::move(err)); });
//     }
// }
