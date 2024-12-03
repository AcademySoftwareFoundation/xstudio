// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::playhead;
using namespace xstudio::media;
using namespace caf;

PlayheadSelectionActor::PlayheadSelectionActor(
    caf::actor_config &cfg, const utility::JsonStore &jsn, caf::actor monitored_playlist)
    : caf::event_based_actor(cfg),
      base_(static_cast<utility::JsonStore>(jsn["base"])),
      playlist_(std::move(monitored_playlist)) {


    // how do we restore.. ?

    // // parse and generate tracks/stacks.
    // for (const auto& [key, value] : jsn["actors"].items()) {
    // 	if(value["base"]["container"]["type"] == "TrackActor"){
    // 		try {
    // 			actors_[key] = spawn<TrackActor>(playlist,
    // static_cast<utility::JsonStore>(value)); 			link_to(actors_[key]);
    // } catch (const std::exception &e) { 			spdlog::error("{}", e.what());
    // 		}
    // 	}
    // 	else if(value["base"]["container"]["type"] == "ClipActor"){
    // 		try {
    // 			actors_[key] = spawn<ClipActor>(playlist,
    // static_cast<utility::JsonStore>(value)); 			link_to(actors_[key]);
    // } catch (const std::exception &e) { 			spdlog::error("{}", e.what());
    // 		}
    // 	}
    // 	else if(value["base"]["container"]["type"] == "StackActor"){
    // 		try {
    // 			actors_[key] = spawn<StackActor>(playlist,
    // static_cast<utility::JsonStore>(value)); 			link_to(actors_[key]);
    // } catch (const std::exception &e) { 			spdlog::error("{}", e.what());
    // 		}
    // 	}
    // }

    init();
}

PlayheadSelectionActor::PlayheadSelectionActor(
    caf::actor_config &cfg, const std::string &name, caf::actor monitored_playlist)
    : caf::event_based_actor(cfg),
      base_(name, "PlayheadSelection"),
      playlist_(std::move(monitored_playlist)) {
    init();
}

PlayheadSelectionActor::~PlayheadSelectionActor() {}

void PlayheadSelectionActor::on_exit() { playlist_ = caf::actor(); }

caf::message_handler PlayheadSelectionActor::message_handler() {
    return caf::message_handler{
        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

        [=](playhead::delete_selection_from_playlist_atom) {
            if (base_.items().empty())
                return;

            utility::Uuid deleted_media_source_uuid = base_.items().front().uuid_;

            request(
                playlist_,
                infinite,
                playlist::get_next_media_atom_v,
                deleted_media_source_uuid,
                -1)
                .then(
                    [=](UuidActor media_actor) mutable {
                        utility::Uuid new_selection_uuid = media_actor.uuid();
                        request(
                            playlist_, infinite, playlist::remove_media_atom_v, base_.items())
                            .await(
                                [=](const bool) {
                                    select_media(UuidVector({new_selection_uuid}));
                                },
                                [=](error &err) {
                                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                });
                    },
                    [=](error &err) mutable {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media_hook::gather_media_sources_atom) {
            anon_send(playlist_, media_hook::gather_media_sources_atom_v, base_.items());
        },

        [=](playhead::evict_selection_from_cache_atom) -> result<media::MediaKeyVector> {
            auto rp = make_response_promise<media::MediaKeyVector>();
            std::vector<caf::actor> actors;

            for (const auto &i : source_actors_)
                actors.push_back(i.second);

            fan_out_request<policy::select_all>(
                actors, infinite, media::invalidate_cache_atom_v)
                .then(
                    [=](std::vector<media::MediaKeyVector> erased_keys) mutable {
                        media::MediaKeyVector result;
                        for (const auto &i : erased_keys)
                            result.insert(result.end(), i.begin(), i.end());
                        rp.deliver(result);
                    },
                    [=](const error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](playhead::get_selection_atom) -> UuidList { return base_.items(); },

        [=](playhead::get_selection_atom, bool) -> std::vector<caf::actor> {
            std::vector<caf::actor> r;
            for (const auto &i : source_actors_) {
                r.push_back(i.second);
            }
            return r;
        },

        [=](playhead::get_selection_atom, caf::actor requester) {
            anon_send(
                requester,
                utility::event_atom_v,
                playhead::selection_changed_atom_v,
                base_.items());
        },

        [=](playhead::select_next_media_atom, const int skip_by) {
            select_next_media_item(skip_by);
        },

        [=](playlist::select_all_media_atom) { select_all(); },

        [=](playlist::select_media_atom, const UuidVector &media_uuids, bool retry) -> bool {
            if (media_uuids.empty()) {
                select_one();
            } else {
                select_media(media_uuids, false);
            }
            return true;
        },

        [=](playlist::select_media_atom, const UuidVector &media_uuids) -> bool {
            if (media_uuids.empty()) {
                select_one();
            } else {
                select_media(media_uuids);
            }
            return true;
        },

        [=](playlist::select_media_atom,
            const UuidVector &media_uuids,
            const bool retry,
            const playhead::SelectionMode mode) -> bool {
            if (media_uuids.empty()) {
                select_one();
            } else {
                select_media(media_uuids, retry, mode);
            }
            return true;
        },

        [=](playlist::select_media_atom,
            const UuidVector &media_uuids,
            const playhead::SelectionMode mode) -> bool {
            if (media_uuids.empty())
                select_one();
            else
                select_media(media_uuids, true, mode);

            return true;
        },

        [=](playlist::media_filter_string, const std::string &filter_string) {
            filter_string_ = filter_string;
        },

        [=](playlist::media_filter_string) -> std::string { return filter_string_; },

        [=](playlist::select_media_atom) -> bool {
            // clears the selection
            select_media(UuidVector(), true, SM_CLEAR);
            return true;
        },

        [=](playlist::select_media_atom, utility::Uuid media_uuid) {
            select_media(UuidVector({media_uuid}), true, SM_SELECT);
        },

        [=](utility::event_atom, utility::change_atom) {
            if (current_sender() == playlist_) {
                // the playlist has changed - if it was previously empty but now
                // (perhaps) has media content then try selecting the first item
                if (base_.empty())
                    anon_send(this, playlist::select_media_atom_v, utility::UuidVector());
            }
        },

        [=](session::get_playlist_atom) -> caf::actor { return playlist_; },

        [=](utility::serialise_atom) -> result<JsonStore> {
            JsonStore jsn;
            jsn["base"]   = base_.serialise();
            jsn["actors"] = {};
            return result<JsonStore>(jsn);
        },

        [=](get_selected_sources_atom) -> utility::UuidActorVector {
            utility::UuidActorVector r;
            for (const auto &p : base_.items()) {
                r.emplace_back(p, source_actors_[p]);
            }
            return r;
        },
        [=](utility::event_atom, playlist::move_media_atom, const UuidVector &, const Uuid &) {
        },
        [=](utility::event_atom, playlist::remove_media_atom, const UuidVector &) {},
        [=](utility::event_atom, playlist::add_media_atom, const utility::UuidActorVector &) {
            if (base_.items().empty()) {
                select_one();
            }
        }};
}


void PlayheadSelectionActor::init() {

    spdlog::debug("Created PlayheadSelectionActor {}", base_.name());
    print_on_exit(this, "PlayheadSelectionActor");

    caf::scoped_actor sys(system());
    link_to(playlist_);
    request(playlist_, infinite, uuid_atom_v)
        .then(
            [=](const utility::Uuid &uuid) mutable { base_.set_monitored(uuid); },
            [=](error &) {});
    request(playlist_, infinite, playlist::get_change_event_group_atom_v)
        .then([=](caf::actor grp) mutable { join_broadcast(this, grp); }, [=](error &) {});

    set_down_handler([=](down_msg &msg) {
        // find removed sources..
        remove_dead_actor(msg.source);
    });
}

void PlayheadSelectionActor::select_media(
    const UuidVector &media_uuids, const bool retry, const SelectionMode mode) {
    // this depends on mode...

    // build list of selected / deselected

    // spdlog::warn("select_media {} {}", retry, mode);
    // for(const auto &i: media_uuids)
    //     spdlog::warn("{}", to_string(i));

    if (mode == SM_NO_UPDATE)
        return;

    auto selected   = UuidVector();
    auto deselected = UuidVector();

    if (mode == SM_CLEAR)
        deselected = base_.items_vec();
    else if (mode == SM_DESELECT)
        deselected = media_uuids;
    else if (mode == SM_SELECT)
        selected = media_uuids;
    else if (mode == SM_TOGGLE) {
        auto current_selection = base_.items_vec();
        auto current_selection_set =
            UuidSet(current_selection.begin(), current_selection.end());
        for (const auto &i : media_uuids) {
            if (current_selection_set.count(i))
                deselected.push_back(i);
            else
                selected.push_back(i);
        }
    } else if (mode == SM_CLEAR_AND_SELECT) {
        deselected = base_.items_vec();
        selected   = media_uuids;
    }

    if (not selected.empty()) {
        // grab media from playlist..
        request(playlist_, infinite, playlist::get_media_atom_v)
            .then(
                [=](const std::vector<UuidActor> &media_actors) mutable {
                    // It's possible that a client has told us to select a piece
                    // of media that the playlist hasn't quite got around to
                    // adding yet. e.g. client creates media, adds to playlist
                    // and then tells PlayheadSelectionActor to select it.
                    // Due to async actors the Playlist might not have the new
                    // media quite yet.. in this case order a retry

                    // check to retry
                    auto media_uas = uuidactor_vect_to_map(media_actors);
                    if (retry) {
                        for (const auto &i : selected) {
                            if (not media_uas.count(i) || !media_uas[i]) {
                                delayed_anon_send(
                                    caf::actor_cast<caf::actor>(this),
                                    std::chrono::milliseconds(500),
                                    playlist::select_media_atom_v,
                                    media_uuids,
                                    false,
                                    mode);

                                return;
                            }
                        }
                    }

                    for (const auto &i : deselected) {
                        auto currently_selected = source_actors_.find(i);
                        if (currently_selected != source_actors_.end()) {
                            // part of current selection..
                            demonitor(currently_selected->second);
                            source_actors_.erase(currently_selected);
                            base_.remove_item(i);
                        }
                    }
                    // some uuids might be bogus..
                    // we ignore them..
                    for (const auto &i : selected) {
                        // make sure they not already active..
                        if (not media_uas.count(i)) {
                            spdlog::warn("Invalid media uuid in selection {}", to_string(i));
                        } else if (media_uas.at(i)) {
                            if (not source_actors_.count(i)) {
                                // add to selection..
                                insert_actor(media_uas.at(i), i, Uuid());
                            }
                        }
                    }

                    auto tmp = std::vector<caf::actor>();
                    for (const auto &i : base_.items_vec())
                        tmp.push_back(source_actors_[i]);

                    send(
                        base_.event_group(),
                        utility::event_atom_v,
                        playhead::source_atom_v,
                        tmp);
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    } else {
        // deselect first
        // then select as order matters.
        for (const auto &i : deselected) {
            auto currently_selected = source_actors_.find(i);
            if (currently_selected != source_actors_.end()) {
                // part of current selection..
                demonitor(currently_selected->second);
                source_actors_.erase(currently_selected);
                base_.remove_item(i);
            }
        }

        // we're done flush changes.
        // order matters..
        auto tmp = std::vector<caf::actor>();
        for (const auto &i : base_.items_vec())
            tmp.push_back(source_actors_[i]);
        send(base_.event_group(), utility::event_atom_v, playhead::source_atom_v, tmp);
    }
}

void PlayheadSelectionActor::select_one() {
    // fetch all the media in the playlist
    request(playlist_, infinite, playlist::get_media_atom_v)
        .then(
            [=](std::vector<UuidActor> media_actors) mutable {
                if (!media_actors.empty()) {
                    // select only the first item in the playlist
                    select_media(UuidVector({media_actors[0].uuid()}));

                } else {
                    select_media(UuidVector(), true, SM_CLEAR);
                }
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadSelectionActor::select_all() {

    // fetch all the media in the playlist
    request(playlist_, infinite, playlist::get_media_atom_v)
        .then(
            [=](std::vector<UuidActor> media_actors) mutable {
                // make a list of all the media item uuids in the plauylist
                UuidVector l;
                for (auto i : media_actors) {
                    l.push_back(i.uuid());
                }
                // .. and select them
                select_media(l);
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void PlayheadSelectionActor::select_next_media_item(const int skip_by) {

    // Auto select the next media item after the current selected item
    if (source_actors_.size() != 1) {
        return;
    }

    utility::Uuid current_media_source_uuid = source_actors_.begin()->first;

    // get the next item in the playlist using the uuid of the current item
    // in the selection
    request(
        playlist_,
        infinite,
        playlist::get_next_media_atom_v,
        current_media_source_uuid,
        skip_by,
        filter_string_)
        .then(
            [=](UuidActor media_actor) mutable {
                select_media(UuidVector({media_actor.uuid()}));
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}


void PlayheadSelectionActor::insert_actor(
    caf::actor actor, const utility::Uuid media_uuid, const utility::Uuid &before_uuid) {

    // we wrap sources in selection_clip..
    base_.insert_item(media_uuid, before_uuid);
    source_actors_[media_uuid] = actor;
    monitor(actor);
}

void PlayheadSelectionActor::remove_dead_actor(caf::actor_addr actor) {
    utility::Uuid media_uuid;
    for (const auto &i : source_actors_) {
        if (caf::actor_cast<caf::actor_addr>(i.second) == actor)
            media_uuid = i.first;
    }

    if (not media_uuid.is_null()) {
        demonitor(source_actors_[media_uuid]);
        base_.remove_item(media_uuid);
        source_actors_.erase(media_uuid);
    }
}
