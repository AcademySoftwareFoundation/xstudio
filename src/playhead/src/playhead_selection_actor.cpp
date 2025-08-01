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


void PlayheadSelectionActor::init() {

    spdlog::debug("Created PlayheadSelectionActor {}", base_.name());
    print_on_exit(this, "PlayheadSelectionActor");
    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

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
                                    select_media(UuidList({new_selection_uuid}));
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

        [=](playlist::select_media_atom, const UuidList &media_uuids) -> bool {
            if (media_uuids.empty()) {
                select_one();
            } else {
                select_media(media_uuids);
            }
            return true;
        },

        [=](playlist::select_media_atom) -> bool {
            // clears the selection
            select_media(UuidList());
            return true;
        },

        [=](playlist::select_media_atom, utility::Uuid media_uuid) {
            auto modified_selection = base_.items();
            modified_selection.push_back(media_uuid);
            select_media(modified_selection);
        },

        [=](utility::event_atom, utility::change_atom) {
            if (current_sender() == playlist_) {
                // the playlist has changed - if it was previously empty but now
                // (perhaps) has media content then try selecting the first item
                if (base_.empty())
                    anon_send(this, playlist::select_media_atom_v, UuidList());
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
        [=](utility::event_atom, playlist::add_media_atom, utility::UuidActor media) {
            if (base_.items().empty()) {
                select_one();
            }
        }

    );
}

void PlayheadSelectionActor::select_media(const UuidList &media_uuids) {
    if (base_.items_vec() == std::vector<Uuid>(media_uuids.begin(), media_uuids.end())) {
        return;
    }

    if (media_uuids.empty()) {
        for (const auto &i : source_actors_)
            demonitor(i.second);
        source_actors_.clear();
        base_.clear();
        send(
            event_group_,
            utility::event_atom_v,
            playhead::source_atom_v,
            std::vector<caf::actor>());
    } else {

        // fetch all the media in the playlist
        request(playlist_, infinite, playlist::get_media_atom_v)
            .then(
                [=](const std::vector<UuidActor> &media_actors) mutable {
                    // re can have nasty races..
                    // we should try and give a grace period
                    auto media_uas = uuidactor_vect_to_map(media_actors);
                    for (const auto &i : media_uuids) {
                        if (not media_uas.count(i))
                            return delayed_anon_send(
                                caf::actor_cast<caf::actor>(this),
                                std::chrono::milliseconds(50),
                                playlist::select_media_atom_v,
                                media_uuids);
                    }

                    for (const auto &i : source_actors_)
                        demonitor(i.second);
                    source_actors_.clear();
                    base_.clear();

                    std::vector<caf::actor> selected_media_actors;

                    for (auto p : media_uuids) {
                        if (media_uas.count(p)) {
                            insert_actor(media_uas[p], p, Uuid());
                            selected_media_actors.push_back(media_uas[p]);
                        }
                    }

                    send(
                        event_group_,
                        utility::event_atom_v,
                        playhead::source_atom_v,
                        selected_media_actors);
                },
                [=](error &err) mutable {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}

void PlayheadSelectionActor::select_one() {

    // fetch all the media in the playlist
    request(playlist_, infinite, playlist::get_media_atom_v)
        .then(
            [=](std::vector<UuidActor> media_actors) mutable {
                if (!media_actors.empty()) {
                    // select only the first item in the playlist
                    select_media(UuidList({media_actors[0].uuid()}));

                } else {
                    select_media(UuidList());
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
                UuidList l;
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
        skip_by)
        .then(
            [=](UuidActor media_actor) mutable {
                UuidList l({media_actor.uuid()});
                select_media(l);
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
