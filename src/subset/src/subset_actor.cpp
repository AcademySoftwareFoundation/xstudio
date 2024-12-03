// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/subset/subset_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::subset;
using namespace xstudio::playlist;

SubsetActor::SubsetActor(
    caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg),
      playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(static_cast<utility::JsonStore>(jsn["base"])) {

    anon_send(this, playhead::source_atom_v, playlist, UuidUuidMap());

    // need to scan playlist to relink our media..
    init();
}

SubsetActor::SubsetActor(caf::actor_config &cfg, caf::actor playlist, const std::string &name, const std::string &override_type)
    : caf::event_based_actor(cfg),
      playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(name, override_type) {
    init();
}

void SubsetActor::add_media(
    caf::actor actor, const utility::Uuid &uuid, const utility::Uuid &before_uuid) {
    if (not base_.contains_media(uuid)) {
        base_.insert_media(uuid, before_uuid);
        actors_[uuid] = actor;
        monitor(actor);
    }
}

bool SubsetActor::remove_media(caf::actor actor, const utility::Uuid &uuid) {
    bool result = false;

    if (base_.remove_media(uuid)) {
        demonitor(actor);
        actors_.erase(uuid);
        result = true;
    }

    return result;
}
caf::message_handler SubsetActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_change_event_group_atom) -> caf::actor { return change_event_group_; },

        [=](session::media_rate_atom atom) {
            delegate(caf::actor_cast<caf::actor>(playlist_), atom);
        },


        [=](duplicate_atom) -> result<UuidActor> {
            // clone ourself..
            auto actor = spawn<subset::SubsetActor>(
                caf::actor_cast<caf::actor>(playlist_), base_.name());
            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());
            // get uuid from actor..
            try {
                caf::scoped_actor sys(system());
                // get uuid..
                Uuid uuid = request_receive<Uuid>(*sys, actor, utility::uuid_atom_v);

                // maybe not be safe.. as ordering isn't implicit..
                std::vector<UuidActor> media_actors;
                for (const auto &i : base_.media())
                    media_actors.emplace_back(UuidActor(i, actors_[i]));

                request_receive<bool>(
                    *sys, actor, playlist::add_media_atom_v, media_actors, Uuid());

                return UuidActor(uuid, actor);

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playhead::playhead_rate_atom) -> FrameRate { return base_.playhead_rate(); },

        [=](playhead::playhead_rate_atom, const FrameRate &rate) {
            base_.set_playhead_rate(rate);
            base_.send_changed();
        },

        [=](playhead::source_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(playlist_);
        },

        // set source (playlist), triggers relinking
        [=](playhead::source_atom, caf::actor playlist, const UuidUuidMap &swap) -> bool {
            for (const auto &i : actors_)
                demonitor(i.second);
            actors_.clear();
            playlist_ = caf::actor_cast<actor_addr>(playlist);

            caf::scoped_actor sys(system());
            try {
                auto media = request_receive<std::vector<UuidActor>>(
                    *sys, playlist, playlist::get_media_atom_v);

                // build map
                UuidActorMap amap;
                for (const auto &i : media)
                    amap[i.uuid()] = i.actor();

                bool clean = false;
                while (not clean) {
                    clean = true;
                    for (const auto &i : base_.media()) {
                        auto ii = (swap.count(i) ? swap.at(i) : i);
                        if (not amap.count(ii)) {
                            spdlog::error("Failed to find media in playlist {}", to_string(ii));
                            base_.remove_media(i);
                            clean = false;
                            break;
                        }
                    }
                }
                // link
                for (const auto &i : base_.media()) {
                    auto ii = (swap.count(i) ? swap.at(i) : i);
                    if (ii != i) {
                        base_.swap_media(i, ii);
                    }
                    actors_[ii] = amap[ii];
                    monitor(amap[ii]);
                }
            } catch (const std::exception &e) {
                spdlog::error("Failed to init Subset {}", e.what());
                base_.clear();
            }
            base_.send_changed();
            return true;
        },

        [=](playlist::add_media_atom atom,
            const caf::uri &path,
            const bool recursive,
            const utility::Uuid &uuid_before) -> result<std::vector<UuidActor>> {
            auto rp = make_response_promise<std::vector<UuidActor>>();
            request(
                caf::actor_cast<caf::actor>(playlist_),
                infinite,
                atom,
                path,
                recursive,
                uuid_before)
                .then(
                    [=](const std::vector<UuidActor> new_media) mutable {
                        for (const auto &m : new_media) {
                            add_media(m.actor(), m.uuid(), uuid_before);
                        }
                        send(
                            base_.event_group(),
                            utility::event_atom_v,
                            playlist::add_media_atom_v,
                            new_media);
                        base_.send_changed();
                        send(base_.event_group(), utility::event_atom_v, change_atom_v);
                        send(
                            change_event_group_, utility::event_atom_v, utility::change_atom_v);
                        rp.deliver(new_media);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](add_media_atom atom,
            UuidActor ua, // the MediaActor
            const utility::UuidList &final_ordered_uuid_list,
            utility::Uuid before) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            // This handler lets us build a playlist of a given order, even when
            // we add the media in an out of order way. We generate Uuids for the
            // MediaActors that will be used to build the playlist *before* the
            // actual MediaActors are built and added - we pass in the ordered
            // list of Uuids when adding each MediaActor so we can insert it
            // into the correct point as the playlist is being built.
            //
            // This is used in ShotgunDataSourceActor, for example

            const utility::UuidList media = base_.media();

            // get an iterator to the uuid of the next MediaActor item that has
            // been (or will be) added to this playlist
            auto next = std::find(
                final_ordered_uuid_list.begin(), final_ordered_uuid_list.end(), ua.uuid());
            if (next != final_ordered_uuid_list.end())
                next++;

            while (next != final_ordered_uuid_list.end()) {

                // has 'next' already been added to this playlist?
                auto q = std::find(media.begin(), media.end(), *next);
                if (q != media.end()) {
                    // yes - we know where to insert the incoming MediaActor
                    before = *q;
                    break;
                }
                // keep looking
                next++;
            }

            // Note we can't use delegate(this, add_media_atom_v, ua, before)
            // to enact the adding, because it might happen *after* we get
            // another of these add_media messages which would then mess up the
            // ordering algorithm
            add_media(ua, before, rp);

            return rp;
        },

        // bit messy... needs refactor.
        [=](playlist::add_media_atom,
            const UuidActor &ua,
            const Uuid &before_uuid) -> result<UuidActor> {
            // check it exists in playlist..
            auto rp = make_response_promise<UuidActor>();
            add_media(ua, before_uuid, rp);
            return rp;
        },

        [=](playlist::add_media_atom,
            const Uuid &uuid,
            const caf::actor &actor,
            const Uuid &before_uuid) -> bool {
            try {
                add_media(actor, uuid, before_uuid);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    playlist::add_media_atom_v,
                    UuidActorVector({UuidActor(uuid, actor)}));
                base_.send_changed();
                send(base_.event_group(), utility::event_atom_v, change_atom_v);
                send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            return true;
        },

        [=](playlist::add_media_atom,
            caf::actor actor,
            const Uuid &before_uuid) -> result<bool> {
            auto rp = make_response_promise<bool>();

            caf::scoped_actor sys(system());
            try {
                // get uuid..
                Uuid uuid = request_receive<Uuid>(*sys, actor, utility::uuid_atom_v);
                // check playlist owns it..
                request_receive<caf::actor>(
                    *sys,
                    caf::actor_cast<caf::actor>(playlist_),
                    playlist::get_media_atom_v,
                    uuid);

                rp.delegate(
                    caf::actor_cast<caf::actor>(this),
                    playlist::add_media_atom_v,
                    uuid,
                    actor,
                    before_uuid);


                // add_media(actor, uuid, before_uuid);
                // send(
                //     base_.event_group(),
                //     utility::event_atom_v,
                //     playlist::add_media_atom_v,
                //     UuidActorVector({UuidActor(uuid, actor)}));
                // base_.send_changed();
                // send(base_.event_group(), utility::event_atom_v, change_atom_v);
                // send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                rp.deliver(false);
            }

            return rp;
        },

        [=](playlist::add_media_atom,
            const Uuid &uuid,
            const Uuid &before_uuid) -> result<bool> {
            // get actor from playlist..
            auto rp = make_response_promise<bool>();

            request(
                caf::actor_cast<actor>(playlist_), infinite, playlist::get_media_atom_v, uuid)
                .then(
                    [=](caf::actor actor) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this),
                            playlist::add_media_atom_v,
                            uuid,
                            actor,
                            before_uuid);
                        // add_media(actor, uuid, before_uuid);
                        // send(base_.event_group(), utility::event_atom_v, change_atom_v);
                        // send(change_base_.event_group(), utility::event_atom_v,
                        // utility::change_atom_v); send(
                        //     base_.event_group(),
                        //     utility::event_atom_v,
                        //     playlist::add_media_atom_v,
                        //     UuidActorVector({UuidActor(uuid, actor)}));
                        // base_.send_changed();
                        // rp.deliver(true);
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(false);
                    });

            return rp;
        },

        [=](playlist::add_media_atom, const std::vector<UuidActor> &media) -> bool {
            for (const auto &m : media) {
                add_media(m.actor(), m.uuid(), utility::Uuid());
            }
            base_.send_changed();
            send(base_.event_group(), utility::event_atom_v, change_atom_v);
            send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            return true;
        },

        [=](playlist::add_media_atom,
            const UuidVector &media_uuids,
            const utility::Uuid &uuid_before) -> result<bool> {
            // need actors..
            auto rp = make_response_promise<bool>();

            request(caf::actor_cast<actor>(playlist_), infinite, playlist::get_media_atom_v)
                .then(
                    [=](const std::vector<UuidActor> &actors) mutable {
                        bool changed = false;
                        for (const auto &i : media_uuids) {
                            for (const auto &ii : actors) {
                                if (ii.uuid() == i) {
                                    changed = true;
                                    add_media(ii.actor(), i, uuid_before);
                                    send(
                                        base_.event_group(),
                                        utility::event_atom_v,
                                        playlist::add_media_atom_v,
                                        UuidActorVector({UuidActor(i, ii.actor())}));
                                    break;
                                }
                            }
                        }

                        if (changed) {
                            send(base_.event_group(), utility::event_atom_v, change_atom_v);
                            send(
                                change_event_group_,
                                utility::event_atom_v,
                                utility::change_atom_v);
                            base_.send_changed();
                        }
                        rp.deliver(changed);
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(false);
                    });

            return rp;
        },


        [=](media::current_media_source_atom)
            -> caf::result<std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>> {
            auto rp = make_response_promise<
                std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>>();
            if (not actors_.empty()) {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(actors_), infinite, media::current_media_source_atom_v)
                    .then(
                        [=](const std::vector<
                            std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>
                                details) mutable { rp.deliver(details); },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                rp.deliver(
                    std::vector<std::pair<UuidActor, std::pair<UuidActor, UuidActor>>>());
            }
            return rp;
        },


        [=](playlist::add_media_atom,
            const std::vector<UuidActor> &media_actors,
            const utility::Uuid &uuid_before) -> result<bool> {
            for (const auto &i : media_actors) {
                add_media(i.actor(), i.uuid(), uuid_before);
            }
            send(
                base_.event_group(),
                utility::event_atom_v,
                playlist::add_media_atom_v,
                media_actors);
            send(base_.event_group(), utility::event_atom_v, change_atom_v);
            send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            base_.send_changed();
            return true;
        },

        [=](playlist::create_playhead_atom) -> UuidActor {
            if (playhead_)
                return playhead_;
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(
                std::string("Subset Playhead"),
                selection_actor_,
                uuid,
                caf::actor_cast<caf::actor_addr>(this));
            link_to(actor);

            anon_send(actor, playhead::playhead_rate_atom_v, base_.playhead_rate());

            playhead_ = UuidActor(uuid, actor);
            return playhead_;
        },
        [=](playlist::get_playhead_atom) {
            delegate(caf::actor_cast<caf::actor>(this), playlist::create_playhead_atom_v);
        },

        [=](playlist::get_media_atom) -> std::vector<UuidActor> {
            std::vector<UuidActor> result;

            for (const auto &i : base_.media())
                result.emplace_back(UuidActor(i, actors_[i]));

            return result;
        },

        [=](playlist::get_media_atom, const Uuid &uuid) -> result<caf::actor> {
            if (base_.contains_media(uuid))
                return actors_[uuid];
            return make_error(xstudio_error::error, "Invalid uuid");
        },

        [=](playlist::get_media_atom, const bool) -> result<std::vector<ContainerDetail>> {
            if (not actors_.empty()) {
                auto rp = make_response_promise<std::vector<ContainerDetail>>();
                // collect media data..
                std::vector<caf::actor> actors;
                for (const auto &i : actors_)
                    actors.push_back(i.second);

                fan_out_request<policy::select_all>(actors, infinite, utility::detail_atom_v)
                    .then(
                        [=](const std::vector<ContainerDetail> details) mutable {
                            std::vector<ContainerDetail> reordered_details;

                            for (const auto &i : base_.media()) {
                                for (const auto &ii : details) {
                                    if (ii.uuid_ == i) {
                                        reordered_details.push_back(ii);
                                        break;
                                    }
                                }
                            }

                            rp.deliver(reordered_details);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }
            std::vector<ContainerDetail> result;
            return result;
        },

        [=](playlist::get_media_uuid_atom) -> UuidVector { return base_.media_vector(); },

        [=](playlist::move_media_atom atom, const Uuid &uuid, const Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this), atom, utility::UuidVector({uuid}), uuid_before);
        },

        [=](playlist::move_media_atom atom,
            const UuidList &media_uuids,
            const Uuid &uuid_before) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                utility::UuidVector(media_uuids.begin(), media_uuids.end()),
                uuid_before);
        },

        [=](playlist::move_media_atom,
            const UuidVector &media_uuids,
            const Uuid &uuid_before) -> bool {
            bool result = false;
            for (auto uuid : media_uuids) {
                result |= base_.move_media(uuid, uuid_before);
            }
            if (result) {
                base_.send_changed();
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    playlist::move_media_atom_v,
                    media_uuids,
                    uuid_before);
                send(base_.event_group(), utility::event_atom_v, change_atom_v);
                send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            }
            return result;
        },

        [=](playlist::remove_media_atom atom, const utility::UuidList &uuids) {
            delegate(
                actor_cast<caf::actor>(this),
                atom,
                utility::UuidVector(uuids.begin(), uuids.end()));
        },

        [=](playlist::remove_media_atom atom, const Uuid &uuid) {
            delegate(actor_cast<caf::actor>(this), atom, utility::UuidVector({uuid}));
        },

        [=](playlist::remove_media_atom, const utility::UuidVector &uuids) -> bool {
            // this needs to propergate to children somehow..
            utility::UuidVector removed;

            for (const auto &uuid : uuids) {
                if (actors_.count(uuid) and remove_media(actors_[uuid], uuid)) {
                    removed.push_back(uuid);
                }
            }

            if (not removed.empty()) {
                send(base_.event_group(), utility::event_atom_v, change_atom_v);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    playlist::remove_media_atom_v,
                    removed);
                send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
                base_.send_changed();
            }
            return not removed.empty();
        },

        [=](playlist::selection_actor_atom) -> caf::actor { return selection_actor_; },

        [=](playlist::sort_by_media_display_info_atom,
            const int sort_column_index,
            const bool ascending) { sort_by_media_display_info(sort_column_index, ascending); },

        [=](playlist::filter_media_atom,
            const std::string &filter_string) -> result<utility::UuidList> {
            // for each media item in the playlist, check if any fields in the
            // 'media display info' for the media matches filter_string. If
            // it does, include it in the result. We have to do some awkward
            // shenanegans thanks to async requests ... the usual stuff!!

            if (filter_string.empty())
                return base_.media();

            auto rp = make_response_promise<utility::UuidList>();
            // share ptr to store result for each media piece (in order)
            auto vv = std::make_shared<std::vector<Uuid>>(base_.media().size());
            // keep count of responses with this
            auto ct                        = std::make_shared<int>(base_.media().size());
            const auto filter_string_lower = utility::to_lower(filter_string);
            auto check_deliver             = [=]() mutable {
                (*ct)--;
                if (*ct == 0) {
                    utility::UuidList r;
                    for (const auto &v : *vv) {
                        if (!v.is_null()) {
                            r.push_back(v);
                        }
                    }
                    rp.deliver(r);
                }
            };


            int idx = 0;
            for (const auto &uuid : base_.media()) {
                if (!actors_.count(uuid)) {
                    check_deliver();
                    continue;
                }
                UuidActor media(uuid, actors_[uuid]);
                request(media.actor(), infinite, media::media_display_info_atom_v)
                    .then(
                        [=](const utility::JsonStore &media_display_info) mutable {
                            if (media_display_info.is_array()) {
                                for (int i = 0; i < media_display_info.size(); ++i) {
                                    std::string data = media_display_info[i].dump();
                                    if (utility::to_lower(data).find(filter_string_lower) !=
                                        std::string::npos) {
                                        (*vv)[idx] = media.uuid();
                                        break;
                                    }
                                }
                            }
                            check_deliver();
                        },
                        [=](caf::error &err) mutable { check_deliver(); });
                idx++;
            }
            return rp;
        },

        [=](get_next_media_atom,
            const utility::Uuid &after_this_uuid,
            int skip_by,
            const std::string &filter_string) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();

            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                playlist::filter_media_atom_v,
                filter_string)
                .then(
                    [=](const utility::UuidList &media) mutable {
                        if (skip_by > 0) {
                            auto i = std::find(media.begin(), media.end(), after_this_uuid);
                            if (i == media.end()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    "playlist::get_next_media_atom called with uuid that is "
                                    "not in "
                                    "subset"));
                            }
                            while (skip_by--) {
                                i++;
                                if (i == media.end()) {
                                    i--;
                                    break;
                                }
                            }
                            if (actors_.count(*i))
                                rp.deliver(UuidActor(*i, actors_[*i]));

                        } else {
                            auto i = std::find(media.rbegin(), media.rend(), after_this_uuid);
                            if (i == media.rend()) {
                                // not found!
                                rp.deliver(make_error(
                                    xstudio_error::error,
                                    "playlist::get_next_media_atom called with uuid that is "
                                    "not in "
                                    "playlist"));
                            }
                            while (skip_by++) {
                                i++;
                                if (i == media.rend()) {
                                    i--;
                                    break;
                                }
                            }

                            if (actors_.count(*i))
                                rp.deliver(UuidActor(*i, actors_[*i]));
                        }
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        // [=](json_store::get_json_atom atom, const std::string &path) {
        //     delegate(caf::actor_cast<caf::actor>(playlist_), atom, path);
        // },

        // [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
        //     delegate(caf::actor_cast<caf::actor>(playlist_), atom, json, path);
        // },

        [=](session::session_atom) {
            delegate(caf::actor_cast<caf::actor>(playlist_), session::session_atom_v);
        },

        [=](utility::serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        }};
}

void SubsetActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(change_event_group_);

    selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
        "SubsetPlayheadSelectionActor", caf::actor_cast<caf::actor>(this));
    link_to(selection_actor_);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        for (auto it = std::begin(actors_); it != std::end(actors_); ++it) {
            if (msg.source == it->second) {
                spdlog::debug("Remove media {}", to_string(it->first));
                remove_media(it->second, it->first);
                send(base_.event_group(), utility::event_atom_v, change_atom_v);
                send(
                    base_.event_group(),
                    utility::event_atom_v,
                    playlist::remove_media_atom_v,
                    UuidVector({it->first}));
                base_.send_changed();
                break;
            }
        }
    });
}

void SubsetActor::add_media(
    const utility::UuidActor &ua,
    const utility::Uuid &before_uuid,
    caf::typed_response_promise<utility::UuidActor> rp) {

    try {

        caf::scoped_actor sys(system());

        // does the media already belong to parent playlist?
        auto actor = request_receive<caf::actor>(
            *sys,
            caf::actor_cast<caf::actor>(playlist_),
            playlist::get_media_atom_v,
            ua.uuid(),
            true);

        if (!actor) {

            // no, parent playlist doesn't have this media so we must add it first
            actor = request_receive<UuidActor>(
                        *sys,
                        caf::actor_cast<caf::actor>(playlist_),
                        playlist::add_media_atom_v,
                        ua,
                        before_uuid)
                        .actor();
        }

        add_media(actor, ua.uuid(), before_uuid);
        send(
            base_.event_group(),
            utility::event_atom_v,
            playlist::add_media_atom_v,
            UuidActorVector({ua}));
        base_.send_changed();
        send(base_.event_group(), utility::event_atom_v, change_atom_v);
        send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
        rp.deliver(ua);

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void SubsetActor::sort_by_media_display_info(
    const int sort_column_index, const bool ascending) {

    using SourceAndUuid = std::pair<std::string, utility::Uuid>;
    auto sort_keys_vs_uuids =
        std::make_shared<std::vector<std::pair<std::string, utility::Uuid>>>();

    int idx = 0;
    for (const auto &i : base_.media()) {

        // Pro tip: because i is a reference, it's the reference that is captured in our lambda
        // below and therefore it is 'unstable' so we make a copy here and use that in the
        // lambda as this is object-copied in the capture instead.
        UuidActor media_actor(i, actors_[i]);
        idx++;

        request(media_actor.actor(), infinite, media::media_display_info_atom_v)
            .await(

                [=](const utility::JsonStore &media_display_info) mutable {
                    // media_display_info should be an array of arrays. Each
                    // array is the data shown in the media list columns.
                    // So info_set_idx corresponds to the media list (in the UI)
                    // from which the sort was requested. And the info_item_idx
                    // corresponds to the column of that media list.

                    // default sort key keeps current sorting but should always
                    // put it after the last element that did have a sort key
                    std::string sort_key = fmt::format("ZZZZZZ{}", idx);

                    if (media_display_info.is_array() &&
                        sort_column_index < media_display_info.size()) {
                        sort_key = media_display_info[sort_column_index].dump();
                    }

                    (*sort_keys_vs_uuids)
                        .push_back(std::make_pair(sort_key, media_actor.uuid()));

                    if (sort_keys_vs_uuids->size() == base_.media().size()) {

                        std::sort(
                            sort_keys_vs_uuids->begin(),
                            sort_keys_vs_uuids->end(),
                            [ascending](
                                const SourceAndUuid &a, const SourceAndUuid &b) -> bool {
                                return ascending ? a.first < b.first : a.first > b.first;
                            });

                        utility::UuidList ordered_uuids;
                        for (const auto &p : (*sort_keys_vs_uuids)) {
                            ordered_uuids.push_back(p.second);
                        }

                        anon_send(
                            caf::actor_cast<caf::actor>(this),
                            move_media_atom_v,
                            ordered_uuids,
                            utility::Uuid());
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}
