// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <limits>
#include <stdlib.h>

#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>
#include <caf/unit.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/string_out_actor.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace caf;

namespace {
/* Simple one-trick actor that enacts the playback loop. */
class PlayLoopActor : public caf::event_based_actor {
  public:
    PlayLoopActor(caf::actor_config &cfg, caf::actor playhead)
        : caf::event_based_actor(cfg), playhead_(playhead) {

        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](step_atom) {
                // step the playhead
                mail(step_atom_v)
                    .request(playhead, infinite)
                    .then(
                        [=](const PlayheadBase::OptionalTimePoint &tp) {
                            if (tp) {
                                // playhead is still going, trigger the next step by sending a
                                // step atom to ourselves
                                if (tp > utility::clock::now()) {
                                    anon_mail(step_atom_v)
                                        .schedule(*tp)
                                        .send(caf::actor_cast<caf::actor>(this));
                                    // scheduled_anon_send(
                                    //     caf::actor_cast<caf::actor>(this), *tp, step_atom_v);
                                } else {
                                    // I *think* a scheduled send to a time point in the past
                                    // will not get sent, hence we brute force continue in case
                                    // there has been some delays in the mailbox
                                    anon_mail(step_atom_v)
                                        .send(caf::actor_cast<caf::actor>(this));
                                }
                            } else {
                                send_exit(this, caf::exit_reason::user_shutdown);
                            }
                        },
                        [=](const caf::error &e) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
                        });
            });
        anon_mail(step_atom_v).delay(std::chrono::milliseconds(5)).send(this);
    }

    ~PlayLoopActor() override = default;

    caf::behavior behavior_;
    caf::actor playhead_;

    caf::behavior make_behavior() override { return behavior_; }
};

std::vector<caf::actor> to_actor_vector(const utility::UuidActorVector &v) {
    std::vector<caf::actor> result;
    for (const auto &a : v) {
        result.push_back(a.actor());
    }
    return result;
}

utility::UuidVector to_uuid_vector(const utility::UuidActorVector &v) {
    utility::UuidVector result;
    for (const auto &a : v) {
        result.push_back(a.uuid());
    }
    return result;
}

bool check_actor_down(caf::actor actor_down, utility::UuidActor &v) {
    if (v.actor() == actor_down) {
        v = utility::UuidActor();
        return true;
    }
    return false;
}

bool check_actor_down(caf::actor actor_down, utility::UuidActorVector &v) {

    const size_t sz = v.size();
    auto p          = v.begin();
    while (p != v.end()) {
        if (p->actor() == actor_down)
            p = v.erase(p);
        else
            p++;
    }
    return sz != v.size();
}

} // namespace


PlayheadActor::PlayheadActor(caf::actor_config &cfg, const std::string &name)
    : caf::event_based_actor(cfg),
      PlayheadBase(name, std::move(utility::Uuid::generate())),
      audio_path_(NO_AUDIO),
      offscreen_only_(true) {

    init();
    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
}

PlayheadActor::PlayheadActor(
    caf::actor_config &cfg,
    const std::string &name,
    const AudioPath audio_path,
    caf::actor playlist_selection,
    const utility::Uuid uuid,
    caf::actor_addr parent_playlist)
    : caf::event_based_actor(cfg),
      PlayheadBase(name, std::move(uuid)),
      audio_path_(audio_path),
      parent_playlist_(parent_playlist) {

    init();
    set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
    playlist_selection_addr_ = caf::actor_cast<caf::actor_addr>(playlist_selection);
    anon_mail(playlist::selection_actor_atom_v, playlist_selection)
        .send(actor_cast<caf::actor>(this));

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

    make_source_menu_model();
}

void PlayheadActor::on_exit() { parent_actor_exiting(); }

void PlayheadActor::init() {
    // get global reader and steal mrm..
    spdlog::debug("Created PlayheadActor {}", name());

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    attach_functor([=](const caf::error &reason) {
        spdlog::debug("PLAYHEAD exited: {}", to_string(reason));
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
    fps_moniotor_group_          = spawn<broadcast::BroadcastActor>(this);
    viewport_events_group_       = spawn<broadcast::BroadcastActor>(this);
    playhead_media_events_group_ = spawn<broadcast::BroadcastActor>(this);

    link_to(broadcast_);
    link_to(fps_moniotor_group_);
    link_to(viewport_events_group_);
    link_to(playhead_media_events_group_);

    // ensure we have a source and a child playhead, due to many messages
    // delagated to the child playhead
    empty_clip_ = utility::UuidActor(utility::Uuid::generate(), spawn<media::MediaActor>());
    // link_to(empty_clip_.actor());
    make_child_playhead(empty_clip_);
    switch_key_playhead(0);

    if (audio_path_ != playhead::NO_AUDIO) {
        audio_output_actor_ =
            system().registry().template get<caf::actor>(audio_output_registry);
    }

    playhead_events_actor_ =
        system().registry().template get<caf::actor>(global_playhead_events_actor);

    pre_reader_ = system().registry().template get<caf::actor>(media_reader_registry);

    // set_default_handler(
    //     [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
    //         //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES
    //         spdlog::warn(
    //             "Got unwanted messate from {} {}", to_string(current_sender()),
    //             to_string(msg));

    //         mail(utility::name_atom_v)
    //             .request(caf::actor_cast<caf::actor>(current_sender()), infinite)
    //             .then(
    //                 [=](const std::string &nm) { std::cerr << "NAME " << nm << "\n"; },
    //                 [=](caf::error &err) { std::cerr << "NAME " << to_string(err) << "\n";
    //                 });

    //         return message{};
    //     });

    apply_compare_prefs();

    behavior_.assign(
        [=](caf::exit_msg &msg) {
            disconnect_from_ui();

            if (audio_path_ == playhead::INDEPENDENT_AUDIO && audio_output_actor_) {
                // this tells global audio output that we are exiting
                mail(sound_audio_atom_v, uuid(), false).send(audio_output_actor_);
            }

            audio_output_actor_ = caf::actor();
            empty_clip_         = utility::UuidActor();
            image_cache_        = caf::actor();
            hero_sub_playhead_  = utility::UuidActor();
            pre_reader_         = caf::actor();
            audio_src_          = utility::UuidActor();

            // for (auto &i : sub_playheads_) {
            //     unlink_from(i.actor());
            //     send_exit(i.actor(), caf::exit_reason::user_shutdown);
            // }
            // send_exit(audio_playhead_, caf::exit_reason::user_shutdown);

            sub_playheads_.clear();
            source_actors_.clear();
            previous_source_actors_.clear();
            string_audio_sources_.clear();
            timeline_track_actors_.clear();
            dynamic_source_actors_.clear();

            // default_exit_handler(a, m);

            audio_playhead_ = caf::actor();

            quit(msg.reason);
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },

        [=](utility::parent_atom) -> caf::actor_addr { return parent_playlist_; },

        [=](actual_playback_rate_atom atom) {
            return mail(atom).delegate(hero_sub_playhead_.actor());
        },

        [=](clear_precache_requests_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            clear_all_precache_requests(rp);
            return rp;
        },

        [=](const error &err) { spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err)); },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](compare_mode_atom) -> std::string { return compare_mode_->value(); },

        [=](compare_mode_atom, const std::string &compare_mode) -> bool {
            compare_mode_->set_value(compare_mode);
            return true;
        },

        [=](dropped_frame_atom) { throttle(); },

        [=](fps_event_group_atom) -> caf::actor { return fps_moniotor_group_; },

        [=](viewport_events_group_atom) -> caf::actor { return viewport_events_group_; },

        [=](playlist::selection_actor_atom, caf::actor selection_actor) -> result<bool> {
            // allows us to connect to a selection actor, fetch the current selection, and
            // return a result. We use this to set-up our source list before deserialisation
            auto rp = make_response_promise<bool>();
            connect_to_playlist_selection_actor(selection_actor, rp);
            return rp;
        },

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
                if (i.actor() == sub_playhead) {
                    anon_mail(
                        jump_atom_v,
                        position(),
                        forward(),
                        velocity(),
                        playing(),
                        true,
                        connected_to_ui(),
                        user_is_frame_scrubbing_->value())
                        .send(i.actor());
                }
            }
        },

        [=](jump_atom, const int64_t frame) {
            return mail(jump_atom_v, (int)frame).delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](jump_atom, const int frame) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // by requesting duration from self we ensure that we have updated
            // internal data about source duration, incase this jump message
            // has come immediately after a change in the source, for example
            mail(duration_flicks_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const timebase::flicks duration) mutable {
                        if (duration != timebase::k_flicks_zero_seconds) {
                            mail(logical_frame_to_flicks_atom_v, frame, true)
                                .request(hero_sub_playhead_.actor(), infinite)
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

        [=](jump_atom, const timebase::flicks flicks) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // by requesting duration from self we ensure that we have updated
            // internal data about source duration, incase this jump message
            // has come immediately after a change in the source, for example
            mail(duration_flicks_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const timebase::flicks duration) mutable {
                        if (duration != timebase::k_flicks_zero_seconds) {
                            set_position(flicks);
                            update_child_playhead_positions(true);
                        } else {
                            set_position(timebase::k_flicks_zero_seconds);
                        }
                        // we only deliver the result when we have waited for
                        // the key playhead to update its position. This means
                        // that whever made the original request can be sure
                        // that the playhead is ready to deliver the frame for
                        // the requested 'flicks' position
                        mail(
                            jump_atom_v,
                            position(),
                            forward(),
                            velocity(),
                            playing(),
                            true,
                            connected_to_ui(),
                            user_is_frame_scrubbing_->value())
                            .request(hero_sub_playhead_.actor(), infinite)
                            .then(
                                [=]() mutable { rp.deliver(true); },
                                [=](const caf::error &err) mutable { rp.deliver(err); });
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](jump_atom, const utility::Uuid &media_uuid) { jump_to_source(media_uuid); },

        [=](key_child_playhead_atom) -> utility::UuidVector {
            // fetch the Uuids of the playheads. The last entry tells
            // us the key playhead address
            utility::UuidVector r = to_uuid_vector(sub_playheads_);
            r.push_back(hero_sub_playhead_.uuid());
            return r;
        },

        [=](key_playhead_index_atom) -> int {
            for (size_t i = 0; i < sub_playheads_.size(); ++i) {
                if (sub_playheads_[i] == hero_sub_playhead_)
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

        [=](logical_frame_atom atom) {
            return mail(atom).delegate(hero_sub_playhead_.actor());
        },

        [=](loop_atom) -> LoopMode { return playhead::LoopMode(loop()); },

        [=](loop_atom, const LoopMode loop) -> unit_t {
            set_loop(loop);
            return unit;
        },

        [=](media::source_offset_frames_atom atom) {
            return mail(atom).delegate(hero_sub_playhead_.actor());
        },

        [=](media::source_offset_frames_atom atom, caf::actor sub_playhead, const int offset) {
            // pass up to the main playhead that the offset has changed
            if (sub_playhead == hero_sub_playhead_) {

                source_offset_frames_->set_value(offset, false);

                // the cached frames display might need updating
                rebuild_cached_frames_status();
            }
        },

        [=](media_events_group_atom) -> caf::actor { return playhead_media_events_group_; },

        [=](media_atom atom) { return mail(atom).delegate(hero_sub_playhead_.actor()); },

        [=](media_atom, const bool kick) {
            // 'kick' all sub-playheads to re-broadcast their media items. Used by
            // the PlayheadGlobalEventsActor to force a playhead to tell any
            // watchers what the current on-screen media is.
            for (auto &sub_playhead : sub_playheads_) {
                mail(media_atom_v, kick).send(sub_playhead.actor());
            }
        },

        [=](media_source_atom atom) { return mail(atom).delegate(hero_sub_playhead_.actor()); },

        [=](media_source_atom atom, bool) {
            return mail(atom, true).delegate(hero_sub_playhead_.actor());
        },

        // this is a 'hack'. We need to force the playhead to re-broadcast info about
        // the current media source when setting up a new viewport. Calling
        // connected_to_ui_changed will do what we want. see viewport_frame_queue_actor.cpp
        [=](media_source_atom atom, bool, bool) {
            if (connected_to_ui()) {
                connected_to_ui_changed();
            }
        },

        [=](bookmark::get_bookmarks_atom) {
            mail(utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_frames_ranges_)
                .send(event_group_);

            mail(utility::event_atom_v, bookmark::get_bookmarks_atom_v, bookmark_frames_ranges_)
                .send(playhead_media_events_group_);
        },

        [=](utility::event_atom,
            bookmark::get_bookmarks_atom,
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                &bookmark_ranges) {
            if (caf::actor_cast<caf::actor>(current_sender()) == hero_sub_playhead_.actor()) {

                bookmark_frames_ranges_ = bookmark_ranges;

                std::vector<int> ranges;
                std::vector<std::string> colours;
                for (const auto &bm : bookmark_ranges) {
                    colours.push_back(std::get<1>(bm));
                    ranges.push_back(std::get<2>(bm));
                    ranges.push_back(std::get<3>(bm) - std::get<2>(bm));
                }
                bookmarked_frames_->set_value(ranges);
                bookmarked_frames_->set_role_data(module::Attribute::StringChoices, colours);

                mail(
                    utility::event_atom_v,
                    bookmark::get_bookmarks_atom_v,
                    bookmark_frames_ranges_)
                    .send(event_group_);

                mail(
                    utility::event_atom_v,
                    bookmark::get_bookmarks_atom_v,
                    bookmark_frames_ranges_)
                    .send(playhead_media_events_group_);

                // force fetch of a new frame with up-to-date bookmark/annotation
                // sidecar data
                anon_mail(jump_atom_v).send(this);
            }
        },

        [=](media_cache::keys_atom atom) {
            return mail(atom).delegate(hero_sub_playhead_.actor());
        },

        [=](play_atom) -> bool { return playing(); },

        [=](play_atom, const bool _play) -> bool {
            set_playing(_play);
            return true;
        },

        [=](play_atom, const float left_right_key_id) {
            // the messsage comes in with a delay when user hits forward step
            // or backward step hotkey. If the user is continuing to hold
            // down the hotkey (wihtout lifting it since the delayed message
            // was scheduled) then we can start playback
            if (step_keypress_event_id_ == left_right_key_id) {
                set_forward(step_keypress_event_id_ > 0.0f);
                set_playing(true);
            }
        },

        [=](play_forward_atom) -> bool { return forward(); },

        [=](play_forward_atom, const bool forward, const bool faster) { play_faster(forward); },

        [=](play_forward_atom, const bool forward) -> unit_t {
            set_forward(forward);
            update_child_playhead_positions(true);
            mail(utility::event_atom_v, play_forward_atom_v, forward).send(event_group_);
            mail(utility::event_atom_v, play_forward_atom_v, forward).send(fps_moniotor_group_);
            return unit;
        },

        [=](play_rate_mode_atom) -> utility::TimeSourceMode { return play_rate_mode(); },

        [=](play_rate_mode_atom, const utility::TimeSourceMode play_rate_mode) -> result<bool> {
            auto rp = make_response_promise<bool>();
            set_play_rate_mode(play_rate_mode);
            mail(utility::event_atom_v, play_rate_mode_atom_v, play_rate_mode)
                .send(event_group_);
            // this will ensure the duration is updated before we deliver the response
            mail(duration_flicks_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
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

        [=](playhead::duration_flicks_atom, bool offscreen) -> result<timebase::flicks> {
            // This message can come from the offscreen renderer - we ensure that
            // sub-playheads are built ready for playback and then return the
            // timline duration
            offscreen_only_ = offscreen;
            new_source_list(true);
            auto rp = make_response_promise<timebase::flicks>();
            update_duration(rp);
            return rp;
        },

        [=](playhead::duration_frames_atom) {
            // spdlog::warn("playhead delegate duration_frames_atom {}",
            // to_string(hero_sub_playhead_));
            return mail(duration_frames_atom_v).delegate(hero_sub_playhead_.actor());
        },

        [=](playhead::duration_frames_atom, bool offscreen) {
            // ensure the sub playheads are built and then return the
            offscreen_only_ = offscreen;
            new_source_list(true);
            return mail(duration_frames_atom_v).delegate(hero_sub_playhead_.actor());
        },

        [=](playhead_rate_atom) -> FrameRate { return playhead_rate(); },

        [=](playhead_rate_atom, const FrameRate &rate) -> result<bool> {
            auto rp = make_response_promise<bool>();
            set_playhead_rate(rate);
            mail(utility::event_atom_v, playhead_rate_atom_v, rate).send(event_group_);
            // this will ensure the duration is updated before we deliver the response
            mail(duration_flicks_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](timebase::flicks) mutable { rp.deliver(true); },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](utility::event_atom,
            media::add_media_source_atom,
            const utility::UuidActorVector &) { current_media_changed(media_actor_, true); },

        [=](playlist::select_media_atom, const UuidVector &selection) {
            return mail(playlist::select_media_atom_v, selection).delegate(playlist_selection_);
        },

        [=](playlist::select_media_atom,
            const Uuid &media_id,
            const int playhead_idx,
            const bool add_select) {
            // this message comes from the Viewport. It sends it when the user clicks on
            // the viewport to select an image in a Grid layout. If we are not doing
            // a contact sheet we don't want to modify the selection this way as it
            // will immediately change what's in the Grid, for example
            if (parent_playlist_ && contact_sheet_mode_) {
                auto playlist = caf::actor_cast<caf::actor>(parent_playlist_);
                mail(playlist::selection_actor_atom_v)
                    .request(playlist, infinite)
                    .then(
                        [=](caf::actor selection_actor) {
                            if (selection_actor)
                                anon_mail(playlist::select_media_atom_v, UuidVector({media_id}))
                                    .send(selection_actor);
                        },
                        [=](caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            } else {
                switch_key_playhead(playhead_idx);
            }
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
                next_video_refresh - last_playhead_set_timepoint());
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
            const utility::Timecode &tc,
            const std::string &uri) {
            // handles incoming notification from a child playhead that their
            // logical frame has changed
            if (child == hero_sub_playhead_.actor()) {

                playhead_logical_frame_->set_value(logical_frame, false);
                playhead_position_seconds_->set_value(timebase::to_seconds(position()));
                playhead_position_flicks_->set_value(position().count());
                playhead_media_logical_frame_->set_value(media_logical_frame, false);
                current_frame_timecode_->set_value(to_string(tc), false);
                current_frame_timecode_as_frame_->set_value(tc.total_frames(), false);
                playhead_media_frame_->set_value(media_frame, false);
                image_uri_->set_value(uri);

                mail(
                    utility::event_atom_v,
                    playhead::position_atom_v,
                    position(),
                    logical_frame,
                    media_frame,
                    media_logical_frame,
                    tc)
                    .send(playhead_media_events_group_);

                media_frame_per_media_uuid_[source_uuid] = media_logical_frame;

                mail(
                    utility::event_atom_v,
                    position_atom_v,
                    logical_frame,
                    media_frame,
                    media_logical_frame,
                    user_is_frame_scrubbing_->value(),
                    media_rate,
                    min(position(), duration() - playhead_rate()),
                    tc)
                    .send(event_group_);
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
            const std::vector<AudioBufPtr> &audio_buffers,
            const bool scrubbing) {
            if (audio_output_actor_ &&
                caf::actor_cast<caf::actor>(current_sender()) == audio_playhead_) {
                anon_mail(
                    sound_audio_atom_v,
                    audio_buffers,
                    child_playhead_uuid,
                    audio_path_ == playhead::GLOBAL_AUDIO,
                    uuid(),
                    scrubbing,
                    position())
                    .send(audio_output_actor_);
            }
        },

        [=](audio::audio_samples_atom,
            const std::vector<AudioBufPtr> &audio_buffers,
            timebase::flicks playhead_position) {
            // the audio playhead broadcasts a smaller set of audio samples for
            // the current playhead position - this is used for visualising
            // the audio waveform, for example
            if (audio_output_actor_ &&
                caf::actor_cast<caf::actor>(current_sender()) == audio_playhead_) {
                anon_mail(audio::audio_samples_atom_v, audio_buffers, playhead_position, uuid())
                    .send(audio_output_actor_);
            }
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

            mail(
                show_atom_v,
                child_playhead_uuid,
                buf,
                playhead_rate(),
                playing(),
                is_onscreen_frame)
                .send(broadcast_);

            if (child_playhead_uuid == hero_sub_playhead_.uuid()) {

                // Tell anything measuring FPS that a new frame has been sent for display
                mail(show_atom_v).send(fps_moniotor_group_);

                if (playhead_events_actor_) {
                    mail(show_atom_v, buf).send(playhead_events_actor_);
                }

                mail(ui::show_buffer_atom_v, playing()).send(viewport_events_group_);
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
            mail(colour_pipeline_lookahead_atom_v, frame_ids_for_colour_mgmnt_lookeahead)
                .send(broadcast_);
        },

        [=](audio_buffer_atom,
            const timebase::flicks tp,
            AudioBufPtr buffer_to_fill) -> result<AudioBufPtr> {
            auto rp = make_response_promise<AudioBufPtr>();
            // Fill the provided buffer with audio samples, starting at the playhead timeline
            // timepoint 'tp'. Used by VideoRenderPlugin, for example
            if (audio_playhead_) {
                rp.delegate(audio_playhead_, audio_buffer_atom_v, tp, buffer_to_fill);
            } else {
                rp.deliver(buffer_to_fill);
            }
            return rp;
        },

        [=](image_buffer_atom, const utility::Uuid &id) -> result<ImageBufPtr> {
            auto rp = make_response_promise<ImageBufPtr>();
            // fetch an image buffer for the given sub-playhead id
            for (const auto &i : sub_playheads_) {
                if (i.uuid() == id && (assembly_mode() != AM_ONE || i == hero_sub_playhead_)) {
                    rp.delegate(i.actor(), image_buffer_atom_v);
                    return rp;
                }
            }
            rp.deliver(ImageBufPtr());
            return rp;
        },

        [=](image_buffer_atom) {
            // fetch an image buffer from the hero sub-playhead
            return mail(image_buffer_atom_v).delegate(hero_sub_playhead_.actor());
        },

        // child playhead is broadcasting frames *about* to show on screen
        // during playback, so we can start uploading pixels to GPU ahead
        // of when they are needed
        [=](show_atom,
            const Uuid &child_playhead_uuid,
            const std::vector<ImageBufPtr> &future_frames) {
            // see note in the other 'show_atom' handler
            if (assembly_mode() == AM_ALL || assembly_mode() == AM_TEN ||
                child_playhead_uuid == hero_sub_playhead_.uuid()) {
                mail(show_atom_v, child_playhead_uuid, future_frames).send(broadcast_);
            }

            // have we got all the frames we expected? - if not, it's probably
            // because the lookahead read/cache can't keep up,
            // Note - if we've got the first 4 'future_frames' that's enough
            // for what the viewport needs in terms of doing async pixel transfer
            // to the GPU.
            bool missing_future_frame = false;
            int i                     = 0;
            for (const auto &frame : future_frames) {
                if (!frame) {
                    missing_future_frame = true;
                    break;
                }
                ++i;
                if (i == 4)
                    break;
            }

            if (child_playhead_uuid == hero_sub_playhead_.uuid() && missing_future_frame) {
                throttle();
            } else {
                revert_throttle();
            }
        },

        [=](logical_frame_to_flicks_atom, const int64_t frame) {
            return mail(logical_frame_to_flicks_atom_v, frame)
                .delegate(hero_sub_playhead_.actor());
        },

        [=](logical_frame_to_flicks_atom, const int frame) {
            return mail(logical_frame_to_flicks_atom_v, int64_t(frame))
                .delegate(hero_sub_playhead_.actor());
        },

        [=](simple_loop_end_atom) {
            // loop end in frames of 'key' child
            return mail(flicks_to_logical_frame_atom_v, loop_end())
                .delegate(hero_sub_playhead_.actor());
        },

        [=](simple_loop_end_atom, const int loop_end_frame) {
            loop_end_frame_->set_value(loop_end_frame);
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
            return mail(flicks_to_logical_frame_atom_v, loop_start())
                .delegate(hero_sub_playhead_.actor());
        },

        [=](simple_loop_start_atom, const int loop_start_frame) {
            loop_start_frame_->set_value(loop_start_frame);
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
            // make a note of the time that the playhead position was updated

            update_child_playhead_positions(false);

            if (_playing != playing()) {
                // the call to play_step can turn off playback if we are in 'play once' mode and
                // we have got to the end of the in/out point region
                anon_mail(global::busy_atom_v, playing())
                    .send(system().registry().template get<caf::actor>(global_registry));
                mail(utility::event_atom_v, play_atom_v, playing()).send(event_group_);
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
            mail(step_atom_v, position(), step_frames, loop() == LM_LOOP)
                .request(hero_sub_playhead_.actor(), infinite)
                .await(

                    [=](const timebase::flicks new_flicks) mutable {
                        user_is_frame_scrubbing_->set_value(true);
                        set_position(new_flicks);
                        update_child_playhead_positions(true);
                        user_is_frame_scrubbing_->set_value(false);
                    },
                    [=](const caf::error &err) mutable {
                        spdlog::warn(
                            "Failed to get logical frame from child playhead {}",
                            to_string(err));
                    });
        },

        [=](skip_to_clip_atom, bool next_clip) {
            auto rp = make_response_promise<bool>();
            // skip the playhead position to the start of the next clip, the
            // beginning of the current clip, or if we are already at the beginning
            // of the current clip then the beginning of the preceeding clip
            mail(skip_to_clip_atom_v, position(), next_clip)
                .request(hero_sub_playhead_.actor(), infinite)
                .then(
                    [=](const timebase::flicks new_position) mutable {
                        set_position(new_position);
                        update_child_playhead_positions(true);
                        rp.deliver(true);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
        },

        [=](skip_to_media_atom, const bool next_media) { step_to_next_media(next_media); },

        [=](skip_to_bookmark_atom, bool next_book) -> bool {
            int cframe = playhead_logical_frame_->value();
            int d      = std::numeric_limits<int>::max();
            int r      = -1;

            for (const auto &bm : bookmark_frames_ranges_) {
                int delta = next_book ? std::get<2>(bm) - cframe : cframe - std::get<2>(bm);
                if (delta > 0 && delta < d) {
                    r = std::get<2>(bm);
                    d = delta;
                }
            }

            if (r != -1)
                anon_mail(jump_atom_v, r).send(caf::actor_cast<caf::actor>(this));

            return (r != -1 ? true : false);
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

            mail(utility::event_atom_v, use_loop_range_atom_v, use_loop_range())
                .send(event_group_);
            return unit;
        },

        [=](precache_atom) {
            // send a staggered beat to each sub-playhead telling it to request the reader
            // to load frames that will be on-screen soon. We stagger the message send so
            // that the reader is not swamped by the requests coming from multiple sub-playheads
            // at one time. Each sub-playhead is sent this message about once a second.
            if (assembly_mode() == AM_ONE) {
                // if we aren't comparing multiple sources, only make the
                // 'hero' playhead trigger a precache
                anon_mail(precache_atom_v).send(hero_sub_playhead_.actor());
                if (audio_playhead_)
                    anon_mail(precache_atom_v).send(audio_playhead_);
                anon_mail(precache_atom_v).delay(std::chrono::milliseconds(1000)).send(this);
            } else if (sub_playheads_.size()) {
                sub_playhead_precache_idx_ =
                    (sub_playhead_precache_idx_ + 1) % sub_playheads_.size();
                anon_mail(precache_atom_v)
                    .send(sub_playheads_[sub_playhead_precache_idx_].actor());
                if (audio_playhead_ &&
                    sub_playheads_[sub_playhead_precache_idx_] == hero_sub_playhead_) {
                    anon_mail(precache_atom_v).send(audio_playhead_);
                }
                if (playing()) {
                    anon_mail(precache_atom_v)
                        .delay(std::chrono::milliseconds(1000 / sub_playheads_.size()))
                        .send(this);
                }
            }
        },

        [=](utility::event_atom, change_atom, caf::actor p, bool key_playhead) {
            // a child playhead's data has possibly changed, force an update
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(p));

            child_playhead_changed_ = false;

            if (key_playhead) {
                rebuild_cached_frames_status();
                // this will trigger an update to the duration
                anon_mail(duration_flicks_atom_v).send(this);

                // this will update the 'image_source_' attribute so it shows the correct
                // list of available sources in the UI, in case it has changed
                mail(media_atom_v)
                    .request(hero_sub_playhead_.actor(), infinite)
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
            if (p == hero_sub_playhead_.actor() and not child_playhead_changed_) {
                child_playhead_changed_ = true;
                mail(utility::event_atom_v, change_atom_v, p, true)
                    .delay(std::chrono::milliseconds(250))
                    .send(this);
            }
        },

        [=](utility::event_atom,
            playhead::media_frame_ranges_atom,
            const std::vector<int> &ranges) {
            if (current_sender() == hero_sub_playhead_.actor()) {
                media_transition_frames_->set_value(ranges);
            }
        },

        [=](utility::event_atom, timeline::item_atom, const utility::JsonStore &, bool) {
            // timeline change event ... ignore as its taken care of by sub playhead
        },

        [=](utility::event_atom,
            media_source_atom,
            utility::UuidActor media,
            utility::UuidActor media_source,
            const int sub_playhead_index,
            const bool check_rate) {
            // message from one of our sub-playheads ... the media source and/or
            // media item for the current playhead frame has changed
            if (sub_playhead_index == key_playhead_index_->value()) {

                if (to_string(media.uuid()) != current_media_uuid_->value() or
                    to_string(media_source.uuid()) != current_media_source_uuid_->value()) {

                    current_media_uuid_->set_value(to_string(media.uuid()));
                    current_media_source_uuid_->set_value(to_string(media_source.uuid()));
                    current_media_changed(media.actor(), true);
                    update_playback_rate();
                } else if (check_rate) {
                    update_playback_rate();
                }

                mail(utility::event_atom_v, media_source_atom_v, media_source)
                    .send(event_group_);

                if (connected_to_ui()) {
                    mail(utility::event_atom_v, media_source_atom_v, media_source)
                        .send(broadcast_);
                }
            }
            // send new on screen media to global playhead events actor
            mail(show_atom_v, media, media_source, sub_playhead_index)
                .send(playhead_events_actor_);
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

        [=](source_atom, const utility::UuidActorVector &sources) -> bool {
            // This comes from contact sheet objects - the sources are all the
            // media in the contact sheet
            if (sources != dynamic_source_actors_) {
                contact_sheet_mode_    = true;
                dynamic_source_actors_ = sources;
                new_source_list();
            }
            return true;
        },

        [=](utility::event_atom, source_atom, const std::vector<caf::actor> &source_list) {
            // This comes from the 'PlaylistSelectionActor' and sets the
            // viewable(s) that this playhead will play. A child playhead is
            // made for each source in the source list
            if (!contact_sheet_mode_) {
                dynamic_source_actors_ = to_uuid_actor_vec(source_list);
                new_source_list();
            } else if (!source_list.empty()) {
                // for contact sheet mode, we just make sure the 'hero playhead' respects
                // the first item in source_list
                for (int i = 0; i < dynamic_source_actors_.size(); ++i) {
                    if (dynamic_source_actors_[i].actor() == source_list[0]) {
                        switch_key_playhead(i);
                        break;
                    }
                }
            }
        },

        [=](source_atom,
            const utility::UuidActor &timeline,
            const utility::UuidActorVector &timeline_tracks) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // this is broadcast from the timeline on change events, if the parent of the
            // playhead is a timeline. We only rebuild if the track actors have changed.
            if (timeline_actor_ != timeline || timeline_track_actors_ != timeline_tracks) {
                timeline_actor_        = timeline;
                timeline_track_actors_ = timeline_tracks;

                // for timelines, there is one global frame rate set by the timline
                // itself - we need to get that now.
                if (timeline_actor_) {
                    mail(utility::rate_atom_v)
                        .request(timeline_actor_.actor(), infinite)
                        .then(
                            [=](const utility::FrameRate &r) mutable {
                                set_playhead_rate(r);
                                new_source_list();
                                rp.deliver(true);
                            },
                            [=](caf::error &err) mutable {
                                new_source_list();
                                rp.deliver(err);
                            });
                } else {
                    new_source_list();
                    rp.deliver(true);
                }
            } else {
                rp.deliver(true);
            }
            return rp;
        },

        [=](source_atom, const std::vector<caf::actor> &source_list) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // This message can be pushed to a playhead so it can start p[laying
            // medioa - the contents of the incoming source_list must be MediaActor type
            // or a wrapper that has the message handlers for playback
            dynamic_source_actors_ = to_uuid_actor_vec(source_list);

            new_source_list();
            // spdlog::warn("Fanout orig {} dyn {} new {}", dynamic_source_actors_.size(),
            // source_list.size(), sub_playheads_.size());

            // calling source_atom on SubPlayhead and waiting for response
            // ensures that the SubPlayhead is 'up-to-date' in other wordfs
            // it has got all the data it needs from its source to start playback
            // like duration, timecode and AVFrameID list

            // Not sure if this is right, but stops it breaking when there are no sub playheads.
            if (sub_playheads_.empty()) {
                mail(duration_flicks_atom_v)
                    .request(caf::actor_cast<caf::actor>(this), infinite)
                    .then(
                        [=](timebase::flicks) mutable { rp.deliver(true); },
                        [=](const error &err) mutable {
                            spdlog::warn("{} duration {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else {
                fan_out_request<policy::select_all>(
                    to_actor_vector(sub_playheads_), infinite, source_atom_v)
                    .then(
                        [=](const std::vector<caf::actor>) mutable {
                            // now sending duration_flicks to ourselves means that duration,
                            // in/out points etc. are ensured to be up-to-date before we deliver
                            // the response
                            mail(duration_flicks_atom_v)
                                .request(caf::actor_cast<caf::actor>(this), infinite)
                                .then(
                                    [=](timebase::flicks) mutable { rp.deliver(true); },
                                    [=](const error &err) mutable {
                                        spdlog::warn(
                                            "{} duration {}",
                                            __PRETTY_FUNCTION__,
                                            to_string(err));
                                        rp.deliver(err);
                                    });
                        },
                        [=](const error &err) mutable {
                            spdlog::warn("{} Fan {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            }

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
                mail(utility::name_atom_v)
                    .request(media_source.actor(), infinite)
                    .then(
                        [=](const std::string name) {
                            updating_source_list_ = true;
                            if (mt == media::MT_IMAGE) {
                                image_source_->set_value(to_string(media_source.uuid()), false);
                                image_source_name_->set_value(name);
                            } else if (mt == media::MT_AUDIO) {
                                audio_source_->set_value(to_string(media_source.uuid()), false);
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
                    image_source_name_->set_value("-");
                } else if (mt == media::MT_AUDIO) {
                    audio_source_->set_value("-");
                }
                updating_source_list_ = false;
            }
        },

        [=](utility::event_atom,
            media_reader::get_thumbnail_atom,
            const thumbnail::ThumbnailBufferPtr &buf) {},

        [=](utility::event_atom, media::media_status_atom, const media::MediaStatus ms) {},

        [=](utility::event_atom,
            media::media_display_info_atom,
            const utility::JsonStore &,
            caf::actor_addr &) {},

        [=](utility::event_atom, media::media_display_info_atom, const utility::JsonStore &) {},

        // controls creation and destruction of children
        [&](utility::event_atom, utility::change_atom) {
            if (current_sender() != this) {
                // change has bubbled up from a child playhead, force a redraw
                current_media_uuid_->set_value(to_string(utility::Uuid()));
                current_media_source_uuid_->set_value(to_string(utility::Uuid()));

                anon_mail(jump_atom_v).send(this);
                mail(utility::event_atom_v, utility::change_atom_v).send(event_group_);
            } else {
                anon_mail(jump_atom_v).send(this);
            }
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::get_group_atom) -> caf::actor { return broadcast_; },

        [=](broadcast::join_broadcast_atom atom, caf::actor joiner) {
            return mail(atom, joiner).delegate(broadcast_);
        },

        [=](broadcast::leave_broadcast_atom, caf::actor leaver) {
            return mail(broadcast::leave_broadcast_atom_v, leaver).delegate(broadcast_);
        },

        [=](utility::rate_atom atom) -> result<FrameRate> {
            auto rp = make_response_promise<FrameRate>();
            mail(utility::rate_atom_v)
                .request(hero_sub_playhead_.actor(), infinite)
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
            auto rp = make_response_promise<JsonStore>();

            // We need to update the offsets per sub-playhead before completing
            // serialisation ...
            auto ct = std::make_shared<int>(sub_playheads_.size());
            source_alignment_values_->set_value(std::vector<int>(sub_playheads_.size(), 0));
            if (sub_playheads_.empty()) {
                rp.deliver(serialise());
                return rp;
            }

            for (size_t idx = 0; idx < sub_playheads_.size(); ++idx) {

                mail(media::source_offset_frames_atom_v)
                    .request(sub_playheads_[idx].actor(), infinite)
                    .then(
                        [=](const int64_t offset) mutable {
                            auto r = source_alignment_values_->value();
                            if (idx < r.size())
                                r[idx] = offset;
                            source_alignment_values_->set_value(r);
                            (*ct)--;
                            if (!(*ct)) {
                                rp.deliver(serialise());
                            }
                        },
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            (*ct)--;
                            if (!(*ct)) {
                                rp.deliver(serialise());
                            }
                        });
            }
            return rp;
        },

        [=](module::deserialise_atom, const utility::JsonStore &j) -> result<bool> {
            // before we de-serialise, we have to be sure that we have been set-up with the
            // playback sources provided by the playhead selection actor by waiting for this
            // self-message to return
            auto rp = make_response_promise<bool>();
            mail(
                playlist::selection_actor_atom_v,
                caf::actor_cast<caf::actor>(playlist_selection_addr_))
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](bool) mutable {
                        if (j.is_null()) {
                            rp.deliver(false);
                            return;
                        }

                        deserialise(j);
                        auto layouts_manager =
                            self()->home_system().registry().template get<caf::actor>(
                                viewport_layouts_manager);

                        // following desrialisation, we set-up the compare mode and set the
                        // frame offsets per sub-playhead
                        mail(playhead::compare_mode_atom_v, compare_mode_->value())
                            .request(layouts_manager, infinite)
                            .then(
                                [=](std::pair<
                                    xstudio::playhead::AutoAlignMode,
                                    xstudio::playhead::AssemblyMode> mode) mutable {
                                    set_assembly_mode(mode.second);
                                    source_actors_.clear();
                                    new_source_list();
                                    align_clip_frame_numbers();
                                    align_audio_playhead();

                                    auto offset_frames = source_alignment_values_->value();
                                    int idx            = 0;
                                    for (auto &sub_playhead : sub_playheads_) {
                                        if (idx < offset_frames.size())
                                            anon_mail(
                                                media::source_offset_frames_atom_v,
                                                (int64_t)offset_frames[idx++],
                                                false)
                                                .send(sub_playhead.actor());
                                    }
                                    notify_loop_end_changed();
                                    notify_loop_start_changed();
                                    mail(utility::event_atom_v, loop_atom_v, loop())
                                        .send(event_group_);
                                    rp.deliver(true);
                                },
                                [=](caf::error &err) mutable {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });

                        // broadcast the compare mode to viewport(s) that are attached to this
                        // playhead
                        mail(compare_mode_atom_v, compare_mode_->value())
                            .send(viewport_events_group_);
                    },
                    [=](caf::error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(err);
                    });
            return rp;
        },

        [=](velocity_atom) -> float { return velocity(); },

        [=](velocity_multiplier_atom) -> float { return velocity_multiplier(); },

        [=](velocity_multiplier_atom, const float velocity_multiplier) -> unit_t {
            set_velocity_multiplier(velocity_multiplier);
            mail(utility::event_atom_v, velocity_multiplier_atom_v, velocity_multiplier)
                .send(event_group_);
            mail(utility::event_atom_v, velocity_multiplier_atom_v, velocity_multiplier)
                .send(fps_moniotor_group_);
            return unit;
        },

        [=](full_precache_atom,
            const bool all_playheads,
            const bool force,
            const std::vector<utility::UuidActor> sub_playheads) {
            if (sub_playheads == sub_playheads_) {
                if (all_playheads) {
                    for (auto &ph : sub_playheads) {
                        anon_mail(full_precache_atom_v, true, force).send(ph.actor());
                    }
                } else if (hero_sub_playhead_) {
                    anon_mail(full_precache_atom_v, true, force)
                        .send(hero_sub_playhead_.actor());
                }

                if (audio_playhead_) {
                    anon_mail(full_precache_atom_v, true, force).send(audio_playhead_);
                }
            }
        },

        [=](clear_precache_queue_atom,
            const std::vector<utility::UuidActor> &old_sub_playheads,
            const utility::UuidActor &video_string_out_actor) {
            // this (delayed) message comes from clear_child_playheads method.
            mail(child_playheads_deleted_atom_v, to_uuid_vector(old_sub_playheads))
                .send(broadcast_);
            for (auto &i : old_sub_playheads) {
                unlink_from(i.actor());
                send_exit(i.actor(), caf::exit_reason::user_shutdown);
            }
            if (video_string_out_actor) {
                unlink_from(video_string_out_actor.actor());
                send_exit(video_string_out_actor.actor(), caf::exit_reason::user_shutdown);
            }
        },

        [=](bool) {});
}

void PlayheadActor::connect_to_playlist_selection_actor(
    caf::actor playlist_selection, caf::typed_response_promise<bool> rp) {

    if (playlist_selection) {

        // Here we subscribe to event messages from a PlaylistSelectionActor -
        // this will push lists of media to the playhead, when media is selected
        // from a playlist for playback. See 'new_source_list'.

        // Note that we make sure we've joined the event group, before then
        // requesting the current selected sources from the PlaylistSelectionActor
        // This avoids a race condition where the PlaylistSelectionActor selection
        // changes before we have joined the event group but after we make our
        // direct request to get the current selection.
        playlist_selection_addr_ = caf::actor_cast<caf::actor_addr>(playlist_selection);
        mail(utility::get_event_group_atom_v)
            .request(playlist_selection, caf::infinite)
            .then(
                [=](caf::actor grp) mutable {
                    mail(broadcast::join_broadcast_atom_v)
                        .request(grp, caf::infinite)
                        .then(
                            [=](const bool) mutable {
                                if (!contact_sheet_mode_) {
                                    mail(playhead::get_selected_sources_atom_v)
                                        .request(playlist_selection, infinite)
                                        .then(
                                            [=](const utility::UuidActorVector
                                                    &selection) mutable {
                                                dynamic_source_actors_ = selection;
                                                new_source_list();
                                                rp.deliver(true);
                                            },
                                            [=](const caf::error &e) mutable {
                                                spdlog::warn(
                                                    "{} {}", __PRETTY_FUNCTION__, to_string(e));
                                                rp.deliver(false);
                                            });
                                } else {
                                    rp.deliver(true);
                                }
                            },
                            [=](const error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(false);
                            });
                },
                [=](const error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else {
        rp.deliver(false);
    }
}

void PlayheadActor::clear_all_precache_requests(caf::typed_response_promise<bool> rp) {

    fan_out_request<policy::select_all>(to_actor_vector(sub_playheads_), infinite, uuid_atom_v)
        .then(
            [=](const std::vector<Uuid> uuids) mutable {
                // stop any read-ahead activity for these playheads
                mail(clear_precache_queue_atom_v, uuids)
                    .request(pre_reader_, infinite)
                    .then(
                        [=](bool r) mutable { rp.deliver(r); },
                        [=](const error &err) mutable { rp.deliver(err); });
            },
            [=](const error &err) mutable { rp.deliver(err); });
}

void PlayheadActor::clear_child_playheads() {

    // stop any read-ahead activity for these playheads
    anon_mail(clear_precache_queue_atom_v, to_uuid_vector(sub_playheads_)).send(pre_reader_);


    // send a message to delete these things in 2 seconds. We want
    // a delay as they may be fetching images and providing them to
    // the Viewport while we are setting up new sub-playheads (for
    // new sources)
    anon_mail(clear_precache_queue_atom_v, sub_playheads_, video_string_out_actor_)
        .delay(std::chrono::seconds(2))
        .send(this);

    sub_playheads_.clear();
    hero_sub_playhead_ = utility::UuidActor();
}

caf::actor PlayheadActor::make_child_playhead(utility::UuidActor source) {

    std::stringstream nmstr;
    nmstr << "ChildPlayhead" << sub_playheads_.size();

    const auto uuid   = utility::Uuid::generate();
    auto sub_playhead = utility::UuidActor(
        uuid,
        spawn<SubPlayhead>(
            nmstr.str(),
            source,
            actor_cast<actor>(this),
            int(sub_playheads_.size()),
            timeline_mode(),
            loop_start(),
            loop_end(),
            play_rate_mode(),
            playhead_rate(),
            media::MediaType::MT_IMAGE,
            uuid));

    link_to(sub_playhead.actor());
    sub_playheads_.push_back(sub_playhead);

    join_event_group(this, sub_playhead.actor());
    return sub_playhead;
}

void PlayheadActor::make_audio_child_playhead(const int source_index) {

    if (!audio_output_actor_)
        return;

    if (source_index >= (int)source_actors_.size())
        return;

    auto remove_retimer = [=]() {
        if (audio_playhead_retimer_) {
            unlink_from(audio_playhead_retimer_);
            send_exit(audio_playhead_retimer_, caf::exit_reason::user_shutdown);
            audio_playhead_retimer_ = caf::actor();
        }
    };

    if (timeline_mode()) {
        // Are we already hooked up to the timeline as the audio source?
        if (audio_src_ == timeline_actor_)
            return;

        audio_src_ = timeline_actor_;
        remove_retimer();

    } else {

        if (assembly_mode() == AM_STRING) {

            // source_actors_ is unchanged since we were last here?
            if (string_audio_sources_ == source_actors_)
                return;

            remove_retimer();

            string_audio_sources_   = source_actors_;
            audio_playhead_retimer_ = spawn<playhead::StringOutActor>(string_audio_sources_);
            link_to(audio_playhead_retimer_);
            audio_src_ = utility::UuidActor(utility::Uuid::generate(), audio_playhead_retimer_);

        } else {

            if (audio_src_ == source_actors_[source_index])
                return;
            audio_src_ = source_actors_[source_index];
            remove_retimer();
        }
    }

    if (audio_playhead_) {
        // delete the old audio sub-playhead
        unlink_from(audio_playhead_);
        send_exit(audio_playhead_, caf::exit_reason::user_shutdown);
    }

    audio_playhead_ = spawn<SubPlayhead>(
        "AudioPlayhead",
        audio_src_,
        actor_cast<actor>(this),
        -1,
        timeline_mode(),
        loop_start(),
        loop_end(),
        play_rate_mode(),
        playhead_rate(),
        media::MediaType::MT_AUDIO);

    join_event_group(this, audio_playhead_);
}

void PlayheadActor::new_source_list(const bool force_build) {

    timeline_mode_->set_value(timeline_mode());

    // if we're not connected to the UI - i.e. if we don't have a viewport
    // connected to us, then we don't need to build our subplayheads for
    // image delivery
    if (!connected_to_ui() && !offscreen_only_ && !force_build)
        return;

    const auto &sl = timeline_mode() ? timeline_track_actors_ : dynamic_source_actors_;

    if (sl == source_actors_)
        return;

    source_actors_ = sl;

    if (timeline_mode()) {
        rebuild_from_timeline_sources();
    } else {
        rebuild_from_dynamic_sources();
    }

    if (previous_source_actors_ != source_actors_) {

        // One last check ... do we have a completely new set of sources AND
        // previous_source_actors_ is not empty (i.e. we're not entering this
        // part of the code for the first time)?
        // If so, we should turn off any loop region as we are now viewing
        // completely different media to what was being viewed before.
        bool match = false;
        for (const auto &a : previous_source_actors_) {
            for (const auto &b : source_actors_) {
                if (a == b) {
                    match = true;
                    break;
                }
            }
        }

        if (previous_source_actors_.size() && !match) {

            set_loop_start(timebase::k_flicks_low);
            set_loop_end(timebase::k_flicks_max);
            set_use_loop_range(false);
        }
        notify_loop_start_changed();
        notify_loop_end_changed();
        previous_source_actors_ = source_actors_;
    }

    // finally,. apply auto-alignment
    if (!(assembly_mode() == AM_ONE || assembly_mode() == AM_STRING)) {

        // for A/B mode, grid mode etc we need the child playheads to match
        // their durations so that they map to a common (shared) timeline
        align_clip_frame_numbers();
    }

    align_audio_playhead();
}

void PlayheadActor::rebuild_from_timeline_sources() {

    clear_child_playheads();

    if (!timeline_actor_) {

        make_child_playhead(empty_clip_);
        switch_key_playhead(0);
        set_position(timebase::k_flicks_zero_seconds);

        // broadcast and empty buffer to clear the viewport
        anon_mail(show_atom_v, hero_sub_playhead_.uuid(), ImageBufPtr(), true).send(this);

    } else if (assembly_mode() == AM_STRING || assembly_mode() == AM_ONE) {

        // if there's just one video track or we're not in a video compare mode
        // we just play the timeline
        make_child_playhead(timeline_actor_);
        switch_key_playhead(0);

    } else {

        // compare timeline video tracks
        int count = 1;
        for (auto source : timeline_track_actors_) {

            make_child_playhead(source);

            // in grid, A/B compare modes etc we must limit the number of child playheads
            // in the case that the user has, say, selected 100 clips as it's too many for
            // the UI to cope with.
            if (assembly_mode() != AM_ONE && count > max_compare_sources_->value()) {
                spdlog::warn(
                    "{} {} {}",
                    __PRETTY_FUNCTION__,
                    "Trying to compare too many things, limiting to first ",
                    max_compare_sources_->value());
                break;
            } else if (assembly_mode() == AM_TEN && count == 10) {
                break;
            }
            count++;
        }

        // passing a -1 as the index forces a search for a child playhead that
        // is showing the current on-screen source
        switch_key_playhead(-1);

        match_video_track_durations();
    }

    num_sub_playheads_->set_value(sub_playheads_.size());

    mail(key_child_playhead_atom_v, to_uuid_vector(sub_playheads_)).send(broadcast_);

    anon_mail(duration_flicks_atom_v).send(this);
}

void PlayheadActor::rebuild_from_dynamic_sources() {

    clear_child_playheads();

    if (!source_actors_.size()) {

        make_child_playhead(empty_clip_);
        switch_key_playhead(0);
        set_position(timebase::k_flicks_zero_seconds);

        // broadcast and empty buffer to clear the viewport
        anon_mail(show_atom_v, hero_sub_playhead_.uuid(), ImageBufPtr(), true).send(this);


    } else if (source_actors_.size() == 1 || assembly_mode() == AM_ONE) {

        int count = 1;
        for (auto source : source_actors_) {
            make_child_playhead(source);
        }
        // passing a -1 as the index forces a search for a child playhead that
        // is showing the current on-screen source
        switch_key_playhead(-1);

    } else if (assembly_mode() == AM_STRING) {

        video_string_out_actor_ = utility::UuidActor(
            utility::Uuid::generate(), spawn<playhead::StringOutActor>(source_actors_));
        link_to(video_string_out_actor_.actor());
        make_child_playhead(video_string_out_actor_);
        switch_key_playhead(0);

    } else {

        int count = 1;
        for (auto source : source_actors_) {

            make_child_playhead(source);

            // in grid, A/B compare modes etc we must limit the number of child playheads
            // in the case that the user has, say, selected 100 clips as it's too many for
            // the UI to cope with.
            if (assembly_mode() != AM_ONE && count > max_compare_sources_->value()) {
                spdlog::warn(
                    "{} {} {}",
                    __PRETTY_FUNCTION__,
                    "Trying to compare too many things, limiting to first ",
                    max_compare_sources_->value());
                break;
            } else if (assembly_mode() == AM_TEN && count == 10) {
                break;
            }
            count++;
        }
        // passing a -1 as the index forces a search for a child playhead that
        // is showing the current on-screen source
        switch_key_playhead(-1);
    }

    num_sub_playheads_->set_value(sub_playheads_.size());

    mail(key_child_playhead_atom_v, to_uuid_vector(sub_playheads_)).send(broadcast_);
}

void PlayheadActor::switch_key_playhead(int idx) {

    if (idx < sub_playheads_.size() && idx >= 0 && hero_sub_playhead_ == sub_playheads_[idx])
        return;

    caf::scoped_actor sys(system());
    bool force_move        = false;
    const bool new_sources = idx == -1;
    if (idx < 0) {
        // let's see if one of the child playheads is showing a media source that matches
        // current_media_uuid_, if so we'll pick that one.
        try {
            int i = 0;
            for (auto &ph : sub_playheads_) {

                // this ensures that the child playhead is up-to-date and has
                // got all the data it needs from its source
                request_receive<caf::actor>(*sys, ph.actor(), source_atom_v);

                if (to_string(request_receive<utility::UuidActor>(
                                  *sys, ph.actor(), media_source_atom_v, true)
                                  .uuid()) == current_media_source_uuid_->value()) {
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
            spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
            idx = 0;
        }
    }

    if (idx >= 0 && idx < (int)sub_playheads_.size()) {

        hero_sub_playhead_ = sub_playheads_[idx];
        key_playhead_index_->set_value(idx);
        anon_mail(bookmark::get_bookmarks_atom_v).send(hero_sub_playhead_.actor());

        if (assembly_mode() != AM_STRING && assembly_mode() != AM_ONE &&
            sub_playheads_.size() > 1 && !new_sources) {

            // If we're in A/B mode, say, with multiple things selected we want to be
            // able to rapidly switch the on-screen source via 1,2,3 hotkeys. This
            // message will get to the ViewportFrameQueueActor to tell it the 'hero'
            // sub playhead has changed. It then does an immediate frame retrieve
            // and disaplay to get 'snappy' switching.
            mail(key_child_playhead_atom_v, hero_sub_playhead_, utility::clock::now())
                .send(broadcast_);
        }

        try {

            auto source_actor =
                request_receive<caf::actor>(*sys, hero_sub_playhead_.actor(), source_atom_v);

            make_audio_child_playhead(idx);
            // this should update the 'image_source_' attribute so it shows the correct
            // list of available sources in the UI
            auto media_actor =
                request_receive<caf::actor>(*sys, hero_sub_playhead_.actor(), media_atom_v);
            if (media_actor)
                current_media_changed(media_actor);

            // send the change notification
            mail(utility::event_atom_v, utility::change_atom_v).send(event_group_);
            mail(utility::event_atom_v, playhead::key_playhead_index_atom_v, idx)
                .send(event_group_);

            check_if_loop_range_makes_sense();

            // 'jump' to the last viewed frame of the current on-screen source
            if (!timeline_mode()) {
                if (assembly_mode() == AM_STRING) {
                    move_playhead_to_last_viewed_frame_of_given_source(
                        utility::Uuid(current_media_source_uuid_->value()));
                } else if (assembly_mode() == AM_ONE || force_move) {
                    move_playhead_to_last_viewed_frame_of_current_source();
                } else {
                    update_child_playhead_positions(true);
                }
            } else {
                update_child_playhead_positions(true);
            }

            const auto switchpoint = utility::clock::now();

            // this will trigger an update to the duration, at which point we can
            // update other dependent data like loop range etc.
            mail(duration_flicks_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](timebase::flicks) {
                        // send update events on properties of the playhead that depend on the
                        // child

                        notify_offset_changed();
                        update_playback_rate();
                        rebuild_cached_frames_status();
                        restart_readahead_cacheing(assembly_mode() != AM_ONE);

                        // if 'switch_key_playhead' is called rapidly, the broadcast made below
                        // can reach the receiver out of order, so we need to give it a
                        // timestamp so they can know if they have got an out-of-order
                        // notification and ignore it
                        mail(key_child_playhead_atom_v, hero_sub_playhead_, switchpoint)
                            .send(broadcast_);

                        align_audio_playhead();
                    },
                    [=](const caf::error &err) {
                        spdlog::warn(
                            "{} {} {}", Module::name(), __PRETTY_FUNCTION__, to_string(err));
                    });

            mail(media_frame_ranges_atom_v)
                .request(hero_sub_playhead_.actor(), infinite)
                .then(
                    [=](const std::vector<int> &media_frame_ranges) {
                        media_transition_frames_->set_value(media_frame_ranges);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });

            // kick the key sub-playhead to re-broadcast its current Media, which thisw
            // PlayheadActor receives and re-broadcasts
            if (idx < sub_playheads_.size()) {
                mail(media_atom_v, true).send(sub_playheads_[idx].actor());
            }
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

void PlayheadActor::update_child_playhead_positions(const bool force_broadcast) {

    if (audio_output_actor_) {
        anon_mail(
            position_atom_v,
            adjusted_position(),
            forward(),
            velocity(),
            playing(),
            last_playhead_set_timepoint(),
            audio_path_ == playhead::GLOBAL_AUDIO,
            uuid())
            .send(audio_output_actor_);
    }

    if (audio_playhead_) {
        anon_mail(
            jump_atom_v,
            adjusted_position(),
            forward(),
            velocity(),
            playing(),
            force_broadcast,
            connected_to_ui(),
            user_is_frame_scrubbing_->value())
            .send(audio_playhead_);
    }

    if (assembly_mode() == AM_ONE) {

        anon_mail(
            jump_atom_v,
            position(),
            forward(),
            velocity(),
            playing(),
            force_broadcast,
            connected_to_ui(),
            user_is_frame_scrubbing_->value())
            .send(hero_sub_playhead_.actor());

    } else {

        for (const auto &i : sub_playheads_) {
            anon_mail(
                jump_atom_v,
                position(),
                forward(),
                velocity(),
                playing(),
                force_broadcast,
                connected_to_ui(),
                user_is_frame_scrubbing_->value())
                .send(i.actor());
        }
    }

    if (user_is_frame_scrubbing_->value()) {
        // if the user is scrubbing make sure that we aren't trying
        // to do lookahead reads for playback
        for (const auto &i : sub_playheads_) {
            anon_mail(full_precache_atom_v, false, false).send(i.actor());
        }
    }
}

void PlayheadActor::notify_loop_end_changed() {

    for (const auto &i : sub_playheads_) {
        anon_mail(simple_loop_end_atom_v, loop_end()).send(i.actor());
    }
    if (audio_playhead_) {
        anon_mail(simple_loop_end_atom_v, loop_end()).send(audio_playhead_);
    }

    update_child_playhead_positions(false);

    mail(flicks_to_logical_frame_atom_v, loop_end())
        .request(hero_sub_playhead_.actor(), infinite)
        .then(

            [=](const int loop_end) {
                loop_end_frame_->set_value(loop_end, false);
                mail(utility::event_atom_v, simple_loop_end_atom_v, loop_end)
                    .send(event_group_);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::notify_loop_start_changed() {

    for (const auto &i : sub_playheads_) {
        anon_mail(simple_loop_start_atom_v, loop_start()).send(i.actor());
    }
    if (audio_playhead_) {
        anon_mail(simple_loop_start_atom_v, loop_start()).send(audio_playhead_);
    }

    update_child_playhead_positions(false);

    mail(flicks_to_logical_frame_atom_v, loop_start())
        .request(hero_sub_playhead_.actor(), infinite)
        .then(

            [=](const int loop_start) {
                loop_start_frame_->set_value(loop_start, false);
                mail(utility::event_atom_v, simple_loop_start_atom_v, loop_start)
                    .send(event_group_);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::notify_offset_changed() {

    mail(media::source_offset_frames_atom_v)
        .request(hero_sub_playhead_.actor(), infinite)
        .then(
            [=](const int64_t offset) { source_offset_frames_->set_value(offset, false); },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::update_duration(caf::typed_response_promise<timebase::flicks> rp) {

    mail(duration_flicks_atom_v)
        .request(hero_sub_playhead_.actor(), infinite)
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
                        mail(utility::event_atom_v, use_loop_range_atom_v, use_loop_range())
                            .send(event_group_);
                    }
                } else {
                    set_duration(duration);
                }
                duration_seconds_->set_value(timebase::to_seconds(duration));
                rp.deliver(duration);
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });

    mail(duration_frames_atom_v)
        .request(hero_sub_playhead_.actor(), infinite)
        .then(
            [=](const size_t duration) {
                duration_frames_->set_value(duration);
                mail(utility::event_atom_v, duration_frames_atom_v, duration)
                    .send(event_group_);
            },
            [=](const error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::jump_to_source(const utility::Uuid &media_uuid) {
    if (assembly_mode() == AM_STRING) {

        move_playhead_to_last_viewed_frame_of_given_source(media_uuid);

    } else {

        for (size_t idx = 0; idx < sub_playheads_.size(); ++idx) {
            mail(playlist::get_media_uuid_atom_v)
                .request(sub_playheads_[idx].actor(), infinite)
                .await(
                    [=](const utility::Uuid &playhead_media_uuid) {
                        if (playhead_media_uuid == media_uuid) {
                            switch_key_playhead(idx);
                            if (assembly_mode() == AM_ONE) {
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
    if (hero_sub_playhead_ && play_rate_mode() == utility::TimeSourceMode::DYNAMIC) {
        mail(actual_playback_rate_atom_v)
            .request(hero_sub_playhead_.actor(), infinite)
            .then(
                [=](const utility::FrameRate &rate) {
                    if (rate != playhead_rate()) {
                        set_playhead_rate(rate);
                        notify_loop_end_changed();
                        notify_loop_start_changed();
                    }
                    mail(utility::event_atom_v, actual_playback_rate_atom_v, rate)
                        .send(fps_moniotor_group_);
                },
                [=](const error &) {
                    // no media, fallback to 'default' playback rate
                    mail(utility::event_atom_v, playhead_rate_atom_v, playhead_rate())
                        .send(event_group_);
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

    std::pair<int, int> r;

    auto count = static_cast<int>(all_frame_ids_.size());
    auto scale = (count / 2048) + 1;

    utility::JsonStore cached_frames = nlohmann::json::array();

    int prev_frame_status = -1;

    nlohmann::json j;
    j["start"]      = 0;
    j["duration"]   = 0;
    j["colour_idx"] = 0;

    // this data is used in XsViewerTimeline.qml to draw the cached status.
    // We use the status idx as follows:
    // 0: frame in cache
    // 1: frame is in the cache, but is a held frame
    // 2: frame is not in the cache and is not available on disk
    // For frames not in the cache but possibly on disk we don't draw anything

    for (auto i = 0; i < count; i += scale) {

        int frame_status = -1;
        if (all_frame_ids_[i]) {
            if (frames_cached_.count(all_frame_ids_[i]->key())) {
                // frame is cached - but is it a held frame?
                if (all_frame_ids_[i]->frame_status() == media::FS_HELD_FRAME)
                    frame_status = 1;
                else if (all_frame_ids_[i]->frame_status() == media::FS_NOT_ON_DISK)
                    frame_status = 2;
                else
                    frame_status = 0;
            } else if (all_frame_ids_[i]->frame_status() == media::FS_NOT_ON_DISK) {
                // not in cache but marked as not on disk
                frame_status = 2;
            } else {
                // not cached (yet)
                frame_status = -1;
            }
        }

        if (prev_frame_status != frame_status) {
            if (prev_frame_status != -1) {
                j["duration"]   = i - j["start"].get<int>();
                j["colour_idx"] = prev_frame_status;
                cached_frames.push_back(j);
            }
            prev_frame_status = frame_status;
            j["start"]        = i;
        }
    }

    if (prev_frame_status != -1) {
        j["duration"]   = count - j["start"].get<int>();
        j["colour_idx"] = prev_frame_status;
        cached_frames.push_back(j);
    }

    cached_frames_->set_value(utility::JsonStore(cached_frames));
}

void PlayheadActor::rebuild_cached_frames_status() {
    if (hero_sub_playhead_ && image_cache_) {
        // fetch the full list of keys for every frame in the timeline
        mail(media_cache::keys_atom_v)
            .request(hero_sub_playhead_.actor(), infinite)
            .then(

                [=](const media::AVFrameIDs &frame_ids) mutable {
                    all_frame_ids_ = frame_ids;

                    // now fetch the full list of keys for frames that are in the cache
                    mail(media_cache::keys_atom_v)
                        .request(image_cache_, infinite)
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
        all_frame_ids_.clear();
        frames_cached_.clear();
        update_cached_frames_status();
    }
}

void PlayheadActor::match_video_track_durations() {


    if (sub_playheads_.empty())
        return;

    scoped_actor sys{system()};

    // get the max duration of video tracks
    auto max_duration = std::numeric_limits<timebase::flicks>::lowest();
    auto timeout      = std::chrono::milliseconds(500);

    for (auto &sub_playhead : sub_playheads_) {

        try {
            max_duration = std::max(
                max_duration,
                request_receive_wait<timebase::flicks>(
                    *sys,
                    sub_playhead.actor(),
                    timeout,
                    playhead::duration_flicks_atom_v,
                    true // bool here means we get the duration *before* retiming/extension is
                         // done
                    ));
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    // now extend the range of each sub playhead to match the max duration
    for (auto &sub_playhead : sub_playheads_) {

        try {
            request_receive_wait<bool>(
                *sys, sub_playhead.actor(), timeout, timeline::duration_atom_v, max_duration);
        } catch (...) {
        }
    }
}

void PlayheadActor::align_audio_playhead() {

    try {

        if (!audio_playhead_)
            return;

        scoped_actor sys{system()};

        auto timeout = std::chrono::milliseconds(500);

        if (timeline_mode()) {

            // get the offset in frames applied to the hero playhead
            int64_t hero_offset = request_receive_wait<int64_t>(
                *sys, hero_sub_playhead_.actor(), timeout, media::source_offset_frames_atom_v);

            // apply the offset to line up the audio to the hero source
            request_receive_wait<bool>(
                *sys,
                audio_playhead_,
                timeout,
                media::source_offset_frames_atom_v,
                hero_offset,
                true // reset the duration
            );

            // now get duration of hero playhead
            auto dur = request_receive_wait<timebase::flicks>(
                *sys, hero_sub_playhead_.actor(), timeout, duration_flicks_atom_v);

            // set the audio playhead to match
            request_receive_wait<bool>(
                *sys, audio_playhead_, timeout, timeline::duration_atom_v, dur);

        } else {

            // get the first frame of the hero playhead
            const auto hero_first_frame = request_receive_wait<media::AVFrameID>(
                *sys, hero_sub_playhead_.actor(), timeout, first_frame_media_pointer_atom_v);

            // get the offset in frames applied to the hero playhead
            int64_t hero_offset = request_receive_wait<int64_t>(
                *sys, hero_sub_playhead_.actor(), timeout, media::source_offset_frames_atom_v);

            // get the first frame of the hero playhead
            const auto audio_first_frame = request_receive_wait<media::AVFrameID>(
                *sys, audio_playhead_, timeout, first_frame_media_pointer_atom_v);

            // apply the offset to line up the audio to the hero source
            request_receive_wait<bool>(
                *sys,
                audio_playhead_,
                timeout,
                media::source_offset_frames_atom_v,
                int(hero_first_frame.timecode().total_frames()) -
                    int(audio_first_frame.timecode().total_frames()) + hero_offset,
                true // reset the duration
            );

            // now get duration of hero playhead
            auto dur = request_receive_wait<timebase::flicks>(
                *sys, hero_sub_playhead_.actor(), timeout, duration_flicks_atom_v);

            // set the audio playhead to match
            request_receive_wait<bool>(
                *sys, audio_playhead_, timeout, timeline::duration_atom_v, dur);
        }

    } catch (std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void PlayheadActor::align_clip_frame_numbers() {

    try {

        if (sub_playheads_.size() < 2)
            return;

        scoped_actor sys{system()};

        auto timeout = std::chrono::milliseconds(500);

        // in timeline_mode we always comparing video tracks from the same timeline.
        // We therefore do not need to align or trim - we just extend the duration
        // of video tracks to match the longest.
        const bool align = timeline_mode() ? false : auto_align_mode() != AAM_ALIGN_OFF;
        const bool trim  = timeline_mode() ? false : auto_align_mode() == AAM_ALIGN_TRIM;

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

        if (align) {

            int first_frame =
                trim ? std::numeric_limits<int>::lowest() : std::numeric_limits<int>::max();

            for (auto &sub_playhead : sub_playheads_) {

                // get the first frame (timecode frames) of the source
                const int source_first_frame =
                    request_receive_wait<media::AVFrameID>(
                        *sys, sub_playhead.actor(), timeout, first_frame_media_pointer_atom_v)
                        .timecode()
                        .total_frames();

                // evaluate our overall first frame, depending on trim setting
                first_frame = trim ? std::max(first_frame, source_first_frame)
                                   : std::min(first_frame, source_first_frame);
            }

            for (auto sub_playhead : sub_playheads_) {

                const auto source_first_frame = request_receive_wait<media::AVFrameID>(
                    *sys, sub_playhead.actor(), timeout, first_frame_media_pointer_atom_v);

                int64_t frames_shift =
                    first_frame - (int)source_first_frame.timecode().total_frames();

                // apply the time offset
                request_receive_wait<bool>(
                    *sys,
                    sub_playhead.actor(),
                    timeout,
                    media::source_offset_frames_atom_v,
                    frames_shift,
                    true // reset the any duration override that might have been applied
                );
            }
        } else {

            for (auto sub_playhead : sub_playheads_) {

                // remove time offset
                request_receive_wait<bool>(
                    *sys,
                    sub_playhead.actor(),
                    timeout,
                    media::source_offset_frames_atom_v,
                    int64_t(0),
                    true // reset the duration
                );
            }
        }

        // now find the sub-playhead with the longest duration after the offset
        timebase::flicks dur = trim ? timebase::k_flicks_max : timebase::flicks(0);
        for (auto sub_playhead : sub_playheads_) {

            auto d = request_receive_wait<timebase::flicks>(
                *sys, sub_playhead.actor(), timeout, duration_flicks_atom_v);
            dur = trim ? std::min(d, dur) : std::max(d, dur);
        }

        for (auto sub_playhead : sub_playheads_) {

            request_receive_wait<bool>(
                *sys, sub_playhead.actor(), timeout, timeline::duration_atom_v, dur);
        }


        source_offset_frames_->set_value(
            request_receive_wait<int64_t>(
                *sys, hero_sub_playhead_.actor(), timeout, media::source_offset_frames_atom_v),
            false);

        // the cached frames display might need updating
        rebuild_cached_frames_status();

        if (position() > dur) {
            // make sure the playhead position is within the new
            // duration of aligned sources
            set_position(timebase::k_flicks_zero_seconds);
        }

    } catch (std::exception &e) {

        // supressing errors as 'No Frames' is thrown when the source is empty,
        // which isn't really an error
        spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void PlayheadActor::move_playhead_to_last_viewed_frame_of_current_source() {

    try {

        scoped_actor sys{system()};

        const auto uuid = request_receive<utility::UuidActor>(
                              *sys, hero_sub_playhead_.actor(), media_source_atom_v, true)
                              .uuid();

        if (uuid) {
            const auto p          = media_frame_per_media_uuid_.find(uuid);
            int last_viewed_frame = 0;
            if (p != media_frame_per_media_uuid_.end()) {
                last_viewed_frame = p->second;
            }

            const auto position = request_receive<timebase::flicks>(
                *sys,
                hero_sub_playhead_.actor(),
                media_frame_to_flicks_atom_v,
                uuid,
                last_viewed_frame);

            set_position(position);
        }
        anon_mail(jump_atom_v).send(this);

    } catch (std::exception &e) {

        anon_mail(jump_atom_v).send(this);

        // supressing errors as 'No Frames' is thrown when the source is empty,
        // which isn't really an error
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
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
            *sys,
            hero_sub_playhead_.actor(),
            media_frame_to_flicks_atom_v,
            source_uuid,
            last_viewed_frame);

        set_position(position);
        anon_mail(jump_atom_v).send(this);

    } catch (std::exception &e) {

        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
        move_playhead_to_last_viewed_frame_of_current_source();
    }
}

void PlayheadActor::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    if (role != module::Attribute::Value) {
        PlayheadBase::attribute_changed(attr_uuid, role);
        return;
    }

    if (attr_uuid == pinned_source_mode_->uuid()) {

        new_source_list();

    } else if (attr_uuid == auto_align_mode_->uuid()) {

        if (!(assembly_mode() == AM_ONE || assembly_mode() == AM_STRING)) {

            // for A/B mode, grid mode etc we need the child playheads to match
            // their durations so that they map to a common (shared) timeline
            align_clip_frame_numbers();
            align_audio_playhead();
            anon_mail(duration_flicks_atom_v).send(this);
        }

    } else if (attr_uuid == velocity_->uuid()) {
        mail(utility::event_atom_v, velocity_atom_v, velocity()).send(fps_moniotor_group_);
        mail(utility::event_atom_v, velocity_atom_v, velocity()).send(broadcast_);
    } else if (attr_uuid == velocity_multiplier_->uuid()) {
        mail(utility::event_atom_v, velocity_multiplier_atom_v, velocity_multiplier())
            .send(fps_moniotor_group_);
    } else if (attr_uuid == forward_->uuid()) {
        update_child_playhead_positions(true);
        mail(utility::event_atom_v, play_forward_atom_v, forward()).send(fps_moniotor_group_);
        mail(play_forward_atom_v, forward()).send(broadcast_);
    } else if (attr_uuid == image_source_->uuid() && !updating_source_list_) {
        switch_media_source(image_source_->value(), media::MT_IMAGE);
    } else if (attr_uuid == audio_source_->uuid() && !updating_source_list_) {
        switch_media_source(audio_source_->value(), media::MT_AUDIO);
    } else if (attr_uuid == image_stream_->uuid() && !updating_source_list_ && media_actor_) {
        switch_media_stream(media_actor_, image_stream_->value(), media::MT_IMAGE, true);
    } else if (attr_uuid == playing_->uuid()) {

        if (playing()) {

            revert_throttle();
            last_step_ = clock::now();

            // This is where playback starts! The PlayLoopActor pings this PlayheadBase
            // with 'step' atoms in a ping-pong loop at regular intervals determined
            // by the PlayheadBase base class. See step_atom message handlers in this file.
            spawn<PlayLoopActor>(this);

            // starts our pre-cache loop
            anon_mail(precache_atom_v).send(this);

        } else {
            // reset FFWD/FFRV when playback stops
            velocity_multiplier_->set_value(1.0, false);
        }

        mail(play_atom_v, playing()).send(broadcast_);
        mail(utility::event_atom_v, play_atom_v, playing()).send(playhead_media_events_group_);
        if (audio_output_actor_) {
            anon_mail(play_atom_v, playing(), audio_path_ == playhead::GLOBAL_AUDIO, uuid())
                .send(audio_output_actor_);
        }
        anon_mail(global::busy_atom_v, playing())
            .send(system().registry().template get<caf::actor>(global_registry));
        mail(utility::event_atom_v, play_atom_v, playing()).send(event_group_);
        mail(utility::event_atom_v, play_atom_v, playing()).send(fps_moniotor_group_);
        update_child_playhead_positions(true);

    } else if (attr_uuid == playhead_logical_frame_->uuid()) {

        anon_mail(jump_atom_v, playhead_logical_frame_->value())
            .send(caf::actor_cast<caf::actor>(this));

    } else if (attr_uuid == loop_end_frame_->uuid()) {

        try {

            scoped_actor sys{system()};
            // if loop end is set to frame 100, say, we need to include the
            // duration of frame 100. So we compute the flicks for frame 101 and
            // then subtract playback_step_increment, which ensures the loop
            // end is just inside the duration of frame 100.
            const timebase::flicks loop_end_flicks = request_receive<timebase::flicks>(
                *sys,
                hero_sub_playhead_.actor(),
                logical_frame_to_flicks_atom_v,
                loop_end_frame_->value() + 1);

            if (set_loop_end(loop_end_flicks - PlayheadBase::playback_step_increment)) {
                // position or loop end were also changed
                notify_loop_start_changed();
                update_child_playhead_positions(false);
            }
            notify_loop_end_changed();

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

    } else if (attr_uuid == loop_start_frame_->uuid()) {

        try {

            scoped_actor sys{system()};

            const timebase::flicks loop_start_flicks = request_receive<timebase::flicks>(
                *sys,
                hero_sub_playhead_.actor(),
                logical_frame_to_flicks_atom_v,
                loop_start_frame_->value());
            if (set_loop_start(loop_start_flicks)) {
                // position or loop end were also changed
                notify_loop_end_changed();
                update_child_playhead_positions(false);
            }
            notify_loop_start_changed();
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

    } else if (attr_uuid == loop_mode_->uuid()) {
        notify_loop_end_changed();
        notify_loop_start_changed();
        mail(utility::event_atom_v, loop_atom_v, loop()).send(event_group_);
    } else if (attr_uuid == key_playhead_index_->uuid()) {
        switch_key_playhead(key_playhead_index_->value());
    } else if (attr_uuid == loop_range_enabled_->uuid()) {
        notify_loop_start_changed();
        notify_loop_end_changed();
    } else if (attr_uuid == source_offset_frames_->uuid()) {
        mail(media::source_offset_frames_atom_v, source_offset_frames_->value(), false)
            .request(hero_sub_playhead_.actor(), infinite)
            .then(
                [=](const bool changed) {
                    if (changed) {
                        // update the audio playhead offset
                        align_audio_playhead();
                        // force broacast image buffers so we see the source
                        // offset change in viewport etc.
                        anon_mail(jump_atom_v).send(this);
                    }
                },
                [=](caf::error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else if (attr_uuid == compare_mode_->uuid()) {

        auto layouts_manager =
            self()->home_system().registry().template get<caf::actor>(viewport_layouts_manager);

        // get the assembly mode for the new compare mode
        mail(playhead::compare_mode_atom_v, compare_mode_->value())
            .request(layouts_manager, infinite)
            .then(
                [=](std::pair<xstudio::playhead::AutoAlignMode, xstudio::playhead::AssemblyMode>
                        mode) {
                    if (mode.second != assembly_mode()) {
                        set_assembly_mode(mode.second);
                        source_actors_.clear();
                        new_source_list();
                        // get the default align mode
                    }

                    align_clip_frame_numbers();
                    align_audio_playhead();
                    anon_mail(duration_flicks_atom_v).send(this);

                    // Note: not using the default align mode provided by the
                    // layout plugin, because we are driving this with a preference
                    // per container type (Playlsit, Subset, Contact Sheet etc)

                    // set_auto_align_mode(mode.first);
                },
                [=](caf::error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });

        // broadcast the compare mode to viewport(s) that are attached to this
        // playhead
        mail(compare_mode_atom_v, compare_mode_->value()).send(viewport_events_group_);

    } else if (attr_uuid == connect_to_ui_attr_->uuid()) {
        if (!connected_to_ui())
            connect_to_ui();
    } else {
        PlayheadBase::attribute_changed(attr_uuid, role);
    }
}

void PlayheadActor::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string &context, const std::string &window) {

    // If the context starts with 'viewport' the hotkey was hit while a viewport
    // had mouse focus. If that viewport is NOT attached to this playhead we
    // ignore the hotkey
    if (active_viewports_.empty() ||
        (context.find("viewport") == 0 &&
         active_viewports_.find(context) == active_viewports_.end())) {
        return;
    }

    if (hotkey_uuid == move_selection_down_hotkey_) {

        step_to_next_media(true);

    } else if (hotkey_uuid == move_selection_up_hotkey_) {

        step_to_next_media(false);

    } else if (hotkey_uuid == jump_to_previous_note_hotkey_) {
        anon_mail(skip_to_bookmark_atom_v, false).send(caf::actor_cast<caf::actor>(this));
    } else if (hotkey_uuid == jump_to_next_note_hotkey_) {
        anon_mail(skip_to_bookmark_atom_v, true).send(caf::actor_cast<caf::actor>(this));
    } else if (
        switch_key_playhead_hotkeys_.find(hotkey_uuid) != switch_key_playhead_hotkeys_.end()) {
        switch_key_playhead(switch_key_playhead_hotkeys_[hotkey_uuid]);
    } else if (hotkey_uuid == set_loop_in_) {
        anon_mail(simple_loop_start_atom_v, position()).send(caf::actor_cast<caf::actor>(this));
    } else if (hotkey_uuid == set_loop_out_) {

        mail(flicks_to_logical_frame_atom_v, position())
            .request(hero_sub_playhead_.actor(), infinite)
            .then(
                [=](int logical) { loop_end_frame_->set_value(logical); },
                [=](caf::error &err) {

                });

    } else if (hotkey_uuid == step_forward_) {

        set_playing(false);
        anon_mail(step_atom_v, 1).send(caf::actor_cast<caf::actor>(this));

        // make some random number as in 'ID' for the key press. If the key
        // is STILL being pressed 500ms later (we know this if step_keypress_event_id_
        // matches the value in the delayed message), we start playback
        step_keypress_event_id_ = (double)rand() / RAND_MAX;
        anon_mail(play_atom_v, step_keypress_event_id_)
            .delay(std::chrono::milliseconds(500))
            .send(this);
    } else if (hotkey_uuid == step_backward_) {
        set_playing(false);
        anon_mail(step_atom_v, -1).send(caf::actor_cast<caf::actor>(this));
        step_keypress_event_id_ = -(double)rand() / RAND_MAX;
        anon_mail(play_atom_v, step_keypress_event_id_)
            .delay(std::chrono::milliseconds(500))
            .send(this);
    } else if (hotkey_uuid == jump_to_first_frame_) {

        if (loop_start_frame_->value() > 0 && loop_range_enabled_->value()) {
            anon_mail(jump_atom_v, loop_start_frame_->value())
                .send(caf::actor_cast<caf::actor>(this));
        } else {
            anon_mail(jump_atom_v, 0).send(caf::actor_cast<caf::actor>(this));
        }

    } else if (hotkey_uuid == jump_to_last_frame_) {

        if (loop_end_frame_->value() > 0 && loop_range_enabled_->value()) {
            anon_mail(jump_atom_v, loop_end_frame_->value())
                .send(caf::actor_cast<caf::actor>(this));
        } else {
            anon_mail(jump_atom_v, std::numeric_limits<int>::max())
                .send(caf::actor_cast<caf::actor>(this));
        }

    } else if (hotkey_uuid == cycle_image_layer_up_) {

        auto stream_names = image_stream_->get_role_data<std::vector<std::string>>(
            module::Attribute::StringChoices);
        auto curr_stream = image_stream_->value();
        auto p           = std::find(stream_names.begin(), stream_names.end(), curr_stream);
        if (p != stream_names.end()) {
            size_t idx = std::distance(stream_names.begin(), p);
            if (idx > 0) {
                image_stream_->set_value(stream_names[idx - 1]);
            }
        }

    } else if (hotkey_uuid == cycle_image_layer_down_) {

        auto stream_names = image_stream_->get_role_data<std::vector<std::string>>(
            module::Attribute::StringChoices);
        auto curr_stream = image_stream_->value();
        auto p           = std::find(stream_names.begin(), stream_names.end(), curr_stream);
        if (p != stream_names.end()) {
            size_t idx = std::distance(stream_names.begin(), p);
            if (idx < (stream_names.size() - 1)) {
                image_stream_->set_value(stream_names[idx + 1]);
            }
        }

    } else {
        PlayheadBase::hotkey_pressed(hotkey_uuid, context, window);
    }
}

void PlayheadActor::hotkey_released(
    const utility::Uuid &hotkey_uuid,
    const std::string &context,
    const bool due_to_focus_change) {

    if (hotkey_uuid == step_forward_ || hotkey_uuid == step_backward_) {
        set_playing(false);
        step_keypress_event_id_ = 0.0f;
    }
}

void PlayheadActor::connected_to_ui_changed() {

    if (connected_to_ui()) {

        new_source_list();

        restart_readahead_cacheing(assembly_mode() != AM_ONE);
        update_playback_rate();
        mail(utility::event_atom_v, velocity_atom_v, velocity()).send(fps_moniotor_group_);
        mail(utility::event_atom_v, velocity_multiplier_atom_v, velocity_multiplier())
            .send(fps_moniotor_group_);
        mail(utility::event_atom_v, play_forward_atom_v, forward()).send(fps_moniotor_group_);

        // this 'kicks' sub playheads to force them to broadcast current media
        // back up to this class, which then re-broadcasts the current media to
        // other components of xSTUDIO that are interested in what is on-screen
        // (for example, HUD plugins displaying media info in the Viewport)
        for (auto &sub_playhead : sub_playheads_) {
            mail(media_atom_v, true).send(sub_playhead.actor());
        }

    } else {

        connect_to_ui_attr_->set_value(false, false);
        // stop all cacheing
        for (auto ph : sub_playheads_) {
            anon_mail(clear_precache_queue_atom_v).send(ph.actor());
        }
        if (audio_playhead_) {
            anon_mail(clear_precache_queue_atom_v).send(audio_playhead_);
        }
        clear_child_playheads();
        source_actors_.clear();
    }
}

void PlayheadActor::current_media_changed(caf::actor media_actor, const bool force) {

    if (not force and media_actor_ == media_actor)
        return;
    media_actor_ = media_actor;
    mail(utility::event_atom_v, playhead::media_atom_v, media_actor)
        .send(playhead_media_events_group_);

    update_source_multichoice(image_source_, media::MT_IMAGE);
    update_source_multichoice(audio_source_, media::MT_AUDIO);
    update_stream_multichoice(image_stream_, media::MT_IMAGE);
}

void PlayheadActor::update_source_multichoice(
    module::StringChoiceAttribute *attr, const media::MediaType mt) {

    // get the MediaSource items in the MediaActor
    mail(media::current_media_source_atom_v, mt)
        .request(media_actor_, infinite)
        .then(
            [=](const UuidActor &current_source) mutable {
                using uuid_name = std::pair<utility::Uuid, std::string>;

                mail(utility::name_atom_v)
                    .request(current_source.actor(), infinite)
                    .then(
                        [=](const std::string &current_source_name) mutable {
                            mail(media::get_media_source_names_atom_v, mt)
                                .request(media_actor_, infinite)
                                .then(
                                    [=](std::vector<uuid_name> source_uuid_names) mutable {
                                        updating_source_list_ = true;
                                        attr->set_value(to_string(current_source.uuid()));
                                        if (attr == image_source_) {
                                            image_source_name_->set_value(current_source_name);
                                        }

                                        std::sort(
                                            source_uuid_names.begin(),
                                            source_uuid_names.end(),
                                            [](const uuid_name &a, const uuid_name &b) -> bool {
                                                return a.second < b.second;
                                            });

                                        std::vector<std::string> source_names =
                                            vpair_second_to_v(source_uuid_names);
                                        std::vector<utility::Uuid> source_uuids =
                                            vpair_first_to_v(source_uuid_names);

                                        if (source_names.empty()) {
                                            source_names.emplace_back(
                                                mt == media::MT_IMAGE ? "No Video"
                                                                      : "No Audio");
                                            source_uuids.emplace_back(utility::Uuid());
                                        }

                                        attr->set_role_data(
                                            module::Attribute::StringChoices, source_names);
                                        attr->set_role_data(
                                            module::Attribute::AbbrStringChoices, source_names);
                                        attr->set_role_data(
                                            module::Attribute::StringChoicesIds, source_uuids);
                                        updating_source_list_ = false;
                                    },
                                    [=](error &err) mutable {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        updating_source_list_ = false;
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

void PlayheadActor::update_stream_multichoice(
    module::StringChoiceAttribute *streams_attr, const media::MediaType mt) {

    auto clear_attr = [=]() {
        streams_attr->set_role_data(
            module::Attribute::StringChoices, std::vector<std::string>(), false);
        streams_attr->set_value("", false);
    };

    auto receive_error = [=](error &err) mutable {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(err));
        clear_attr();
    };

    // get the MediaSource items in the MediaActor
    mail(media::get_media_stream_atom_v, mt)
        .request(media_actor_, infinite)
        .then(
            [=](const std::vector<UuidActor> &streams) mutable {
                if (streams.size() <= 1) {
                    // There is only one stream. We can blank the attr as
                    // the streams menu will be hidden in this case
                    clear_attr();
                }

                // get the name of the current source
                mail(media::current_media_stream_atom_v, mt)
                    .request(media_actor_, infinite)
                    .then(
                        [=](const UuidActor currentStream) mutable {
                            std::vector<caf::actor> stream_actors;
                            for (const auto &a : streams) {
                                stream_actors.push_back(a.actor());
                            }

                            // get the container details of the streams
                            fan_out_request<policy::select_all>(
                                stream_actors, infinite, detail_atom_v)
                                .then(
                                    [=](std::vector<ContainerDetail> streams_details) mutable {
                                        std::string current_stream;
                                        std::vector<std::string> stream_names;
                                        for (const auto &stream_detail : streams_details) {
                                            stream_names.push_back(stream_detail.name_);
                                            if (stream_detail.uuid_ == currentStream) {
                                                current_stream = stream_detail.name_;
                                            }
                                        }
                                        std::sort(stream_names.begin(), stream_names.end());

                                        streams_attr->set_value(current_stream, false);
                                        streams_attr->set_role_data(
                                            module::Attribute::StringChoices,
                                            stream_names,
                                            false);
                                        streams_attr->set_role_data(
                                            module::Attribute::AbbrStringChoices,
                                            stream_names,
                                            false);
                                    },
                                    receive_error);
                        },
                        receive_error);
            },
            receive_error);
}

void PlayheadActor::restart_readahead_cacheing(
    const bool all_child_playheads, const bool force) {

    if (!playing() && connected_to_ui()) {

        // first make sure we've wiped all outstanding precache requests...
        mail(clear_precache_requests_atom_v)
            .request(caf::actor_cast<caf::actor>(this), infinite)
            .then(
                [=](bool r) {
                    // wait one second, and then tell sub-playheads that they can start cacheing
                    // frames ready for playback. We wait one second because the user might be
                    // scrolling the selection up or down the media list rapidly, meaning
                    // subplayheads are being made and destroyed rapidly. We don't want them
                    // swamping the media readers with pre-cache requests until we're 'steady'

                    anon_mail(full_precache_atom_v, all_child_playheads, force, sub_playheads_)
                        .delay(std::chrono::seconds(1))
                        .send(this);
                },
                [=](caf::error &err) {

                });
    }
}

void PlayheadActor::switch_media_source(
    const std::string new_source_uuid, const media::MediaType mt) {

    auto siwtch_all_selected = [=](const std::string source_name) {
        // now, if we have a selection actor we want to try and switch all the
        // media sources in the selected media too:
        auto playlist_selection = caf::actor_cast<caf::actor>(playlist_selection_addr_);
        if (playlist_selection) {

            mail(get_selection_atom_v, true)
                .request(playlist_selection, infinite)
                .then(
                    [=](std::vector<caf::actor> selected_sources) mutable {
                        for (auto &media_src : selected_sources) {
                            anon_mail(media_source_atom_v, source_name, mt).send(media_src);
                        }
                    },
                    [=](error &err) mutable {
                        if (to_string(err) != "No frames")
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    };

    if (!hero_sub_playhead_ || new_source_uuid == "N/A" || new_source_uuid == "-")
        return;

    utility::Uuid source_uuid(new_source_uuid);

    // going via the sub-playhead (which resolves which actual MediaActor is
    // on screen now), we make the request to change the active MediaSource
    // for the MediaActor
    mail(media_source_atom_v, source_uuid, mt)
        .request(hero_sub_playhead_.actor(), infinite)
        .then(

            [=](const std::string &new_source_name) mutable {
                // re-kick cacheing following switch
                if (connected_to_ui()) {
                    restart_readahead_cacheing(false, true);
                }

                siwtch_all_selected(new_source_name);
            },
            [=](error &err) mutable {
                if (to_string(err) != "No frames")
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadActor::switch_media_stream(
    caf::actor media_actor,
    const std::string new_stream_name,
    const media::MediaType mt,
    bool apply_to_all_selected) {

    auto error_handler = [=](error &err) mutable {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
    };

    auto apply_to_all = [=]() {
        // now, if we have a selection actor we want to try and switch all the
        // media sources in the selected media too:
        auto playlist_selection = caf::actor_cast<caf::actor>(playlist_selection_addr_);
        if (playlist_selection) {

            mail(get_selection_atom_v, true)
                .request(playlist_selection, infinite)
                .then(
                    [=](std::vector<caf::actor> selected_sources) mutable {
                        for (auto &media : selected_sources) {
                            switch_media_stream(media, new_stream_name, mt, false);
                        }
                    },
                    [=](error &err) mutable {
                        if (to_string(err) != "No frames")
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    };

    // get the MediaSource items in the MediaActor
    mail(media::get_media_stream_atom_v, mt)
        .request(media_actor, infinite)
        .then(
            [=](const std::vector<UuidActor> &streams) mutable {
                std::vector<caf::actor> stream_actors;
                for (const auto &a : streams) {
                    stream_actors.push_back(a.actor());
                }

                // get the container details of the streams
                fan_out_request<policy::select_all>(stream_actors, infinite, detail_atom_v)
                    .then(
                        [=](std::vector<ContainerDetail> streams_details) mutable {
                            for (const auto &detail : streams_details) {
                                if (detail.name_ != new_stream_name)
                                    continue;
                                mail(media::current_media_stream_atom_v, mt, detail.uuid_)
                                    .request(media_actor, infinite)
                                    .then(
                                        [=](bool changed) {
                                            if (changed) {
                                                restart_readahead_cacheing(false, true);
                                                if (apply_to_all_selected) {
                                                    apply_to_all();
                                                }
                                            }
                                        },
                                        error_handler);
                            }
                        },
                        error_handler);
            },
            error_handler);
}


void PlayheadActor::check_if_loop_range_makes_sense() {

    // current behaviour is to turn off the loop range in string mode when
    // we have a new selection etc.
    if (use_loop_range()) {
        if (assembly_mode() == AM_STRING) {
            set_use_loop_range(false);
            notify_loop_start_changed();
            notify_loop_end_changed();
            mail(utility::event_atom_v, use_loop_range_atom_v, use_loop_range())
                .send(event_group_);
        }
    }
}

void PlayheadActor::make_source_menu_model() {

    // Make hotkeys for switching the on-screen (primary) source (e.g. for A/B compare)
    for (int i = 0; i < 9; ++i) {

        // numeric keys on a querty keyboard start with key '1' having a keyboard
        // key id of 0x31
        auto hotkey_id = register_hotkey(
            0x31 + i,
            ui::NoModifier,
            fmt::format("View source {}", i + 1),
            "When comparing multiple selected items hit the hotkey to switch the corresponding "
            "item to show in the viewport.",
            false,
            "Playback");
        switch_key_playhead_hotkeys_[hotkey_id] = i;
    }

    move_selection_up_hotkey_ = register_hotkey(
        "Up",
        "Move backwards through media/clips",
        "When comparing multiple selected items hit the hotkey to cycle back through the "
        "selection. If only one item is selected then select the previous item in the "
        "playlist.",
        true,
        "Playback");

    move_selection_down_hotkey_ = register_hotkey(
        "Down",
        "Move Forwards through media/clips",
        "When comparing multiple selected items hit the hotkey to cycle forward through the "
        "selection. If only one item is selected then select the next item in the playlist.",
        true,
        "Playback");

    jump_to_previous_note_hotkey_ = register_hotkey(
        ",",
        "Move backwards to previous note",
        "Jump the playhead backwards to the next note.",
        true,
        "Playback");

    jump_to_next_note_hotkey_ = register_hotkey(
        ".",
        "Move forwards to next note",
        "Jump the playhead forwards to the next note ",
        true,
        "Playback");

    // here we make a menu model unique to each instance of the playheadactor.
    // We use the uuid of the playhead to construct the unique menu model name.
    // This is picked up in the XsViewerSourceSelectorButton.qml script that
    // adds the source selector button to the viewport toolbar

    // set the image source and audio source attrs to be 'radiogroup' type
    image_source_->set_role_data(module::Attribute::Type, "RadioGroup");
    audio_source_->set_role_data(module::Attribute::Type, "RadioGroup");
    // image_stream_->set_role_data(module::Attribute::Type, "RadioGroup");

    const std::string source_menu_model_name =
        "{" + to_string(Module::uuid()) + "}" + "source_menu";

    // add a labelled divider saying 'Video Source'
    insert_menu_item(source_menu_model_name, "Video Source", "", 1.0f, nullptr, true);

    insert_menu_item(source_menu_model_name, "", "", 2.0f, image_source_);

    insert_menu_item(source_menu_model_name, "Image Layer", "", 2.25f, nullptr, true);

    insert_menu_item(source_menu_model_name, "USE_ATTR_VALUE", "", 2.5f, image_stream_);

    // add a labelled divider saying 'Audio Source'
    insert_menu_item(source_menu_model_name, "Audio Source", "", 3.0f, nullptr, true);

    insert_menu_item(source_menu_model_name, "", "", 4.0f, audio_source_);
}

utility::UuidActorVector
PlayheadActor::to_uuid_actor_vec(const std::vector<caf::actor> &actors) {
    caf::scoped_actor sys(system());

    utility::UuidActorVector r;
    r.resize(actors.size());

    int idx = 0;
    for (auto &a : actors) {
        try {
            auto uuid = request_receive<utility::Uuid>(*sys, a, utility::uuid_atom_v);
            r[idx]    = utility::UuidActor(uuid, a);
        } catch (...) {
        }
        idx++;
    }
    return r;
}

void PlayheadActor::apply_compare_prefs() {

    auto owner = caf::actor_cast<caf::actor>(parent_playlist_);
    if (owner) {
        mail(utility::detail_atom_v)
            .request(owner, infinite)
            .then(
                [=](utility::ContainerDetail &detail) {
                    // this playhead has been set-up from serialisation data, so
                    // we don't apply the default compare prefs
                    if (deserialised_)
                        return;

                    utility::JsonStore j;
                    try {
                        auto prefs = GlobalStoreHelper(system());
                        if (detail.type_ == "Playlist") {
                            j = prefs.value<utility::JsonStore>(
                                "/core/playhead/playlist_default_compare");
                        } else if (detail.type_ == "Subset") {
                            j = prefs.value<utility::JsonStore>(
                                "/core/playhead/subset_default_compare");
                        } else if (detail.type_ == "Contact Sheet") {
                            j = prefs.value<utility::JsonStore>(
                                "/core/playhead/contact_sheet_default_compare");
                        } else if (detail.type_ == "Timeline") {
                            j = prefs.value<utility::JsonStore>(
                                "/core/playhead/timeline_default_compare");
                        }

                        if (j.is_array() && j.size() == 2 && j[0].is_string() &&
                            j[1].is_string()) {
                            std::string compare_mode = j[0].get<std::string>();
                            std::string align        = j[1].get<std::string>();
                            compare_mode_->set_value(compare_mode);
                            auto_align_mode_->set_value(align);
                        }

                    } catch (std::exception &e) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                    }
                },
                [=](caf::error &err) {});
    }
}

void PlayheadActor::step_to_next_media(const bool forwards) {

    if (timeline_mode() || (assembly_mode() == AM_STRING && source_actors_.size() > 1)) {

        anon_mail(skip_to_clip_atom_v, forwards).send(caf::actor_cast<caf::actor>(this));

    } else {

        if (sub_playheads_.size() <= 1) {

            auto playlist_selection = caf::actor_cast<caf::actor>(playlist_selection_addr_);
            if (playlist_selection) {
                anon_mail(playhead::select_next_media_atom_v, forwards ? 1 : -1)
                    .send(playlist_selection);
            }

        } else {
            for (size_t i = 0; i < sub_playheads_.size(); ++i) {
                if (sub_playheads_[i] == hero_sub_playhead_) {
                    if (forwards) {
                        if (i == (sub_playheads_.size() - 1)) {
                            switch_key_playhead(0);
                        } else {
                            switch_key_playhead(i + 1);
                        }
                    } else if (!i) {
                        switch_key_playhead(sub_playheads_.size() - 1);
                    } else {
                        switch_key_playhead(i - 1);
                    }
                    break;
                }
            }
        }
    }
}

void PlayheadActor::connected_viewports_changed(std::set<caf::actor> &connected_viewports) {

    if (offscreen_only_)
        return;

    if (connected_viewports.empty()) {
        disconnect_from_ui();
    } else if (!connected_to_ui()) {
        connect_to_ui();
    }
}
