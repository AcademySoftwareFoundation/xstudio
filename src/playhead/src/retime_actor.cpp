// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/playhead/retime_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::playhead;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

RetimeActor::RetimeActor(
    caf::actor_config &cfg,
    const std::string &name,
    caf::actor &source,
    const media::MediaType mt)
    : caf::event_based_actor(cfg), source_(source), frames_offset_(0) {

    spdlog::debug("Created RetimeActor {}", name);
    print_on_exit(this, "RetimeActor");

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    // N.B. this is blocking
    join_event_group(this, source);

    get_source_edit_list(mt);

    behavior_.assign(
        [=](utility::event_atom, utility::change_atom) {
            // my sources have changed..
            caf::scoped_actor sys(system());

            get_source_edit_list(mt);

            // if duration was previously set externally, we want to retain that
            set_duration(forced_duration_);
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },
        [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &uav) {
            send(event_group_, utility::event_atom_v, media::add_media_source_atom_v, uav);
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

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},
        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &,
            const media::MediaType) {
            caf::scoped_actor sys(system());

            get_source_edit_list(mt);
            // if duration was previously set externally, we want to retain that
            set_duration(forced_duration_);
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
        },

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

        [=](utility::uuid_atom) { delegate(source_, utility::uuid_atom_v); },

        [=](timeline::duration_atom, const timebase::flicks &new_duration) -> bool {
            set_duration(new_duration);
            return true;
        },

        [=](duration_flicks_atom,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<timebase::flicks> {
            return retime_edit_list_.duration_flicks(tsm, override_rate);
        },

        [=](duration_frames_atom,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> result<int> {
            // spdlog::warn("retime duration_frames_atom {}",
            // (int)retime_edit_list_.duration_frames()); spdlog::warn("retime
            // duration_frames_atom {}", (int)retime_edit_list_.duration_frames(tsm,
            // override_rate));
            return (int)retime_edit_list_.duration_frames(tsm, override_rate);
        },

        [=](playhead::overflow_mode_atom) -> int { return static_cast<int>(overflow_mode_); },

        [=](media::get_edit_list_atom, const Uuid & /*override_uuid*/) -> utility::EditList {
            // spdlog::warn("get_edit_list_atom {}", __PRETTY_FUNCTION__);
            // we don't use the override uuid, instead the uuid in the edit list correspond to
            // the source
            return retime_edit_list_;
        },

        [=](media::get_media_pointer_atom atom, const int logical_frame) {
            delegate(caf::actor_cast<actor>(this), atom, media::MT_IMAGE, logical_frame);
        },

        [=](media::get_media_pointer_atom,
            const media::MediaType media_type,
            const int logical_frame) -> result<media::AVFrameID> {
            auto rp = make_response_promise<media::AVFrameID>();

            RetimeFrameResult retime_result = IN_RANGE;
            int retimed_logical_frame       = apply_retime(logical_frame, retime_result);

            if (retime_result == FAIL) {
                rp.deliver(make_error(xstudio_error::error, "No frames left"));
            } else if (retime_result == OUT_OF_RANGE) {
                rp.deliver(*(media::make_blank_frame(media_type)));
            } else {

                request(
                    source_,
                    infinite,
                    media::get_media_pointer_atom_v,
                    media_type,
                    retimed_logical_frame)
                    .then(
                        [=](media::AVFrameID &mp) mutable {
                            if (retime_result == HELD_FRAME) {
                                mp.params_["HELD_FRAME"] = true;
                            }
                            rp.deliver(mp);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
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

        [=](media::source_offset_frames_atom) -> int { return frames_offset_; },

        [=](media::source_offset_frames_atom, const int offset) -> bool {
            frames_offset_ = offset;
            send(
                event_group_,
                utility::event_atom_v,
                media::source_offset_frames_atom_v,
                frames_offset_);
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
            return true;
        },

        [=](playhead::media_atom, const int logical_frame) -> result<caf::actor> {
            auto rp = make_response_promise<caf::actor>();

            RetimeFrameResult retime_result = IN_RANGE;
            apply_retime(logical_frame, retime_result);

            if (retime_result == FAIL) {
                rp.deliver(make_error(xstudio_error::error, "No frames left"));
            } else if (retime_result == OUT_OF_RANGE) {
                rp.deliver(make_error(xstudio_error::error, "Out of range"));
            } else {
                rp.deliver(source_);
            }
            return rp;
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
                return retime_edit_list_.logical_frame(tsm, flicks, override_rate);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](playlist::get_media_uuid_atom) -> result<utility::Uuid> {
            auto rp = make_response_promise<utility::Uuid>();
            request(source_, infinite, utility::uuid_atom_v)
                .then(
                    [=](const utility::Uuid &uuid) mutable { rp.deliver(uuid); },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
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
                return retime_edit_list_.flicks_from_frame(tsm, logical_frame, override_rate);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](rate_atom, const int logical_frame) -> result<FrameRate> {
            try {
                return retime_edit_list_.frame_rate_at_frame(logical_frame - frames_offset_);
            } catch (std::exception &e) {
                return make_error(xstudio_error::error, e.what());
            }
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &changes, bool) {
            // ignoring timeline events
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}

void RetimeActor::set_duration(const timebase::flicks &new_duration) {

    forced_duration_ = new_duration;
    if (forced_duration_.count()) {
        retime_edit_list_.clear();
        auto clip = source_edit_list_.section_list()[0];
        clip.frame_rate_and_duration_.set_duration(new_duration);
        retime_edit_list_.append(clip);
    } else {
        retime_edit_list_ = source_edit_list_;
    }
    send(event_group_, utility::event_atom_v, utility::change_atom_v);
}

int RetimeActor::apply_retime(const int logical_frame, RetimeFrameResult &retime_result) {
    int retimed_frame         = logical_frame - frames_offset_;
    const int duration_frames = source_edit_list_.duration_frames(TimeSourceMode::DYNAMIC);
    if (retimed_frame < 0) {
        if (overflow_mode_ == OM_FAIL) {
            retime_result = FAIL;
        } else if (overflow_mode_ == OM_NULL) {
            retime_result = OUT_OF_RANGE;
        } else if (overflow_mode_ == OM_HOLD) {
            retimed_frame = 0;
            retime_result = HELD_FRAME;
        } else if (overflow_mode_ == OM_LOOP) {
            while (retimed_frame < 0) {
                retimed_frame += duration_frames;
            }
            retime_result = LOOPED_RANGE;
        }
    } else if (retimed_frame >= duration_frames) {
        if (overflow_mode_ == OM_FAIL) {
            retime_result = FAIL;
        } else if (overflow_mode_ == OM_NULL) {
            retime_result = OUT_OF_RANGE;
        } else if (overflow_mode_ == OM_HOLD) {
            retimed_frame = duration_frames - 1;
            retime_result = HELD_FRAME;
        } else if (overflow_mode_ == OM_LOOP) {
            retimed_frame %= duration_frames;
            retime_result = LOOPED_RANGE;
        }
    } else {
        retime_result = IN_RANGE;
    }
    return retimed_frame;
}

void RetimeActor::recursive_deliver_all_media_pointers(
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

    if (clip_index >= (int)source_edit_list_.size()) {
        // we're done, we've exhausted the number of clips. Deliver the result
        // and return
        // Now we need something funky ...
        // we meed to add a timepoint at the end so we know when the
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

    // get a copy of the source clip
    const auto clip = source_edit_list_.section_list()[clip_index];

    const utility::Timecode tc = clip.timecode_;

    // number of logical frames in the source clip
    const int num_clip_frames = clip.frame_rate_and_duration_.frames(
        tsm == TimeSourceMode::FIXED ? override_rate : FrameRate());

    // get the retimed duration
    int retimed_duration = (int)retime_edit_list_.duration_frames(tsm, override_rate);

    // now we get media all pointers for the source clip ... we then apply
    // the retime to those pointers

    request(
        source_,
        infinite,
        media::get_media_pointers_atom_v,
        media_type,
        // media::LogicalFrameRanges({{0, std::max(0,num_clip_frames-1)}}),
        media::LogicalFrameRanges({{0, num_clip_frames - 1}}),
        override_rate)
        .await(
            [=](const media::AVFrameIDs &mps) mutable {
                if ((int)mps.size() != num_clip_frames) {
                    rp.deliver(make_error(
                        xstudio_error::error,
                        "RetimeActor::recursive_deliver_all_media_pointers media pointers "
                        "returned by media source not matching requested number of frames."));
                    return;
                }

                for (int f = 0; f < retimed_duration; f++) {

                    RetimeFrameResult r;
                    int retime_frame                 = apply_retime(f, r);
                    const int playhead_logical_frame = result->size();

                    if (r == FAIL) {
                        rp.deliver(make_error(xstudio_error::error, "No frames left"));
                        return;
                    } else if (retime_frame >= 0 && retime_frame < (int)mps.size()) {
                        if (r == IN_RANGE) {
                            // re-use the pointer we were given
                            (*result)[time_point] = mps[retime_frame];
                        } else {
                            // dupliacte frame, need to duplicate the data
                            media::AVFrameID mptr(*mps[retime_frame]);
                            if (r == HELD_FRAME) {
                                mptr.params_["HELD_FRAME"] = true;
                            }
                            (*result)[time_point] =
                                std::make_shared<const media::AVFrameID>(mptr);
                        }
                        const_cast<media::AVFrameID *>((*result)[time_point].get())->timecode_ =
                            tc + f - frames_offset_;

                    } else { // OUT_OF_RANGE
                        (*result)[time_point] = media::make_blank_frame(media_type);
                    }

                    // set the logical frame
                    const_cast<media::AVFrameID *>((*result)[time_point].get())
                        ->playhead_logical_frame_ = playhead_logical_frame;

                    time_point += tsm == TimeSourceMode::FIXED
                                      ? override_rate
                                      : clip.frame_rate_and_duration_.rate();
                }

                recursive_deliver_all_media_pointers(
                    tsm, override_rate, media_type, clip_index + 1, time_point, result, rp);
            },
            [=](error &err) mutable {
                // Something is wrong with the media source (e.g. file is not
                // on the file system). Make blank frames instead and add the
                // error message

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
}

void RetimeActor::get_source_edit_list(const media::MediaType mt) {

    caf::scoped_actor sys(system());
    try {

        // we try and get the edit list for the desired media type ... however, if
        // we can't provide one (say we want MT_IMAGE and the media_source only
        // provides MT_AUDIO sources) we retry and continue. This means that the
        // playhead can 'play' a source that has no video and audio only - the audio
        // only source will provide empty video frames so playback can still happen.

        source_edit_list_ =
            request_receive<EditList>(*sys, source_, media::get_edit_list_atom_v, mt, Uuid());
        retime_edit_list_ = source_edit_list_;

    } catch (std::exception & /*e*/) {

        try {
            source_edit_list_ = request_receive<EditList>(
                *sys,
                source_,
                media::get_edit_list_atom_v,
                mt == media::MT_AUDIO ? media::MT_IMAGE : media::MT_AUDIO,
                Uuid());
            retime_edit_list_ = source_edit_list_;
        } catch (std::exception & /*e*/) {
        }

        // suppressing 'no streams' warning for dud media
        // spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}