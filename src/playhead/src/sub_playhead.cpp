// SPDX-License-Identifier: Apache-2.0
#include <chrono>

#include <caf/policy/select_all.hpp>
#include <caf/unit.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace xstudio::colour_pipeline;
using namespace caf;

SubPlayhead::SubPlayhead(
    caf::actor_config &cfg,
    const std::string &name,
    caf::actor source,
    caf::actor parent,
    const timebase::flicks loop_in_point,
    const timebase::flicks loop_out_point,
    const utility::TimeSourceMode time_source_mode,
    const utility::FrameRate override_frame_rate,
    const media::MediaType media_type)
    : caf::event_based_actor(cfg),
      base_(name, "ChildPlayhead"),
      source_(std::move(source)),
      parent_(std::move(parent)),
      loop_in_point_(loop_in_point),
      loop_out_point_(loop_out_point),
      time_source_mode_(time_source_mode),
      override_frame_rate_(override_frame_rate),
      media_type_(media_type) {

    init();
}

void SubPlayhead::init() {

    // if (parent_)
    //    link_to(parent_);

    // get global reader and steal mrm..
    spdlog::debug("Created SubPlayhead {}", base_.name());
    // print_on_exit(this, "SubPlayhead");

    try {

        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
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

    if (!source_) {
        throw std::runtime_error("Creating child playhead actor without a source.");
    }

    monitor(source_);

    set_down_handler([=](down_msg &msg) {
        // is source down?
        if (msg.source == source_) {
            spdlog::debug("ChildPlayhead source down: {}", to_string(msg.reason));
            source_ = caf::actor();
        }
    });

    request(source_, std::chrono::milliseconds(240), utility::get_event_group_atom_v)
        .then(
            [=](caf::actor grp) mutable {
                request(grp, caf::infinite, broadcast::join_broadcast_atom_v)
                    .then(
                        [=](const bool) mutable {},
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const caf::error &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
            });

    // this triggers us to fetch the frames information from the source
    anon_send(this, source_atom_v);

    // this ensures that all pre-cache requests are removed
    set_exit_handler([=](scheduled_actor *a, caf::exit_msg &m) {
        anon_send(pre_reader_, clear_precache_queue_atom_v, base_.uuid());
        default_exit_handler(a, m);
    });

    set_default_handler(
        [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
            //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

            spdlog::warn(
                "Got unwanted messate from {} {}", to_string(current_sender()), to_string(msg));

            return message{};
        });

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

        [=](actual_playback_rate_atom) -> result<utility::FrameRate> {
            auto rp = make_response_promise<utility::FrameRate>();
            request(
                caf::actor_cast<caf::actor>(this), infinite, media::get_media_pointer_atom_v)
                .then(
                    [=](const media::AVFrameID &id) mutable { rp.deliver(id.rate_); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](clear_precache_queue_atom) {
            delegate(pre_reader_, clear_precache_queue_atom_v, base_.uuid());
        },

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {
            // 		if(msg.source == store_events)
            // unsubscribe();
        },

        [=](source_atom) -> result<caf::actor> {
            auto rp = make_response_promise<caf::actor>();
            get_full_timeline_frame_list(rp);
            return rp;
        },

        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &,
            const media::MediaType) {
            anon_send(this, source_atom_v); // triggers refresh of frames_time_list_
        },

        [=](timeline::duration_atom, const timebase::flicks &new_duration) -> result<bool> {
            // request to force a new duration on the source, need to update
            // our full_timeline_frames_ afterwards
            auto rp = make_response_promise<bool>();
            request(source_, infinite, timeline::duration_atom_v, new_duration)
                .then(
                    [=](bool) mutable {
                        request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v)
                            .then(
                                [=](caf::actor) mutable { rp.deliver(true); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](duration_flicks_atom atom) -> result<timebase::flicks> {
            if (up_to_date_) {
                if (full_timeline_frames_.size() < 2) {
                    return timebase::flicks(0);
                }
                return std::chrono::duration_cast<timebase::flicks>(
                    full_timeline_frames_.rbegin()->first -
                    full_timeline_frames_.begin()->first);
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<timebase::flicks>();
            request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v)
                .then(
                    [=](caf::actor) mutable {
                        if (full_timeline_frames_.size() < 2) {
                            rp.deliver(timebase::flicks(0));
                        } else {
                            rp.deliver(std::chrono::duration_cast<timebase::flicks>(
                                full_timeline_frames_.rbegin()->first -
                                full_timeline_frames_.begin()->first));
                        }
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](duration_frames_atom atom) -> result<size_t> {
            if (up_to_date_) {
                return full_timeline_frames_.size() ? full_timeline_frames_.size() - 1 : 0;
            }
            // not up to date, we need to get the timeline frames list from
            // the source
            auto rp = make_response_promise<size_t>();
            request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v)
                .then(
                    [=](caf::actor) mutable {
                        rp.deliver(
                            full_timeline_frames_.size() ? full_timeline_frames_.size() - 1
                                                         : 0);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](flicks_to_logical_frame_atom atom, timebase::flicks flicks) -> int {
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(flicks, frame_period, timeline_pts);
            return frame ? frame->playhead_logical_frame_ : 0;
        },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string & /*path*/,
            const JsonStore &full) mutable {
            try {

                pre_cache_read_ahead_frames_ =
                    preference_value<size_t>(full, "/core/playhead/read_ahead");
                static_cache_delay_milliseconds_ =
                    std::chrono::milliseconds(preference_value<size_t>(
                        full, "/core/playhead/static_cache_delay_milliseconds"));

            } catch (std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        },

        [=](json_store::update_atom, const JsonStore &js) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, js, "", js);
        },

        [=](jump_atom,
            const timebase::flicks flicks,
            const bool forwards,
            const float velocity,
            const bool playing,
            const bool force_updates,
            const bool active_in_ui) -> unit_t {
            // if the playhead has jumped, we set read_ahead_frames_ to zero so
            // that we make a fresh request for the readahead frames that we
            // need
            set_position(flicks, forwards, playing, velocity, force_updates, active_in_ui);
            return unit;
        },

        [=](logical_frame_atom) -> int { return logical_frame_; },

        [=](logical_frame_to_flicks_atom, int logical_frame) -> result<timebase::flicks> {
            if (full_timeline_frames_.size() < 2) {
                return make_error(xstudio_error::error, "No Frames");
            }

            // to get to the last frame, due to the last 'dummy' frame that is appended
            // to account for the duration of the last frame, step back twice from end
            auto last_frame = full_timeline_frames_.end();
            last_frame--;
            last_frame--;

            auto frame = full_timeline_frames_.begin();
            while (logical_frame > 0 && frame != last_frame) {
                frame++;
                logical_frame--;
            }

            auto tp = frame->first;
            if (logical_frame) {
                // if logical_frame goes beyond our last frame then use the
                // duration of the final last frame to extend the result
                auto dummy_last = full_timeline_frames_.end();
                dummy_last--;
                auto last_frame_duration = dummy_last->first - last_frame->first;
                tp += logical_frame * last_frame_duration;
            }

            return tp;
        },

        [=](media_frame_to_flicks_atom,
            const utility::Uuid &media_uuid,
            int logical_media_frame) -> result<timebase::flicks> {
            if (logical_media_frame < 0)
                return make_error(xstudio_error::error, "Out of range");
            // loop over frames until we hit the media item
            auto frame = full_timeline_frames_.begin();
            while (frame != full_timeline_frames_.end()) {
                if (frame->second && frame->second->media_uuid_ == media_uuid) {
                    break;
                }
                frame++;
            }
            if (frame == full_timeline_frames_.end()) {
                return make_error(xstudio_error::error, "Media Not Found");
            }

            // get the time of the first frame for the media we are interested
            // in, in case we don't match with logical_media_frame
            timebase::flicks result = frame->first;

            // now loop over frames for the media item until we match to the logical_media_frame
            while (frame != full_timeline_frames_.end()) {
                if (frame->second && frame->second->media_uuid_ != media_uuid) {
                    return make_error(xstudio_error::error, "Out of range");
                } else if (frame->second && frame->second->frame_ >= logical_media_frame) {
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

        [=](media::get_edit_list_atom _get_edit_list_atom, const Uuid &uuid) {
            delegate(source_, _get_edit_list_atom, media_type_, uuid);
        },

        [=](media::source_offset_frames_atom atom) { delegate(source_, atom); },

        [=](media::source_offset_frames_atom atom, const int offset) -> result<bool> {
            // change the time offset on the source ... this means we have
            // to rebuild full_timeline_frames_ too - so we don't respond until
            // that has been done
            auto rp = make_response_promise<bool>();
            request(source_, infinite, atom, offset)
                .then(
                    [=](bool) mutable {
                        request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v)
                            .then(
                                [=](caf::actor) mutable { rp.deliver(true); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](first_frame_media_pointer_atom) -> result<media::AVFrameID> {
            if (full_timeline_frames_.size()) {
                if (!full_timeline_frames_.begin()->second) {
                    return make_error(xstudio_error::error, "Empty frame");
                }
                return *(full_timeline_frames_.begin()->second);
            }
            return make_error(xstudio_error::error, "No frames");
        },

        [=](last_frame_media_pointer_atom) -> result<media::AVFrameID> {
            if (full_timeline_frames_.size() > 1) {
                // remember the last entry in full_timeline_frames_ is
                // a dummy frame marking the end of the last frame
                auto p = full_timeline_frames_.rbegin();
                p++;
                if (!p->second) {
                    return make_error(xstudio_error::error, "Empty frame");
                }
                return *(p->second);
            }
            return make_error(xstudio_error::error, "No frames");
        },

        [=](media::get_media_pointer_atom) -> result<media::AVFrameID> {
            if (up_to_date_) {
                auto frame = full_timeline_frames_.lower_bound(position_flicks_);
                if (full_timeline_frames_.size() && frame != full_timeline_frames_.end()) {
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
            request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v)
                .then(
                    [=](caf::actor) mutable {
                        auto frame = full_timeline_frames_.lower_bound(position_flicks_);
                        if (full_timeline_frames_.size() &&
                            frame != full_timeline_frames_.end()) {
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
            // built full_timeline_frames_) before we can fetch the media on
            // the current frame
            request(caf::actor_cast<caf::actor>(this), infinite, source_atom_v).then(
                [=](caf::actor) mutable {

                    auto frame = full_timeline_frames_.lower_bound(position_flicks_);
                    caf::actor result;
                    if (frame != full_timeline_frames_.end() && frame->second) {
                        result = caf::actor_cast<caf::actor>(frame->second->actor_addr_);
                    }
                    rp.deliver(result);
                },
                [=](const error &err) mutable { rp.deliver(err); });            
            return rp;
        },

        [=](media_source_atom,
            std::string source_name,
            const media::MediaType mt) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // get the media actor on the current frame
            request(caf::actor_cast<caf::actor>(this), infinite, media_atom_v)
                .then(
                    [=](caf::actor media_actor) mutable {
                        // no media ?
                        if (!media_actor) {
                            rp.deliver(false);
                            return;
                        }

                        // now get it to switched to the named MediaSource
                        request(media_actor, infinite, media_source_atom_v, source_name, mt)
                            .then(

                                [=](bool) mutable {
                                    up_to_date_ = false;
                                    // now ensure we have rebuilt ourselves to reflect the new
                                    // source i.e. we have checked out the new frames_time_list_
                                    request(
                                        caf::actor_cast<caf::actor>(this),
                                        infinite,
                                        source_atom_v)
                                        .then(
                                            [=](caf::actor) mutable { rp.deliver(true); },
                                            [=](const error &err) mutable { rp.deliver(err); });
                                },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_atom) -> result<caf::actor> {

            // MediaActor at current playhead position

            auto rp = make_response_promise<caf::actor>();
            request(caf::actor_cast<caf::actor>(this), infinite, media_source_atom_v).then(
                [=](caf::actor media_source) mutable {
                    if (!media_source)
                        rp.deliver(caf::actor());
                    else {
                        request(media_source, infinite, utility::parent_atom_v)
                            .then(
                                [=](caf::actor media_actor) mutable { rp.deliver(media_actor); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    }

                },
                [=](const error &err) mutable { rp.deliver(err); });            
            return rp;
        },

        [=](media_source_atom, bool) -> utility::Uuid {
            auto frame = full_timeline_frames_.lower_bound(position_flicks_);
            utility::Uuid result;
            if (frame != full_timeline_frames_.end() && frame->second) {
                result = frame->second->media_uuid_;
            }
            return result;
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &uav) {
            send(parent_, utility::event_atom_v, media::add_media_source_atom_v, uav);
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &changes, bool) {
            up_to_date_ = false;
            anon_send(this, source_atom_v); // triggers refresh of frames_time_list_
        },

        [=](media_cache::keys_atom) -> media::MediaKeyVector {
            media::MediaKeyVector result;
            result.reserve(full_timeline_frames_.size());
            for (const auto &pp : full_timeline_frames_) {
                if (pp.second) {
                    result.push_back(pp.second->key_);
                }
            }
            return result;
        },

        [=](bookmark::get_bookmarks_atom) {
            send(
                parent_,
                utility::event_atom_v,
                bookmark::get_bookmarks_atom_v,
                bookmark_ranges_);
        },

        [=](buffer_atom) -> result<ImageBufPtr> {
            auto rp = make_response_promise<ImageBufPtr>();
            timebase::flicks frame_period, timeline_pts;
            std::shared_ptr<const media::AVFrameID> frame =
                get_frame(position_flicks_, frame_period, timeline_pts);

            if (!frame) {
                rp.deliver(ImageBufPtr());
                return rp;
            }
            request(
                pre_reader_,
                std::chrono::milliseconds(5000),
                media_reader::get_image_atom_v,
                *(frame.get()),
                false,
                base_.uuid())
                .then(

                    [=](ImageBufPtr image_buffer) mutable {
                        image_buffer.when_to_display_ = utility::clock::now();
                        image_buffer.set_timline_timestamp(timeline_pts);
                        image_buffer.set_frame_id(*(frame.get()));
                        add_annotations_data_to_frame(image_buffer);

                        if (image_buffer) {
                            image_buffer->params()["playhead_frame"] =
                                frame->playhead_logical_frame_;
                            if (frame->params_.find("HELD_FRAME") != frame->params_.end()) {
                                image_buffer->params()["HELD_FRAME"] = true;
                            } else {
                                image_buffer->params()["HELD_FRAME"] = false;
                            }
                            // image_buffer.colour_pipe_data_ = colour_pipe_data;
                        }
                        rp.deliver(image_buffer);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](media_reader::push_image_atom,
            ImageBufPtr image_buffer,
            const media::AVFrameID &mptr,
            const time_point &tp) { receive_image_from_cache(image_buffer, mptr, tp); },

        [=](playlist::get_media_uuid_atom) -> result<utility::Uuid> {
            auto rp = make_response_promise<utility::Uuid>();
            request(
                caf::actor_cast<caf::actor>(this), infinite, media::get_media_pointer_atom_v)
                .then(
                    [=](const media::AVFrameID &frameid) mutable {
                        rp.deliver(frameid.media_uuid_);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](rate_atom) -> result<utility::FrameRate> {
            auto rp = make_response_promise<utility::FrameRate>();
            request(
                caf::actor_cast<caf::actor>(this), infinite, media::get_media_pointer_atom_v)
                .then(
                    [=](const media::AVFrameID &frameid) mutable { rp.deliver(frameid.rate_); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
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

        [=](skip_through_sources_atom, const int skip_by) {
            // if logical_frame_ sits on source N in an edit list, then this returns
            // the uuid of the source (N+skip_by) in the edit list
            delegate(source_, skip_through_sources_atom_v, skip_by, logical_frame_);
        },

        [=](step_atom,
            timebase::flicks current_playhead_position,
            const int step_frames,
            const bool loop) -> result<timebase::flicks> {
            auto rp = make_response_promise<timebase::flicks>();
            get_position_after_step_by_frames(current_playhead_position, rp, step_frames, loop);
            return rp;
        },

        [=](utility::event_atom, media::source_offset_frames_atom atom, const int offset) {
            // pass up to the main playhead that the offset has changed
            if (parent_)
                anon_send(parent_, atom, this, offset);
        },

        [=](utility::event_atom, utility::change_atom, const bool) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__,
            // to_string(caf::actor_cast<caf::actor>(this)));
            content_changed_ = false;
            up_to_date_      = false;
            anon_send(this, source_atom_v); // triggers refresh of frames_time_list_
        },

        [=](utility::event_atom, utility::change_atom) {
            if (not content_changed_) {
                content_changed_ = true;
                delayed_send(
                    this,
                    std::chrono::milliseconds(250),
                    utility::event_atom_v,
                    change_atom_v,
                    true);
            }
        },

        [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
            if (not content_changed_) {
                content_changed_ = true;
                delayed_send(
                    this,
                    std::chrono::milliseconds(250),
                    utility::event_atom_v,
                    change_atom_v,
                    true);
            }
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
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {
            // this can come from a MediaActor source, for example
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        },
        [=](precache_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            update_playback_precache_requests(rp);
            return rp;
        },
        [=](full_precache_atom, bool start_precache, const bool force) -> result<bool> {
            auto rp = make_response_promise<bool>();
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
                anon_send(this, full_precache_atom_v, true, false);
            } else if (logical_frame != logical_frame_) {
                // otherwise stop any pre cacheing
                precache_start_frame_ = std::numeric_limits<int>::lowest();
            }
        },
        [=](utility::event_atom,
            bookmark::remove_bookmark_atom,
            const utility::Uuid &bookmark_uuid) { bookmark_deleted(bookmark_uuid); },
        [=](utility::event_atom, bookmark::add_bookmark_atom, const utility::UuidActor &n) {
            full_bookmarks_update();
        },
        [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::UuidActor &a) {
            bookmark_changed(a);
        });

    scoped_actor sys{system()};
    try {
        auto session = utility::request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(studio_registry),
            session::session_atom_v);
        auto bookmark_manager =
            utility::request_receive<caf::actor>(*sys, session, bookmark::get_bookmark_atom_v);

        utility::join_event_group(this, bookmark_manager);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}
// move playhead to position
void SubPlayhead::set_position(
    const timebase::flicks time,
    const bool forwards,
    const bool playing,
    const float velocity,
    const bool force_updates,
    const bool active_in_ui) {

    position_flicks_   = time;
    playing_forwards_  = forwards;
    playback_velocity_ = velocity;

    timebase::flicks frame_period, timeline_pts;
    std::shared_ptr<const media::AVFrameID> frame = get_frame(time, frame_period, timeline_pts);
    int logical_frame                             = frame ? frame->playhead_logical_frame_ : 0;

    if (logical_frame_ != logical_frame || force_updates) {

        logical_frame_ = logical_frame;

        auto now = utility::clock::now();

        // get the image from the image readers or cache and also request the
        // next frame so we can do async texture uploads in the viewer
        if (playing || (force_updates && active_in_ui)) {

            // make a blocking request to retrieve the image
            if (media_type_ == media::MediaType::MT_IMAGE) {

                broadcast_image_frame(now, frame, playing, timeline_pts);

            } else if (media_type_ == media::MediaType::MT_AUDIO && playing) {
                broadcast_audio_frame(now, frame, false);
            }

        } else if (frame && (active_in_ui || force_updates)) {

            // The user is scrubbing the timeline, or some other update changing
            // the playhead position, so we do a lazy fetch which won't block
            // us. The reader actor will send the image back to us if/wen it's
            // got it (see 'push_image_atom' handler above). Subsequent requests
            // will override this one if the reader isn't able to keep up.
            if (media_type_ == media::MediaType::MT_IMAGE) {
                media::AVFrameID mptr(*(frame.get()));
                mptr.playhead_logical_frame_ = logical_frame_;
                anon_send(
                    pre_reader_,
                    media_reader::get_image_atom_v,
                    mptr,
                    actor_cast<caf::actor>(this),
                    base_.uuid(),
                    now,
                    logical_frame_);
            }
        }

        // update the parent playhead with our position
        if (frame && (previous_frame_ != frame || force_updates)) {
            anon_send(
                parent_,
                position_atom_v,
                this,
                logical_frame_,
                frame->media_uuid_,
                frame->frame_ - frame->first_frame_,
                frame->frame_,
                frame->rate_,
                frame->timecode_);
        }

        if (!playing && active_in_ui) {
            // this delayed message allows us to kick-off the
            // pre-cache operation *if* the playhead has stopped
            // moving for static_cache_delay_milliseconds_, as we
            // will check in the message handler if the playhead
            // has indeed changed position and if not start the
            // pre-cache
            delayed_anon_send(
                this,
                static_cache_delay_milliseconds_,
                check_logical_frame_changing_atom_v,
                logical_frame_);
        }

        if (playing) {
            if (read_ahead_frames_ < 1 || force_updates) {
                auto rp = make_response_promise<bool>();
                update_playback_precache_requests(rp);
                read_ahead_frames_ = 8;
            } else {
                read_ahead_frames_--;
            }
        } else {
            read_ahead_frames_ = 0;
        }

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
            send(parent_, dropped_frame_atom_v);
            return;
        } else {
            // we're not playing, assumption is that the timeline is being scrubbed
            // and the reader(s) can't keep up. We can't request more frames until
            // the reader has responded or we could build up a big list of pending frame
            // requests. Instead, we'll send a delayed message to the parent playhead
            // to re-broadcast its position to us so that (when the reader is less busy)
            // we can try again to fetch the current frame and broadcast to the viewer
            delayed_anon_send(
                parent_,
                std::chrono::milliseconds(10),
                jump_atom_v,
                caf::actor_cast<caf::actor_addr>(this));
            return;
        }
    }

    if (!frame_media_pointer || frame_media_pointer->is_nil()) {
        // If there is no media pointer, tell the parent playhead that we have
        // no media to show
        send(
            parent_,
            event_atom_v,
            media_source_atom_v,
            caf::actor(),
            caf::actor_cast<caf::actor>(this),
            Uuid(),
            Uuid(),
            0);
        waiting_for_next_frame_ = false;
        return;
    }

    waiting_for_next_frame_ = true;

    request(
        pre_reader_,
        infinite,
        media_reader::get_image_atom_v,
        *(frame_media_pointer.get()),
        false,
        base_.uuid())
        .then(

            [=](ImageBufPtr image_buffer) mutable {
                image_buffer.when_to_display_ = when_to_show_frame;
                image_buffer.set_timline_timestamp(timeline_pts);
                image_buffer.set_frame_id(*(frame_media_pointer.get()));
                add_annotations_data_to_frame(image_buffer);

                if (image_buffer) {
                    image_buffer->params()["playhead_frame"] =
                        frame_media_pointer->playhead_logical_frame_;
                    if (frame_media_pointer->params_.find("HELD_FRAME") !=
                        frame_media_pointer->params_.end()) {
                        image_buffer->params()["HELD_FRAME"] = true;
                    } else {
                        image_buffer->params()["HELD_FRAME"] = false;
                    }
                    // image_buffer.colour_pipe_data_ = colour_pipe_data;
                }

                send(
                    parent_,
                    show_atom_v,
                    base_.uuid(), // the uuid of this playhead
                    image_buffer, // the image
                    true          // is this the frame that should be on-screen now?
                );

                auto m = caf::actor_cast<caf::actor>(frame_media_pointer->actor_addr_);
                if (m) {
                    send(
                        parent_,
                        event_atom_v,
                        media_source_atom_v,
                        m,
                        actor_cast<actor>(this),
                        frame_media_pointer->media_uuid_,
                        frame_media_pointer->source_uuid_,
                        frame_media_pointer->frame_);
                }

                waiting_for_next_frame_ = false;

                // We have got the frame that we want for *immediate* display,
                // now we also want to fetch the next N frames to allow the
                // viewport to upload pixel data to the GPU for subsequent
                // re-draws during playback
                if (playing)
                    request_future_frames();
            },

            [=](const caf::error &err) mutable {
                waiting_for_next_frame_ = false;
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::broadcast_audio_frame(
    const utility::time_point /*when_to_show_frame*/,
    std::shared_ptr<const media::AVFrameID> /*frame_media_pointer*/,
    const bool /*is_future_frame*/) {

    media::AVFrameIDsAndTimePoints future_frames;
    get_lookahead_frame_pointers(future_frames, 50);

    // now fetch audio samples for playback
    request(
        pre_reader_,
        std::chrono::milliseconds(5000),
        media_reader::get_audio_atom_v,
        future_frames,
        base_.uuid())
        .then(

            [=](const std::vector<AudioBufPtr> &audio_buffers) mutable {
                send(
                    parent_,
                    sound_audio_atom_v,
                    base_.uuid(), // the uuid of this playhead
                    audio_buffers);
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

std::vector<timebase::flicks> SubPlayhead::get_lookahead_frame_pointers(
    media::AVFrameIDsAndTimePoints &result, const int max_num_frames) {

    std::vector<timebase::flicks> tps;
    if (full_timeline_frames_.size() < 2) {
        return tps;
    }

    timebase::flicks current_frame_tp =
        std::min(out_frame_->first, std::max(in_frame_->first, position_flicks_));

    auto frame = full_timeline_frames_.upper_bound(current_frame_tp);
    if (frame != full_timeline_frames_.begin())
        frame--;

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

        if (frame->second && !frame->second->source_uuid_.is_null()) {
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

    request(
        pre_reader_,
        std::chrono::milliseconds(5000),
        media_reader::get_future_frames_atom_v,
        future_frames,
        base_.uuid())
        .then(

            [=](std::vector<ImageBufPtr> image_buffers) mutable {
                auto tp   = timeline_pts_vec.begin();
                auto idsp = future_frames.begin();
                for (auto &imbuf : image_buffers) {
                    imbuf.set_timline_timestamp(*(tp++));
                    std::shared_ptr<const media::AVFrameID> av_idx = (idsp++)->second;
                    if (av_idx) {
                        imbuf.set_frame_id(*(av_idx.get()));
                        add_annotations_data_to_frame(imbuf);
                    }
                }
                send(
                    parent_,
                    show_atom_v,
                    base_.uuid(), // the uuid of this playhead
                    image_buffers);
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::update_playback_precache_requests(caf::typed_response_promise<bool> &rp) {

    auto tt = utility::clock::now();

    // get the AVFrameID iterator for the current frame
    media::AVFrameIDsAndTimePoints requests;
    get_lookahead_frame_pointers(requests, pre_cache_read_ahead_frames_);

    make_prefetch_requests_for_colour_pipeline(requests);

    request(
        pre_reader_, infinite, media_reader::playback_precache_atom_v, requests, base_.uuid())
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

        request(
            pre_reader_, infinite, media_reader::static_precache_atom_v, requests, base_.uuid())
            .await(
                [=](bool requests_processed) mutable { rp.deliver(requests_processed); },
                [=](const error &err) mutable { rp.deliver(err); });

    } else {
        request(
            pre_reader_,
            infinite,
            media_reader::static_precache_atom_v,
            media::AVFrameIDsAndTimePoints(),
            base_.uuid())
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
        if (r.second->source_uuid_ != curr_uuid) {
            frame_ids_for_colour_precompute_frame_ids.push_back(r);
            curr_uuid = r.second->source_uuid_;
        }
    }

    send(parent_, colour_pipeline_lookahead_atom_v, frame_ids_for_colour_precompute_frame_ids);
}


void SubPlayhead::receive_image_from_cache(
    ImageBufPtr image_buffer, const media::AVFrameID mptr, const time_point tp) {

    // are we receiving an image from the cache that was requested *before* the previous
    // image we received, i.e. it's being sent from the readers out of order?
    // If so, skip it.
    if (last_image_timepoint_ > tp)
        return;
    last_image_timepoint_ = tp;

    if (image_buffer) {
        image_buffer->params()["playhead_frame"] = mptr.playhead_logical_frame_;
        if (mptr.params_.find("HELD_FRAME") != mptr.params_.end()) {
            image_buffer->params()["HELD_FRAME"] = true;
        } else {
            image_buffer->params()["HELD_FRAME"] = false;
        }
    }

    image_buffer.when_to_display_ = utility::clock::now();
    if (mptr.playhead_logical_frame_ < (int)full_timeline_frames_.size()) {
        auto p = full_timeline_frames_.begin();
        std::advance(p, mptr.playhead_logical_frame_);
        image_buffer.set_timline_timestamp(p->first);
    } else {
        image_buffer.set_timline_timestamp(position_flicks_);
    }
    image_buffer.set_frame_id(mptr);
    add_annotations_data_to_frame(image_buffer);

    send(
        parent_,
        show_atom_v,
        base_.uuid(), // the uuid of this playhead
        image_buffer, // the image
        true          // this image supposed to be shown on-screen NOW
    );

    if (auto m = caf::actor_cast<caf::actor>(mptr.actor_addr_)) {
        send(
            parent_,
            event_atom_v,
            media_source_atom_v,
            m,
            actor_cast<actor>(this),
            mptr.media_uuid_,
            mptr.source_uuid_,
            mptr.frame_);
    }
}

void SubPlayhead::get_full_timeline_frame_list(caf::typed_response_promise<caf::actor> rp) {

    if (up_to_date_) {
        rp.deliver(source_);
        return;
    }
    // Note that the 'await' is important here ... there are a number of
    // message handlers that require full_timeline_frames_ to be up-to-date
    // so we don't want those to be available until this has completed
    request(
        source_,
        infinite,
        media::get_media_pointers_atom_v,
        media_type_,
        time_source_mode_,
        override_frame_rate_)
        .await(
            [=](const media::FrameTimeMap &mpts) mutable {
                full_timeline_frames_ = mpts;

                if (full_timeline_frames_.size() && full_timeline_frames_.rbegin()->second) {
                    // the logic here is crucial ... full_timeline_frames_ is used to
                    // evaluate the full duration of what's being played. We need to drop
                    // in an empty frame at the end, with a timestamp that matches the
                    // point just *after* the last frame's timestamp plus its duration.
                    // Thus, for a single frame sourc that is 24pfs, say, we will have
                    // two entries in full_timeline_frames_ ... one entry a t=0that is
                    // the frame. The second is a nullptr at t = 1.0/24.0s.
                    //
                    // We test if the last frame is empty in case our source has already
                    // taken care of this for us.
                    auto last_frame_timepoint = full_timeline_frames_.rbegin()->first;
                    last_frame_timepoint += time_source_mode_ == TimeSourceMode::FIXED
                                                ? override_frame_rate_
                                                : full_timeline_frames_.rbegin()->second->rate_;
                    full_timeline_frames_[last_frame_timepoint].reset();
                }


                // int logical_frame = 0;
                all_media_uuids_.clear();
                utility::Uuid media_uuid;
                for (const auto &f : full_timeline_frames_) {
                    // f.second->playhead_logical_frame_ = logical_frame++;
                    if (f.second && f.second->media_uuid_ != media_uuid) {
                        media_uuid = f.second->media_uuid_;
                        all_media_uuids_.insert(media_uuid);
                    } else if (!f.second)
                        media_uuid = utility::Uuid();
                }

                set_in_and_out_frames();

                full_bookmarks_update();

                // our data has changed (full_timeline_frames_ describes most)
                // things that are important about the timeline, so send change
                // notification
                send(
                    event_group_,
                    utility::event_atom_v,
                    utility::change_atom_v,
                    actor_cast<actor>(this));

                rp.deliver(source_);
                up_to_date_ = true;
            },
            [=](const error &err) mutable { rp.deliver(err); });
}

std::shared_ptr<const media::AVFrameID> SubPlayhead::get_frame(
    const timebase::flicks &time,
    timebase::flicks &frame_period,
    timebase::flicks &timeline_pts) {

    // If rate = 25fps (40ms per frame) ...
    // if first frame is shown at time = 0, second frame is shown at time = 40ms.
    // Therefore if time = 39ms then we return first frame
    //
    // Also, note there will be an extra null pointer at the end of
    // full_timeline_frames_ corresponding to last_frame + duration of last frame.
    // So if we have a source with only one frame full_timeline_frames_
    // would look like this:
    // {
    //     {0ms, media::AVFrameID(first frame)},
    //     {40ms, media::AVFrameID(null frame)}
    // }
    if (full_timeline_frames_.size() < 2) {
        // and give the others values something valid ???
        frame_period = timebase::k_flicks_zero_seconds;
        timeline_pts = timebase::k_flicks_zero_seconds;
        return std::shared_ptr<media::AVFrameID>();
    }

    timebase::flicks t = std::min(last_frame_->first, std::max(first_frame_->first, time));

    // get the frame to be show *after* time point t and decrement to get our
    // frame.
    auto tt    = utility::clock::now();
    auto frame = full_timeline_frames_.upper_bound(t);
    if (frame != full_timeline_frames_.begin())
        frame--;

    // see above, there is a dummy frame at the end of full_timeline_frames_ so
    // this is always valid:
    auto next_frame = frame;
    next_frame++;
    frame_period = next_frame->first - frame->first;
    timeline_pts = frame->first;

    return frame->second;
}

void SubPlayhead::get_position_after_step_by_frames(
    const timebase::flicks start_position,
    caf::typed_response_promise<timebase::flicks> &rp,
    int step_frames,
    const bool loop) {

    if (full_timeline_frames_.size() < 2) {
        rp.deliver(make_error(xstudio_error::error, "No Frames"));
        return;
    }

    timebase::flicks t =
        std::min(out_frame_->first, std::max(in_frame_->first, start_position));

    auto frame = full_timeline_frames_.upper_bound(t);
    if (frame != full_timeline_frames_.begin())
        frame--;

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

void SubPlayhead::set_in_and_out_frames() {

    if (full_timeline_frames_.size() < 2) {
        out_frame_   = full_timeline_frames_.begin();
        in_frame_    = full_timeline_frames_.begin();
        last_frame_  = full_timeline_frames_.begin();
        first_frame_ = full_timeline_frames_.begin();
        return;
    }

    // to get to the last frame, due to the last 'dummy' frame that is appended
    // to account for the duration of the last frame, step back twice from end
    last_frame_ = full_timeline_frames_.end();
    last_frame_--;
    last_frame_--;
    first_frame_ = full_timeline_frames_.begin();

    if (loop_out_point_ > last_frame_->first) {
        out_frame_ = last_frame_;
    } else {
        out_frame_ = full_timeline_frames_.upper_bound(loop_out_point_);
        if (out_frame_ != first_frame_)
            out_frame_--;
    }

    if (loop_in_point_ > out_frame_->first) {
        in_frame_ = out_frame_;
    } else {
        in_frame_ = full_timeline_frames_.upper_bound(loop_in_point_);
        if (in_frame_ != first_frame_)
            in_frame_--;
    }
}

void SubPlayhead::full_bookmarks_update() {

    // the goal here is to work out which frames are bookmarked and make
    // a list of each bookmark and its frame range (in the playhead timeline).
    // Note that the same bookmark can appear twice in the case where the same
    // piece of media appears twice in a timeline, say

    if (all_media_uuids_.empty()) {
        fetch_bookmark_annotations(BookmarkRanges());
    }

    auto global = system().registry().template get<caf::actor>(global_registry);
    request(global, infinite, bookmark::get_bookmark_atom_v)
        .then(
            [=](caf::actor bookmarks_manager) {
                // here we get all bookmarks that match any and all of the media
                // that appear in our timline
                request(
                    bookmarks_manager,
                    infinite,
                    bookmark::bookmark_detail_atom_v,
                    all_media_uuids_)
                    .then(
                        [=](const std::vector<bookmark::BookmarkDetail> &bookmark_details) {
                            // make a map of the bookmarks against the uuid of the media
                            // that owns the bookmark
                            std::map<utility::Uuid, std::vector<bookmark::BookmarkDetail>>
                                bookmarks;

                            BookmarkRanges result;

                            if (bookmark_details.empty()) {
                                fetch_bookmark_annotations(result);
                            };

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
                            for (const auto &f : full_timeline_frames_) {

                                // check if media changed and if so are there bookmarks?
                                if (f.second && f.second->media_uuid_ != curr_media_uuid) {
                                    curr_media_uuid = f.second->media_uuid_;
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
                                        f.second->frame_ - f.second->first_frame_;
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

                            fetch_bookmark_annotations(result);
                        },
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
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

void SubPlayhead::fetch_bookmark_annotations(BookmarkRanges bookmark_ranges) {
    if (!bookmark_ranges.size()) {
        bookmark_ranges_.clear();
        bookmarks_.clear();
        send(parent_, utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_ranges_);
        return;
    }
    utility::UuidList bookmark_ids;
    for (const auto &p : bookmark_ranges) {
        bookmark_ids.push_back(std::get<0>(p));
    }

    // first we need to get to the 'bookmarks_manager'
    auto global = system().registry().template get<caf::actor>(global_registry);
    request(global, infinite, bookmark::get_bookmark_atom_v)
        .then(
            [=](caf::actor bookmarks_manager) {
                // get the bookmark actors for bookmarks that are in our timline
                request(
                    bookmarks_manager, infinite, bookmark::get_bookmark_atom_v, bookmark_ids)
                    .then(
                        [=](const std::vector<UuidActor> &bookmarks) {
                            // now we are ready to build our vector of bookmark, annotations and
                            // associated logical frame ranges
                            auto result =
                                std::shared_ptr<xstudio::bookmark::BookmarkAndAnnotations>(
                                    new xstudio::bookmark::BookmarkAndAnnotations);
                            auto count = std::make_shared<int>(bookmarks.size());

                            for (auto bookmark : bookmarks) {

                                // now ask the bookmark actor for its detail and
                                // annotation data (if any)
                                request(
                                    bookmark.actor(),
                                    infinite,
                                    bookmark::bookmark_detail_atom_v,
                                    bookmark::get_annotation_atom_v)
                                    .then(
                                        [=](bookmark::BookmarkAndAnnotationPtr data) mutable {
                                            for (const auto &p : bookmark_ranges) {
                                                if (data->detail_.uuid_ == std::get<0>(p)) {
                                                    // set the frame ranges. Note this
                                                    // const_cast is safe because this shared
                                                    // ptr has not been shared with anyone yet.
                                                    auto d = const_cast<
                                                        bookmark::BookmarkAndAnnotation *>(
                                                        data.get());
                                                    d->start_frame_ = std::get<2>(p);
                                                    d->end_frame_   = std::get<3>(p);
                                                    result->emplace_back(data);
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
                                                send(
                                                    parent_,
                                                    utility::event_atom_v,
                                                    bookmark::get_bookmarks_atom_v,
                                                    bookmark_ranges_);
                                            }
                                        },
                                        [=](const error &err) mutable {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        });
                            }
                        },
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void SubPlayhead::add_annotations_data_to_frame(ImageBufPtr &frame) {

    xstudio::bookmark::BookmarkAndAnnotations bookmarks;
    int logical_frame = frame.frame_id().playhead_logical_frame_;
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
    auto p = bookmarks_.begin();
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

    if (n != bookmark_ranges_.size()) {
        send(parent_, utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_ranges_);
    }
}

void SubPlayhead::bookmark_changed(const utility::UuidActor bookmark) {

    // if a bookmark has changed, and its a bookmar that is in our timleine,
    // do a full rebuild to make sure we're fully up to date.
    for (auto &p : bookmarks_) {
        if (p->detail_.uuid_ == bookmark.uuid()) {
            full_bookmarks_update();
            return;
        }
    }

    // even though this doesn't look like our bookmark, the change that has
    // happened to it might have been associating it with media that IS in
    // our timeline, in which case we need to rebuild our bookmarks data
    request(bookmark.actor(), infinite, bookmark::bookmark_detail_atom_v)
        .then(
            [=](const bookmark::BookmarkDetail &detail) {
                if (detail.owner_) {
                    auto p = std::find(
                        all_media_uuids_.begin(),
                        all_media_uuids_.end(),
                        (*(detail.owner_)).uuid());
                    if (p != all_media_uuids_.end()) {
                        full_bookmarks_update();
                    }
                }
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}
