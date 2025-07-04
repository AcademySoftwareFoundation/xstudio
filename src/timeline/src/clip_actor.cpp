// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace std::literals;

namespace {
// helper function that makes a set of std::pair<int,int>, condensing
// contiguous numbers to their end points.
// e.g. 1,2,3,8,9,10,20,30,31 -> (1,3), (8,9), (20,20), (30,31)
struct RangeMaker {
    inline static const int invalid_frame = -1234442;
    int a                                 = {invalid_frame};
    int b                                 = {invalid_frame};
    void extend(int f) {
        if (a == invalid_frame) {
            a = f;
            b = f;
        } else if (f > (b + 1)) {
            r.emplace_back(a, b);
            a = f;
            b = f;
        } else {
            b = f;
        }
    }
    media::LogicalFrameRanges result() {
        if (a != invalid_frame) {
            r.emplace_back(a, b);
        }
        return r;
    }
    media::LogicalFrameRanges r;
};
} // namespace

ClipActor::ClipActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);

    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const JsonStore &jsn, Item &pitem)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);

    pitem = base_.item().clone();
    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const Item &item)
    : caf::event_based_actor(cfg), base_(item, this) {
    init();
}

ClipActor::ClipActor(caf::actor_config &cfg, const Item &item, Item &nitem)
    : ClipActor(cfg, item) {
    nitem = base_.item().clone();
}

ClipActor::ClipActor(
    caf::actor_config &cfg, const UuidActor &media, const std::string &name, const Uuid &uuid)
    : caf::event_based_actor(cfg),
      // playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(name, uuid, this, media.uuid()) {

    // already done in base clase ?
    // base_.item().set_name(name);

    if (media.actor()) {

        media_ = caf::actor_cast<caf::actor_addr>(media.actor());

        // monitor_media(media.actor());

        join_event_group(this, media.actor());

        try {
            caf::scoped_actor sys(system());
            auto ref = request_receive<std::pair<Uuid, MediaReference>>(
                           *sys, media.actor(), media::media_reference_atom_v, Uuid())
                           .second;

            if (name.empty()) {
                mail(name_atom_v)
                    .delay(1s)
                    .request(media.actor(), infinite)
                    .then(
                        [=](const std::string &value) mutable {
                            anon_mail(item_name_atom_v, value).send(this);
                        },
                        [=](const caf::error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
                // base_.item().set_name(
                //     fs::path(uri_to_posix_path(ref.uri())).filename().string());
            }

            if (ref.frame_count()) {
                // base_.item().set_available_range(FrameRange(ref.duration()));
                auto tc_start = static_cast<int>(ref.timecode().total_frames());

                auto mfr = FrameRange(
                    FrameRateDuration(tc_start, ref.duration().rate()), ref.duration());
                base_.item().set_range(mfr, mfr);
                base_.override_media_rate(ref.duration().rate());

                auto m_actor =
                    system().registry().template get<caf::actor>(media_hook_registry);
                anon_mail(media_hook::get_clip_hook_atom_v, this).send(m_actor);
            } else
                mail(media::acquire_media_detail_atom_v)
                    .delay(std::chrono::milliseconds(100))
                    .send(caf::actor_cast<caf::actor>(this));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    init();
}

void ClipActor::link_media(
    caf::typed_response_promise<bool> rp, const UuidActor &media, const bool refresh) {

    // spdlog::warn("link_media_atom {}", to_string(media));
    auto old_media_actor = caf::actor_cast<caf::actor>(media_);
    if (old_media_actor)
        leave_event_group(this, old_media_actor);

    if (media.actor()) {
        // monitor(media.actor());
        join_event_group(this, media.actor());
        media_ = caf::actor_cast<caf::actor_addr>(media.actor());
    } else
        media_ = caf::actor_addr();

    auto jsn = base_.set_media_uuid(media.uuid());
    if (not jsn.is_null())
        mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());

    // clear ptr cache
    image_ptr_cache_.clear();
    audio_ptr_cache_.clear();

    if (refresh) {
        // force update ?
        // cache available range ?
        if (media.actor()) {
            mail(media::acquire_media_detail_atom_v)
                .delay(std::chrono::milliseconds(100))
                .send(caf::actor_cast<caf::actor>(this));

            // replace clip name ?
            mail(name_atom_v)
                .request(media.actor(), infinite)
                .then(
                    [=](const std::string &value) mutable {
                        anon_mail(item_name_atom_v, value).send(this);
                    },
                    [=](const caf::error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        } else {
            anon_mail(item_name_atom_v, "NO CLIP").send(this);
            auto m_actor = system().registry().template get<caf::actor>(media_hook_registry);
            anon_mail(media_hook::get_clip_hook_atom_v, this).send(m_actor);
        }
    }

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
                        // monitor_media(media_actor);
                        // shouldn't we leave previous events ?
                        // leave_event_group(this, media_);
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
            const UuidActorMap &media) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (swap.count(base_.media_uuid())) {
                // spdlog::warn("{} {} {}", base_.item().name(), to_string(base_.media_uuid()),
                // to_string(media.at(swap.at(base_.media_uuid()))));
                link_media(
                    rp,
                    UuidActor(
                        swap.at(base_.media_uuid()), media.at(swap.at(base_.media_uuid()))),
                    false);
            } else
                rp.deliver(false);

            return rp;
        },


        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_type_atom) -> ItemType { return base_.item().item_type(); },

        [=](item_lock_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_locked(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](rate_atom atom, const media::MediaType media_type) {
            return mail(atom).delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](rate_atom,
            const utility::FrameRate &new_rate,
            const bool force_media_rate_to_match) -> bool {
            base_.item().override_frame_rate(new_rate, force_media_rate_to_match);
            if (force_media_rate_to_match)
                base_.override_media_rate(new_rate);
            return true;
        },

        [=](item_prop_atom, const JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const bool merge) -> JsonStore {
            auto p = base_.item().prop();
            p.update(value);
            auto jsn = base_.item().set_prop(p);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const std::string &path) -> JsonStore {
            auto prop = base_.item().prop();
            try {
                auto ptr = nlohmann::json::json_pointer(path);
                prop.at(ptr).update(value, true);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            auto jsn = base_.item().set_prop(prop);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](active_range_atom) -> std::optional<FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> FrameRange { return base_.item().trimmed_range(); },

        [=](trimmed_range_atom,
            const FrameRange &avail,
            const FrameRange &active) -> JsonStore {
            auto jsn = base_.item().set_range(avail, active);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
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

        [=](event_atom, item_atom, const JsonStore &update, const bool hidden) {
            // where is this coming from??
        },

        [=](item_atom) -> Item { return base_.item().clone(); },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            mail(serialise_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item().clone()));
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

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

        [=](media::acquire_media_detail_atom, const Uuid &uuid) {
            auto actor = caf::actor_cast<caf::actor>(media_);
            // spdlog::warn("acquire_media_detail_atom {} {}", to_string(uuid),
            // to_string(actor));
            if (actor) {
                mail(media::media_reference_atom_v, uuid)
                    .request(actor, infinite)
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

                                auto jsn = base_.item().set_available_range(FrameRange(
                                    FrameRateDuration(tc_start, ref.second.duration().rate()),
                                    ref.second.duration()));

                                base_.override_media_rate(ref.second.duration().rate());

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
                                    mail(event_atom_v, item_atom_v, jsn, false)
                                        .send(base_.event_group());

                                auto m_actor = system().registry().template get<caf::actor>(
                                    media_hook_registry);
                                anon_mail(media_hook::get_clip_hook_atom_v, this).send(m_actor);
                            } else {
                                // retry ?
                                // spdlog::warn("delayed get detail");

                                mail(media::acquire_media_detail_atom_v, uuid)
                                    .delay(std::chrono::seconds(1))
                                    .send(caf::actor_cast<caf::actor>(this));
                            }
                        },
                        [=](const error &err) {
                            // spdlog::warn("errored get detail");
                            // retry not ready ? Or no sources?
                            mail(media::acquire_media_detail_atom_v, uuid)
                                .delay(std::chrono::seconds(1))
                                .send(caf::actor_cast<caf::actor>(this));
                        });
            }
        },

        [=](media::acquire_media_detail_atom atom) {
            return mail(atom, Uuid()).delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](event_atom,
            playlist::reflag_container_atom,
            const Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](event_atom, bookmark::bookmark_change_atom, const Uuid &bookmark_uuid) {
            // not sure why we want this..
            // mail(//     event_atom_v,
            //     bookmark::bookmark_change_atom_v,
            //     bookmark_uuid).send(//     event_group_);
        },
        [=](event_atom, bookmark::bookmark_change_atom, const Uuid &, const UuidList &) {},

        [=](event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](event_atom, bookmark::remove_bookmark_atom, const Uuid &) {},
        [=](event_atom, bookmark::add_bookmark_atom, const UuidActor &) {},

        [=](event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](event_atom, media::media_display_info_atom, const JsonStore &, caf::actor_addr &) {
        },

        [=](event_atom, media::media_display_info_atom, const JsonStore &) {},

        // ak this need's to know if the clip uses audio or video SOB.
        // we'll use video for the moment.. BUT THIS IS WRONG.
        [=](event_atom,
            media::current_media_source_atom,
            const UuidActor &ua,
            const media::MediaType mt) {
            // media source has changed, we need to acquire the new available range...

            // spdlog::warn("media::current_media_source_atom {}", to_string(ua.uuid()));

            if (mt == media::MediaType::MT_IMAGE) {

                mail(media::acquire_media_detail_atom_v, ua.uuid())
                    .delay(std::chrono::milliseconds(10))
                    .send(caf::actor_cast<caf::actor>(this));
            }

            image_ptr_cache_.clear();
            audio_ptr_cache_.clear();

            mail(event_atom_v, change_atom_v).send(base_.event_group());
        },

        [=](event_atom, change_atom) {
            // something has changed. It means we might need to re-generate our
            // frame pointers for playback. For example, if media source metadata
            // has changed that affects colour management, we need to re-gernerate
            // the frame pointers that carry the colour management data to the
            // playhead and up to the viewport.
            image_ptr_cache_.clear();
            audio_ptr_cache_.clear();
            mail(event_atom_v, change_atom_v).send(base_.event_group());
        },

        [=](json_store::update_atom,
            const JsonStore &,
            const std::string &,
            const JsonStore &) {},

        [=](json_store::update_atom, const JsonStore &) mutable {},

        [=](event_atom, media::add_media_source_atom, const UuidActorVector &) {},

        [=](playlist::get_media_atom, const bool) -> UuidUuidActor {
            return UuidUuidActor(
                base_.uuid(),
                UuidActor(base_.media_uuid(), caf::actor_cast<caf::actor>(media_)));
        },

        // how do we deal with lazy loading of metadata..
        [=](playlist::get_media_atom,
            json_store::get_json_atom,
            const Uuid &uuid,
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
            const std::vector<timebase::flicks> timepoints,
            const FrameRate override_rate) -> result<media::AVFrameIDs> {
            if (media_) {


                // TODO: Optimise and refactor this!! Too slow (often over > 1ms to
                // evaulate)

                // the 'result' here is a vector of std::shared_ptr<const media::AVFrameID> -
                // one element per frame timepoint. We initialise it with blank frames and
                // the logic below will fill in actual frames where possible
                media::AVFrameID blank = *(media::make_blank_frame(
                    media_type, base_.media_uuid(), Uuid(), base_.uuid()));
                blank.set_frame_status(media::FS_NOT_ON_DISK);

                auto blank_ptr = std::make_shared<const media::AVFrameID>(blank);

                // the result data
                auto result = std::make_shared<media::AVFrameIDs>(timepoints.size(), blank_ptr);

                // the response
                auto rp = make_response_promise<media::AVFrameIDs>();

                auto trimmed_start = base_.item().trimmed_start();
                auto available_start =
                    (base_.item().available_start() ? *base_.item().available_start()
                                                    : trimmed_start);

                // get a ref to the appropriate AvFrameID cache
                auto &frame_ptr_cache = media_type == media::MediaType::MT_IMAGE
                                            ? image_ptr_cache_
                                            : audio_ptr_cache_;


                // Note: a clip's frame rate may be different to the frame rate
                // of the media wrapped by the clip. This is how retiming works
                // so that media of different rates can be played in a timeline
                // of fixed rate.
                const auto reference_frame_rate =
                    base_.media_rate() != timebase::k_flicks_zero_seconds ? base_.media_rate()
                                                                          : base_.item().rate();

                RangeMaker range_maker;

                // loop over the timeline timepoints for which we've been asked
                // for an AVFrameID
                for (size_t i = 0; i < timepoints.size(); ++i) {

                    // convert the timepoint into a logical frame
                    auto frd = FrameRateDuration(
                        timepoints[i] - available_start.to_flicks(), override_rate);

                    auto media_logical_frame = frd.frames(reference_frame_rate);

                    // have we cached an AVFrameID for this frame already?
                    if (!frame_ptr_cache.count(media_logical_frame)) {
                        range_maker.extend(media_logical_frame);
                    } else {
                        (*result)[i] = frame_ptr_cache[media_logical_frame];
                    }
                }

                const auto frame_ranges = range_maker.result();

                if (frame_ranges.empty()) {
                    // all our frames were in the cache
                    rp.deliver(*result);
                } else {
                    mail(
                        media::get_media_pointers_atom_v,
                        media_type,
                        frame_ranges,
                        FrameRate(),
                        base_.uuid())
                        .request(caf::actor_cast<caf::actor>(media_), infinite)
                        .then(
                            [=](const media::AVFrameIDs &mps) mutable {
                                /*spdlog::warn("const media::AVFrameIDs &mps {} {}",
                                     name(), mps.size());*/

                                auto &frame_ptr_cache = media_type == media::MediaType::MT_IMAGE
                                                            ? image_ptr_cache_
                                                            : audio_ptr_cache_;

                                auto mp = mps.begin();
                                for (const auto &rng : frame_ranges) {

                                    for (auto ii = rng.first; ii <= rng.second; ii++) {

                                        // this should never happen, but just in case
                                        if (mp == mps.end())
                                            break;

                                        std::shared_ptr<const media::AVFrameID> foo = *mp;
                                        frame_ptr_cache[ii]                         = foo;
                                        mp++;
                                    }
                                }

                                for (size_t i = 0; i < timepoints.size(); ++i) {

                                    // convert the timepoint into a logical frame
                                    auto frd = FrameRateDuration(
                                        timepoints[i] - available_start.to_flicks(),
                                        override_rate);

                                    auto media_logical_frame = frd.frames(reference_frame_rate);

                                    // have we cached an AVFrameID for this frame already?
                                    if (frame_ptr_cache.count(media_logical_frame)) {
                                        (*result)[i] = frame_ptr_cache[media_logical_frame];
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

        [=](duplicate_atom) -> result<UuidActor> {
            JsonStore jsn;
            auto dup    = base_.duplicate();
            jsn["base"] = dup.serialise();

            auto actor = spawn<ClipActor>(jsn);
            UuidActorMap media_map;
            media_map[base_.media_uuid()] = caf::actor_cast<caf::actor>(media_);

            auto rp = make_response_promise<UuidActor>();

            mail(link_media_atom_v, media_map, false)
                .request(actor, infinite)
                .then(
                    [=](const bool) mutable { rp.deliver(UuidActor(dup.uuid(), actor)); },
                    [=](const caf::error &err) mutable {
                        send_exit(actor, caf::exit_reason::user_shutdown);
                        rp.deliver(err);
                    });

            return rp;
        },

        [=](serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        },

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const TimeSourceMode tsm,
            const FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
            // This is required by SubPlayhead actor to make the track
            // playable.
            return base_.item().get_all_frame_IDs(media_type, tsm, override_rate);
        }};
}

void ClipActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());
}
