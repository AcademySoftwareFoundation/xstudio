// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
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

namespace {
            class DebugTimer {
          public:
            DebugTimer(std::string p, int frame) : path_(std::move(p)), frame_(frame) {
                t1_ = utility::clock::now();
            }

            ~DebugTimer() {
                std::cerr << "Reading" << path_ << " @ " << frame_ << " done in "
                          << double(std::chrono::duration_cast<std::chrono::microseconds>(
                                        utility::clock::now() - t1_)
                                        .count()) /
                                 1000000.0
                          << " seconds\n";
            }
            void gronk() {
                std::cerr << "BAT\n";
            }
          private:
            utility::time_point t1_;
            const std::string path_;
            const int frame_;
        };


}

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

    pitem = base_.item().clone();
    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const Item &item)
    : caf::event_based_actor(cfg), base_(item, this) {
    base_.item().set_system(&system());
    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const Item &item, Item &nitem)
    : ClipActor(cfg, item) {
    nitem = base_.item().clone();
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
            auto ref = request_receive<std::pair<Uuid, MediaReference>>(
                           *sys, media.actor(), media::media_reference_atom_v, utility::Uuid())
                           .second;

            if (name.empty()) {
                base_.item().set_name(
                    fs::path(uri_to_posix_path(ref.uri())).filename().string());
            }

            if (ref.frame_count()) {
                // base_.item().set_available_range(utility::FrameRange(ref.duration()));
                auto tc_start = static_cast<int>(ref.timecode().total_frames());

                auto mfr = utility::FrameRange(
                    FrameRateDuration(tc_start, ref.duration().rate()), ref.duration());
                base_.item().set_range(mfr, mfr);
            } else
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

void ClipActor::link_media(caf::typed_response_promise<bool> rp, const UuidActor &media) {

    // spdlog::warn("link_media_atom {}", to_string(media));
    auto old_media_actor = caf::actor_cast<caf::actor>(media_);
    if (old_media_actor) {
        demonitor(old_media_actor);
        leave_event_group(this, old_media_actor);
    }

    monitor(media.actor());
    join_event_group(this, media.actor());
    media_ = caf::actor_cast<caf::actor_addr>(media.actor());

    auto jsn = base_.set_media_uuid(media.uuid());
    if (not jsn.is_null())
        send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);

    // clear ptr cache
    image_ptr_cache_.clear();
    audio_ptr_cache_.clear();

    // force update ?
    // cache available range ?
    delayed_send(
        caf::actor_cast<caf::actor>(this),
        std::chrono::milliseconds(100),
        media::acquire_media_detail_atom_v);

    // replace clip name ?
    request(media.actor(), infinite, name_atom_v)
        .then(
            [=](const std::string &value) mutable { anon_send(this, item_name_atom_v, value); },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });

    rp.deliver(true);
}

caf::message_handler ClipActor::message_handler() {
    return caf::message_handler{
        // replace current media..
        [=](link_media_atom, const UuidActor &media) -> result<bool> {
            auto rp = make_response_promise<bool>();
            link_media(rp, media);
            return rp;
        },

        [=](link_media_atom, const UuidActorMap &media, const bool force) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (media.count(base_.media_uuid())) {
                if (force) {
                    link_media(rp, UuidActor(base_.media_uuid(), media.at(base_.media_uuid())));
                } else {
                    auto media_actor = media.at(base_.media_uuid());
                    auto addr        = caf::actor_cast<caf::actor_addr>(media_actor);

                    if (media_ != addr) {
                        monitor(media_actor);
                        join_event_group(this, media_actor);
                        media_ = addr;
                    }
                    rp.deliver(true);
                }
            } else {
                media_ = caf::actor_addr();
                rp.deliver(true);
            }

            return rp;
        },

        [=](playhead::source_atom,
            const UuidUuidMap &swap,
            const utility::UuidActorMap &media) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (swap.count(base_.media_uuid())) {
                // spdlog::warn("{} {} {}", base_.item().name(), to_string(base_.media_uuid()),
                // to_string(media.at(swap.at(base_.media_uuid()))));
                link_media(
                    rp,
                    UuidActor(
                        swap.at(base_.media_uuid()), media.at(swap.at(base_.media_uuid()))));
            } else
                rp.deliver(false);

            return rp;
        },


        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_lock_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_locked(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](utility::rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](utility::rate_atom atom, const media::MediaType media_type) {
            delegate(caf::actor_cast<caf::actor>(this), atom);
        },

        [=](item_prop_atom, const utility::JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_prop_atom,
            const utility::JsonStore &value,
            const std::string &path) -> JsonStore {
            auto prop = base_.item().prop();
            try {
                auto ptr = nlohmann::json::json_pointer(path);
                prop.at(ptr).update(value, true);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            auto jsn = base_.item().set_prop(prop);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> utility::FrameRange { return base_.item().trimmed_range(); },

        [=](trimmed_range_atom,
            const FrameRange &avail,
            const FrameRange &active) -> JsonStore {
            auto jsn = base_.item().set_range(avail, active);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().undo(hist);
            return true;
        },

        [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().redo(hist);
            return true;
        },

        [=](utility::event_atom,
            timeline::item_atom,
            const utility::JsonStore &update,
            const bool hidden) {
            // where is this coming from??
        },

        [=](item_atom) -> Item { return base_.item().clone(); },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            request(caf::actor_cast<caf::actor>(this), infinite, utility::serialise_atom_v)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item().clone()));
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

        // [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},
        // events from media actor

        // re-evaluate media reference.., needed for lazy loading
        // [=](media_reference_atom atom,
        //     const Uuid &uuid) -> caf::result<std::pair<Uuid, MediaReference>> {

        [=](playlist::reflag_container_atom) -> result<std::tuple<std::string, std::string>> {
            auto rp    = make_response_promise<std::tuple<std::string, std::string>>();
            auto actor = caf::actor_cast<caf::actor>(media_);
            if (actor)
                rp.delegate(actor, playlist::reflag_container_atom_v);
            else
                rp.deliver(make_error(xstudio_error::error, "No media assigned."));

            return rp;
        },


        [=](media::media_reference_atom,
            const media::MediaType media_type) -> result<std::pair<Uuid, MediaReference>> {
            auto rp    = make_response_promise<std::pair<Uuid, MediaReference>>();
            auto actor = caf::actor_cast<caf::actor>(media_);
            if (actor)
                rp.delegate(actor, media::media_reference_atom_v, media_type, Uuid());
            else
                rp.deliver(make_error(xstudio_error::error, "No media assigned."));

            return rp;
        },

        [=](media::acquire_media_detail_atom, const utility::Uuid &uuid) {
            auto actor = caf::actor_cast<caf::actor>(media_);
            // spdlog::warn("acquire_media_detail_atom {} {}", to_string(uuid),
            // to_string(actor));
            if (actor) {
                request(actor, infinite, media::media_reference_atom_v, uuid)
                    .then(
                        [=](const std::pair<Uuid, MediaReference> &ref) {
                            // spdlog::warn("{}",ref.second.frame_count());
                            if (ref.second.frame_count()) {
                                // clear ptr cache
                                auto tc_start =
                                    static_cast<int>(ref.second.timecode().total_frames());

                                // spdlog::warn("{} {} {}", base_.item().name(), tc_start,
                                // ref.second.timecode().to_string());

                                // if(base_.item().available_range()) {
                                //     spdlog::warn("before {} available_range start {} duration
                                //     {}",
                                //         base_.item().name(),
                                //         base_.item().available_range()->frame_start().frames(),
                                //         base_.item().available_range()->frame_duration().frames()
                                //     );
                                //     spdlog::warn("before {} trimmed start {} duration {}",
                                //         base_.item().name(),
                                //         base_.item().trimmed_range().frame_start().frames(),
                                //         base_.item().trimmed_range().frame_duration().frames()
                                //     );
                                // }
                                // else
                                //     spdlog::warn("no available range
                                //     {}",base_.item().name());

                                auto jsn = base_.item().set_available_range(utility::FrameRange(
                                    FrameRateDuration(tc_start, ref.second.duration().rate()),
                                    ref.second.duration()));

                                // spdlog::warn("after {} available_range start {} duration {}",
                                //     base_.item().name(),
                                //     base_.item().available_range()->frame_start().frames(),
                                //     base_.item().available_range()->frame_duration().frames()
                                // );
                                // spdlog::warn("after {} trimmed start {} duration {}",
                                //     base_.item().name(),
                                //     base_.item().trimmed_range().frame_start().frames(),
                                //     base_.item().trimmed_range().frame_duration().frames()
                                // );

                                if (not jsn.is_null())
                                    send(
                                        base_.event_group(),
                                        event_atom_v,
                                        item_atom_v,
                                        jsn,
                                        false);
                            } else {
                                // retry ?
                                // spdlog::warn("delayed get detail");
                                delayed_send(
                                    caf::actor_cast<caf::actor>(this),
                                    std::chrono::seconds(1),
                                    media::acquire_media_detail_atom_v,
                                    uuid);
                            }
                        },
                        [=](const error &err) {
                            // spdlog::warn("errored get detail");
                            // retry not ready ? Or no sources?
                            delayed_send(
                                caf::actor_cast<caf::actor>(this),
                                std::chrono::seconds(1),
                                media::acquire_media_detail_atom_v,
                                uuid);
                        });
            }
        },

        [=](media::acquire_media_detail_atom atom) {
            delegate(caf::actor_cast<caf::actor>(this), atom, Uuid());
        },

        // [=](media::acquire_media_detail_atom) {
        //     auto actor = caf::actor_cast<caf::actor>(media_);
        //     if (actor) {
        //         request(actor, infinite, media::media_reference_atom_v)
        //             .then(
        //                 [=](const std::vector<MediaReference> &refs) {
        //                     if (not refs.empty() and refs[0].frame_count()) {
        //                         // clear ptr cache
        //                         auto jsn = base_.item().set_available_range(
        //                             utility::FrameRange(refs[0].duration()));

        //                         if (not jsn.is_null())
        //                             send(event_group_, event_atom_v, item_atom_v, jsn,
        //                             false);
        //                     } else {
        //                         // retry ?
        //                         // spdlog::warn("delayed get detail");
        //                         delayed_send(
        //                             caf::actor_cast<caf::actor>(this),
        //                             std::chrono::seconds(1),
        //                             media::acquire_media_detail_atom_v);
        //                     }
        //                 },
        //                 [=](const error &err) {});
        //     }
        // },

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
        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &,
            const utility::UuidList &) {},

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](utility::event_atom, bookmark::remove_bookmark_atom, const utility::Uuid &) {},
        [=](utility::event_atom, bookmark::add_bookmark_atom, const utility::UuidActor &) {},

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &,
            caf::actor_addr &) {},

        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &) {},

        // ak this need's to know if the clip uses audio or video SOB.
        // we'll use video for the moment.. BUT THIS IS WRONG.
        [=](utility::event_atom,
            media::current_media_source_atom,
            const UuidActor &ua,
            const media::MediaType mt) {
            // media source has changed, we need to acquire the new available range...

            // spdlog::warn("media::current_media_source_atom {}", to_string(ua.uuid()));

            if (mt == media::MediaType::MT_IMAGE) {
                delayed_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::milliseconds(10),
                    media::acquire_media_detail_atom_v,
                    ua.uuid());
            }

            image_ptr_cache_.clear();
            audio_ptr_cache_.clear();

            send(base_.event_group(), utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom, utility::change_atom) {
            send(base_.event_group(), utility::event_atom_v, utility::change_atom_v);
        },

        [=](json_store::update_atom,
            const JsonStore &,
            const std::string &,
            const JsonStore &) {},

        [=](json_store::update_atom, const JsonStore &) mutable {},

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &) {},

        [=](playlist::get_media_atom, const bool) -> UuidUuidActor {
            return UuidUuidActor(
                base_.uuid(),
                UuidActor(base_.media_uuid(), caf::actor_cast<caf::actor>(media_)));
        },

        // how do we deal with lazy loading of metadata..
        [=](playlist::get_media_atom,
            json_store::get_json_atom,
            const utility::Uuid &uuid,
            const std::string &path) -> result<JsonStore> {
            auto rp    = make_response_promise<JsonStore>();
            auto actor = caf::actor_cast<caf::actor>(media_);
            if (actor)
                rp.delegate(actor, json_store::get_json_atom_v, uuid, path);
            else
                rp.deliver(make_error(xstudio_error::error, "No media assigned."));

            return rp;
        },

        [=](playlist::get_media_atom) -> UuidActor {
            return UuidActor(base_.media_uuid(), caf::actor_cast<caf::actor>(media_));
        },

        [=](xstudio::media::current_media_source_atom atom,
            const media::MediaType mt) -> caf::result<UuidActor> {
            if (media_) {
                auto rp = make_response_promise<UuidActor>();
                rp.delegate(caf::actor_cast<caf::actor>(media_), atom, mt);
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


                // TODO: Optimise and refactor this!! Too slow (often over > 1ms to
                // evaulate)

                // the 'result' here is a vector of std::shared_ptr<const media::AVFrameID> -
                // one element per frame timepoint. We initialise it with blank frames and
                // the logic below will fill in actual frames where possible
                media::AVFrameID blank = *(media::make_blank_frame(media_type, base_.media_uuid(), utility::Uuid(), base_.uuid()));
                auto blank_ptr         = std::make_shared<const media::AVFrameID>(blank);

                // the result data
                auto result = std::make_shared<media::AVFrameIDs>(timepoints.size(), blank_ptr);

                // the response
                auto rp = make_response_promise<media::AVFrameIDs>();

                auto trimmed_start = base_.item().trimmed_start();
                auto available_start =
                    (base_.item().available_start() ? *base_.item().available_start()
                                                    : trimmed_start);

                // build a set of frame ranges for which we haven't already
                // generated AvFrameIDs (and stored in our cache)
                auto ranges = media::LogicalFrameRanges();
                // for each range, we have an index into 'result'
                static const int invalid_frame = std::numeric_limits<int>::lowest();
                auto indexs                    = std::vector<int>();
                auto range_not_in_cache = std::pair<int, int>(invalid_frame, invalid_frame);

                // get a ref to the appropriate AvFrameID cache
                auto &frame_ptr_cache = media_type == media::MediaType::MT_IMAGE
                                            ? image_ptr_cache_
                                            : audio_ptr_cache_;

                auto index = 0;
                // loop over the timeline timepoints for which we've been asked
                // for an AVFrameID
                for (const auto timepoint : timepoints) {

                    // convert the timepoint into a logical frame
                    auto frd = FrameRateDuration(
                        FrameRate(timepoint.to_flicks() - available_start.to_flicks()),
                        override_rate);
                    auto logical_frame = frd.frames(base_.item().rate());

                    // have we cached an AVFrameID for this frame already?
                    if (frame_ptr_cache.count(logical_frame)) {
                        (*result)[index] = frame_ptr_cache[logical_frame];

                        if (range_not_in_cache.first != invalid_frame) {
                            // previous frame was NOT in cache - store the
                            // range for which we need AvFrameIDs
                            ranges.push_back(range_not_in_cache);
                            range_not_in_cache.first  = invalid_frame;
                            range_not_in_cache.second = invalid_frame;
                        }
                    } else {

                        // extend the frame range for which we need to
                        if (range_not_in_cache.first != invalid_frame) {
                            if (logical_frame != range_not_in_cache.second + 1) {

                                ranges.push_back(range_not_in_cache);
                                range_not_in_cache.first  = invalid_frame;
                                range_not_in_cache.second = invalid_frame;
                            } else {
                                range_not_in_cache.second = logical_frame;
                            }
                        }

                        // start a new range to record frames where we don't
                        // already have AvFrameIDs in the cache
                        if (range_not_in_cache.first == invalid_frame) {
                            range_not_in_cache.first  = logical_frame;
                            range_not_in_cache.second = logical_frame;
                            indexs.push_back(index);
                        }
                    }

                    index++;
                }

                if (range_not_in_cache.first != invalid_frame) {
                    ranges.push_back(range_not_in_cache);
                }

                if (indexs.empty()) {
                    // all our frames were in the cache
                    rp.deliver(*result);
                } else {
                    request(
                        caf::actor_cast<caf::actor>(media_),
                        infinite,
                        media::get_media_pointers_atom_v,
                        media_type,
                        ranges,
                        FrameRate(),
                        base_.uuid())
                        .then(
                            [=](const media::AVFrameIDs &mps) mutable {
                                // spdlog::warn("const media::AVFrameIDs &mps {} {}",
                                // base_.item().name(), mps.size());

                                auto mp = mps.begin();
                                int ct  = 0;
                                for (size_t i = 0; i < indexs.size(); i++) {
                                    auto ind = indexs[i];
                                    auto rng = ranges[i];
                                    for (auto ii = rng.first; ii <= rng.second; ii++, ind++) {

                                        // if we got our logic above correct this shouldn't
                                        // happen as indexs and ranges match in length and
                                        // mps size is the sum of ranges
                                        if (mp == mps.end())
                                            break;

                                        if (media_type == media::MediaType::MT_IMAGE) {
                                            image_ptr_cache_[ii] = *mp;
                                            (*result)[ind]       = *mp;//image_ptr_cache_[ii];

                                            // spdlog::warn("{}", (*mp)->key_);
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
                                // can get an error for missing media - instead
                                // of passing error up we allow delivery of
                                // the blank frames
                                // rp.deliver(std::move(err));
                                rp.deliver(*result);
                            });
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


        // [=](xstudio::media::current_media_source_atom atom) {
        //     delegate(caf::actor_cast<caf::actor>(media_), atom);
        // },
        // [=](xstudio::playlist::reflag_container_atom atom) {
        //     delegate(caf::actor_cast<caf::actor>(media_), atom);
        // },

        [=](utility::duplicate_atom) -> result<UuidActor> {
            JsonStore jsn;
            auto dup    = base_.duplicate();
            jsn["base"] = dup.serialise();

            auto actor = spawn<ClipActor>(jsn);
            UuidActorMap media_map;
            media_map[base_.media_uuid()] = caf::actor_cast<caf::actor>(media_);

            auto rp = make_response_promise<UuidActor>();

            request(actor, infinite, link_media_atom_v, media_map, false)
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
        },

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
                
            // This is required by SubPlayhead actor to make the track
            // playable.
            return base_.item().get_all_frame_IDs(
                media_type,
                tsm,
                override_rate);

        }};
}

void ClipActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    // we die if our media dies.
    set_down_handler([=](down_msg &msg) {
        if (msg.source == media_) {
            demonitor(caf::actor_cast<caf::actor>(media_));
            send_exit(this, caf::exit_reason::user_shutdown);
        }
    });
}

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
