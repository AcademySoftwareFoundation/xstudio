// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/playhead/edit_list_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::playhead;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

EditListActor::EditListActor(
    caf::actor_config &cfg,
    const std::string &name,
    const std::vector<caf::actor> &ordered_media_sources,
    const media::MediaType mt)
    : caf::event_based_actor(cfg), source_actors_(ordered_media_sources), frames_offset_(0) {

    spdlog::debug("Created EditListActor {}", name);
    print_on_exit(this, "EditListActor");

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    // this bit is blocking ... we are getting the edit list from each source and building
    // a linear edit of these into our 'edit_list_' object. We're also getting the uuids
    caf::scoped_actor sys(system());
    for (auto media_source : ordered_media_sources) {

        // we try and get the edit list for the desired media type ... however, if
        // we can't provide one (say we want MT_IMAGE and the media_source only
        // provides MT_AUDIO sources) we retry and continue. This means that the
        // playhead can 'play' a source that has no video and audio only - the audio
        // only source will provide empty video frames so playback can still happen.
        try {
            auto edl = request_receive<EditList>(
                *sys, media_source, media::get_edit_list_atom_v, mt, Uuid());
            edit_list_.extend(edl);
        } catch (...) {
            try {
                auto edl = request_receive<EditList>(
                    *sys,
                    media_source,
                    media::get_edit_list_atom_v,
                    mt == media::MT_AUDIO ? media::MT_IMAGE : media::MT_AUDIO,
                    Uuid());
                edit_list_.extend(edl);
            } catch (...) {
            }
        }

        try {

            auto uuid =
                request_receive<utility::Uuid>(*sys, media_source, utility::uuid_atom_v);

            source_actors_per_uuid_[uuid] = media_source;
            join_event_group(this, media_source);
        } catch (std::exception &e) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
    behavior_.assign(
        [=](utility::event_atom, utility::change_atom) {
            // send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &uav) {
            send(event_group_, utility::event_atom_v, media::add_media_source_atom_v, uav);
        },

        [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },
        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},
        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookmark_uuid) {
            send(
                event_group_,
                utility::event_atom_v,
                bookmark::bookmark_change_atom_v,
                bookmark_uuid);
        },
        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &,
            const media::MediaType) {
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

        [=](timeline::duration_atom, const timebase::flicks &d) -> bool {
            if (d != timebase::k_flicks_zero_seconds) {
                spdlog::warn(
                    "{} {}", __PRETTY_FUNCTION__, "Attempt to reset duration on EditListActor");
            }
            return false;
        },

        [=](duration_flicks_atom,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<timebase::flicks> {
            return edit_list_.duration_flicks(tsm, override_rate);
        },

        [=](duration_frames_atom,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<int> {
            return (int)edit_list_.duration_frames(tsm, override_rate);
        },

        [=](media::get_edit_list_atom, const Uuid & /*uuid*/) -> utility::EditList {
            return edit_list_;
        },

        [=](media::get_media_pointer_atom atom, const int logical_frame) {
            delegate(caf::actor_cast<actor>(this), atom, media::MT_IMAGE, logical_frame);
        },

        [=](media::get_media_pointer_atom,
            const media::MediaType media_type,
            const int logical_frame) -> result<media::AVFrameID> {
            auto rp = make_response_promise<media::AVFrameID>();

            try {
                int clip_frame;
                EditListSection section =
                    edit_list_.media_frame(logical_frame - frames_offset_, clip_frame);

                if (source_actors_per_uuid_.find(section.media_uuid_) !=
                    source_actors_per_uuid_.end()) {
                    request(
                        source_actors_per_uuid_[section.media_uuid_],
                        infinite,
                        media::get_media_pointer_atom_v,
                        media_type,
                        clip_frame)
                        .then(
                            [=](const media::AVFrameID &mp) mutable { rp.deliver(mp); },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } else {
                    rp.deliver(media::AVFrameID());
                }
            } catch (std::exception &e) {
                rp.deliver(make_error(xstudio_error::error, e.what()));
            }
            return rp;
        },

        [=](media::get_media_pointers_atom,
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<media::FrameTimeMap> {
            auto rp     = make_response_promise<media::FrameTimeMap>();
            auto result = std::make_shared<media::FrameTimeMap>();
            recursive_deliver_all_media_pointers(
                tsm, override_rate, media_type, 0, timebase::flicks(0), result, rp);
            return rp;
        },

        [=](media::source_offset_frames_atom) -> int { return 0; },

        [=](media::source_offset_frames_atom, const int o) -> bool {
            if (o) {

                spdlog::warn(
                    "{} {}",
                    __PRETTY_FUNCTION__,
                    "Attempt to set retime offset on EditListActor which can't be retimed "
                    "directly.");
            }
            return true;
        },

        [=](playhead::flicks_to_logical_frame_atom,
            const timebase::flicks flicks,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<int> {
            // convert between type limits
            if (flicks ==
                timebase::flicks(std::numeric_limits<timebase::flicks::rep>::lowest()))
                return std::numeric_limits<int>::lowest();
            if (flicks == timebase::flicks(std::numeric_limits<timebase::flicks::rep>::max()))
                return std::numeric_limits<int>::max();

            try {
                return edit_list_.logical_frame(tsm, flicks, override_rate);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](playlist::get_media_uuid_atom) -> utility::Uuid {
            if (source_actors_per_uuid_.size() == 1) {
                return source_actors_per_uuid_.begin()->first;
            } else {
                return utility::Uuid();
            }
        },

        [=](playhead::logical_frame_to_flicks_atom,
            const int logical_frame,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<timebase::flicks> {
            // convert between type limits
            if (logical_frame == std::numeric_limits<int>::lowest())
                return timebase::flicks(std::numeric_limits<timebase::flicks::rep>::lowest());
            if (logical_frame == std::numeric_limits<int>::max())
                return timebase::flicks(std::numeric_limits<timebase::flicks::rep>::max());

            try {
                return edit_list_.flicks_from_frame(tsm, logical_frame, override_rate);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](playhead::media_atom, const int logical_frame) -> result<caf::actor> {
            auto rp = make_response_promise<caf::actor>();
            try {
                int clip_frame;
                EditListSection section =
                    edit_list_.media_frame(logical_frame - frames_offset_, clip_frame);
                if (source_actors_per_uuid_.find(section.media_uuid_) !=
                    source_actors_per_uuid_.end()) {
                    rp.deliver(source_actors_per_uuid_[section.media_uuid_]);
                } else {
                    rp.deliver(caf::actor());
                }
            } catch (std::exception &e) {
                if (!strcmp(e.what(), "No frames left") && source_actors_per_uuid_.size()) {
                    rp.deliver(source_actors_per_uuid_.begin()->second);
                } else {
                    rp.deliver(make_error(xstudio_error::error, e.what()));
                }
            }
            return rp;
        },

        [=](playhead::skip_through_sources_atom,
            const int skip_by,
            const int ref_frame) -> result<utility::Uuid> {
            try {

                EditListSection section =
                    edit_list_.skip_sections(ref_frame - frames_offset_, skip_by);

                return section.media_uuid_;

            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](rate_atom, const int logical_frame) -> result<FrameRate> {
            try {
                return edit_list_.frame_rate_at_frame(logical_frame);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &changes, bool) {
            // ignoring timeline events
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

caf::actor EditListActor::media_source_actor(const utility::Uuid &source_uuid) {
    auto p = source_actors_per_uuid_.find(source_uuid);
    if (p != source_actors_per_uuid_.end()) {
        return p->second;
    }
    return caf::actor();
}

void EditListActor::recursive_deliver_all_media_pointers(
    const utility::TimeSourceMode tsm,
    const utility::FrameRate &override_rate,
    const media::MediaType media_type,
    const int clip_index,
    const timebase::flicks clip_start_time_point,
    std::shared_ptr<media::FrameTimeMap> result,
    caf::typed_response_promise<media::FrameTimeMap> rp) {

    // this function is crucial. It works by recursively self calling until the
    // 'clip_index' is greater than the number of clips (incrementind clip_index)
    // in this edit list.
    // We use it to get a full list of media pointers for this edit list from
    // start to finish. A 'AVFrameID' is a struct that contains all the
    // information we need to read/retrieve a frame of video/audio data. Along
    // with the media pointers we also get the associated timepoint for each
    // media pointer, that is where it lies on the timeline (or when they should
    // be displayed if we start playback from time=0)

    if (clip_index >= (int)edit_list_.size()) {

        // we're done, we've exhausted the number of clips. Now we need something
        // funky ... we meed to add a timepoint at the end so we know when the
        // last frame's duration runs out. To understand this, imagine we have
        // a source with a single frame. It shows at time=0 but it should be
        // on screen for 1/fps seconds ... so we add a timepoint with a null
        // media pointer at this time. Note that we increment time_point for
        // every frame that's been added to result, so it time_point is already
        // where we need it
        (*result)[clip_start_time_point].reset();

        rp.deliver(*result);
        return;
    }

    // the TP of the first frame in the current clip
    timebase::flicks time_point = clip_start_time_point;

    // get a copy of the current clip
    const auto clip = edit_list_.section_list()[clip_index];

    const utility::Timecode tc = clip.timecode_;

    // number of logical frames in the clip
    const int num_clip_frames = clip.frame_rate_and_duration_.frames(
        tsm == TimeSourceMode::FIXED ? override_rate : FrameRate());

    // get the media actor (or other source actor type) for the clip
    const utility::Uuid &uuid = clip.media_uuid_;
    caf::actor media_source   = media_source_actor(uuid);

    if (media_source) {

        // now we  get media pointers for the number of frames in the clip
        // (bear in mind, the media source might have more or fewer frames
        // than our 'clip'- the clip is a time slice of the media source's
        // total possible duration

        request(
            media_source,
            infinite,
            media::get_media_pointers_atom_v,
            media_type,
            media::LogicalFrameRanges({{0, num_clip_frames - 1}}),
            override_rate)
            .await(
                [=](const media::AVFrameIDs &mps) mutable {
                    if ((int)mps.size() != num_clip_frames) {
                        rp.deliver(make_error(
                            xstudio_error::error,
                            "EditListActor::recursive_deliver_all_media_pointers media "
                            "pointers returned by media source not matching requested number "
                            "of frames."));
                        return;
                    }

                    for (int f = 0; f < num_clip_frames; f++) {
                        const int playhead_logical_frame = result->size();
                        (*result)[time_point]            = mps[f];
                        const_cast<media::AVFrameID *>((*result)[time_point].get())
                            ->playhead_logical_frame_ = playhead_logical_frame;
                        const_cast<media::AVFrameID *>(mps[f].get())->timecode_ = tc + f;
                        time_point += tsm == TimeSourceMode::FIXED
                                          ? override_rate
                                          : clip.frame_rate_and_duration_.rate();
                    }

                    recursive_deliver_all_media_pointers(
                        tsm, override_rate, media_type, clip_index + 1, time_point, result, rp);
                },
                [=](error &err) mutable {
                    // something is wrong with the media source ... let's print it's
                    // name and the error and continue

                    auto blank_frame = media::make_blank_frame(media_type);
                    auto *m_ptr      = const_cast<media::AVFrameID *>(blank_frame.get());
                    m_ptr->error_    = to_string(err);

                    for (int f = 0; f < num_clip_frames; f++) {
                        (*result)[time_point] = blank_frame;
                        time_point += tsm == TimeSourceMode::FIXED
                                          ? override_rate
                                          : clip.frame_rate_and_duration_.rate();
                    }

                    recursive_deliver_all_media_pointers(
                        tsm, override_rate, media_type, clip_index + 1, time_point, result, rp);
                });
    } else {

        for (int f = 0; f < num_clip_frames; f++) {
            (*result)[time_point] = media::make_blank_frame(media_type);
            time_point += tsm == TimeSourceMode::FIXED ? override_rate
                                                       : clip.frame_rate_and_duration_.rate();
        }

        // shouldn't we deliver on the promise...
        recursive_deliver_all_media_pointers(
            tsm, override_rate, media_type, clip_index + 1, time_point, result, rp);
    }
}
