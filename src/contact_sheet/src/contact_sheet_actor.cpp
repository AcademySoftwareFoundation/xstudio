// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/contact_sheet/contact_sheet_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::contact_sheet;
using namespace xstudio::playlist;

ContactSheetActor::ContactSheetActor(
    caf::actor_config &cfg, caf::actor playlist, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg),
      playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(static_cast<utility::JsonStore>(jsn["base"])) {

    anon_send(this, playhead::source_atom_v, playlist, UuidUuidMap());

    // need to scan playlist to relink our media..
    init();
}

ContactSheetActor::ContactSheetActor(
    caf::actor_config &cfg, caf::actor playlist, const std::string &name)
    : caf::event_based_actor(cfg),
      playlist_(caf::actor_cast<actor_addr>(playlist)),
      base_(name) {
    init();
}

void ContactSheetActor::add_media(
    caf::actor actor, const utility::Uuid &uuid, const utility::Uuid &before_uuid) {
    base_.insert_media(uuid, before_uuid);
    actors_[uuid] = actor;
    monitor(actor);
}

bool ContactSheetActor::remove_media(caf::actor actor, const utility::Uuid &uuid) {
    bool result = false;

    if (base_.remove_media(uuid)) {
        demonitor(actor);
        actors_.erase(uuid);
        result = true;
    }

    return result;
}


void ContactSheetActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    auto event_group_        = spawn<broadcast::BroadcastActor>(this);
    auto change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    link_to(change_event_group_);

    auto selection_actor_ = spawn<playhead::PlayheadSelectionActor>(
        "ContactSheetPlayheadSelectionActor", caf::actor_cast<caf::actor>(this));
    link_to(selection_actor_);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        for (auto it = std::begin(actors_); it != std::end(actors_); ++it) {
            if (msg.source == it->second) {
                spdlog::debug("Remove media {}", to_string(it->first));
                remove_media(it->second, it->first);
                send(event_group_, utility::event_atom_v, change_atom_v);
                base_.send_changed(event_group_, this);
                break;
            }
        }
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
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_change_event_group_atom) -> caf::actor { return change_event_group_; },


        [=](duplicate_atom) -> result<UuidActor> {
            // clone ourself..
            auto actor = spawn<contact_sheet::ContactSheetActor>(
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

        [=](media::get_edit_list_atom, const Uuid &uuid) -> result<utility::EditList> {
            std::vector<caf::actor> actors;
            for (const auto &i : base_.media())
                actors.push_back(actors_[i]);

            if (not actors.empty()) {
                auto rp = make_response_promise<utility::EditList>();

                fan_out_request<policy::select_all>(
                    actors, infinite, media::get_edit_list_atom_v, Uuid())
                    .then(
                        [=](std::vector<utility::EditList> sections) mutable {
                            utility::EditList ordered_sections;
                            for (const auto &i : base_.media()) {
                                for (const auto &ii : sections) {
                                    const auto &[ud, rt, tc] = ii.section_list()[0];
                                    if (ud == i) {
                                        if (uuid.is_null())
                                            ordered_sections.push_back(ii.section_list()[0]);
                                        else
                                            ordered_sections.push_back({uuid, rt, tc});
                                        break;
                                    }
                                }
                            }
                            rp.deliver(ordered_sections);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }

            return result<utility::EditList>(utility::EditList());
        },

        [=](playhead::playhead_rate_atom) -> FrameRate { return base_.playhead_rate(); },

        [=](playhead::playhead_rate_atom, const FrameRate &rate) {
            base_.set_playhead_rate(rate);
            base_.send_changed(event_group_, this);
        },

        [=](playlist::selection_actor_atom) -> caf::actor { return selection_actor_; },

        [=](playhead::source_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(playlist_);
        },

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
                spdlog::error("Failed to init ContactSheet {}", e.what());
                base_.clear();
            }
            base_.send_changed(event_group_, this);
            return true;
        },

        [=](playlist::add_media_atom, caf::actor actor, const Uuid &before_uuid) -> bool {
            caf::scoped_actor sys(system());
            bool result = false;
            try {
                // get uuid..
                Uuid uuid = request_receive<Uuid>(*sys, actor, utility::uuid_atom_v);
                // check playlist owns it..
                request_receive<caf::actor>(
                    *sys,
                    caf::actor_cast<caf::actor>(playlist_),
                    playlist::get_media_atom_v,
                    uuid);
                add_media(actor, uuid, before_uuid);
                send(
                    event_group_,
                    utility::event_atom_v,
                    playlist::add_media_atom_v,
                    UuidActor(uuid, actor));
                base_.send_changed(event_group_, this);
                send(event_group_, utility::event_atom_v, change_atom_v);
                send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
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
                        add_media(actor, uuid, before_uuid);
                        send(event_group_, utility::event_atom_v, change_atom_v);
                        send(
                            change_event_group_, utility::event_atom_v, utility::change_atom_v);
                        send(
                            event_group_,
                            utility::event_atom_v,
                            playlist::add_media_atom_v,
                            UuidActor(uuid, actor));
                        base_.send_changed(event_group_, this);
                        rp.deliver(true);
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(false);
                    });

            return rp;
        },

        [=](playlist::add_media_atom,
            UuidVector media_uuids,
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
                                        event_group_,
                                        utility::event_atom_v,
                                        playlist::add_media_atom_v,
                                        UuidActor(i, ii.actor()));
                                    break;
                                }
                            }
                        }

                        if (changed) {
                            send(event_group_, utility::event_atom_v, change_atom_v);
                            send(
                                change_event_group_,
                                utility::event_atom_v,
                                utility::change_atom_v);
                            base_.send_changed(event_group_, this);
                        }
                        rp.deliver(changed);
                    },
                    [=](error &err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        rp.deliver(false);
                    });

            return rp;
        },


        [=](playlist::add_media_atom,
            std::vector<UuidActor> media_actors,
            const utility::Uuid &uuid_before) -> result<bool> {
            for (const auto &i : media_actors) {
                add_media(i.actor(), i.uuid(), uuid_before);
                send(event_group_, utility::event_atom_v, playlist::add_media_atom_v, i);
            }
            send(event_group_, utility::event_atom_v, change_atom_v);
            send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
            base_.send_changed(event_group_, this);
            return true;
        },

        [=](playlist::create_playhead_atom) -> result<UuidActor> {
            if (playhead_)
                return playhead_;

            auto rp   = make_response_promise<UuidActor>();
            auto uuid = utility::Uuid::generate();

            auto playhead = spawn<playhead::PlayheadActor>(
                std::string("ContactSheet Playhead"), selection_actor_, uuid);
            link_to(playhead);

            // anon_send(actor, playhead::source_atom_v, tactor);
            anon_send(playhead, playhead::playhead_rate_atom_v, base_.playhead_rate());

            playhead_ = UuidActor(uuid, playhead);
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
                base_.send_changed(event_group_, this);
                send(event_group_, utility::event_atom_v, change_atom_v);
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
            bool changed = false;
            for (const auto &uuid : uuids) {
                if (actors_.count(uuid) and remove_media(actors_[uuid], uuid)) {
                    spdlog::warn("remove {}", to_string(uuid));
                    changed = true;
                }
            }
            if (changed) {
                send(event_group_, utility::event_atom_v, change_atom_v);
                send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
                base_.send_changed(event_group_, this);
            }
            return changed;
        },

        [=](playlist::sort_alphabetically_atom) { sort_alphabetically(); },

        [=](get_next_media_atom,
            const utility::Uuid &after_this_uuid,
            int skip_by) -> result<UuidActor> {
            const utility::UuidList media = base_.media();
            if (skip_by > 0) {
                auto i = std::find(media.begin(), media.end(), after_this_uuid);
                if (i == media.end()) {
                    // not found!
                    return make_error(
                        xstudio_error::error,
                        "playlist::get_next_media_atom called with uuid that is not in "
                        "subset");
                }
                while (skip_by--) {
                    i++;
                    if (i == media.end()) {
                        i--;
                        break;
                    }
                }
                if (actors_.count(*i))
                    return UuidActor(*i, actors_[*i]);

            } else {
                auto i = std::find(media.rbegin(), media.rend(), after_this_uuid);
                if (i == media.rend()) {
                    // not found!
                    return make_error(
                        xstudio_error::error,
                        "playlist::get_next_media_atom called with uuid that is not in "
                        "playlist");
                }
                while (skip_by++) {
                    i++;
                    if (i == media.rend()) {
                        i--;
                        break;
                    }
                }

                if (actors_.count(*i))
                    return UuidActor(*i, actors_[*i]);
            }

            return make_error(
                xstudio_error::error,
                "playlist::get_next_media_atom called with uuid for which no media actor "
                "exists");
        },

        [=](session::session_atom) {
            delegate(caf::actor_cast<caf::actor>(playlist_), session::session_atom_v);
        },

        [=](utility::serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        });
}

void ContactSheetActor::sort_alphabetically() {

    using SourceAndUuid = std::pair<std::string, utility::Uuid>;
    auto media_names_vs_uuids =
        std::make_shared<std::vector<std::pair<std::string, utility::Uuid>>>();

    for (const auto &i : base_.media()) {

        // Pro tip: because i is a reference, it's the reference that is captured in our lambda
        // below and therefore it is 'unstable' so we make a copy here and use that in the
        // lambda as this is object-copied in the capture instead.
        UuidActor media_actor(i, actors_[i]);

        request(media_actor.actor(), infinite, media::media_reference_atom_v, utility::Uuid())
            .await(

                [=](const std::pair<Uuid, MediaReference> &m_ref) mutable {
                    std::string path = uri_to_posix_path(m_ref.second.uri());
                    path             = std::string(path, path.rfind("/") + 1);
                    path             = to_lower(path);

                    (*media_names_vs_uuids).push_back(std::make_pair(path, media_actor.uuid()));

                    if (media_names_vs_uuids->size() == base_.media().size()) {

                        std::sort(
                            media_names_vs_uuids->begin(),
                            media_names_vs_uuids->end(),
                            [](const SourceAndUuid &a, const SourceAndUuid &b) -> bool {
                                return a.first < b.first;
                            });

                        utility::UuidList ordered_uuids;
                        for (const auto &p : (*media_names_vs_uuids)) {
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
