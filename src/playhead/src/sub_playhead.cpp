// SPDX-License-Identifier: Apache-2.0
#include <chrono>

#include <caf/policy/select_all.hpp>
#include <caf/unit.hpp>
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace xstudio::colour_pipeline;
using namespace caf;
using namespace std::chrono_literals;

template <typename T>
void copy_audio_samples(
    AudioBufPtr &buffer_to_fill,
    const timebase::flicks buffer_to_fill_timestamp,
    const AudioBufPtr &source_buffer,
    const timebase::flicks source_buffer_timestamp);

SubPlayhead::SubPlayhead(
    caf::actor_config &cfg,
    const std::string &name,
    utility::UuidActor source,
    caf::actor parent,
    const int index,
    const bool source_is_timeline,
    const timebase::flicks loop_in_point,
    const timebase::flicks loop_out_point,
    const utility::TimeSourceMode time_source_mode,
    const utility::FrameRate override_frame_rate,
    const media::MediaType media_type,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg),
      name_(name),
      source_(std::move(source)),
      parent_(std::move(parent)),
      sub_playhead_index_(index),
      source_is_timeline_(source_is_timeline),
      loop_in_point_(loop_in_point),
      loop_out_point_(loop_out_point),
      time_source_mode_(time_source_mode),
      override_frame_rate_(override_frame_rate),
      media_type_(media_type),
      uuid_(uuid) {

    init();
}

SubPlayhead::~SubPlayhead() {}

void SubPlayhead::on_exit() {
    parent_ = caf::actor();
    source_ = utility::UuidActor();
}

void SubPlayhead::init() {

    // if (parent_)
    //    link_to(parent_);

    // get global reader and steal mrm..
    spdlog::debug("Created SubPlayhead {}", name_);
    // print_on_exit(this, "SubPlayhead");

    auto global_prefs_actor = caf::actor();

    try {

        auto prefs = GlobalStoreHelper(system());
        JsonStore j;

        global_prefs_actor = prefs.get_jsonactor();
        join_broadcast(this, prefs.get_group(j));
        pre_cache_read_ahead_frames_ = preference_value<size_t>(j, "/core/playhead/read_ahead");
        static_cache_delay_milliseconds_ = std::chrono::milliseconds(
            preference_value<size_t>(j, "/core/playhead/static_cache_delay_milliseconds"));

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    pre_reader_ = system().registry().template get<caf::actor>(media_reader_registry);
    // link_to(pre_reader_);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    // subscribe to source..

    if (source_) {

        monitor(source_.actor(), [this, addr = source_.actor().address()](const error &err) {
            if (addr == source_.actor()) {
                spdlog::debug("ChildPlayhead source down: {}", to_string(err));
                source_ = utility::UuidActor();
            }
        });

        utility::join_event_group(this, source_);
        // we need a snwsible default rate to pad out frames if we have to extend
        // our duration
        mail(utility::rate_atom_v, media_type_)
            .request(source_.actor(), infinite)
            .then(
                [=](const utility::FrameRate &r) mutable { default_rate_ = r; },
                [=](const caf::error &err) mutable {});
    }

    // this triggers us to fetch the frames information from the source
    anon_mail(source_atom_v).send(this);

    behavior_.assign(

        // [=](caf::message &msg) -> caf::skippable_result {
        //     //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

        //     spdlog::warn(
        //         "Got unwanted messate from {} {}", to_string(current_sender()),
        //        to_string(msg));

        //     return message{};
        // },

        [=](caf::exit_msg &msg) {
            anon_mail(clear_precache_queue_atom_v, uuid_).send(pre_reader_);
            quit(msg.reason);
        },

        [=](utility::event_atom, utility::notification_atom, const utility::JsonStore &) {},

        [=](utility::uuid_atom) -> utility::Uuid { return uuid_; },

        [=](get_event_group_atom) -> caf::actor { return event_group_; },

        [=](actual_playback_rate_atom) -> result<utility::FrameRate> {
            auto rp = make_response_promise<utility::FrameRate>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        if (num_retimed_frames_ < 2) {
                            rp.deliver(make_error(xstudio_error::error, "No Frames"));
                            return;
                        }

                        // to get the instantaneous frame rate, we use the delta
                        // between the current frame and the next frame
                        auto frame = retimed_frames_.lower_bound(position_flicks_);
                        if (frame == retimed_frames_.end())
                            frame--;
                        auto next_frame = frame;
                        next_frame++;
                        if (next_frame != retimed_frames_.end()) {
                            rp.deliver(utility::FrameRate(next_frame->first - frame->first));
                        } else if (frame != retimed_frames_.begin()) {
                            auto prev_frame = frame;
                            prev_frame--;
                            rp.deliver(utility::FrameRate(frame->first - prev_frame->first));
                        } else {
                            rp.deliver(frame->second->rate());
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](clear_precache_queue_atom) {
            return mail(clear_precache_queue_atom_v, uuid_).delegate(pre_reader_);
        },

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        // [=](source_atom, utility::time_point update_tp) -> result<caf::actor> {
        //     // the message will be sent with a delay - the time it was sent
        //     // is update_tp, which, if it matches last_change_timepoint_, then
        //     // no other updates have been requested since so we continue with
        //     // the develop
        //     auto rp = make_response_promise<caf::actor>();
        //     if (last_change_timepoint_ == update_tp) {
        //         get_full_timeline_frame_list(rp);
        //     } else {
        //         // a new update request has been issued since the message we're
        //         // processing now was sent so we can skip.
        //         rp.deliver(caf::actor());
        //     }
        //     return rp;
        // },

        [=](source_atom) -> result<caf::actor> {
            auto rp = make_response_promise<caf::actor>();
            get_full_timeline_frame_list(rp);
            return rp;
        },

        // [=](source_atom, bool /*retry*/) -> result<caf::actor> {
        //     auto rp = make_response_promise<caf::actor>();
        //     get_full_timeline_frame_list(rp);
        //     return rp;
        // },

        [=](utility::event_atom, playlist::add_media_atom, const utility::UuidActorVector &) {},

        [=](utility::event_atom, playlist::remove_media_atom, const utility::UuidVector &) {},

        [=](utility::event_atom,
            playlist::move_media_atom,
            const utility::UuidVector &,
            const utility::Uuid &) {},

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &a,
            const media::MediaType) {
            up_to_date_ = false;
            anon_mail(source_atom_v).send(this); // triggers refresh of frames_time_list_
        },

        [=](timeline::duration_atom, const timebase::flicks &new_duration) -> result<bool> {
            forced_duration_ = new_duration;
            update_retiming();
            anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
            return true;
        },

        [=](duration_flicks_atom atom,
            bool /*before retiming, extension*/) -> result<timebase::flicks> {
            if (up_to_date_) {
                if (num_source_frames_ < 2) {
                    return timebase::flicks(0);
                }
                return full_timeline_frames_.rbegin()->first;
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<timebase::flicks>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        if (num_source_frames_ < 2) {
                            rp.deliver(timebase::flicks(0));
                        } else {
                            rp.deliver(full_timeline_frames_.rbegin()->first);
                        }
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(timebase::flicks(0));
                    });
            return rp;
        },

        [=](duration_flicks_atom atom) -> result<timebase::flicks> {
            if (up_to_date_) {
                if (num_retimed_frames_ < 2) {
                    return timebase::flicks(0);
                }
                return retimed_frames_.rbegin()->first;
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<timebase::flicks>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        if (num_retimed_frames_ < 2) {
                            rp.deliver(timebase::flicks(0));
                        } else {
                            rp.deliver(retimed_frames_.rbegin()->first);
                        }
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(timebase::flicks(0));
                    });
            return rp;
        },

        [=](duration_frames_atom atom) -> result<size_t> {
            if (up_to_date_) {
                return size_t(num_retimed_frames_ ? num_retimed_frames_ - 1 : 0);
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<size_t>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        rp.deliver(size_t(num_retimed_frames_ ? num_retimed_frames_ - 1 : 0));
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](flicks_to_logical_frame_atom atom, timebase::flicks flicks) -> int {
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(flicks, frame_period, timeline_pts);
            return logical_frame_from_pts(timeline_pts);
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) mutable {
            if (current_sender() == global_prefs_actor) {
                try {
                    pre_cache_read_ahead_frames_ =
                        preference_value<size_t>(full, "/core/playhead/read_ahead");
                    static_cache_delay_milliseconds_ =
                        std::chrono::milliseconds(preference_value<size_t>(
                            full, "/core/playhead/static_cache_delay_milliseconds"));
                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            }
        },

        [=](json_store::update_atom, const JsonStore &full) {
            if (current_sender() == global_prefs_actor) {
                try {
                    pre_cache_read_ahead_frames_ =
                        preference_value<size_t>(full, "/core/playhead/read_ahead");
                    static_cache_delay_milliseconds_ =
                        std::chrono::milliseconds(preference_value<size_t>(
                            full, "/core/playhead/static_cache_delay_milliseconds"));
                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            }
        },

        [=](jump_atom,
            const timebase::flicks flicks,
            const bool forwards,
            const float velocity,
            const bool playing,
            const bool force_updates,
            const bool active_in_ui,
            const bool scrubbing) -> unit_t {
            // if the playhead has jumped, we set read_ahead_frames_ to zero so
            // that we make a fresh request for the readahead frames that we
            // need
            set_position(
                flicks, forwards, playing, velocity, force_updates, active_in_ui, scrubbing);
            return unit;
        },

        [=](logical_frame_atom) -> int { return logical_frame_; },

        [=](logical_frame_to_flicks_atom, int64_t logical_frame) {
            return mail(logical_frame_to_flicks_atom_v, int(logical_frame))
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](logical_frame_to_flicks_atom, int logical_frame) {
            return mail(logical_frame_to_flicks_atom_v, logical_frame, false)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](logical_frame_to_flicks_atom,
            int logical_frame,
            const bool clamp_to_range) -> result<timebase::flicks> {
            if (num_retimed_frames_ < 2) {
                return make_error(xstudio_error::error, "No Frames");
            }

            // to get to the last frame, due to the last 'dummy' frame that is appended
            // to account for the duration of the last frame, step back twice from end
            auto last_frame = retimed_frames_.end();
            last_frame--;
            last_frame--;

            // woopsie - this loop is not an efficient way to work out if
            // we will hit the end frame!!
            auto frame = retimed_frames_.begin();
            while (logical_frame > 0 && frame != last_frame) {
                frame++;
                logical_frame--;
            }

            auto tp = frame->first;
            if (logical_frame && !clamp_to_range) {
                // if logical_frame goes beyond our last frame then use the
                // duration of the final last frame to extend the result
                auto dummy_last = retimed_frames_.end();
                dummy_last--;
                auto last_frame_duration = dummy_last->first - last_frame->first;
                tp += logical_frame * last_frame_duration;
            }

            return tp;
        },

        [=](media_frame_to_flicks_atom,
            const utility::Uuid &media_source_uuid,
            int logical_media_frame) -> result<timebase::flicks> {
            if (logical_media_frame < 0)
                return make_error(xstudio_error::error, "Out of range");
            // loop over frames until we hit the media item
            auto frame = retimed_frames_.begin();
            while (frame != retimed_frames_.end()) {
                if (frame->second && frame->second->source_uuid() == media_source_uuid) {
                    break;
                }
                frame++;
            }
            if (frame == retimed_frames_.end()) {
                return make_error(xstudio_error::error, "Media Not Found");
            }

            // get the time of the first frame for the media we are interested
            // in, in case we don't match with logical_media_frame
            timebase::flicks result = frame->first;

            // now loop over frames for the media item until we match to the logical_media_frame
            while (frame != retimed_frames_.end()) {
                if (frame->second && frame->second->source_uuid() != media_source_uuid) {
                    return make_error(xstudio_error::error, "Out of range");
                } else if (frame->second && frame->second->frame() >= logical_media_frame) {
                    // note the >= .... if logical_media_frame is *less* than
                    // the frame's media frame, we return the timestamp for
                    // 'frame'. The reason is that if we've been asked to get
                    // the time stamp for frame zero, but we have frame nubers
                    // that start at 1001, say, we just fall back to returning
                    // the first frame
                    return frame->first;
                }
                frame++;
            }
            return result;
        },

        [=](media::source_offset_frames_atom atom) -> result<int64_t> { return frame_offset_; },

        [=](media::source_offset_frames_atom atom,
            const int64_t offset,
            const bool reset_duration) -> result<bool> {
            if (offset != frame_offset_) {
                if (reset_duration)
                    forced_duration_ = timebase::k_flicks_zero_seconds;
                frame_offset_ = offset;
                update_retiming();
                anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
                return true;
            } else if (reset_duration && forced_duration_ != timebase::k_flicks_zero_seconds) {
                forced_duration_ = timebase::k_flicks_zero_seconds;
                update_retiming();
                anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
                return true;
            }
            return false;
        },

        [=](first_frame_media_pointer_atom) -> result<media::AVFrameID> {
            if (up_to_date_) {
                if (!full_timeline_frames_.empty()) {
                    if (!full_timeline_frames_.begin()->second) {
                        return make_error(xstudio_error::error, "Empty frame");
                    }
                    return *(full_timeline_frames_.begin()->second);
                } else {
                    return make_error(xstudio_error::error, "No frames");
                }
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<media::AVFrameID>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        if (!full_timeline_frames_.empty()) {
                            if (!full_timeline_frames_.begin()->second) {
                                rp.deliver(make_error(xstudio_error::error, "Empty frame"));
                            } else {
                                rp.deliver(*(full_timeline_frames_.begin()->second));
                            }
                        } else {
                            rp.deliver(make_error(xstudio_error::error, "No frames"));
                        }
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](last_frame_media_pointer_atom) -> result<media::AVFrameID> {
            if (num_retimed_frames_ > 1) {
                // remember the last entry in retimed_frames_ is
                // a dummy frame marking the end of the last frame
                auto p = retimed_frames_.rbegin();
                p++;
                if (!p->second) {
                    return make_error(xstudio_error::error, "Empty frame");
                }
                return *(p->second);
            }
            return make_error(xstudio_error::error, "No Frames");
        },

        [=](media::get_media_pointer_atom) -> result<media::AVFrameID> {
            if (up_to_date_) {
                auto frame = retimed_frames_.lower_bound(position_flicks_);
                if (num_retimed_frames_ && frame != retimed_frames_.end()) {
                    if (frame->second) {
                        return *(frame->second);
                    } else {
                        return make_error(xstudio_error::error, "No Frame");
                    }
                } else {
                    return make_error(xstudio_error::error, "No Frame");
                }
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<media::AVFrameID>();
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        auto frame = retimed_frames_.lower_bound(position_flicks_);
                        if (num_retimed_frames_ && frame != retimed_frames_.end()) {
                            rp.deliver(*(frame->second));
                        } else {
                            rp.deliver(make_error(xstudio_error::error, "No Frame"));
                        }
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_source_atom) -> result<caf::actor> {
            // MediaSourceActor at current playhead position

            auto rp = make_response_promise<caf::actor>();
            // we have to have run the 'source_atom' handler first (to have
            // built retimed_frames_) before we can fetch the media on
            // the current frame
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable {
                        auto frame = retimed_frames_.lower_bound(position_flicks_);
                        caf::actor result;
                        if (frame != retimed_frames_.end() && frame->second) {
                            result =
                                caf::actor_cast<caf::actor>(frame->second->media_source_addr());
                        }
                        rp.deliver(result);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_source_atom,
            utility::Uuid source_uuid,
            const media::MediaType mt) -> result<std::string> {
            // switch the media source and return the name of the new source

            auto rp = make_response_promise<std::string>();
            // get the media actor on the current frame
            mail(media_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor media_actor) mutable {
                        // no media ?
                        if (!media_actor) {
                            rp.deliver("");
                            return;
                        }
                        // switch the source
                        mail(media::current_media_source_atom_v, source_uuid, mt)
                            .request(media_actor, infinite)
                            .then(
                                [=](const bool) mutable {
                                    // now get the name of the new source
                                    mail(media::current_media_source_atom_v, mt)
                                        .request(media_actor, infinite)
                                        .then(
                                            [=](UuidActor media_source) mutable {
                                                rp.delegate(
                                                    media_source.actor(), utility::name_atom_v);
                                            },
                                            [=](const error &err) mutable { rp.deliver(err); });
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_source_atom,
            std::string source_name,
            const media::MediaType mt) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // get the media actor on the current frame
            mail(media_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor media_actor) mutable {
                        // no media ?
                        if (!media_actor) {
                            rp.deliver(false);
                            return;
                        }

                        // now get it to switched to the named MediaSource
                        mail(media_source_atom_v, source_name, mt)
                            .request(media_actor, infinite)
                            .then(

                                [=](bool) mutable {
                                    up_to_date_ = false;
                                    // now ensure we have rebuilt ourselves to reflect the new
                                    // source i.e. we have checked out the new frames_time_list_
                                    mail(source_atom_v)
                                        .request(caf::actor_cast<caf::actor>(this), infinite)
                                        .then(
                                            [=](caf::actor) mutable { rp.deliver(true); },
                                            [=](const error &err) mutable { rp.deliver(err); });
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_atom, const bool) {
            // this is a 'kick' message telling us to re-broadcast current media
            // back to the parent playhead
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(position_flicks_, frame_period, timeline_pts);
            if (frame)
                current_source_ = utility::Uuid();
            check_if_media_changed(frame ? frame.get() : nullptr);
        },

        [=](media_atom) -> result<caf::actor> {
            // MediaActor at current playhead position
            auto frame = retimed_frames_.upper_bound(position_flicks_);
            if (frame != retimed_frames_.begin())
                frame--;
            utility::UuidActor result;
            if (frame != retimed_frames_.end() && frame->second) {
                auto m = caf::actor_cast<caf::actor>(frame->second->media_addr());
                result = utility::UuidActor(frame->second->media_uuid(), m);
            }
            return result;
        },

        [=](media_source_atom, bool) -> utility::UuidActor {
            auto frame = retimed_frames_.upper_bound(position_flicks_);
            if (frame != retimed_frames_.begin())
                frame--;
            utility::UuidActor result;
            if (frame != retimed_frames_.end() && frame->second) {
                auto m = caf::actor_cast<caf::actor>(frame->second->media_source_addr());
                result = utility::UuidActor(frame->second->source_uuid(), m);
            }
            return result;
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &uav) {
            mail(utility::event_atom_v, media::add_media_source_atom_v, uav).send(parent_);
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &changes, bool) {
            // when a timeline autoconform is happening it triggers a flood of
            // these item_atom events as the timeline is rebuilt. We don't want
            // to keep requesting frames from the timeline while its still busy
            // doing the autoconform. By delaying the update request, and
            // checking if another update has come in since this request was
            // made, we can skip excessive updates
            up_to_date_            = false;
            last_change_timepoint_ = utility::clock::now();
            anon_mail(source_atom_v).send(this);
        },

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus) {
            // this can come from a MediActor that is our source_
        },

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &,
            caf::actor_addr &) {},

        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &) {},

        [=](media_cache::keys_atom) -> media::AVFrameIDs {
            media::AVFrameIDs result;
            result.reserve(num_retimed_frames_);
            for (const auto &pp : retimed_frames_) {
                result.push_back(pp.second);
            }
            return result;
        },

        [=](bookmark::get_bookmarks_atom) {
            mail(bookmark::get_bookmarks_atom_v, true)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) {
                        mail(
                            utility::event_atom_v,
                            bookmark::get_bookmarks_atom_v,
                            bookmark_ranges_)
                            .send(parent_);
                    },
                    [=](const error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](bookmark::get_bookmarks_atom, bool wait) -> result<bool> {
            auto rp = make_response_promise<bool>();
            full_bookmarks_update(rp);
            return rp;
        },

        [=](audio_buffer_atom,
            const timebase::flicks buffer_pts,
            AudioBufPtr buffer_to_fill) -> result<AudioBufPtr> {
            // here a request is made for an audio buffer corresponding to a precise
            // timepoint in the playhead timeline and with number of samples corresponding
            // to duration. See VideoRenderPlugin, which uses this for an accurate
            // contiguous audio stream

            // The awkard thing here is that the audio buffers that the reader delivers
            // are 'lumpy' (due to the way that ffmpeg decodes audio frames in a way
            // that doesn't align with xSTUDIO frames) and not likely to align with
            // what 'buffer_to_fill' wants. As such, we ask the reader to give us
            // 3 audio buffers, the one that is closest to buffer_pts and one either
            // side. 'copy_audio_sample' then takes care of copying the samples from
            // each of those 3 buffers from the reader into buffer_to_fill

            auto rp     = make_response_promise<AudioBufPtr>();
            auto rcount = std::make_shared<int>(0);

            auto check_and_deliver = [=]() mutable {
                (*rcount)++;
                if (*rcount == 3)
                    rp.deliver(buffer_to_fill);
            };

            for (int i = -1; i <= 1; ++i) {

                timebase::flicks frame_period, frame_pts;
                std::shared_ptr<const media::AVFrameID> frame =
                    get_frame(buffer_pts, frame_period, frame_pts, i);

                if (!frame) {

                    check_and_deliver();
                    continue;
                }

                mail(media_reader::get_audio_atom_v, *(frame.get()), false, uuid_)
                    .request(pre_reader_, std::chrono::seconds(20))
                    .then(

                        [=](const AudioBufPtr &audio_buffer) mutable {
                            if (audio_buffer) {
                                copy_audio_samples<int16_t>(
                                    buffer_to_fill, buffer_pts, audio_buffer, frame_pts);
                            }

                            check_and_deliver();
                        },
                        [=](const error &err) mutable {
                            spdlog::critical(
                                "SubPlayhead buffer read timeout for frame {}",
                                to_string(frame->key()));
                            check_and_deliver();
                        });
            }

            return rp;
        },

        [=](audio_buffer_atom) -> result<AudioBufPtr> {
            // audio buf retrieval for the current playhead position
            auto rp = make_response_promise<AudioBufPtr>();
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(position_flicks_, frame_period, timeline_pts);

            if (!frame) {

                rp.deliver(AudioBufPtr());
                return rp;
            }

            mail(media_reader::get_audio_atom_v, *(frame.get()), false, uuid_)
                .request(pre_reader_, std::chrono::seconds(20))
                .then(

                    [=](AudioBufPtr audio_buffer) mutable {
                        audio_buffer.when_to_display_ = utility::clock::now();
                        audio_buffer.set_timline_timestamp(timeline_pts);
                        rp.deliver(audio_buffer);
                    },
                    [=](const error &err) mutable {
                        spdlog::critical(
                            "SubPlayhead buffer read timeout for frame {}",
                            to_string(frame->key()));
                        rp.deliver(err);
                    });

            return rp;
        },

        [=](image_buffer_atom) -> result<ImageBufPtr> {
            auto rp = make_response_promise<ImageBufPtr>();
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(position_flicks_, frame_period, timeline_pts);

            if (!frame) {

                rp.deliver(ImageBufPtr());
                return rp;
            }

            mail(media_reader::get_image_atom_v, *(frame.get()), false, uuid_, timeline_pts)
                .request(pre_reader_, std::chrono::seconds(20))
                .then(

                    [=](ImageBufPtr image_buffer) mutable {
                        image_buffer.when_to_display_ = utility::clock::now();
                        image_buffer.set_timline_timestamp(timeline_pts);
                        image_buffer.set_frame_id(*(frame.get()));
                        image_buffer.set_playhead_logical_frame(
                            logical_frame_from_pts(timeline_pts));
                        image_buffer.set_playhead_logical_duration(logical_frames_.size());
                        add_annotations_data_to_frame(image_buffer);
                        rp.deliver(image_buffer);
                    },
                    [=](const error &err) mutable {
                        spdlog::critical(
                            "SubPlayhead buffer read timeout for frame {}",
                            to_string(frame->key()));
                        rp.deliver(err);
                    });

            return rp;
        },

        [=](media_reader::push_image_atom,
            ImageBufPtr image_buffer,
            const media::AVFrameID &mptr,
            const time_point &tp,
            const timebase::flicks timeline_pts) {
            receive_image_from_cache(image_buffer, mptr, tp, timeline_pts);
        },

        [=](playlist::get_media_uuid_atom) -> result<utility::Uuid> {
            auto rp = make_response_promise<utility::Uuid>();
            mail(media::get_media_pointer_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const media::AVFrameID &frameid) mutable {
                        rp.deliver(frameid.media_uuid());
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](rate_atom) -> result<utility::FrameRate> {
            auto rp = make_response_promise<utility::FrameRate>();
            mail(media::get_media_pointer_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const media::AVFrameID &frameid) mutable {
                        rp.deliver(frameid.rate());
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](playhead::media_frame_ranges_atom) -> result<std::vector<int>> {
            auto rp = make_response_promise<std::vector<int>>();
            // we have to have run the 'source_atom' handler first (to have
            // built retimed_frames_) before we know the list of frames
            // where media changes in the timeline
            mail(source_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](caf::actor) mutable { rp.deliver(media_ranges_); },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](simple_loop_end_atom, const timebase::flicks flicks) {
            loop_out_point_ = flicks;
            set_in_and_out_frames();
        },

        [=](simple_loop_start_atom, const timebase::flicks flicks) {
            loop_in_point_ = flicks;
            set_in_and_out_frames();
        },

        [=](step_atom,
            timebase::flicks current_playhead_position,
            const int step_frames,
            const bool loop) -> result<timebase::flicks> {
            auto rp = make_response_promise<timebase::flicks>();
            get_position_after_step_by_frames(current_playhead_position, rp, step_frames, loop);
            return rp;
        },

        [=](skip_to_clip_atom,
            timebase::flicks current_playhead_position,
            const bool next_clip) -> timebase::flicks {
            // get the position corresponding to the first frame of the next
            // clip of the previous (current) clip
            return get_next_or_previous_clip_start_position(
                current_playhead_position, next_clip);
        },

        [=](utility::event_atom, utility::change_atom) {
            // up_to_date_            = false;
            // last_change_timepoint_ = utility::clock::now();

            up_to_date_            = false;
            last_change_timepoint_ = utility::clock::now();
            anon_mail(source_atom_v).send(this);

            // anon_mail(source_atom_v, last_change_timepoint_)
            //     .delay(std::chrono::milliseconds(50))
            //     .send(this);
        },

        [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
            return mail(utility::event_atom_v, utility::change_atom_v)
                .delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &bookmark_uuid) {
            // this comes from MediaActor, EditListActor or RetimeActor .. we
            // just ignore it as we listen to bookmark events coming from the
            // main BookmarkManager
        },

        [=](utility::event_atom,
            bookmark::bookmark_change_atom,
            const utility::Uuid &,
            const utility::UuidList &) {},

        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](precache_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            update_playback_precache_requests(rp);
            return rp;
        },
        [=](full_precache_atom, bool start_precache, const bool force) -> result<bool> {
            full_precache_activated_ = true;
            auto rp                  = make_response_promise<bool>();
            if (start_precache && (precache_start_frame_ != logical_frame_ || force)) {
                // we've been asked to start precaching, and the previous
                // sarting point is different to current position, so go ahead
                precache_start_frame_ = logical_frame_;
                make_static_precache_request(rp, start_precache);
            } else if (!start_precache) {
                // reset precache_start_frame_ so we know that there is no
                // pre-cacheing in flight
                precache_start_frame_ = std::numeric_limits<int>::lowest();
                make_static_precache_request(rp, start_precache);
            } else {
                // we've been asked to start precaching, but playhead hasn't
                // moved since last precache request which is either in
                // flight or hasn't finished
                rp.deliver(false);
            }
            return rp;
        },
        [=](check_logical_frame_changing_atom, const int logical_frame) {
            if (logical_frame == logical_frame_ && logical_frame != precache_start_frame_) {
                // logical frame is not changing! Kick off a full precache
                anon_mail(full_precache_atom_v, true, false).send(this);
            } else if (logical_frame != logical_frame_) {
                // otherwise stop any pre cacheing
                precache_start_frame_ = std::numeric_limits<int>::lowest();
            }
        },
        [=](utility::event_atom,
            bookmark::remove_bookmark_atom,
            const utility::Uuid &bookmark_uuid) { bookmark_deleted(bookmark_uuid); },
        [=](utility::event_atom, bookmark::add_bookmark_atom, const utility::UuidActor &n) {
            anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
        },
        [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::UuidActor &a) {
            bookmark_changed(a);
        });

    // slightly awkward...we need to join event group of bookmark manager
    mail(session::session_atom_v)
        .request(system().registry().template get<caf::actor>(studio_registry), infinite)
        .then(
            [=](caf::actor session) {
                mail(bookmark::get_bookmark_atom_v)
                    .request(session, infinite)
                    .then(
                        [=](caf::actor bookmark_manager) {
                            utility::join_event_group(this, bookmark_manager);
                        },
                        [=](caf::error &err) {});
            },
            [=](caf::error &err) {});
}

// move playhead to position
void SubPlayhead::set_position(
    const timebase::flicks time,
    const bool forwards,
    const bool playing,
    const float velocity,
    const bool force_updates,
    const bool active_in_ui,
    const bool scrubbing) {

    position_flicks_   = time;
    playing_forwards_  = forwards;
    playback_velocity_ = velocity;

    timebase::flicks frame_period, timeline_pts;
    std::shared_ptr<const media::AVFrameID> frame = get_frame(time, frame_period, timeline_pts);
    int logical_frame                             = logical_frame_from_pts(timeline_pts);

    if (playing && precache_start_frame_ != std::numeric_limits<int>::lowest()) {
        // a big stream of static pre-cache frame requests is probably sitting
        // with the reader. But we are playing back now so we need to cancel
        // the pre-cache requests
        precache_start_frame_ = std::numeric_limits<int>::lowest();
        anon_mail(media_reader::static_precache_atom_v, media::AVFrameIDsAndTimePoints(), uuid_)
            .send(pre_reader_);
    }

    if (logical_frame_ != logical_frame || force_updates || scrubbing) {

        const bool logical_changed = logical_frame_ != logical_frame;
        logical_frame_             = logical_frame;

        auto now = utility::clock::now();

        // get the image from the image readers or cache and also request the
        // next frame so we can do async texture uploads in the viewer
        if (playing || ((force_updates || scrubbing) && active_in_ui)) {

            // make a blocking request to retrieve the image
            if (media_type_ == media::MediaType::MT_IMAGE) {

                broadcast_image_frame(now, frame, playing, timeline_pts);

            } else if (
                media_type_ == media::MediaType::MT_AUDIO &&
                (playing || (scrubbing && logical_changed)) && logical_changed) {
                // only broadcast audio when playing OR scrubbing, not doing
                // a 'force_update'
                broadcast_audio_frame(now, frame, scrubbing);
            }

        } else if (frame && (active_in_ui || force_updates)) {

            // The user is scrubbing the timeline, or some other update changing
            // the playhead position, so we do a lazy fetch which won't block
            // us. The reader actor will send the image back to us if/wen it's
            // got it (see 'push_image_atom' handler above). Subsequent requests
            // will override this one if the reader isn't able to keep up.
            if (media_type_ == media::MediaType::MT_IMAGE) {
                media::AVFrameID mptr(*(frame.get()));
                anon_mail(
                    media_reader::get_image_atom_v,
                    mptr,
                    actor_cast<caf::actor>(this),
                    uuid_,
                    now,
                    timeline_pts)
                    .send(pre_reader_);
            } else if (media_type_ == media::MediaType::MT_AUDIO) {
                // don't sound audio if not playing or scrubbing
            }
        }

        if (media_type_ == media::MediaType::MT_AUDIO) {
            // we always broadcast the current audio frames - this is for
            // visualisation only not sounding.
            broadcast_audio_samples();
        }

        // update the parent playhead with our position
        if (frame && (previous_frame_ != frame || force_updates || logical_changed)) {
            anon_mail(
                position_atom_v,
                this,
                logical_frame_,
                frame->source_uuid(),
                frame->frame() - frame->first_frame(),
                frame->frame(),
                frame->rate(),
                frame->timecode(),
                to_string(frame->uri()))
                .send(parent_);
        }

        if (!playing && active_in_ui && full_precache_activated_) {
            // this delayed message allows us to kick-off the
            // pre-cache operation *if* the playhead has stopped
            // moving for static_cache_delay_milliseconds_, as we
            // will check in the message handler if the playhead
            // has indeed changed position and if not start the
            // pre-cache
            anon_mail(check_logical_frame_changing_atom_v, logical_frame_)
                .delay(static_cache_delay_milliseconds_)
                .send(this);
        }

        /*if (playing) {
            if (read_ahead_frames_ < 1 || force_updates) {
                auto rp = make_response_promise<bool>();
                update_playback_precache_requests(rp);
                read_ahead_frames_ = pre_cache_read_ahead_frames_/4;
            } else {
                read_ahead_frames_--;
            }
        } else {
            read_ahead_frames_ = int(drand48()*double(pre_cache_read_ahead_frames_)/4.0);
        }*/

        previous_frame_ = frame;
    }
}

void SubPlayhead::broadcast_image_frame(
    const utility::time_point when_to_show_frame,
    std::shared_ptr<const media::AVFrameID> frame_media_pointer,
    const bool playing,
    const timebase::flicks timeline_pts) {

    if (waiting_for_next_frame_) {
        // we have entered this function, trying to get an up-to-date frame to
        // broadcast (to the viewer, for example) but the last frame we
        // requested hasn't come back yet ...
        if (playing) {
            // .. we are in playback, assumption is that the readers/cache
            // can't decode frames fast enough - so we have to tell the parent
            // that we are dropping frames
            mail(dropped_frame_atom_v).send(parent_);
            return;
        } else {
            // we still haven't received the last frame we asked for a previous
            // time we entered this method. Instead of flooding the reader with
            // more frame read requests, we just return here. When we do get
            // the frame we asked for, we then force another update so that we
            // do request the latest frame based on the parent playhead position
            return;
        }
    }

    if (!frame_media_pointer || frame_media_pointer->is_nil()) {
        // If there is no media pointer, tell the parent playhead that we have
        // no media to show
        check_if_media_changed(frame_media_pointer ? frame_media_pointer.get() : nullptr);
        return;
    }

    waiting_for_next_frame_ = true;

    const int broadcast_logical = logical_frame_;

    mail(
        media_reader::get_image_atom_v,
        *(frame_media_pointer.get()),
        false,
        uuid_,
        timeline_pts)
        .request(pre_reader_, infinite)
        .then(

            [=](ImageBufPtr image_buffer) mutable {
                image_buffer.when_to_display_ = when_to_show_frame;
                image_buffer.set_timline_timestamp(timeline_pts);
                image_buffer.set_frame_id(*(frame_media_pointer.get()));
                image_buffer.set_playhead_logical_frame(logical_frame_from_pts(timeline_pts));
                image_buffer.set_playhead_logical_duration(logical_frames_.size());
                add_annotations_data_to_frame(image_buffer);

                mail(
                    show_atom_v,
                    uuid_,        // the uuid of this playhead
                    image_buffer, // the image
                    true          // is this the frame that should be on-screen now?
                    )
                    .send(parent_);

                check_if_media_changed(frame_media_pointer.get());


                waiting_for_next_frame_ = false;

                // We have got the frame that we want for *immediate* display,
                // now we also want to fetch the next N frames to allow the
                // viewport to upload pixel data to the GPU for subsequent
                // re-draws during playback
                if (playing)
                    request_future_frames();
                else if (broadcast_logical != logical_frame_) {
                    // This is crucial ... what if the playhead has moved since we
                    // requested the frame to broadcast? This could happen if the user
                    // is scrubbing frames and the reader can't keep up. If that is the
                    // case, we send a jump_atom message to the parent playhead, which
                    // in turn sends us back a jump_atom with its state and we request
                    // another broadcast frame
                    anon_mail(jump_atom_v, caf::actor_cast<caf::actor_addr>(this))
                        .send(parent_);
                }
            },

            [=](const caf::error &err) mutable {
                waiting_for_next_frame_ = false;
                if (broadcast_logical != logical_frame_) {
                    // see above
                    anon_mail(jump_atom_v, caf::actor_cast<caf::actor_addr>(this))
                        .send(parent_);
                }
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::broadcast_audio_frame(
    const utility::time_point /*when_to_show_frame*/,
    std::shared_ptr<const media::AVFrameID> frame_media_pointer,
    const bool scrubbing) {

    // This function is called every time we play a new frame of audio ..
    // We actually retrieve 20 whole frames of audio ahead of where the playhead
    // is right now. The reason is to allow a decent buffer for 'pre-roll' as
    // some audio output might need a substantial number of samples to be
    // buffered up.
    media::AVFrameIDsAndTimePoints future_frames;

    std::vector<timebase::flicks> tps;

    if (scrubbing) {

        // pick the next 5 frames in the timeline to send audio
        // samples for sounding when we scrub
        auto frame = current_frame_iterator();
        if (frame == retimed_frames_.end() || !frame->second) {
            return;
        }
        if (frame != first_frame_)
            frame--;
        auto tt = utility::clock::now();
        for (int i = 0; i < 5; ++i) {
            future_frames.emplace_back(tt, frame->second);
            tps.emplace_back(frame->first);
            auto a = frame->first;
            frame++;
            if (frame == retimed_frames_.end() || !frame->second)
                break;
            auto b = frame->first;
            tt     = tt + std::chrono::duration_cast<std::chrono::milliseconds>(b - a);
        }

    } else {
        tps = get_lookahead_frame_pointers(future_frames, 20);
    }

    // now fetch audio samples for playback
    mail(media_reader::get_audio_atom_v, future_frames, uuid_)
        .request(pre_reader_, std::chrono::milliseconds(5000))
        .then(

            [=](std::vector<AudioBufPtr> &audio_buffers) mutable {
                auto ab = audio_buffers.begin();
                auto fp = future_frames.begin();
                auto tt = tps.begin();
                while (ab != audio_buffers.end() && fp != future_frames.end()) {
                    ab->when_to_display_ = (*fp).first;
                    ab->set_timline_timestamp(*(tt++));
                    ab++;
                    fp++;
                }

                mail(
                    sound_audio_atom_v,
                    uuid_, // the uuid of this playhead
                    audio_buffers,
                    scrubbing)
                    .send(parent_);
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::broadcast_audio_samples() {

    // this is a lot like broadcast_audio_frame but we broadcast audio frames
    // either side of and including the current audio frame - this is used
    // for audio waveform visualisation on the viewport, for example
    media::AVFrameIDsAndTimePoints future_frames;
    std::vector<timebase::flicks> tps;

    // pick the next 1 or two frames in the timeline to send audio
    // samples for sounding
    auto frame = current_frame_iterator();
    if (frame == retimed_frames_.end()) {
        return;
    }

    // step backwards two frames
    if (frame != retimed_frames_.begin())
        frame--;
    if (frame != retimed_frames_.begin())
        frame--;

    int i   = 0;
    auto tt = utility::clock::now();
    while (frame != retimed_frames_.end() && i < 5) {
        if (frame->second) {
            future_frames.emplace_back(tt, frame->second);
            tps.emplace_back(frame->first);
        }
        i++;
        frame++;
    }

    const auto playhead_position = position_flicks_;

    // now fetch audio samples for playback
    mail(media_reader::get_audio_atom_v, future_frames, uuid_)
        .request(pre_reader_, std::chrono::milliseconds(5000))
        .then(

            [=](std::vector<AudioBufPtr> &audio_buffers) mutable {
                auto ab = audio_buffers.begin();
                auto fp = future_frames.begin();
                auto tt = tps.begin();
                while (ab != audio_buffers.end() && fp != future_frames.end()) {
                    ab->when_to_display_ = (*fp).first;
                    ab->set_timline_timestamp(*(tt++));
                    ab++;
                    fp++;
                }
                mail(
                    audio::audio_samples_atom_v,
                    audio_buffers, // the uuid of this playhead
                    playhead_position)
                    .send(parent_);
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

std::vector<timebase::flicks> SubPlayhead::get_lookahead_frame_pointers(
    media::AVFrameIDsAndTimePoints &result, const int max_num_frames) {

    std::vector<timebase::flicks> tps;
    if (num_retimed_frames_ < 2) {
        return tps;
    }

    timebase::flicks current_frame_tp =
        std::min(out_frame_->first, std::max(in_frame_->first, position_flicks_));

    auto frame = current_frame_iterator(current_frame_tp);

    const auto start_point = frame;

    auto tt = utility::clock::now();
    int r   = max_num_frames;

    while (r--) {
        if (playing_forwards_) {
            if (frame != out_frame_)
                frame++;
            else
                frame = in_frame_;
        } else {
            if (frame != in_frame_)
                frame--;
            else
                frame = out_frame_;
        }

        auto frame_plus = frame;
        frame_plus++;
        timebase::flicks frame_duration = frame_plus->first - frame->first;
        tt += std::chrono::duration_cast<std::chrono::microseconds>(
            frame_duration / playback_velocity_);

        bool repeat_frame = !result.empty() && result.back().second == frame->second;

        if (frame->second && !frame->second->source_uuid().is_null() && !repeat_frame) {
            // we don't send pre-read requests for 'blank' frames where
            // source_uuid is null
            result.emplace_back(tt, frame->second);
            tps.push_back(frame->first);
        }

        // this tests if we've looped around the full range before hitting
        // pre_cache_read_ahead_frames_, i.e. pre_cache_read_ahead_frames_ >
        // loop range
        if (frame == start_point)
            break;
    }
    return tps;
}


void SubPlayhead::request_future_frames() {

    media::AVFrameIDsAndTimePoints future_frames;
    auto timeline_pts_vec = get_lookahead_frame_pointers(future_frames, 4);

    mail(media_reader::get_future_frames_atom_v, future_frames, uuid_)
        .request(pre_reader_, std::chrono::milliseconds(5000))
        .then(

            [=](std::vector<ImageBufPtr> image_buffers) mutable {
                auto tp   = timeline_pts_vec.begin();
                auto idsp = future_frames.begin();
                for (auto &imbuf : image_buffers) {
                    imbuf.set_playhead_logical_frame(logical_frame_from_pts(*(tp)));
                    imbuf.set_playhead_logical_duration(logical_frames_.size());
                    imbuf.set_timline_timestamp(*(tp++));
                    std::shared_ptr<const media::AVFrameID> av_idx = (idsp++)->second;
                    if (av_idx) {
                        imbuf.set_frame_id(*(av_idx.get()));

                        add_annotations_data_to_frame(imbuf);
                    }
                }
                mail(
                    show_atom_v,
                    uuid_, // the uuid of this playhead
                    image_buffers)
                    .send(parent_);
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::update_playback_precache_requests(caf::typed_response_promise<bool> &rp) {

    // get the AVFrameID iterator for the current frame
    media::AVFrameIDsAndTimePoints requests;
    get_lookahead_frame_pointers(requests, pre_cache_read_ahead_frames_);

    make_prefetch_requests_for_colour_pipeline(requests);

    mail(media_reader::playback_precache_atom_v, requests, uuid_, media_type_)
        .request(pre_reader_, infinite)
        .then(
            [=](const bool requests_processed) mutable { rp.deliver(requests_processed); },
            [=](const error &err) mutable { rp.deliver(err); });
}

void SubPlayhead::make_static_precache_request(
    caf::typed_response_promise<bool> &rp, const bool start_precache) {

    // N.B. if we're not starting the precache, we are going to stop it
    // by just sending an empty request to the pre-reader
    if (start_precache) {

#pragma message                                                                                \
    "This needs fixing - for now, hardcoding fetch max 2048 frames for idle precache"

        media::AVFrameIDsAndTimePoints requests;
        get_lookahead_frame_pointers(
            requests, 2048
            // std::numeric_limits<int>::max() // this will fetch *ALL* frames in the source
        );

        make_prefetch_requests_for_colour_pipeline(requests);

        mail(media_reader::static_precache_atom_v, requests, uuid_)
            .request(pre_reader_, infinite)
            .await(
                [=](bool requests_processed) mutable { rp.deliver(requests_processed); },
                [=](const error &err) mutable { rp.deliver(err); });

    } else {
        mail(media_reader::static_precache_atom_v, media::AVFrameIDsAndTimePoints(), uuid_)
            .request(pre_reader_, infinite)
            .await(
                [=](bool requests_processed) mutable { rp.deliver(requests_processed); },
                [=](const error &err) mutable { rp.deliver(err); });
    }
}

void SubPlayhead::make_prefetch_requests_for_colour_pipeline(
    const media::AVFrameIDsAndTimePoints &lookeahead_frames) {

    // Looping through all the frames looking for each individual source that
    // might hit the screen soon. We broadcast the colour metadata for these sources
    // to be picked up by the colour management plugin so it has a chance to
    // do heavy work, like loading and parsing LUT files, ahead of time.
    media::AVFrameIDsAndTimePoints frame_ids_for_colour_precompute_frame_ids;
    utility::Uuid curr_uuid;
    for (const auto &r : lookeahead_frames) {
        if (r.second->source_uuid() != curr_uuid) {
            frame_ids_for_colour_precompute_frame_ids.push_back(r);
            curr_uuid = r.second->source_uuid();
        }
    }

    mail(colour_pipeline_lookahead_atom_v, frame_ids_for_colour_precompute_frame_ids)
        .send(parent_);
}


void SubPlayhead::receive_image_from_cache(
    ImageBufPtr image_buffer,
    const media::AVFrameID mptr,
    const time_point tp,
    const timebase::flicks timeline_pts) {

    // are we receiving an image from the cache that was requested *before* the previous
    // image we received, i.e. it's being sent from the readers out of order?
    // If so, skip it.
    if (last_image_timepoint_ > tp)
        return;
    last_image_timepoint_ = tp;

    image_buffer.when_to_display_ = utility::clock::now();
    image_buffer.set_timline_timestamp(timeline_pts);
    image_buffer.set_playhead_logical_frame(logical_frame_from_pts(timeline_pts));
    image_buffer.set_playhead_logical_duration(logical_frames_.size());
    image_buffer.set_frame_id(mptr);
    add_annotations_data_to_frame(image_buffer);

    mail(
        show_atom_v,
        uuid_,        // the uuid of this playhead
        image_buffer, // the image
        true          // this image supposed to be shown on-screen NOW
        )
        .send(parent_);

    check_if_media_changed(&mptr);
}

void SubPlayhead::get_full_timeline_frame_list(caf::typed_response_promise<caf::actor> rp) {

    if (up_to_date_) {
        rp.deliver(source_.actor());
        // also clean queue..
        for (auto &i : inflight_update_requests_)
            i.deliver(source_.actor());
        inflight_update_requests_.clear();

        return;
    }

    // resetting this will force a broadcast of media_source_atom in
    // check_if_media_changed, which we need to trigger re-evaulation of
    // on-screen overlays among other things
    current_source_ = utility::Uuid();

    if (utility::clock::now() - last_update_requested_ < std::chrono::milliseconds(10)) {
        // too soon..
        // we just did a request..
        if (inflight_update_requests_.empty())
            anon_mail(source_atom_v).delay(std::chrono::milliseconds(10)).send(this);
        inflight_update_requests_.push_back(rp);
        return;
    }

    inflight_update_requests_.push_back(rp);

    if (!source_.actor()) {
        full_timeline_frames_.clear();
        update_retiming();
        for (auto &rprm : inflight_update_requests_) {
            rprm.deliver(source_.actor());
        }
        inflight_update_requests_.clear();
        up_to_date_            = true;
        last_update_requested_ = utility::clock::now();

        return;
    }

    // check if we've already requested an update (in the request immediately
    // below here) that hasn't come baack yet. If so, don't make the request
    // again
    // if (inflight_update_requests_.size() > 1)
    //     return;

    last_update_requested_ = utility::clock::now();

    const auto request_update_timepoint = utility::clock::now();

    mail(media::get_media_pointers_atom_v, media_type_, time_source_mode_, override_frame_rate_)
        .request(source_.actor(), infinite)
        .then(
            [=](const media::FrameTimeMapPtr &mpts) mutable {
                if (mpts) {
                    full_timeline_frames_ = *mpts;
                } else {
                    full_timeline_frames_.clear();
                }

                update_retiming();

                /*auto tp = utility::clock::now();
                if (media_type_ == media::MT_IMAGE) {
                    std::cerr << "VID FRAMES GEN DELAY " << to_string(uuid_) << " " <<
                std::chrono::duration_cast<std::chrono::microseconds>(tp-request_update_timepoint).count()
                << "\n";
                }*/

                // last step is to get all info on bookmarks for the media that
                // we might be playing
                mail(bookmark::get_bookmarks_atom_v, true)
                    .request(caf::actor_cast<caf::actor>(this), infinite)
                    .then(
                        [=](bool) mutable {
                            // our data has changed (retimed_frames_ describes most)
                            // things that are important about the timeline, so send change
                            // notification
                            mail(
                                utility::event_atom_v,
                                utility::change_atom_v,
                                actor_cast<actor>(this))
                                .send(event_group_);

                            // rp.deliver(source_);
                            for (auto &rprm : inflight_update_requests_) {
                                rprm.deliver(source_.actor());
                            }
                            inflight_update_requests_.clear();

                            mail(
                                utility::event_atom_v,
                                playhead::media_frame_ranges_atom_v,
                                media_ranges_)
                                .send(parent_);


                            if (request_update_timepoint < last_change_timepoint_) {
                                // One last check:
                                // we've been told by the source_ that it has changed
                                // at some point after we did the get_media_pointers_atom
                                // request here... therefore, we must request an
                                // update again to make sure we are fully up-to-date
                                // with the source
                                anon_mail(source_atom_v).send(this);
                                up_to_date_ = false;
                            } else {
                                up_to_date_ = true;
                            }
                        },
                        [=](const error &err) mutable {
                            for (auto &rprm : inflight_update_requests_) {
                                rprm.deliver(err);
                            }
                            inflight_update_requests_.clear();
                        });
            },
            [=](const error &err) mutable {
                if (to_string(err) == "error(\"No streams\")" ||
                    to_string(err) == "error(\"No MediaSources\")") {

                    // We're here because the source has no streams, or it has no media

                    if (media_type_ == media::MT_IMAGE) {
                        // we have no image streams/sources. However, there might be a
                        // valid AUDIO source. In this case, we want to build a blank
                        // image frames to align with audio frames, as the main
                        // PlayheadActor requires a valid IMAGE source to drive frame-rate,
                        // duration etc.
                        mail(
                            media::get_media_pointers_atom_v,
                            media::MT_AUDIO,
                            time_source_mode_,
                            override_frame_rate_)
                            .request(source_.actor(), infinite)
                            .then(
                                [=](const media::FrameTimeMapPtr &mpts) mutable {
                                    if (mpts) {
                                        // replace the Audio frame ptrs with
                                        // blank video frames
                                        full_timeline_frames_ = *mpts;
                                        auto p                = full_timeline_frames_.begin();
                                        while (p != full_timeline_frames_.end()) {
                                            p->second = media::make_blank_frame(
                                                p->second->rate(), media_type_);
                                            p++;
                                        }

                                    } else {
                                        full_timeline_frames_.clear();
                                    }
                                    update_retiming();
                                    for (auto &rprm : inflight_update_requests_) {
                                        rprm.deliver(source_.actor());
                                    }
                                    inflight_update_requests_.clear();
                                    up_to_date_ = true;
                                },
                                [=](const error &err) mutable {
                                    // audio frames fetch aslo failing - fallback to empty
                                    // frame map.
                                    full_timeline_frames_.clear();
                                    update_retiming();
                                    for (auto &rprm : inflight_update_requests_) {
                                        rprm.deliver(source_.actor());
                                    }
                                    inflight_update_requests_.clear();
                                    up_to_date_ = true;
                                });
                    } else {

                        // still no media stream/source
                        full_timeline_frames_.clear();
                        update_retiming();
                        for (auto &rprm : inflight_update_requests_) {
                            rprm.deliver(source_.actor());
                        }
                        inflight_update_requests_.clear();
                        up_to_date_ = true;
                    }

                } else {
                    // rp.deliver(err);
                    for (auto &rprm : inflight_update_requests_) {
                        rprm.deliver(err);
                    }
                    inflight_update_requests_.clear();
                    up_to_date_ = true;
                }
            });
}

std::shared_ptr<const media::AVFrameID> SubPlayhead::get_frame(
    const timebase::flicks &time,
    timebase::flicks &frame_period,
    timebase::flicks &timeline_pts,
    int step_frames) {

    // If rate = 25fps (40ms per frame) ...
    // if first frame is shown at time = 0, second frame is shown at time = 40ms.
    // Therefore if time = 39ms then we return first frame
    //
    // Also, note there will be an extra null pointer at the end of
    // retimed_frames_ corresponding to last_frame + duration of last frame.
    // So if we have a source with only one frame retimed_frames_
    // would look like this:
    // {
    //     {0ms, media::AVFrameID(first frame)},
    //     {40ms, media::AVFrameID(null frame)}
    // }
    if (num_retimed_frames_ < 2) {
        // and give the others values something valid ???
        frame_period = timebase::k_flicks_zero_seconds;
        timeline_pts = timebase::k_flicks_zero_seconds;
        return std::shared_ptr<media::AVFrameID>();
    }

    timebase::flicks t = std::min(last_frame_->first, std::max(first_frame_->first, time));

    // get the frame to be show *after* time point t and decrement to get our
    // frame.
    auto tt    = utility::clock::now();
    auto frame = current_frame_iterator(t);

    while (step_frames < 0 && frame != first_frame_) {
        frame--;
        step_frames++;
    }
    while (step_frames > 0 && frame != last_frame_) {
        frame++;
        step_frames--;
    }

    // see above, there is a dummy frame at the end of retimed_frames_ so
    // this is always valid:
    auto next_frame = frame;
    next_frame++;
    frame_period = next_frame->first - frame->first;
    timeline_pts = frame->first;

    return frame->second;
}

void SubPlayhead::get_position_after_step_by_frames(
    const timebase::flicks ref_position,
    caf::typed_response_promise<timebase::flicks> &rp,
    int step_frames,
    const bool loop) {

    if (num_retimed_frames_ < 2) {
        rp.deliver(make_error(xstudio_error::error, "No Frames"));
        return;
    }

    timebase::flicks t = std::min(out_frame_->first, std::max(in_frame_->first, ref_position));

    auto frame = current_frame_iterator(t);

    const bool forwards = step_frames >= 0;

    while (step_frames) {

        if (forwards) {
            if (!loop && frame == out_frame_) {
                break;
            } else if (frame == out_frame_) {
                frame = in_frame_;
            } else {
                frame++;
            }
            step_frames--;
        } else {
            if (!loop && frame == in_frame_) {
                break;
            } else if (frame == in_frame_) {
                frame = out_frame_;
            } else {
                frame--;
            }
            step_frames++;
        }
    }
    rp.deliver(frame->first);
}

timebase::flicks SubPlayhead::get_next_or_previous_clip_start_position(
    const timebase::flicks ref_position, const bool next_clip) {

    auto result = ref_position;

    if (num_retimed_frames_ < 2) {
        return result;
    }

    timebase::flicks frame_period, timeline_pts;
    std::shared_ptr<const media::AVFrameID> frame =
        get_frame(ref_position, frame_period, timeline_pts);
    int logical = logical_frame_from_pts(timeline_pts);

    if (next_clip) {
        for (int i = 0; i < (int)media_ranges_.size(); ++i) {
            if (media_ranges_[i] > logical) {
                logical = media_ranges_[i];
                break;
            }
        }
    } else {
        for (int i = (int(media_ranges_.size()) - 1); i >= 0; --i) {
            if (media_ranges_[i] < logical) {
                logical = media_ranges_[i];
                break;
            }
        }
    }

    // woopsie - this loop is not an efficient way to work out if
    // we will hit the end frame!!
    auto f = retimed_frames_.begin();
    while (logical && f != retimed_frames_.end()) {
        f++;
        logical--;
    }
    if (f == retimed_frames_.end())
        f--;
    result = f->first;
    return result;
}

void SubPlayhead::update_retiming() {

    // to do retiming we insert held frames at the start or end of
    // full_timeline_frames_, or conversely we trim frames off the start or
    // end.

    num_source_frames_  = full_timeline_frames_.size();
    auto retimed_frames = full_timeline_frames_;

    if (retimed_frames.empty()) {
        // provide a single blank frame, which can then be extended in the loop
        // below to fill the forced duration.
        retimed_frames[timebase::flicks(0)] =
            media::make_blank_frame(default_rate_, media_type_);
    }

    if (frame_offset_ > 0) {

        // if offset is forward, we remove frames at the front
        auto p = retimed_frames.begin();
        std::advance(p, std::min(int64_t(retimed_frames.size()) - 1, frame_offset_));
        retimed_frames.erase(retimed_frames.begin(), p);

    } else if (frame_offset_ < 0) {

        // negative offset means we extend the beginning with held frames,
        // i.e. duplicate the first frame - unless we are timeline (where we
        // want a blank frame, not held frame, or audio where we want silence)
        auto first_frame =
            (source_is_timeline_ || media_type_ == media::MT_AUDIO)
                ? media::make_blank_frame(retimed_frames.begin()->second->rate(), media_type_)
                : retimed_frames.begin()->second;

        timebase::flicks frame_duration = override_frame_rate_;
        if (time_source_mode_ != TimeSourceMode::FIXED && first_frame) {
            frame_duration = first_frame->rate();
        }
        // now mark as a 'held frame'
        if (first_frame) {
            media::AVFrameID ff = *first_frame;
            ff.set_frame_status(media::FS_HELD_FRAME);
            first_frame = std::make_shared<const media::AVFrameID>(ff);
        }
        int64_t off = frame_offset_;
        while (off < 0) {
            retimed_frames[retimed_frames.begin()->first - frame_duration] = first_frame;
            off++;
        }
    }

    // now we need to rebase retimed_frames so that first frame is at t=0
    retimed_frames_.clear();
    if (!retimed_frames.empty()) {
        auto t0 = retimed_frames.begin()->first;
        for (auto &p : retimed_frames) {
            retimed_frames_[p.first - t0] = p.second;
        }
    }

    if (forced_duration_ != timebase::k_flicks_zero_seconds) {

        // trim end frame off until our duration is leq than forced_duration_
        while (retimed_frames_.size() > 1 &&
               retimed_frames_.rbegin()->first >= forced_duration_) {
            retimed_frames_.erase(std::prev(retimed_frames_.end()));
        }

        // if forced_duration_ extends beyond the last entry in retimed_frames_,
        // we duplicate the last frame until this is no longer the case,
        // i.e. held frame behaviour. UNLESS the source is a timeline, in whcih
        // case we want blank frames, or if we're audio playhead in which case
        // we want silence
        auto last_frame =
            (source_is_timeline_ || media_type_ == media::MT_AUDIO)
                ? media::make_blank_frame(retimed_frames_.rbegin()->second->rate(), media_type_)
                : retimed_frames_.rbegin()->second;
        timebase::flicks frame_duration = override_frame_rate_;
        if (time_source_mode_ != TimeSourceMode::FIXED && last_frame) {
            frame_duration = last_frame->rate();
        }
        // now mark as a 'held frame'
        if (last_frame) {
            media::AVFrameID ff = *last_frame;
            ff.set_frame_status(media::FS_HELD_FRAME);
            last_frame = std::make_shared<const media::AVFrameID>(ff);
        }
        while ((retimed_frames_.rbegin()->first + frame_duration) < forced_duration_) {
            retimed_frames_[retimed_frames_.rbegin()->first + frame_duration] = last_frame;
        }
    }


    if (!retimed_frames_.empty() && retimed_frames_.rbegin()->second) {
        // the logic here is crucial ... retimed_frames_ is used to
        // evaluate the full duration of what's being played. We need to drop
        // in an empty frame at the end, with a timestamp that matches the
        // point just *after* the last frame's timestamp plus its duration.
        // Thus, for a single frame sourc that is 24pfs, say, we will have
        // two entries in retimed_frames_ ... one entry a t=0that is
        // the frame. The second is a nullptr at t = 1.0/24.0s.
        //
        // We test if the last frame is empty in case our source has already
        // taken care of this for us.
        auto last_frame_timepoint = retimed_frames_.rbegin()->first;
        last_frame_timepoint += time_source_mode_ == TimeSourceMode::FIXED
                                    ? override_frame_rate_
                                    : retimed_frames_.rbegin()->second->rate();
        retimed_frames_[last_frame_timepoint].reset();
    }

    num_retimed_frames_ = retimed_frames_.size();

    store_media_frame_ranges();

    set_in_and_out_frames();
}

void SubPlayhead::store_media_frame_ranges() {

    all_media_uuids_.clear();
    media_ranges_.clear();
    logical_frames_.clear();

    utility::Uuid media_uuid;
    utility::Uuid clip_uuid;
    int logical_frame = 0;
    for (const auto &f : retimed_frames_) {
        if (f.second && f.second->media_uuid() != media_uuid) {
            media_uuid = f.second->media_uuid();
            all_media_uuids_.insert(media_uuid);
            media_ranges_.push_back(logical_frame);
        } else if (!f.second)
            media_uuid = utility::Uuid();

        // we want the media_ranges_ to give frames where a new clip
        // OR new media starts. We only get clip IDs with Timelines.
        if (f.second && f.second->clip_uuid() != clip_uuid) {
            clip_uuid = f.second->clip_uuid();
            if (media_ranges_.size() && media_ranges_.back() != logical_frame) {
                media_ranges_.push_back(logical_frame);
            }
        } else if (!f.second)
            clip_uuid = utility::Uuid();
        logical_frames_[f.first] = logical_frame++;
    }

    media_ranges_.push_back(logical_frame);
}

void SubPlayhead::set_in_and_out_frames() {

    if (num_retimed_frames_ < 2) {
        out_frame_   = retimed_frames_.begin();
        in_frame_    = retimed_frames_.begin();
        last_frame_  = retimed_frames_.begin();
        first_frame_ = retimed_frames_.begin();
        return;
    }

    // to get to the last frame, due to the last 'dummy' frame that is appended
    // to account for the duration of the last frame, step back twice from end
    last_frame_ = retimed_frames_.end();
    last_frame_--;
    last_frame_--;
    first_frame_ = retimed_frames_.begin();

    if (loop_out_point_ > last_frame_->first) {
        out_frame_ = last_frame_;
    } else {
        // loop out point includes frame duration of last frame in the loop
        // range.
        // So if frame rate is @25hz, frame duration is 40ms
        // If loop in point is 0ms and loop out_point is 80ms, we will loop
        // over frames 0 & 1 only.
        out_frame_ = retimed_frames_.upper_bound(loop_out_point_);
        if (out_frame_ != first_frame_)
            out_frame_--;
    }

    if (loop_in_point_ > out_frame_->first) {
        in_frame_ = out_frame_;
    } else {
        in_frame_ = retimed_frames_.upper_bound(loop_in_point_);
        if (in_frame_ != first_frame_)
            in_frame_--;
    }
}

void SubPlayhead::full_bookmarks_update(caf::typed_response_promise<bool> done) {

    // the goal here is to work out which frames are bookmarked and make
    // a list of each bookmark and its frame range (in the playhead timeline).
    // Note that the same bookmark can appear twice in the case where the same
    // piece of media appears twice in a timeline, say

    // TODO: This code works but it is really hard to understand. This is a
    // downside of our async requests to bookmark and building the bookmarks
    // in the context of the playback timeline. We can massively simplify it
    // by getting the MediaActors to deliver their bookmark ranges via the
    // AVFrameID struct instead.

    if (all_media_uuids_.empty()) {
        fetch_bookmark_annotations(BookmarkRanges(), done);
        return;
    }

    auto global = system().registry().template get<caf::actor>(global_registry);
    mail(bookmark::get_bookmark_atom_v)
        .request(global, infinite)
        .then(
            [=](caf::actor bookmarks_manager) mutable {
                // here we get all bookmarks that match any and all of the media
                // that appear in our timline
                mail(bookmark::bookmark_detail_atom_v, all_media_uuids_)
                    .request(bookmarks_manager, std::chrono::seconds(5))
                    .then(
                        [=](const std::vector<bookmark::BookmarkDetail>
                                &bookmark_details) mutable {
                            // make a map of the bookmarks against the uuid of the media
                            // that owns the bookmark
                            std::map<utility::Uuid, std::vector<bookmark::BookmarkDetail>>
                                bookmarks;

                            BookmarkRanges result;

                            if (bookmark_details.empty()) {
                                fetch_bookmark_annotations(result, done);
                                return;
                            }

                            for (const auto &i : bookmark_details) {
                                if (i.owner_ and i.media_reference_) {
                                    bookmarks[(*(i.owner_)).uuid()].push_back(i);
                                }
                            }

                            utility::Uuid curr_media_uuid;
                            std::vector<bookmark::BookmarkDetail> *curr_media_bookmarks =
                                nullptr;

                            // WARNING!! This is potentially expensive for very long timelines
                            // ... Need to look for an optimisation

                            // loop over the timeline frames, kkep track of current
                            // media (for efficiency) and check against the bookmarks
                            int logical_playhead_frame = 0;
                            for (const auto &f : retimed_frames_) {

                                // check if media changed and if so are there bookmarks?
                                if (f.second && f.second->media_uuid() != curr_media_uuid) {
                                    curr_media_uuid = f.second->media_uuid();
                                    if (bookmarks.count(curr_media_uuid)) {
                                        curr_media_bookmarks = &(bookmarks[curr_media_uuid]);
                                    } else {
                                        curr_media_bookmarks = nullptr;
                                    }

                                } else if (!f.second) {
                                    curr_media_uuid      = utility::Uuid();
                                    curr_media_bookmarks = nullptr;
                                }

                                if (curr_media_bookmarks) {

                                    auto media_frame =
                                        f.second->frame() - f.second->first_frame();
                                    for (const auto &bookmark : *curr_media_bookmarks) {
                                        if (bookmark.start_frame() <= media_frame &&
                                            bookmark.end_frame() >= media_frame) {
                                            extend_bookmark_frame(
                                                bookmark, logical_playhead_frame, result);
                                        }
                                    }
                                }
                                logical_playhead_frame++;
                            }

                            fetch_bookmark_annotations(result, done);
                        },
                        [=](const error &err) mutable {
                            done.deliver(false);
                            spdlog::warn("A {} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                done.deliver(false);
                spdlog::warn("B {} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::extend_bookmark_frame(
    const bookmark::BookmarkDetail &detail,
    const int logical_playhead_frame,
    BookmarkRanges &bookmark_ranges) {
    bool existing_entry_extended = false;
    for (auto &bm_frame_range : bookmark_ranges) {
        if (detail.uuid_ == std::get<0>(bm_frame_range)) {
            if (std::get<3>(bm_frame_range) == (logical_playhead_frame - 1)) {
                std::get<3>(bm_frame_range)++;
                existing_entry_extended = true;
                break;
            }
        }
    }
    if (!existing_entry_extended) {
        bookmark_ranges.emplace_back(std::make_tuple(
            detail.uuid_, detail.colour(), logical_playhead_frame, logical_playhead_frame));
    }
}

void SubPlayhead::fetch_bookmark_annotations(
    BookmarkRanges bookmark_ranges, caf::typed_response_promise<bool> rp) {

    // TODO: this bookmark evaluation code is horrible! refactor.

    if (!bookmark_ranges.size()) {
        bookmark_ranges_.clear();
        bookmarks_.clear();
        mail(utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_ranges_)
            .send(parent_);
        rp.deliver(true);
        return;
    }
    utility::UuidList bookmark_ids;
    for (const auto &p : bookmark_ranges) {
        bookmark_ids.push_back(std::get<0>(p));
    }

    // first we need to get to the 'bookmarks_manager'
    auto global = system().registry().template get<caf::actor>(global_registry);
    mail(bookmark::get_bookmark_atom_v)
        .request(global, infinite)
        .then(
            [=](caf::actor bookmarks_manager) mutable {
                // get the bookmark actors for bookmarks that are in our timline
                mail(bookmark::get_bookmark_atom_v, bookmark_ids)
                    .request(bookmarks_manager, infinite)
                    .then(
                        [=](const std::vector<UuidActor> &bookmarks) mutable {
                            // now we are ready to build our vector of bookmark, annotations and
                            // associated logical frame ranges
                            auto result =
                                std::shared_ptr<xstudio::bookmark::BookmarkAndAnnotations>(
                                    new xstudio::bookmark::BookmarkAndAnnotations);
                            auto count = std::make_shared<int>(bookmarks.size());
                            if (bookmarks.empty())
                                rp.deliver(true);
                            for (auto bookmark : bookmarks) {

                                // now ask the bookmark actor for its detail and
                                // annotation data (if any)
                                mail(
                                    bookmark::bookmark_detail_atom_v,
                                    bookmark::get_annotation_atom_v)
                                    .request(bookmark.actor(), infinite)
                                    .then(
                                        [=](bookmark::BookmarkAndAnnotationPtr data) mutable {
                                            for (const auto &p : bookmark_ranges) {
                                                if (data->detail_.uuid_ == std::get<0>(p)) {
                                                    // set the frame ranges.
                                                    auto d =
                                                        new bookmark::BookmarkAndAnnotation(
                                                            *data);
                                                    d->start_frame_ = std::get<2>(p);
                                                    d->end_frame_   = std::get<3>(p);
                                                    result->emplace_back(d);
                                                }
                                            }

                                            (*count)--;
                                            if (!*count) {

                                                // sortf bookmarks by start frame - makes
                                                // searching them faster
                                                std::sort(
                                                    result->begin(),
                                                    result->end(),
                                                    [](const bookmark::BookmarkAndAnnotationPtr
                                                           &a,
                                                       const bookmark::BookmarkAndAnnotationPtr
                                                           &b) -> bool {
                                                        return a->start_frame_ <
                                                               b->start_frame_;
                                                    });

                                                bookmarks_ = *result;

                                                // now ditch non-visible bookmarks
                                                // (e.g. grades) from our ranges
                                                bookmark_ranges_.clear();
                                                auto p = bookmark_ranges.begin();
                                                while (p != bookmark_ranges.end()) {
                                                    const auto uuid       = std::get<0>(*p);
                                                    bool visible_bookmark = true;
                                                    for (const auto &b : bookmarks_) {
                                                        if (b->detail_.uuid_ == uuid &&
                                                            !(b->detail_.visible_ &&
                                                              *(b->detail_.visible_))) {
                                                            visible_bookmark = false;
                                                            break;
                                                        }
                                                    }
                                                    if (visible_bookmark) {
                                                        bookmark_ranges_.push_back(*p);
                                                    }
                                                    p++;
                                                }

                                                // we've finished, ping the parent PlayheadActor
                                                // with our new bookmark ranges
                                                mail(
                                                    utility::event_atom_v,
                                                    bookmark::get_bookmarks_atom_v,
                                                    bookmark_ranges_)
                                                    .send(parent_);

                                                anon_mail(
                                                    jump_atom_v,
                                                    caf::actor_cast<caf::actor_addr>(this))
                                                    .send(parent_);

                                                rp.deliver(true);
                                            }
                                        },
                                        [=](const error &err) mutable {
                                            rp.deliver(false);
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        });
                            }
                        },
                        [=](const error &err) mutable {
                            rp.deliver(false);
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                rp.deliver(false);
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::add_annotations_data_to_frame(ImageBufPtr &frame) {

    xstudio::bookmark::BookmarkAndAnnotations bookmarks;
    int logical_frame = logical_frame_from_pts(frame.timeline_timestamp());
    for (auto &p : bookmarks_) {
        if (p->start_frame_ <= logical_frame && p->end_frame_ >= logical_frame) {
            bookmarks.push_back(p);
        } else if (p->start_frame_ > logical_frame)
            break;
        // bookmarks_ sorted by start frame so if this bookmark starts after
        // logical_frame we can leave the loop
    }
    frame.set_bookmarks(bookmarks);
}

void SubPlayhead::bookmark_deleted(const utility::Uuid &bookmark_uuid) {

    // update bookmark only if the removed bookmark is in our list...
    const size_t b = bookmarks_.size();
    auto p         = bookmarks_.begin();
    while (p != bookmarks_.end()) {
        if ((*p)->detail_.uuid_ == bookmark_uuid) {
            p = bookmarks_.erase(p);
        } else {
            p++;
        }
    }
    const size_t n = bookmark_ranges_.size();
    auto q         = bookmark_ranges_.begin();
    while (q != bookmark_ranges_.end()) {
        if (std::get<0>(*q) == bookmark_uuid) {
            q = bookmark_ranges_.erase(q);
        } else {
            q++;
        }
    }

    if (n != bookmark_ranges_.size() || b != bookmarks_.size()) {
        mail(utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_ranges_)
            .send(parent_);
    }
}

void SubPlayhead::bookmark_changed(const utility::UuidActor bookmark) {

    // if a bookmark has changed, and its a bookmar that is in our timleine,
    // do a full rebuild to make sure we're fully up to date.
    for (auto &p : bookmarks_) {
        if (p->detail_.uuid_ == bookmark.uuid()) {
            anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
            return;
        }
    }

    // null bookmark actor means a bookmark has been deleted
    if (!bookmark.actor())
        return;

    // even though this doesn't look like our bookmark, the change that has
    // happened to it might have been associating it with media that IS in
    // our timeline, in which case we need to rebuild our bookmarks data
    mail(bookmark::bookmark_detail_atom_v)
        .request(bookmark.actor(), infinite)
        .then(
            [=](const bookmark::BookmarkDetail &detail) {
                if (detail.owner_) {
                    auto p = std::find(
                        all_media_uuids_.begin(),
                        all_media_uuids_.end(),
                        (*(detail.owner_)).uuid());
                    if (p != all_media_uuids_.end()) {
                        anon_mail(bookmark::get_bookmarks_atom_v, true).send(this);
                    }
                }
            },
            [=](const caf::error &err) mutable {
                spdlog::warn(
                    "{} {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(err),
                    to_string(bookmark.uuid()));
            });
}

media::FrameTimeMap::iterator SubPlayhead::current_frame_iterator(const timebase::flicks t) {
    auto frame = retimed_frames_.upper_bound(t);
    if (frame != retimed_frames_.begin()) {
        frame--;
    }
    return frame;
}

media::FrameTimeMap::iterator SubPlayhead::current_frame_iterator() {
    auto frame = retimed_frames_.upper_bound(position_flicks_);
    if (frame != retimed_frames_.begin()) {
        frame--;
    }
    return frame;
}

template <typename T>
void copy_audio_samples(
    AudioBufPtr &buffer_to_fill,
    const timebase::flicks buffer_to_fill_timestamp,
    const AudioBufPtr &source_buffer,
    const timebase::flicks source_buffer_timestamp) {

    if (buffer_to_fill->num_channels() != source_buffer->num_channels() ||
        buffer_to_fill->sample_rate() != source_buffer->sample_rate()) {
        // The sample rate is set globally througout the application with
        // /core/audio/windows_audio_prefs/sample_rate preference, so this
        // can't happen!
        spdlog::warn("{} mismatch in audio buffer sample rates.", __PRETTY_FUNCTION__);
        return;
    }

    const size_t dest_size_samples =
        buffer_to_fill->num_samples() * buffer_to_fill->num_channels();
    const size_t src_size_samples =
        source_buffer->num_samples() * source_buffer->num_channels();
    const size_t fsr = source_buffer->sample_rate() * source_buffer->num_channels();

    // As far as the playhead is concerned, audio buffers are aligned with video frames.
    // However, the sample data in the buffer is not exactly aligned (because the
    // chunking of audio samples coming from ffmpeg is typically independent of the video
    // frame duration).
    const auto source_first_sample_timestamp = std::chrono::duration_cast<timebase::flicks>(
        source_buffer_timestamp + source_buffer->time_delta_to_video_frame());

    size_t offset_into_dest   = 0;
    size_t offset_into_source = 0;
    if (source_first_sample_timestamp < buffer_to_fill_timestamp) {
        const size_t msec = std::chrono::duration_cast<std::chrono::microseconds>(
                                buffer_to_fill_timestamp - source_first_sample_timestamp)
                                .count();
        offset_into_source = msec * fsr / 1000000;
    } else {
        const size_t msec = std::chrono::duration_cast<std::chrono::microseconds>(
                                source_first_sample_timestamp - buffer_to_fill_timestamp)
                                .count();
        offset_into_dest = msec * fsr / 1000000;
    }

    size_t n_samps_to_copy =
        std::min(src_size_samples - offset_into_source, dest_size_samples - offset_into_dest);

    if (offset_into_source > src_size_samples || offset_into_dest > dest_size_samples) {
        // no overlap
        return;
    }

    T *dst = reinterpret_cast<T *>(buffer_to_fill->buffer());
    dst += offset_into_dest;
    T *src = reinterpret_cast<T *>(source_buffer->buffer());
    src += offset_into_source;
    memcpy(dst, src, n_samps_to_copy * sizeof(T));
}

void SubPlayhead::check_if_media_changed(const media::AVFrameID *frame_id) {

    // When the Media or MediaSource for the current frame changes (during playback, scrubbing
    // or on intialisation) we broadcast them up to the parent playhead. Then other things that
    // need to knw about what the on-screen media is (like HUD Plugins or colour pipeline) will
    // recieve this info which is forwarded on by other intermediaries (ViewportFrameQueueActor,
    // GlobalPlayheadEventsActor etc)
    if (frame_id && (frame_id->source_uuid() != current_source_ ||
                     frame_id->media_uuid() != current_media_)) {

        // if current_source_ is null, it means we've had a change event and
        // rebuilt the timeline frames. This could be because the media rate
        // has changed, so we tell the parent playhead to re-fetch the media
        // rate with this flag.
        const bool check_rate = current_source_.is_null();

        current_source_ = frame_id->source_uuid();
        current_media_  = frame_id->media_uuid();
        mail(
            event_atom_v,
            media_source_atom_v,
            utility::UuidActor(
                current_media_, caf::actor_cast<caf::actor>(frame_id->media_addr())),
            utility::UuidActor(
                current_source_, caf::actor_cast<caf::actor>(frame_id->media_source_addr())),
            sub_playhead_index_,
            check_rate)
            .send(parent_);

    } else if (!frame_id && (!current_source_.is_null() || !current_media_.is_null())) {

        current_source_ = utility::Uuid();
        current_media_  = utility::Uuid();
        mail(
            event_atom_v,
            media_source_atom_v,
            utility::UuidActor(),
            utility::UuidActor(),
            sub_playhead_index_,
            false)
            .send(parent_);
    }
}