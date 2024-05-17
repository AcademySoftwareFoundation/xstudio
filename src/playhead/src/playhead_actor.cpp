// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <limits>

#include <caf/policy/select_all.hpp>
#include <caf/unit.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/edit_list_actor.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/retime_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace caf;

static caf::actor media_actor_;

/* Simple one-trick actor that enacts the playback loop. */
class PlayLoopActor : public caf::event_based_actor {
  public:
    PlayLoopActor(caf::actor_config &cfg, caf::actor playhead)
        : caf::event_based_actor(cfg), playhead_(playhead) {

        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](step_atom) {
                // step the playhead
                request(playhead, infinite, step_atom_v)
                    .then(
                        [=](const PlayheadBase::OptionalTimePoint &tp) {
                            if (tp) {
                                // playhead is still going, trigger the next step by sending a
                                // step atom to ourselves
                                if (tp > utility::clock::now()) {
                                    scheduled_anon_send(
                                        caf::actor_cast<caf::actor>(this), *tp, step_atom_v);
                                } else {
                                    // I *think* a scheduled send to a time point in the past
                                    // will not get sent, hence we brute force continue in case
                                    // there has been some delays in the mailbox
                                    anon_send(caf::actor_cast<caf::actor>(this), step_atom_v);
                                }
                            } else {
                                send_exit(this, caf::exit_reason::user_shutdown);
                            }
                        },
                        [=](const caf::error &e) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
                        });
            });
        delayed_anon_send(this, std::chrono::milliseconds(5), step_atom_v);
    }

    ~PlayLoopActor() override = default;

    caf::behavior behavior_;
    caf::actor playhead_;

    caf::behavior make_behavior() override { return behavior_; }
};

PlayheadActor::PlayheadActor(
    caf::actor_config &cfg, const utility::JsonStore &jsn, caf::actor playlist_selection)
    : caf::event_based_actor(cfg), PlayheadBase(static_cast<utility::JsonStore>(jsn["base"])) {
    init();
    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
    connect_to_playlist_selection_actor(playlist_selection);

    // for every attribute we expose it in frontend model data, where the id
    // of the model data set is the uuid of the module here. This means if we have
    // the uuid of a module at the frontend we can get to any and all of its
    // attribute data if/when we need to. For example, this is how we get to
    // the Playhead attribute data in the frontend qml code ... the Playhead
    // Uuid is published by the parent playlist/subset/timeline in the main
    // SessionModel - we use this to connect to the model data of a given
    // Playhead so we can talk to the Playhead of the 'current' timeline, subset,
    // or playlist
    for (auto &attr : attributes_) {
        expose_attribute_in_model_data(
            attr.get(), std::string("{") + to_string(Module::uuid()) + std::string("}"), true);
    }
}

PlayheadActor::PlayheadActor(
    caf::actor_config &cfg,
    const std::string &name,
    caf::actor playlist_selection,
    const utility::Uuid uuid)
    : caf::event_based_actor(cfg), PlayheadBase(name, std::move(uuid)) {

    init();
    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
    connect_to_playlist_selection_actor(playlist_selection);

    // see comment in other constructor above
    for (auto &attr : attributes_) {
        expose_attribute_in_model_data(
            attr.get(), std::string("{") + to_string(Module::uuid()) + std::string("}"), true);
    }
}

void PlayheadActor::init() {
    // get global reader and steal mrm..
    spdlog::debug("Created PlayheadActor {}", name());
    print_on_exit(this, name());

    set_exit_handler([=](scheduled_actor *a, caf::exit_msg &m) {
        disconnect_from_ui();
        audio_output_actor_ = caf::actor();
        empty_clip_         = caf::actor();
        image_cache_        = caf::actor();
        key_playhead_       = caf::actor();
        pre_reader_         = caf::actor();
        sub_playheads_.clear();
        source_wrappers_.clear();
        source_actors_.clear();
        default_exit_handler(a, m);
    });


    set_down_handler([=](down_msg &msg) {
        /*if (msg.source == playlist_selection_) {
            demonitor(playlist_selection_);
            playlist_selection_ = caf::actor();
            leave_broadcast(this, source_events_);
            send(this, utility::event_atom_v, utility::change_atom_v);
        } else {*/
        // child playhead kills itself if its source goes down
        for (auto &ph : sub_playheads_) {
            if (ph == msg.source) {
                anon_send(this, utility::event_atom_v, utility::change_atom_v);
            }
        }
    });

    try {

        caf::scoped_actor sys(system());

        // join cache events
        image_cache_ = system().registry().template get<caf::actor>(image_cache_registry);
        if (image_cache_) {
            auto grp = request_receive<caf::actor>(
                *sys, image_cache_, utility::get_event_group_atom_v);
            request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, this);
        }

        auto audio_cache = system().registry().template get<caf::actor>(audio_cache_registry);
        if (audio_cache) {
            auto grp =
                request_receive<caf::actor>(*sys, audio_cache, utility::get_event_group_atom_v);
            request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, this);
        }
    } catch (...) {
    }

    broadcast_                   = spawn<broadcast::BroadcastActor>(this);
    event_group_                 = spawn<broadcast::BroadcastActor>(this);
    fps_moniotor_group_          = spawn<broadcast::BroadcastActor>(this);
    viewport_events_group_       = spawn<broadcast::BroadcastActor>(this);
    playhead_media_events_group_ = spawn<broadcast::BroadcastActor>(this);

    link_to(broadcast_);
    link_to(event_group_);
    link_to(fps_moniotor_group_);
    link_to(viewport_events_group_);
    link_to(playhead_media_events_group_);

    try {

        scoped_actor sys{system()};

        // Here's we join the viewport refraw group of the global module events actor. Thus,
        // a component like the colour pipeline can ensure the viewport is redrawn
        auto attrs_events_actor =
            system().registry().template get<caf::actor>(module_events_registry);
        auto group = request_receive<caf::actor>(
            *sys, attrs_events_actor, module::redraw_viewport_group_atom_v);
        request_receive<bool>(*sys, group, broadcast::join_broadcast_atom_v, this);

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    // ensure we have a source and a child playhead, due to many messages
    // delagated to the child playhead
    empty_clip_ =
        spawn<EditListActor>("EmptyClip blank", std::vector<caf::actor>(), media::MT_IMAGE);
    link_to(empty_clip_);
    make_child_playhead(empty_clip_);
    switch_key_playhead(0);

    audio_output_actor_ = system().registry().template get<caf::actor>(audio_output_registry);
    playhead_events_actor_ =
        system().registry().template get<caf::actor>(global_playhead_events_actor);

    pre_reader_ = system().registry().template get<caf::actor>(media_reader_registry);

    behavior_.assign(
        make_set_name_handler(event_group_, this),
        make_get_name_handler(),
        make_last_changed_getter(),
        make_last_changed_setter(event_group_, this),
        make_last_changed_event_handler(event_group_, this),
        make_get_uuid_handler(),
        make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        make_get_detail_handler(this, event_group_),

        [=](actual_playback_rate_atom atom) { delegate(key_playhead_, atom); },

        [=](clear_precache_requests_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            clear_all_precache_requests(rp);
            return rp;
        },

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {
            // 		if(msg.source == store_events)
            // unsubscribe();
        },

        [=](dropped_frame_atom) { throttle(); },

        [=](fps_event_group_atom) -> caf::actor { return fps_moniotor_group_; },

        [=](viewport_events_group_atom) -> caf::actor { return viewport_events_group_; },

        /* move all child playheads to current position */
        [=](jump_atom) { update_child_playhead_positions(true); },

        [=](jump_atom, caf::actor_addr sub_playhead_addr) {
            // this is a request made by a sub playhead to get the updated
            // playhead position - it does this during scrubbing when it
            // has to skip a frame broadcast because the reader can't keep
            // up with the speed that the playhead is being moved at. If it
            // skips the frame but the user happend to have just stopped
            // scrubbing, we need to re-request the frame until the reader
            // finally responds and we do this with using the 'jump' atom
            // in a loop between sub-playhead and parent playhead like this:
            auto sub_playhead = caf::actor_cast<caf::actor>(sub_playhead_addr);
            for (const auto &i : sub_playheads_) {
                if (i == sub_playhead) {
                    anon_send(
                        i,
                        jump_atom_v,
                        position(),
                        forward(),
                        velocity(),
                        playing(),
                        true,
                        connected_to_ui());
                }
            }
        },

        [=](jump_atom, const int frame) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // by requesting duration from self we ensure that we have updated
            // internal data about source duration, incase this jump message
            // has come immediately after a change in the source, for example
            request(caf::actor_cast<caf::actor>(this), infinite, duration_flicks_atom_v)
                .then(
                    [=](const timebase::flicks duration) mutable {
                        if (duration != timebase::k_flicks_zero_seconds) {
                            request(
                                key_playhead_, infinite, logical_frame_to_flicks_atom_v, frame)
                                .then(

                                    [=](const timebase::flicks flicks) mutable {
                                        set_position(flicks);
                                        update_child_playhead_positions(true);
                                        rp.deliver(true);
                                    },
                                    [=](const error &err) mutable { rp.deliver(err); });
                        } else {
                            set_position(timebase::k_flicks_zero_seconds);
                            rp.deliver(false);
                        }
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](scrub_frame_atom, const int frame) {
            request(key_playhead_, infinite, logical_frame_to_flicks_atom_v, frame)
                .then(

                    [=](const timebase::flicks flicks) {
                        set_position(flicks);
                        update_child_playhead_positions(true, true);
                    },
                    [=](const error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](scrub_frame_atom, const timebase::flicks flicks) {
            set_position(flicks);
            update_child_playhead_positions(true, true);
        },

        [=](jump_atom, const timebase::flicks flicks) -> unit_t {
            set_position(flicks);
            update_child_playhead_positions(true);
            return unit;
        },

        [=](jump_atom, const utility::Uuid &media_uuid) { jump_to_source(media_uuid); },

        [=](key_child_playhead_atom) {
            // fetch the Uuid of the key_child_playehad
            delegate(key_playhead_, uuid_atom_v);
        },

        [=](key_playhead_index_atom) -> int {
            for (size_t i = 0; i < sub_playheads_.size(); ++i) {
                if (sub_playheads_[i] == key_playhead_)
                    return (int)i;
            }
            return -1;
        },

        [=](key_playhead_index_atom, int idx) {
            try {
                switch_key_playhead(idx);
            } catch (std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](logical_frame_atom atom) { delegate(key_playhead_, atom); },

        [=](loop_atom) -> LoopMode { return playhead::LoopMode(loop()); },

        [=](loop_atom, const LoopMode loop) -> unit_t {
            set_loop(loop);
            notify_loop_end_changed();
            notify_loop_start_changed();
            send(event_group_, utility::event_atom_v, loop_atom_v, loop);
            return unit;
        },

        [=](media::source_offset_frames_atom atom) { delegate(key_playhead_, atom); },

        [=](media::source_offset_frames_atom atom, caf::actor sub_playhead, const int offset) {
            // pass up to the main playhead that the offset has changed
            if (sub_playhead == key_playhead_) {
                send(event_group_, utility::event_atom_v, atom, offset);

                // the cached frames display might need updating
                rebuild_cached_frames_status();
            }
        },

        [=](media::source_offset_frames_atom atom, const int offset) {
            send(key_playhead_, atom, offset);
            update_child_playhead_positions(true);
        },

        [=](media_events_group_atom) -> caf::actor { return playhead_media_events_group_; },

        [=](media_atom atom) { delegate(key_playhead_, atom); },

        [=](media_source_atom atom) { delegate(key_playhead_, atom); },

        [=](media_cache::cached_frames_atom) {
            send(
                event_group_,
                utility::event_atom_v,
                media_cache::cached_frames_atom_v,
                cached_frames_ranges_);
        },

        [=](bookmark::get_bookmarks_atom) {
            send(
                event_group_,
                utility::event_atom_v,
                bookmark::get_bookmarks_atom_v,
                bookmark_frames_ranges_);

            send(
                playhead_media_events_group_,
                utility::event_atom_v,
                bookmark::get_bookmarks_atom_v,
                bookmark_frames_ranges_);
        },

        [=](utility::event_atom,
            bookmark::get_bookmarks_atom,
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                &bookmark_ranges) {
            if (caf::actor_cast<caf::actor>(current_sender()) == key_playhead_) {
                bookmark_frames_ranges_ = bookmark_ranges;
                send(
                    event_group_,
                    utility::event_atom_v,
                    bookmark::get_bookmarks_atom_v,
                    bookmark_frames_ranges_);

                send(
                    playhead_media_events_group_,
                    utility::event_atom_v,
                    bookmark::get_bookmarks_atom_v,
                    bookmark_frames_ranges_);
            }
        },

        [=](media_cache::keys_atom atom) { delegate(key_playhead_, atom); },

        [=](play_atom) -> bool { return playing(); },

        [=](play_atom, const bool _play) -> unit_t {
            set_playing(_play);
            return unit;
        },

        [=](play_forward_atom) -> bool { return forward(); },

        [=](play_forward_atom, const bool forward) -> unit_t {
            set_forward(forward);
            update_child_playhead_positions(true);
            send(event_group_, utility::event_atom_v, play_forward_atom_v, forward);
            send(fps_moniotor_group_, utility::event_atom_v, play_forward_atom_v, forward);
            return unit;
        },

        [=](play_rate_mode_atom) -> utility::TimeSourceMode { return play_rate_mode(); },

        [=](play_rate_mode_atom, const utility::TimeSourceMode play_rate_mode) -> result<bool> {
            auto rp = make_response_promise<bool>();
            set_play_rate_mode(play_rate_mode);
            send(event_group_, utility::event_atom_v, play_rate_mode_atom_v, play_rate_mode);
            // this will ensure the duration is updated before we deliver the response
            request(caf::actor_cast<caf::actor>(this), infinite, duration_flicks_atom_v)
                .then(
                    [=](timebase::flicks) mutable { rp.deliver(true); },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](playhead::duration_flicks_atom) -> result<timebase::flicks> {
            auto rp = make_response_promise<timebase::flicks>();
            update_duration(rp);
            return rp;
        },

        [=](playhead::duration_frames_atom) {
            // spdlog::warn("playhead delegate duration_frames_atom {}",
            // to_string(key_playhead_));
            delegate(key_playhead_, duration_frames_atom_v);
        },

        [=](playhead::skip_through_sources_atom, const int skip_by) -> bool {
            if (source_actors_.size() <= 1)
                return false;
            skip_through_sources(skip_by);
            return true;
        },

        [=](playhead_rate_atom) -> FrameRate { return playhead_rate(); },

        [=](playhead_rate_atom, const FrameRate &rate) -> result<bool> {
            auto rp = make_response_promise<bool>();
            set_playhead_rate(rate);
            send(event_group_, utility::event_atom_v, playhead_rate_atom_v, rate);
            // this will ensure the duration is updated before we deliver the response
            request(caf::actor_cast<caf::actor>(this), infinite, duration_flicks_atom_v)
                .then(
                    [=](timebase::flicks) mutable { rp.deliver(true); },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &) { current_media_changed(media_actor_, true); },

        [=](playlist::select_media_atom, const UuidList &selection) {
            delegate(playlist_selection_, playlist::select_media_atom_v, selection);
        },

        [=](playhead::position_atom,
            const utility::time_point next_video_refresh,
            const timebase::flicks video_refresh_period) -> result<timebase::flicks> {
            // this message is sent from the ViewportFrameQueueActor on
            // every redraw - it wants
            // to know where the playhead will be at the time 'next_video_refresh'
            // so it can pick which image to draw (the playhead has already sent
            // a few frames ahead of time, with display timestamps).

            // we're not moving - just return our position.
            if (!playing()) {
                auto r = position();
                return r;
            }

            // predict where we will be at the timepoint 'next_video_refresh' ...
            const timebase::flicks delta = std::chrono::duration_cast<timebase::flicks>(
                next_video_refresh - utility::clock::now());
            const double v = (forward() ? 1.0f : -1.0f) * velocity() * velocity_multiplier();
            const timebase::flicks estimated_playhead_position =
                timebase::to_flicks(v * timebase::to_seconds(delta)) + position();
            const timebase::flicks clamped_estimated_playhead_position =
                clamp_timepoint_to_loop_range(estimated_playhead_position);

            return clamped_estimated_playhead_position;
        },

        [=](media_logical_frame_atom) -> int { return playhead_media_logical_frame_->value(); },

        [=](media_frame_atom) -> int { return playhead_media_frame_->value(); },

        [=](position_atom,
            actor child,
            const int logical_frame,
            const utility::Uuid &source_uuid,
            const int media_frame,
            const int media_logical_frame,
            const utility::FrameRate &media_rate,
            const utility::Timecode &tc) {
            // handles incoming notification from a child playhead that their
            // logical frame has changed
            if (child == key_playhead_) {

                playhead_logical_frame_->set_value(logical_frame, false);
                playhead_media_logical_frame_->set_value(media_logical_frame, false);
                current_source_frame_timecode_->set_value(to_string(tc), false);
                playhead_media_frame_->set_value(media_frame, false);

                send(
                    playhead_media_events_group_,
                    utility::event_atom_v,
                    playhead::position_atom_v,
                    position(),
                    logical_frame,
                    media_frame,
                    media_logical_frame,
                    tc);

                media_frame_per_media_uuid_[source_uuid] = media_logical_frame;
                send(
                    event_group_,
                    utility::event_atom_v,
                    position_atom_v,
                    logical_frame,
                    media_frame,
                    media_logical_frame,
                    media_rate,
                    min(position(), duration() - playhead_rate()),
                    tc);
            }
        },

        [=](redraw_viewport_atom) {
            // force re-fetch of current frame and push to viewport for redraw,
            // unless we're playing.
            if (not playing() && connected_to_ui()) {
                update_child_playhead_positions(true);
            }
        },

        // child playhead is broadcasting a new audio buffer
        [=](sound_audio_atom,
            const Uuid &child_playhead_uuid,
            const std::vector<AudioBufPtr> &audio_buffers) {
            anon_send(
                audio_output_actor_,
                sound_audio_atom_v,
                audio_buffers,
                child_playhead_uuid,
                playing(),
                forward(),
                velocity());
        },

        // child playhead is broadcasting a new buffer
        [=](show_atom,
            const Uuid &child_playhead_uuid,
            ImageBufPtr buf,
            const bool is_onscreen_frame) {
            const auto delay = std::chrono::duration_cast<timebase::flicks>(
                utility::clock::now() - buf.when_to_display_);

            /* If the image is more than 2 frames late, we assume that the image reader
            can't keep up so we start slowing the playhead down until frames start arriving on
            time*/
            if (playing() && delay > effective_frame_period() * 2) {
                throttle();
            } else {
                revert_throttle();
            }

            send(
                broadcast_,
                show_atom_v,
                child_playhead_uuid,
                buf,
                playhead_rate(),
                playing(),
                is_onscreen_frame);

            if (child_playhead_uuid == key_playhead_uuid_) {

                // Tell anything measuring FPS that a new frame has been sent for display
                send(fps_moniotor_group_, show_atom_v);

                if (playhead_events_actor_) {
                    send(playhead_events_actor_, show_atom_v, buf);
                }

                send(viewport_events_group_, ui::show_buffer_atom_v, playing());
            }
        },

        [=](colour_pipeline_lookahead_atom,
            const media::AVFrameIDsAndTimePoints &frame_ids_for_colour_mgmnt_lookeahead) {
            // For each media source that is within the lookahead read region, during playback,
            // we have one AVFrameID .. these will get forwarded to the colour management
            // plugin so it has a chance to load and parse newcerssary LUTs ahead of time so
            // playback doesn't stutter when we have to put a source on the screen that
            // hasn't been encountered yet and needs some heavy computation/IO for its colour
            // managment particulars
            send(
                broadcast_,
                colour_pipeline_lookahead_atom_v,
                frame_ids_for_colour_mgmnt_lookeahead);
        },

        [=](buffer_atom) { delegate(key_playhead_, buffer_atom_v); },

        // child playhead is broadcasting frames *about* to show on screen
        // during playback, so we can start uploading pixels to GPU ahead
        // of when they are needed
        [=](show_atom,
            const Uuid &child_playhead_uuid,
            const std::vector<ImageBufPtr> &future_frames) {
            send(broadcast_, show_atom_v, child_playhead_uuid, future_frames);

            // have we got all the frames we expected? - if not, it's probably
            // because the lookahead read/cache can't keep up,
            bool missing_future_frame = false;
            for (const auto &frame : future_frames) {
                if (!frame) {
                    missing_future_frame = true;
                    break;
                }
            }

            if (child_playhead_uuid == key_playhead_uuid_ && missing_future_frame) {
                throttle();
            } else {
                revert_throttle();
            }
        },

        [=](simple_loop_end_atom) {
            // loop end in frames of 'key' child
            delegate(key_playhead_, flicks_to_logical_frame_atom_v, loop_end());
        },

        [=](simple_loop_end_atom, const int loop_end_frame) {
            request(key_playhead_, infinite, logical_frame_to_flicks_atom_v, loop_end_frame + 1)
                .then(

                    [=](const timebase::flicks loop_end_flicks) {
                        if (set_loop_end(
                                loop_end_flicks - PlayheadBase::playback_step_increment)) {
                            // position or loop end were also changed
                            notify_loop_start_changed();
                        }
                        notify_loop_end_changed();
                    },
                    [=](const error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](simple_loop_end_atom, const timebase::flicks loop_end_flicks) {
            if (set_loop_end(loop_end_flicks)) {
                // position or loop start we also changed
                notify_loop_start_changed();
            }
            notify_loop_end_changed();
        },

        [=](simple_loop_start_atom) {
            // loop OUT in frames of 'key' child
            delegate(key_playhead_, flicks_to_logical_frame_atom_v, loop_start());
        },

        [=](simple_loop_start_atom, const int loop_start_frame) {
            request(key_playhead_, infinite, logical_frame_to_flicks_atom_v, loop_start_frame)
                .then(

                    [=](const timebase::flicks loop_start_flicks) {
                        if (set_loop_start(loop_start_flicks)) {
                            // position or loop end were also changed
                            notify_loop_end_changed();
                            update_child_playhead_positions(false);
                        }
                        notify_loop_start_changed();
                    },
                    [=](const error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](simple_loop_start_atom, const timebase::flicks loop_start_flicks) {
            if (set_loop_start(loop_start_flicks)) {
                // position or loop end were also changed
                notify_loop_end_changed();
                update_child_playhead_positions(false);
            }
            notify_loop_start_changed();
        },

        /* During playback, this message comes from a PlayLoopActor, which
        will then receive a timepoint reponse which tells it when to re-send
        the step_atom message again and so-on. This is how the playback loop
        works. */
        [=](step_atom) -> PlayheadBase::OptionalTimePoint {
            const bool _playing = playing();

            // playback stopped while this step atom was in flight
            if (!_playing) {
                update_child_playhead_positions(true);
                return std::optional<xstudio::utility::time_point>();
            }

            // If we are playing, the base tells us when we should step it again,
            // an empty opyional is returned if we aren't looping and we've hit
            // the end of the duration
            PlayheadBase::OptionalTimePoint next_step_timepoint = play_step();

            update_child_playhead_positions(false);

            if (_playing != playing()) {
                // the call to play_step can turn off playback if we are in 'play once' mode and
                // we have got to the end of the in/out point region
                anon_send(
                    system().registry().template get<caf::actor>(global_registry),
                    global::busy_atom_v,
                    playing());
                send(event_group_, utility::event_atom_v, play_atom_v, playing());
            }

            return next_step_timepoint;
        },

        [=](step_atom, const int step_frames) {
            // we get the key playhead to work out what step_frames are
            // in terms of flicks, and adjust our position

            // This is somewhat complicated as the main playhead stores the loop
            // range in 'flicks' ... so the child playhead will have to work out
            // when to loop round and which frames it loops out and in through and
            // then convert those frames back to flicks
            request(
                key_playhead_,
                infinite,
                step_atom_v,
                position(),
                step_frames,
                loop() == LM_LOOP)
                .await(

                    [=](const timebase::flicks new_flicks) mutable {
                        set_position(new_flicks);
                        update_child_playhead_positions(true);
                    },
                    [=](const caf::error &err) mutable {
                        spdlog::warn(
                            "Failed to get logical frame from child playhead {}",
                            to_string(err));
                    });
        },

        [=](use_loop_range_atom) -> bool { return use_loop_range(); },

        [=](use_loop_range_atom, const bool _use_loop_range) -> unit_t {
            if (set_use_loop_range(_use_loop_range)) {
                // setting the loop range has re-set the playhead position,
                // probably because it was outside the loop range
                update_child_playhead_positions(false);
            }

            notify_loop_start_changed();
            notify_loop_end_changed();

            send(event_group_, utility::event_atom_v, use_loop_range_atom_v, use_loop_range());
            return unit;
        },

        [=](utility::event_atom, change_atom, caf::actor p, bool key_playhead) {
            // a child playhead's data has possibly changed, force an update
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(p));

            child_playhead_changed_ = false;

            if (key_playhead) {
                rebuild_cached_frames_status();
                // this will trigger an update to the duration
                anon_send(this, duration_flicks_atom_v);

                // this will update the 'image_source_' attribute so it shows the correct
                // list of available sources in the UI, in case it has changed
                request(key_playhead_, infinite, media_atom_v)
                    .then(
                        [=](caf::actor media_actor) {
                            if (media_actor)
                                current_media_changed(media_actor);
                        },
                        [=](const error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            }
        },

        [=](utility::event_atom, change_atom, caf::actor p) {
            update_child_playhead_positions(true);
            if (p == key_playhead_ and not child_playhead_changed_) {
                child_playhead_changed_ = true;
                delayed_send(
                    this,
                    std::chrono::milliseconds(250),
                    utility::event_atom_v,
                    change_atom_v,
                    p,
                    true);
            }
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &, bool) {
            // timeline change event ... ignore as its taken care of by sub playhead
        },

        [=](utility::event_atom,
            media_source_atom,
            caf::actor media_source_actor,
            caf::actor sub_playhead,
            const utility::Uuid &media_uuid,
            const utility::Uuid &source_uuid,
            const int /*media_frame*/) {
            if (sub_playhead == key_playhead_) {

                if ((to_string(media_uuid) != current_media_uuid_->value() or
                     to_string(source_uuid) != current_media_source_uuid_->value()) and
                    media_source_actor) {
                    previous_source_uuid_ = current_media_source_uuid_->value();
                    current_media_uuid_->set_value(to_string(media_uuid));
                    current_media_source_uuid_->set_value(to_string(source_uuid));

                    request(media_source_actor, infinite, utility::parent_atom_v)
                        .then(
                            [=](caf::actor media_actor) {
                                current_media_changed(media_actor, true);

                                send(
                                    event_group_,
                                    utility::event_atom_v,
                                    media_source_atom_v,
                                    UuidActor(media_uuid, media_actor));

                                if (connected_to_ui()) {
                                    send(
                                        broadcast_,
                                        utility::event_atom_v,
                                        media_source_atom_v,
                                        media_source_actor,
                                        source_uuid);
                                }
                            },
                            [=](const error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                    update_playback_rate();
                }
            }
        },

        [=](utility::event_atom,
            media_cache::keys_atom,
            const media::MediaKeyVector new_keys,
            const media::MediaKeyVector remove_keys) {
            update_cached_frames_status(new_keys, remove_keys);
        },

        [=](utility::event_atom,
            playlist::reflag_container_atom,
            const utility::Uuid &,
            const std::tuple<std::string, std::string> &) {},

        [=](utility::event_atom, source_atom, const std::vector<caf::actor> &source_list) {
            // This comes from the 'PlaylistSelectionActor' and sets the
            // viewable(s) that this playhead will play. A child playhead is
            // made for each source in the source list
            new_source_list(source_list);
        },

        [=](source_atom, const std::vector<caf::actor> &source_list) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // This message can be pushed to a playhead so it can start p[laying
            // medioa - the contents of the incoming source_list must be MediaActor type
            // or a wrapper that has the message handlers for playback
            new_source_list(source_list);

            // calling source_atom on SubPlayhead and waiting for response
            // ensures that the SubPlayhead is 'up-to-date' in other wordfs
            // it has got all the data it needs from its source to start playback
            // like duration, timecode and AVFrameID list
            fan_out_request<policy::select_all>(sub_playheads_, infinite, source_atom_v)
                .then(
                    [=](const std::vector<caf::actor>) mutable {
                        // now sending duration_flicks to ourselves means that duration, in/out
                        // points etc. are ensured to be up-to-date before we deliver the
                        // response
                        request(
                            caf::actor_cast<caf::actor>(this), infinite, duration_flicks_atom_v)
                            .then(
                                [=](timebase::flicks) mutable { rp.deliver(true); },
                                [=](const error &err) mutable { rp.deliver(err); });
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](bookmark::get_bookmark_atom)
            -> std::vector<std::tuple<utility::Uuid, std::string, int, int>> {
            return bookmark_frames_ranges_;
        },

        [=](utility::event_atom,
            media::current_media_source_atom,
            UuidActor &media_source,
            const media::MediaType mt) {
            // these come from Media actors (to which we have subscribed to the event group)
            if (media_source.actor()) {
                request(media_source.actor(), infinite, utility::name_atom_v)
                    .then(
                        [=](const std::string name) {
                            updating_source_list_ = true;
                            if (mt == media::MT_IMAGE) {
                                image_source_->set_value(name);
                            } else if (mt == media::MT_AUDIO) {
                                audio_source_->set_value(name);
                            }
                            updating_source_list_ = false;
                        },
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            } else {
                updating_source_list_ = true;
                if (mt == media::MT_IMAGE) {
                    image_source_->set_value("-");
                } else if (mt == media::MT_AUDIO) {
                    audio_source_->set_value("-");
                }
                updating_source_list_ = false;
            }
        },
        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        // controls creation and destruction of children
        [&](utility::event_atom, utility::change_atom) {
            if (current_sender() != this) {
                // change has bubbled up from a child playhead, force a redraw
                current_media_uuid_->set_value(to_string(utility::Uuid()));
                current_media_source_uuid_->set_value(to_string(utility::Uuid()));

                send(this, jump_atom_v);
                send(event_group_, utility::event_atom_v, utility::change_atom_v);
            } else {
                send(this, jump_atom_v);
            }
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::get_group_atom) -> caf::actor { return broadcast_; },

        [=](utility::rate_atom atom) -> result<FrameRate> {
            auto rp = make_response_promise<FrameRate>();
            request(key_playhead_, infinite, utility::rate_atom_v)
                .then(
                    [=](const FrameRate &rate) mutable { rp.deliver(rate); },
                    [=](caf::error &) mutable {
                        // skip errors (due to no source or out of range)
                        // and return a sensible default
                        rp.deliver(FrameRate(timebase::k_flicks_24fps));
                    });

            return rp;
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            JsonStore jsn;
            jsn["base"] = serialise();
            return jsn;
        },

        [=](velocity_atom) -> float { return velocity(); },

        [=](velocity_multiplier_atom) -> float { return velocity_multiplier(); },

        [=](velocity_multiplier_atom, const float velocity_multiplier) -> unit_t {
            set_velocity_multiplier(velocity_multiplier);
            send(
                event_group_,
                utility::event_atom_v,
                velocity_multiplier_atom_v,
                velocity_multiplier);
            send(
                fps_moniotor_group_,
                utility::event_atom_v,
                velocity_multiplier_atom_v,
                velocity_multiplier);
            return unit;
        },

        [=](bool) {});
}

void PlayheadActor::connect_to_playlist_selection_actor(caf::actor playlist_selection) {

    if (playlist_selection) {

        // Here we subscribe to event messages from a PlaylistSelectionActor -
        // this will push lists of media to the playhead, when media is selected
        // from a playlist for playback. See 'new_source_list'.
        playlist_selection_addr_ = caf::actor_cast<caf::actor_addr>(playlist_selection);
        request(playlist_selection, infinite, utility::get_event_group_atom_v)
            .then(
                [=](caf::actor grp) {
                    request(grp, caf::infinite, broadcast::join_broadcast_atom_v)
                        .then(
                            [=](const bool) mutable {
                                request(
                                    playlist_selection,
                                    infinite,
                                    playhead::get_selected_sources_atom_v)
                                    .then(
                                        [=](const utility::UuidActorVector &selection) {
                                            std::vector<caf::actor> actors;
                                            for (auto &s : selection)
                                                actors.push_back(s.actor());
                                            new_source_list(actors);
                                        },
                                        [=](const caf::error &e) {
                                            spdlog::warn(
                                                "{} {}", __PRETTY_FUNCTION__, to_string(e));
                                        });
                            },
                            [=](const error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                },
                [=](const caf::error &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
                });
    }
}

void PlayheadActor::clear_all_precache_requests(caf::typed_response_promise<bool> rp) {

    fan_out_request<policy::select_all>(sub_playheads_, infinite, uuid_atom_v)
        .then(
            [=](const std::vector<Uuid> uuids) mutable {
                // stop any read-ahead activity for these playheads
                request(pre_reader_, infinite, clear_precache_queue_atom_v, uuids)
                    .then(
                        [=](bool r) mutable { rp.deliver(r); },
                        [=](const error &err) mutable { rp.deliver(err); });
            },
            [=](const error &err) mutable { rp.deliver(err); });
}

void PlayheadActor::clear_child_playheads() {

    auto playheads_copy        = sub_playheads_;
    auto source_wrappers_copy_ = source_wrappers_;

    fan_out_request<policy::select_all>(playheads_copy, infinite, uuid_atom_v)
        .then(
            [=](const std::vector<Uuid> uuids) mutable {
                // stop any read-ahead activity for these playheads
                anon_send(pre_reader_, clear_precache_queue_atom_v, uuids);

                send(broadcast_, child_playheads_deleted_atom_v, uuids);

                for (auto &i : playheads_copy) {
                    unlink_from(i);
                    send_exit(i, caf::exit_reason::user_shutdown);
                }
                for (auto &i : source_wrappers_copy_) {
                    unlink_from(i);
                    send_exit(i, caf::exit_reason::user_shutdown);
                }
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });

    sub_playheads_.clear();
    source_wrappers_.clear();
    key_playhead_ = caf::actor();
}

caf::actor PlayheadActor::make_child_playhead(caf::actor source) {

    std::stringstream nmstr;
    nmstr << "ChildPlayhead" << sub_playheads_.size();

    caf::actor sub_playhead = spawn<SubPlayhead>(
        nmstr.str(),
        source,
        actor_cast<actor>(this),
        loop_start(),
        loop_end(),
        play_rate_mode(),
        playhead_rate(),
        media::MediaType::MT_IMAGE);

    link_to(sub_playhead);
    sub_playheads_.push_back(sub_playhead);

    join_event_group(this, sub_playhead);
    return sub_playhead;
}

void PlayheadActor::make_audio_child_playhead(const int source_index) {

    if (source_index >= (int)source_actors_.size())
        return;

    if (audio_playhead_) {
        unlink_from(audio_playhead_);
        send_exit(audio_playhead_, caf::exit_reason::user_shutdown);
        if (audio_playhead_retimer_)
            unlink_from(audio_playhead_retimer_);
        send_exit(audio_playhead_retimer_, caf::exit_reason::user_shutdown);
    }

    // depending on compare mode, audio playhead needs different wrapper for
    // sources
    audio_playhead_retimer_ =
        compare_mode() == CM_OFF ? caf::actor()
        : compare_mode() == CM_STRING
            ? spawn<EditListActor>("EditListActor", source_actors_, media::MT_AUDIO)
            : spawn<RetimeActor>("RetimeActor", source_actors_[source_index], media::MT_AUDIO);

    audio_playhead_ = spawn<SubPlayhead>(
        "AudioPlayhead",
        audio_playhead_retimer_ ? audio_playhead_retimer_ : source_actors_[source_index],
        actor_cast<actor>(this),
        loop_start(),
        loop_end(),
        play_rate_mode(),
        playhead_rate(),
        media::MediaType::MT_AUDIO);

    link_to(audio_playhead_);
    if (audio_playhead_retimer_)
        link_to(audio_playhead_retimer_);

    auto ap = audio_playhead_;
    request(audio_playhead_, infinite, get_event_group_atom_v)
        .then(
            [=](caf::actor event_group) mutable {
                // need this check, as the audio_playhead might have been destroyed
                // and created again (as per first lines of this member function)
                // before we got the response here from the original audio playhead!
                if (ap == audio_playhead_) {
                    join_broadcast(this, event_group);
                }
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::new_source_list(const std::vector<caf::actor> &sl) {

    if (sl == source_actors_)
        return;

    // stop receiving events of old source list
    for (auto &old_source : source_actors_) {
        request(old_source, infinite, utility::get_event_group_atom_v)
            .then(
                [=](caf::actor ev_group) mutable {
                    anon_send(ev_group, broadcast::leave_broadcast_atom_v, this);
                },
                [=](caf::error &) {
                    // don't print errors, as source may just be down (deleted)
                });
    }

    source_actors_ = sl;

    // reset the loop range as we have new sources
    set_loop_start(timebase::k_flicks_low);
    set_loop_end(timebase::k_flicks_max);
    rebuild();
}

void PlayheadActor::rebuild() {

    clear_child_playheads();

    if (!source_actors_.size()) {

        make_child_playhead(empty_clip_);
        switch_key_playhead(0);
        set_position(timebase::k_flicks_zero_seconds);

        // broadcast and empty buffer to clear the viewport
        anon_send(this, show_atom_v, key_playhead_uuid_, ImageBufPtr(), true);


    } else if (source_actors_.size() == 1 || compare_mode() == CM_OFF) {

        int count = 1;
        for (auto source : source_actors_) {

            make_child_playhead(source);

            // in grid, A/B compare modes etc we must limit the number of child playheads
            // in the case that the user has, say, selected 100 clips as it's too many for
            // the UI to cope with.
            if (compare_mode() != CM_OFF && count++ > max_compare_sources_->value()) {
                spdlog::warn(
                    "{} {} {}",
                    __PRETTY_FUNCTION__,
                    "Trying to compare too many things, limiting to first ",
                    max_compare_sources_->value());
                break;
            }
        }
        // passing a -1 as the index forces a search for a child playhead that
        // is showing the current on-screen source
        switch_key_playhead(-1);

    } else if (compare_mode() == CM_STRING) {

        auto foo = spawn<EditListActor>("EditListActor", source_actors_, media::MT_IMAGE);
        link_to(foo);
        make_child_playhead(foo);
        switch_key_playhead(0);
        source_wrappers_.push_back(foo);

    } else {

        int count = 1;
        for (auto source : source_actors_) {

            auto noo = spawn<RetimeActor>("RetimeActor", source, media::MT_IMAGE);
            link_to(noo);
            make_child_playhead(noo);
            source_wrappers_.push_back(noo);

            // in grid, A/B compare modes etc we must limit the number of child playheads
            // in the case that the user has, say, selected 100 clips as it's too many for
            // the UI to cope with.
            if (compare_mode() != CM_OFF && count++ > max_compare_sources_->value()) {
                spdlog::warn(
                    "{} {} {}",
                    __PRETTY_FUNCTION__,
                    "Trying to compare too many things, limiting to first ",
                    max_compare_sources_->value());
                break;
            }
        }
        // passing a -1 as the index forces a search for a child playhead that
        // is showing the current on-screen source
        switch_key_playhead(-1);
    }

    if (!(compare_mode() == CM_OFF || compare_mode() == CM_STRING)) {

        // for A/B mode, grid mode etc we need the child playheads to match
        // their durations so that they map to a common (shared) timeline
        align_clip_frame_numbers();
        anon_send(this, duration_flicks_atom_v);
    } else {
        anon_send(this, duration_flicks_atom_v);
    }
}

void PlayheadActor::switch_key_playhead(int idx) {

    caf::scoped_actor sys(system());
    bool force_move = false;
    if (idx < 0) {
        // let's see if one of the child playheads is showing a media source that matches
        // current_media_uuid_, if so we'll pick that one.
        try {
            int i = 0;
            for (auto &ph : sub_playheads_) {

                // this ensures that the child playhead is up-to-date and has
                // got all the data it needs from its source
                request_receive<caf::actor>(*sys, ph, source_atom_v);

                if (to_string(request_receive<utility::Uuid>(
                        *sys, ph, media_source_atom_v, true)) == current_media_uuid_->value()) {
                    idx = i;
                    break;
                }
                i++;
            }
            if (idx < 0)
                idx = 0;
            else
                force_move = true;
        } catch (std::exception &e) {
            spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
            idx = 0;
        }
    }

    if (idx >= 0 && idx < (int)sub_playheads_.size()) {

        key_playhead_ = sub_playheads_[idx];
        anon_send(key_playhead_, bookmark::get_bookmarks_atom_v);

        try {

            // pass the uuid of the new key playhead to the broadcast group
            const Uuid uuid    = request_receive<Uuid>(*sys, key_playhead_, uuid_atom_v);
            key_playhead_uuid_ = uuid;
            
            // if 'switch_key_playhead' is called rapidly, the broadcast made below
            // can reach the receiver out of order, so we need to give it a timestamp
            // so they can know if they have got an out-of-order notification and ignore it
            const auto switchpoint = utility::clock::now();
            send(broadcast_, key_child_playhead_atom_v, uuid, switchpoint);

            auto source_actor = request_receive<caf::actor>(*sys, key_playhead_, source_atom_v);

            make_audio_child_playhead(idx);
            // this should update the 'image_source_' attribute so it shows the correct
            // list of available sources in the UI
            auto media_actor = request_receive<caf::actor>(*sys, key_playhead_, media_atom_v);
            if (media_actor)
                current_media_changed(media_actor);

            // send the change notification
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
            send(event_group_, utility::event_atom_v, playhead::key_playhead_index_atom_v, idx);

            check_if_loop_range_makes_sense();

            // 'jump' to the last viewed frame of the current on-screen source
            if (compare_mode() == CM_STRING) {
                move_playhead_to_last_viewed_frame_of_given_source(
                    utility::Uuid(current_media_uuid_->value()));
            } else if (compare_mode() == CM_OFF || force_move) {
                move_playhead_to_last_viewed_frame_of_current_source();
            } else {
                update_child_playhead_positions(true);
            }

            // this will trigger an update to the duration, at which point we can
            // update other dependent data like loop range etc.
            request(caf::actor_cast<caf::actor>(this), infinite, duration_flicks_atom_v)
                .then(
                    [=](timebase::flicks) {
                        // send update events on properties of the playhead that depend on the
                        // child
                        notify_offset_changed();
                        update_playback_rate();
                        rebuild_cached_frames_status();
                        restart_readahead_cacheing(compare_mode() != CM_OFF);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });

        } catch (std::exception &e) {
            if (source_actors_.size()) { // suppress errors from empty EditListActor source
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
            rebuild_cached_frames_status();
        }

    } else {
        spdlog::warn(
            "{} {} {}", __PRETTY_FUNCTION__, "Attempt to set playhead to invalid index: ", idx);
    }

    // Keeping track of the previously selected sources count
    previous_selected_sources_count_ = source_actors_.size();
}

void PlayheadActor::update_child_playhead_positions(
    const bool force_broadcast, const bool playhead_scrubbing) {

    if (audio_playhead_) {
        anon_send(
            audio_playhead_,
            jump_atom_v,
            adjusted_position(),
            forward(),
            velocity(),
            playing(),
            playhead_scrubbing ? false : force_broadcast,
            connected_to_ui());
    }

    if (compare_mode() == CM_OFF) {

        anon_send(
            key_playhead_,
            jump_atom_v,
            position(),
            forward(),
            velocity(),
            playing(),
            playhead_scrubbing ? false : force_broadcast,
            connected_to_ui());

        if (playhead_scrubbing) {
            anon_send(key_playhead_, full_precache_atom_v, false, false);
        }

    } else {

        for (const auto &i : sub_playheads_) {
            anon_send(
                i,
                jump_atom_v,
                position(),
                forward(),
                velocity(),
                playing(),
                force_broadcast,
                connected_to_ui());

            if (playhead_scrubbing) {
                anon_send(key_playhead_, full_precache_atom_v, false, false);
            }
        }
    }
}

void PlayheadActor::notify_loop_end_changed() {

    for (const auto &i : sub_playheads_) {
        anon_send(i, simple_loop_end_atom_v, loop_end());
    }
    if (audio_playhead_) {
        anon_send(audio_playhead_, simple_loop_end_atom_v, loop_end());
    }

    update_child_playhead_positions(false);

    request(key_playhead_, infinite, flicks_to_logical_frame_atom_v, loop_end())
        .then(

            [=](const int loop_end) {
                loop_end_frame_->set_value(loop_end);
                send(event_group_, utility::event_atom_v, simple_loop_end_atom_v, loop_end);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::notify_loop_start_changed() {

    for (const auto &i : sub_playheads_) {
        anon_send(i, simple_loop_start_atom_v, loop_start());
    }
    if (audio_playhead_) {
        anon_send(audio_playhead_, simple_loop_start_atom_v, loop_start());
    }

    update_child_playhead_positions(false);

    request(key_playhead_, infinite, flicks_to_logical_frame_atom_v, loop_start())
        .then(

            [=](const int loop_start) {
                loop_start_frame_->set_value(loop_start);
                send(event_group_, utility::event_atom_v, simple_loop_start_atom_v, loop_start);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::notify_offset_changed() {

    request(key_playhead_, infinite, media::source_offset_frames_atom_v)
        .then(

            [=](const int offset) {
                send(
                    event_group_,
                    utility::event_atom_v,
                    media::source_offset_frames_atom_v,
                    offset);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::update_duration(caf::typed_response_promise<timebase::flicks> rp) {

    request(key_playhead_, infinite, duration_flicks_atom_v)
        .then(
            [=](const timebase::flicks duration) mutable {
                if (duration != timebase::k_flicks_zero_seconds) {

                    set_duration(duration);

                    // check that the loop range is valid, i.e. the loop start is
                    // within the (new) source duration
                    if (use_loop_range() && duration < loop_start()) {
                        set_use_loop_range(false);
                        notify_loop_start_changed();
                        notify_loop_end_changed();
                        send(
                            event_group_,
                            utility::event_atom_v,
                            use_loop_range_atom_v,
                            use_loop_range());
                    }
                } else {
                    set_duration(duration);
                }
                rp.deliver(duration);
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });

    request(key_playhead_, infinite, duration_frames_atom_v)
        .then(
            [=](const size_t duration) {
                duration_frames_->set_value(duration);
                send(event_group_, utility::event_atom_v, duration_frames_atom_v, duration);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::skip_through_sources(const int skip_by) {

    if (compare_mode() != CM_STRING) {

        // Items are multi selected, so we only skip through the items selected
        // by switching to the child playhead
        int curr_idx = -1;
        for (size_t i = 0; i < sub_playheads_.size(); ++i) {
            if (sub_playheads_[i] == key_playhead_) {
                curr_idx = (int)i;
            }
        }
        if (curr_idx != -1) {
            auto prefs   = global_store::GlobalStoreHelper(system());
            bool loop    = prefs.value<bool>("/ui/qml/cycle_through_playlist");
            int skip_idx = curr_idx + skip_by;

            curr_idx =
                loop ? skip_idx == -1 ? static_cast<int>(sub_playheads_.size() - 1)
                                      : skip_idx % sub_playheads_.size()
                     : std::max(
                           0, std::min(skip_idx, static_cast<int>(sub_playheads_.size() - 1)));

            switch_key_playhead(curr_idx);
        }

    } else {

        request(key_playhead_, infinite, playhead::skip_through_sources_atom_v, skip_by)
            .then(
                [=](utility::Uuid &new_source_uuid) {
                    move_playhead_to_last_viewed_frame_of_given_source(new_source_uuid);
                },
                [=](const error &err) {
                    spdlog::warn("B {} {}", __PRETTY_FUNCTION__, to_string(err));
                }

            );
    }
}

void PlayheadActor::jump_to_source(const utility::Uuid &media_uuid) {
    if (compare_mode() == CM_STRING) {

        move_playhead_to_last_viewed_frame_of_given_source(media_uuid);

    } else {

        for (size_t idx = 0; idx < sub_playheads_.size(); ++idx) {
            request(sub_playheads_[idx], infinite, playlist::get_media_uuid_atom_v)
                .await(
                    [=](const utility::Uuid &playhead_media_uuid) {
                        if (playhead_media_uuid == media_uuid) {
                            switch_key_playhead(idx);
                            if (compare_mode() == CM_OFF) {
                                move_playhead_to_last_viewed_frame_of_given_source(media_uuid);
                            }
                        }
                    },
                    [=](caf::error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    }
}

void PlayheadActor::update_playback_rate() {

    // The playback rate is either set by the media being played (by the key_playhead)
    // when DYNAMIC otherwise the PlayheadBase overrides it.
    if (key_playhead_ && play_rate_mode() == utility::TimeSourceMode::DYNAMIC) {
        request(key_playhead_, infinite, actual_playback_rate_atom_v)
            .then(
                [=](const utility::FrameRate &rate) {
                    if (rate != playhead_rate()) {
                        set_playhead_rate(rate);
                    }
                    send(
                        fps_moniotor_group_,
                        utility::event_atom_v,
                        actual_playback_rate_atom_v,
                        rate);
                },
                [=](const error &) {
                    // no media, fallback to 'default' playback rate
                    send(
                        event_group_,
                        utility::event_atom_v,
                        playhead_rate_atom_v,
                        playhead_rate());
                });
    }
}

void PlayheadActor::update_cached_frames_status(
    const media::MediaKeyVector &new_keys, const media::MediaKeyVector &remove_keys) {

    for (auto key : new_keys) {
        if (not frames_cached_.count(key))
            frames_cached_.insert(key);
    }

    for (auto key : remove_keys) {
        if (frames_cached_.count(key))
            frames_cached_.erase(key);
    }

    cached_frames_ranges_.clear();
    bool in_cache = false;
    std::pair<int, int> r;

    auto count = static_cast<int>(all_frames_keys_.size());
    auto scale = (count / 2048) + 1;

    for (auto i = 0; i < count; i += scale) {
        if (frames_cached_.count(all_frames_keys_[i]) != in_cache) {
            if (in_cache == false) {
                in_cache = true;
                r.first  = i;
            } else {
                in_cache = false;
                r.second = i - 1;
                cached_frames_ranges_.push_back(r);
            }
        }
    }

    if (in_cache) {
        r.second = count - 1;
        cached_frames_ranges_.push_back(r);
    }

    send(
        event_group_,
        utility::event_atom_v,
        media_cache::cached_frames_atom_v,
        cached_frames_ranges_);
}

void PlayheadActor::rebuild_cached_frames_status() {
    if (key_playhead_ && image_cache_) {
        // fetch the full list of keys for every frame in the timeline
        request(key_playhead_, infinite, media_cache::keys_atom_v)
            .then(

                [=](const media::MediaKeyVector &keys) mutable {
                    all_frames_keys_ = keys;

                    // now fetch the full list of keys for frames that are in the cache
                    request(image_cache_, infinite, media_cache::keys_atom_v)
                        .then(
                            [=](const media::MediaKeyVector &cached_frames_keys) mutable {
                                spdlog::stopwatch sw;
                                frames_cached_.clear();
                                frames_cached_.insert(
                                    cached_frames_keys.begin(), cached_frames_keys.end());
                                update_cached_frames_status();
                            },
                            [=](const error &err) {
                                spdlog::warn("A {} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                },
                [=](const error &err) {
                    spdlog::warn("B {} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else {
        all_frames_keys_.clear();
        frames_cached_.clear();
        update_cached_frames_status();
    }
}

void PlayheadActor::align_clip_frame_numbers() {

    try {

        if (sub_playheads_.empty())
            return;

        scoped_actor sys{system()};

        auto timeout = std::chrono::milliseconds(500);

        const bool align = auto_align_mode() != AAM_ALIGN_OFF;
        const bool trim  = auto_align_mode() == AAM_ALIGN_TRIM;

        // Use timecode to align the sources - if we are trimming, we use trim to the latest
        // start frame and the earliest end frame across all sources. If not we do the reverse,
        // i.e. extend all sources to start on the first frame of the source with the earliest
        // first frame and the last frame of the last source.
        //
        // Source1:                1001---------------------------1020
        // Source2:                              1008-------------------------1030
        // Result with trim:                     1008-------------1020
        // Result without trim:    1001---------------------------------------1030
        //
        // Or:
        //
        // Source1:                ---------------------------------------------
        // Source2:                              -----------------
        // Result with trim:                     -----------------
        // Result without trim:    ---------------------------------------------
        //
        // Etc ...


        int first_frame =
            trim ? std::numeric_limits<int>::lowest() : std::numeric_limits<int>::max();
        int last_frame =
            trim ? std::numeric_limits<int>::max() : std::numeric_limits<int>::lowest();
        int max_duration = std::numeric_limits<int>::lowest();


        for (auto &sub_playhead : sub_playheads_) {

            // reset the offset to zero so we get the 'true' media first & last frame
            request_receive_wait<bool>(
                *sys, sub_playhead, timeout, media::source_offset_frames_atom_v, 0);

            // reset the duration to cancel any retiming already done on the source
            request_receive_wait<bool>(
                *sys,
                sub_playhead,
                timeout,
                timeline::duration_atom_v,
                timebase::k_flicks_zero_seconds);

            // get the first and last frame of the source
            const int source_first_frame =
                request_receive_wait<media::AVFrameID>(
                    *sys, sub_playhead, timeout, first_frame_media_pointer_atom_v)
                    .timecode_.total_frames();

            const int source_last_frame =
                request_receive_wait<media::AVFrameID>(
                    *sys, sub_playhead, timeout, last_frame_media_pointer_atom_v)
                    .timecode_.total_frames();

            // get the timecode frames for first and last frame
            first_frame  = trim ? std::max(first_frame, source_first_frame)
                                : std::min(first_frame, source_first_frame);
            last_frame   = trim ? std::min(last_frame, source_last_frame)
                                : std::max(last_frame, source_last_frame);
            max_duration = std::max(max_duration, source_last_frame - source_first_frame);
        }

        if (align && first_frame >= last_frame) {
            throw std::runtime_error("Attempt to do align + trimmed compare with sources that "
                                     "don't have overlapping frame range!");
        }

        for (auto sub_playhead : sub_playheads_) {

            auto duration_flicks = request_receive_wait<timebase::flicks>(
                *sys,
                sub_playhead,
                timeout,
                logical_frame_to_flicks_atom_v,
                (align ? last_frame - first_frame : max_duration) + 1);

            if (position() > duration_flicks) {
                // make sure the playhead position is within the new
                // duration of aligned sources
                set_position(timebase::k_flicks_zero_seconds);
            }

            if (align) {
                const auto source_first_frame = request_receive_wait<media::AVFrameID>(
                    *sys, sub_playhead, timeout, first_frame_media_pointer_atom_v);

                int frames_shift = source_first_frame.timecode_.total_frames() - first_frame;

                // apply the time offset
                request_receive_wait<bool>(
                    *sys,
                    sub_playhead,
                    timeout,
                    media::source_offset_frames_atom_v,
                    frames_shift);
            }

            // and trim the duration
            request_receive_wait<bool>(
                *sys, sub_playhead, timeout, timeline::duration_atom_v, duration_flicks);
        }

    } catch (std::exception &e) {

        // supressing errors as 'No Frames' is thrown when the source is empty,
        // which isn't really an error
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void PlayheadActor::move_playhead_to_last_viewed_frame_of_current_source() {


    try {

        scoped_actor sys{system()};

        const auto uuid =
            request_receive<utility::Uuid>(*sys, key_playhead_, media_source_atom_v, true);

        if (uuid) {
            const auto p          = media_frame_per_media_uuid_.find(uuid);
            int last_viewed_frame = 0;
            if (p != media_frame_per_media_uuid_.end()) {
                last_viewed_frame = p->second;
            }

            const auto position = request_receive<timebase::flicks>(
                *sys, key_playhead_, media_frame_to_flicks_atom_v, uuid, last_viewed_frame);

            set_position(position);
        }
        anon_send(this, jump_atom_v);

    } catch (std::exception &e) {

        anon_send(this, jump_atom_v);

        // supressing errors as 'No Frames' is thrown when the source is empty,
        // which isn't really an error
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void PlayheadActor::move_playhead_to_last_viewed_frame_of_given_source(
    const utility::Uuid &source_uuid) {

    const auto p          = media_frame_per_media_uuid_.find(source_uuid);
    int last_viewed_frame = 0;
    if (p != media_frame_per_media_uuid_.end()) {
        last_viewed_frame = p->second;
    }

    try {

        scoped_actor sys{system()};

        const auto position = request_receive<timebase::flicks>(
            *sys, key_playhead_, media_frame_to_flicks_atom_v, source_uuid, last_viewed_frame);

        set_position(position);
        anon_send(this, jump_atom_v);

    } catch (std::exception &e) {

        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
        move_playhead_to_last_viewed_frame_of_current_source();
    }
}

void PlayheadActor::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    if (attr_uuid == compare_mode_->uuid() || attr_uuid == auto_align_mode_->uuid()) {
        rebuild();
    } else if (attr_uuid == velocity_->uuid()) {
        send(fps_moniotor_group_, utility::event_atom_v, velocity_atom_v, velocity());
    } else if (attr_uuid == velocity_multiplier_->uuid()) {
        send(
            fps_moniotor_group_,
            utility::event_atom_v,
            velocity_multiplier_atom_v,
            velocity_multiplier());
    } else if (attr_uuid == forward_->uuid()) {
        update_child_playhead_positions(true);
        send(fps_moniotor_group_, utility::event_atom_v, play_forward_atom_v, forward());
        send(broadcast_, play_forward_atom_v, forward());
    } else if (attr_uuid == image_source_->uuid() && !updating_source_list_) {
        switch_media_source(image_source_->value(), media::MT_IMAGE);
    } else if (attr_uuid == audio_source_->uuid() && !updating_source_list_) {
        switch_media_source(audio_source_->value(), media::MT_AUDIO);
    } else if (attr_uuid == playing_->uuid()) {

        if (playing()) {

            revert_throttle();
            last_step_ = clock::now();

            // This is where playback starts! The PlayLoopActor pings this PlayheadBase
            // with 'step' atoms in a ping-pong loop at regular intervals determined
            // by the PlayheadBase base class. See step_atom message handlers in this file.
            spawn<PlayLoopActor>(this);
        }
        send(broadcast_, play_atom_v, playing());
        send(playhead_media_events_group_, utility::event_atom_v, play_atom_v, playing());
        anon_send(audio_output_actor_, play_atom_v, playing());
        anon_send(
            system().registry().template get<caf::actor>(global_registry),
            global::busy_atom_v,
            playing());
        send(event_group_, utility::event_atom_v, play_atom_v, playing());
        send(fps_moniotor_group_, utility::event_atom_v, play_atom_v, playing());
        update_child_playhead_positions(true);

    } else if (attr_uuid == playhead_logical_frame_->uuid()) {
        anon_send(
            caf::actor_cast<caf::actor>(this),
            scrub_frame_atom_v,
            playhead_logical_frame_->value());
    } else {
        PlayheadBase::attribute_changed(attr_uuid, role);
    }
}

void PlayheadActor::connected_to_ui_changed() {

    if (connected_to_ui()) {

        restart_readahead_cacheing(true);
        update_playback_rate();
        send(fps_moniotor_group_, utility::event_atom_v, velocity_atom_v, velocity());
        send(
            fps_moniotor_group_,
            utility::event_atom_v,
            velocity_multiplier_atom_v,
            velocity_multiplier());
        send(fps_moniotor_group_, utility::event_atom_v, play_forward_atom_v, forward());

        request(key_playhead_, infinite, media_source_atom_v)
            .then(
                [=](caf::actor media_actor) {
                    send(
                        broadcast_,
                        utility::event_atom_v,
                        media_source_atom_v,
                        media_actor,
                        utility::Uuid(current_media_source_uuid_->value()));
                },
                [=](caf::error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

    } else {
        // stop all cacheing
        for (auto ph : sub_playheads_) {
            anon_send(ph, clear_precache_queue_atom_v);
        }
        if (audio_playhead_) {
            anon_send(audio_playhead_, clear_precache_queue_atom_v);
        }
    }
}

void PlayheadActor::current_media_changed(caf::actor media_actor, const bool force) {

    if (not force and media_actor_ == media_actor)
        return;
    media_actor_ = media_actor;
    send(
        playhead_media_events_group_,
        utility::event_atom_v,
        playhead::media_atom_v,
        media_actor);

    update_source_multichoice(image_source_, media::MT_IMAGE);
    update_source_multichoice(audio_source_, media::MT_AUDIO);
}

void PlayheadActor::update_source_multichoice(
    module::StringChoiceAttribute *attr, const media::MediaType mt) {

    // get the MediaSource items in the MediaActor
    request(media_actor_, infinite, media::current_media_source_atom_v, mt)
        .then(
            [=](const UuidActor &current_source) mutable {
                if (playhead_events_actor_ && mt == media::MT_IMAGE) {
                    // send new on screen media to global playhead events actor
                    send(
                        playhead_events_actor_,
                        show_atom_v,
                        media_actor_,
                        current_source.actor());
                }

                request(current_source.actor(), infinite, utility::name_atom_v)
                    .then(
                        [=](const std::string &current_source_name) mutable {
                            request(
                                media_actor_,
                                infinite,
                                media::get_media_source_names_atom_v,
                                mt)
                                .then(
                                    [=](const std::vector<std::pair<utility::Uuid, std::string>>
                                            &source_uuid_names) mutable {
                                        updating_source_list_ = true;
                                        attr->set_value(current_source_name);

                                        auto source_names =
                                            vpair_second_to_v(source_uuid_names);
                                        // sort names..
                                        std::sort(source_names.begin(), source_names.end());

                                        if (source_names.empty()) {
                                            source_names.emplace_back(
                                                mt == media::MT_IMAGE ? "No Video"
                                                                      : "No Audio");
                                        }

                                        attr->set_role_data(
                                            module::Attribute::StringChoices, source_names);
                                        attr->set_role_data(
                                            module::Attribute::AbbrStringChoices, source_names);
                                        updating_source_list_ = false;
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](error &err) mutable {
                // media has no sources providing streams of type 'mt'
                attr->set_value("N/A");
                attr->set_role_data(
                    module::Attribute::StringChoices,
                    std::vector<std::string>(
                        {mt == media::MT_IMAGE ? "No Video" : "No Audio"}));
                attr->set_role_data(
                    module::Attribute::AbbrStringChoices,
                    std::vector<std::string>(
                        {mt == media::MT_IMAGE ? "No Video" : "No Audio"}));
            });
}

void PlayheadActor::restart_readahead_cacheing(
    const bool all_child_playheads, const bool force) {

    if (!playing() && connected_to_ui()) {

        // first make sure we've wiped all outstanding precache requests...
        request(caf::actor_cast<caf::actor>(this), infinite, clear_precache_requests_atom_v)
            .then(
                [=](bool r) {
                    if (all_child_playheads) {
                        for (auto ph : sub_playheads_) {
                            anon_send(ph, full_precache_atom_v, true, force);
                        }

                    } else {
                        anon_send(key_playhead_, full_precache_atom_v, true, force);
                    }
                    if (audio_playhead_) {
                        anon_send(audio_playhead_, full_precache_atom_v, true, force);
                    }
                },
                [=](caf::error &err) {

                });
    }
}

void PlayheadActor::switch_media_source(
    const std::string new_source_name, const media::MediaType mt) {

    if (!key_playhead_)
        return;

    // going via the sub-playhead (which resolves which actual MediaActor is
    // on screen now), we make the request to change the active MediaSource
    // for the MediaActor
    request(key_playhead_, infinite, media_source_atom_v, new_source_name, mt)
        .then(

            [=](bool switched) mutable {
                // re-kick cacheing following switch
                if (switched && connected_to_ui()) {
                    restart_readahead_cacheing(false, true);
                }

                // now, if we have a selection actor we want to try and switch all the
                // media sources in the selected media too:
                auto playlist_selection = caf::actor_cast<caf::actor>(playlist_selection_addr_);
                if (playlist_selection) {

                    request(playlist_selection, infinite, get_selection_atom_v, true)
                        .then(
                            [=](std::vector<caf::actor> selected_sources) mutable {
                                for (auto &media_src : selected_sources) {
                                    anon_send(
                                        media_src, media_source_atom_v, new_source_name, mt);
                                }
                            },
                            [=](error &err) mutable {
                                if (to_string(err) != "No frames")
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](error &err) mutable {
                if (to_string(err) != "No frames")
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}


void PlayheadActor::check_if_loop_range_makes_sense() {

    // current behaviour is to turn off the loop range whenever the source(s)
    // changes, and when switching to String compare mode, in which case we sanity check the
    // range and turn off looping if it's outside the source range (see
    // notify_duration_changed)
    if (use_loop_range()) {
        if (has_selection_changed() || compare_mode() == CM_STRING) {
            set_use_loop_range(false);
            notify_loop_start_changed();
            notify_loop_end_changed();
            send(event_group_, utility::event_atom_v, use_loop_range_atom_v, use_loop_range());
        }
    }
}

bool PlayheadActor::has_selection_changed() {
    if (source_actors_.size() == 1) {
        return to_string(previous_source_uuid_) != current_media_source_uuid_->value();
    }

    return static_cast<int>(source_actors_.size()) != previous_selected_sources_count_;
}
