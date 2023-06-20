// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/history/history_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

caf::actor
TimelineActor::deserialise(const utility::JsonStore &value, const bool replace_item) {
    auto key   = utility::Uuid(value["base"]["item"]["uuid"]);
    auto actor = caf::actor();

    if (value["base"]["container"]["type"] == "Stack") {
        auto item = Item();
        actor     = spawn<StackActor>(static_cast<utility::JsonStore>(value), item);
        add_item(UuidActor(key, actor));
        if (replace_item) {
            auto itemit = find_uuid(base_.item().children(), key);
            (*itemit)   = item;
        }
    }

    return actor;
}

// trigger actor creation
void TimelineActor::item_event_callback(const utility::JsonStore &event, Item &item) {

    switch (static_cast<ItemAction>(event["action"])) {
    case IT_INSERT: {
        auto cuuid = utility::Uuid(event["item"]["uuid"]);
        // spdlog::warn("{} {} {} {}", find_uuid(base_.item().children(), cuuid) !=
        // base_.item().cend(), actors_.count(cuuid), not event["blind"].is_null(),
        // event.dump(2)); needs to be child..
        auto child_item_it = find_uuid(base_.item().children(), cuuid);
        if (child_item_it != base_.item().cend() and not actors_.count(cuuid) and
            not event["blind"].is_null()) {
            // our child
            // spdlog::warn("RECREATE MATCH");

            auto actor = deserialise(utility::JsonStore(event["blind"]), false);
            add_item(UuidActor(cuuid, actor));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(actor)));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));
            child_item_it->set_actor_addr(actor);
            // change item actor addr
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

            // item actor_addr will be wrong.. in ancestors
            // send special update..
            // send(event_group_, event_atom_v, item_atom_v,
            // child_item_it->make_actor_addr_update(), true);
        }
    } break;

    case IT_REMOVE: {
        auto cuuid = utility::Uuid(event["item_uuid"]);
        // child destroyed
        if (actors_.count(cuuid)) {
            // spdlog::warn("destroy
            // {}",to_string(caf::actor_cast<caf::actor_addr>(actors_[cuuid])));
            demonitor(actors_[cuuid]);
            send_exit(actors_[cuuid], caf::exit_reason::user_shutdown);
            actors_.erase(cuuid);
        }
    } break;

    case IT_ENABLE:
    case IT_ACTIVE:
    case IT_AVAIL:
    case IT_SPLICE:
    case IT_ADDR:
    case IA_NONE:
    default:
        break;
    }
}

TimelineActor::TimelineActor(
    caf::actor_config &cfg, const utility::JsonStore &jsn, const caf::actor &playlist)
    : caf::event_based_actor(cfg),
      base_(static_cast<utility::JsonStore>(jsn["base"])),
      playlist_(playlist ? caf::actor_cast<caf::actor_addr>(playlist) : caf::actor_addr()) {
    base_.item().set_actor_addr(this);
    // parse and generate tracks/stacks.

    for (const auto &[key, value] : jsn["actors"].items()) {
        try {
            deserialise(value, true);
        } catch (const std::exception &e) {
            spdlog::error("{}", e.what());
        }
    }
    base_.item().set_system(&system());
    base_.item().bind_item_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });

    init();
}

TimelineActor::TimelineActor(
    caf::actor_config &cfg,
    const std::string &name,
    const utility::Uuid &uuid,
    const caf::actor &playlist)
    : caf::event_based_actor(cfg),
      base_(name, uuid, this),
      playlist_(playlist ? caf::actor_cast<caf::actor_addr>(playlist) : caf::actor_addr()) {

    // create default stack
    auto suuid = Uuid::generate();
    auto stack = spawn<StackActor>("Stack", suuid);
    anon_send<message_priority::high>(this, insert_item_atom_v, 0, UuidActor(suuid, stack));
    base_.item().set_system(&system());
    base_.item().bind_item_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });

    init();
}

caf::message_handler TimelineActor::default_event_handler() {
    return {
        [=](utility::event_atom, item_atom, const Item &) {},
        [=](utility::event_atom, item_atom, const JsonStore &, const bool) {},
    };
}


void TimelineActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    auto change_event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(change_event_group_);

    auto history_uuid = Uuid::generate();
    auto history = spawn<history::HistoryMapActor<sys_time_point, JsonStore>>(history_uuid);
    link_to(history);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        for (auto it = std::begin(actors_); it != std::end(actors_); ++it) {
            // if a child dies we won't have enough information to recreate it.
            // we still need to report it up the chain though.

            if (msg.source == it->second) {
                demonitor(it->second);
                actors_.erase(it);

                // remove from base.
                auto it = find_actor_addr(base_.item().children(), msg.source);

                if (it != base_.item().end()) {
                    auto jsn  = base_.item().erase(it);
                    auto more = base_.item().refresh();
                    if (not more.is_null())
                        jsn.insert(jsn.begin(), more.begin(), more.end());

                    // send(event_group_, event_atom_v, item_atom_v, jsn, false);
                }
                break;
            }
        }
    });

    // update_edit_list_ = true;

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

        [=](history::history_atom) -> UuidActor { return UuidActor(history_uuid, history); },

        [=](link_media_atom, const UuidActorMap &media) -> result<bool> {
            auto rp = make_response_promise<bool>();

            // pool direct children for state.
            fan_out_request<policy::select_all>(
                map_value_to_vec(actors_), infinite, link_media_atom_v, media)
                .await(
                    [=](std::vector<bool> items) mutable { rp.deliver(true); },
                    [=](error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null()) {
                // send(event_group_, event_atom_v, item_atom_v, jsn, false);
                anon_send(history, history::log_atom_v, sysclock::now(), jsn);
            }
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null()) {
                // send(event_group_, event_atom_v, item_atom_v, jsn, false);
                anon_send(history, history::log_atom_v, sysclock::now(), jsn);
            }
            return jsn;
        },

        [=](item_atom) -> Item { return base_.item(); },

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null()) {
                // send(event_group_, event_atom_v, item_atom_v, jsn, false);
                anon_send(history, history::log_atom_v, sysclock::now(), jsn);
            }
            return jsn;
        },

        [=](item_atom, int index) -> result<Item> {
            if (static_cast<size_t>(index) >= base_.item().size()) {
                return make_error(xstudio_error::error, "Invalid index");
            }
            auto it = base_.item().cbegin();
            std::advance(it, index);
            return *it;
        },

        [=](utility::event_atom, utility::change_atom, const bool) {
            content_changed_ = false;
            // send(event_group_, event_atom_v, item_atom_v, base_.item());
            send(event_group_, utility::event_atom_v, change_atom_v);
            send(change_event_group_, utility::event_atom_v, utility::change_atom_v);
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

        // handle child change events.
        // [=](event_atom, item_atom, const Item &item) {
        //     // it's possibly one of ours.. so try and substitue the record
        //     if(base_.item().replace_child(item)) {
        //         base_.item().refresh();
        //         send(this, utility::event_atom_v, change_atom_v);
        //     }
        // },

        // handle child change events.
        [=](event_atom, item_atom, const JsonStore &update, const bool hidden) {
            if (base_.item().update(update)) {
                auto more = base_.item().refresh();
                if (not more.is_null()) {
                    more.insert(more.begin(), update.begin(), update.end());
                    // send(event_group_, event_atom_v, item_atom_v, more, hidden);
                    if (not hidden)
                        anon_send(history, history::log_atom_v, sysclock::now(), more);

                    send(this, utility::event_atom_v, change_atom_v);
                    return;
                }
            }

            // send(event_group_, event_atom_v, item_atom_v, update, hidden);
            if (not hidden)
                anon_send(history, history::log_atom_v, sysclock::now(), update);
            send(this, utility::event_atom_v, change_atom_v);
        },

        // loop with timepoint
        [=](history::undo_atom, const sys_time_point &key) -> result<bool> {
            auto rp = make_response_promise<bool>();

            request(history, infinite, history::undo_atom_v, key)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        // loop with timepoint
        [=](history::redo_atom, const sys_time_point &key) -> result<bool> {
            auto rp = make_response_promise<bool>();

            request(history, infinite, history::redo_atom_v, key)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::undo_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            request(history, infinite, history::undo_atom_v)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::redo_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            request(history, infinite, history::redo_atom_v)
                .then(
                    [=](const JsonStore &hist) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().undo(hist);
            if (actors_.empty())
                return true;
            // push to children..
            auto rp = make_response_promise<bool>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(actors_), infinite, history::undo_atom_v, hist)
                .then(
                    [=](std::vector<bool> updated) mutable { rp.deliver(true); },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().redo(hist);
            if (actors_.empty())
                return true;
            // push to children..
            auto rp = make_response_promise<bool>();

            fan_out_request<policy::select_all>(
                map_value_to_vec(actors_), infinite, history::redo_atom_v, hist)
                .then(
                    [=](std::vector<bool> updated) mutable { rp.deliver(true); },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](insert_item_atom, const int index, const UuidActor &ua) -> result<JsonStore> {
            if (not base_.item().empty())
                return make_error(xstudio_error::error, "Only one child allowed");
            auto rp = make_response_promise<JsonStore>();
            // get item..
            request(ua.actor(), infinite, item_atom_v)
                .await(
                    [=](const Item &item) mutable {
                        rp.delegate<message_priority::high>(
                            caf::actor_cast<caf::actor>(this),
                            insert_item_atom_v,
                            index,
                            ua,
                            item);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        // we only allow access to direct children.. ?
        [=](insert_item_atom, const int index, const UuidActor &ua, const Item &item)
            -> result<JsonStore> {
            if (not base_.item().empty())
                return make_error(xstudio_error::error, "Only one child allowed");
            if (not base_.item().valid_child(item)) {
                return make_error(xstudio_error::error, "Invalid child type");
            }

            // take ownership
            add_item(ua);

            auto rp = make_response_promise<JsonStore>();
            // re-aquire item. as we may have gone out of sync..
            request(ua.actor(), infinite, item_atom_v)
                .await(
                    [=](const Item &item) mutable {
                        // insert on index..
                        // cheat..
                        auto it  = base_.item().begin();
                        auto ind = 0;
                        for (int i = 0; it != base_.item().end(); i++, it++) {
                            if (i == index)
                                break;
                        }

                        auto changes = base_.item().insert(it, item);
                        auto more    = base_.item().refresh();
                        if (not more.is_null())
                            changes.insert(changes.begin(), more.begin(), more.end());

                        // broadcast change. (may need to be finer grained)
                        // send(event_group_, event_atom_v, item_atom_v, changes, false);
                        anon_send(history, history::log_atom_v, sysclock::now(), changes);
                        send(this, utility::event_atom_v, change_atom_v);

                        rp.deliver(true);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](insert_item_atom,
            const utility::Uuid &before_uuid,
            const UuidActor &ua) -> result<JsonStore> {
            if (not base_.item().empty())
                return make_error(xstudio_error::error, "Only one child allowed");
            auto rp = make_response_promise<JsonStore>();
            // get item..
            request(ua.actor(), infinite, item_atom_v)
                .then(
                    [=](const Item &item) mutable {
                        rp.delegate(
                            caf::actor_cast<caf::actor>(this),
                            insert_item_atom_v,
                            before_uuid,
                            ua,
                            item);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

            return rp;
        },

        [=](insert_item_atom,
            const utility::Uuid &before_uuid,
            const UuidActor &ua,
            const Item &item) -> result<JsonStore> {
            if (not base_.item().valid_child(item)) {
                return make_error(xstudio_error::error, "Invalid child type");
            }

            if (not base_.item().empty())
                return make_error(xstudio_error::error, "Only one child allowed");

            // take ownership
            add_item(ua);

            auto rp = make_response_promise<JsonStore>();
            // re-aquire item. as we may have gone out of sync..
            request(ua.actor(), infinite, item_atom_v)
                .await(
                    [=](const Item &item) mutable {
                        auto changes = utility::JsonStore();

                        if (before_uuid.is_null()) {
                            changes = base_.item().insert(base_.item().end(), item);
                        } else {
                            auto it = find_uuid(base_.item().children(), before_uuid);
                            if (it == base_.item().end()) {
                                return rp.deliver(
                                    make_error(xstudio_error::error, "Invalid uuid"));
                            }
                            changes = base_.item().insert(it, item);
                        }

                        auto more = base_.item().refresh();
                        if (not more.is_null())
                            changes.insert(changes.begin(), more.begin(), more.end());

                        // send(event_group_, event_atom_v, item_atom_v, changes, false);
                        anon_send(history, history::log_atom_v, sysclock::now(), changes);
                        send(this, utility::event_atom_v, change_atom_v);
                        rp.deliver(true);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](remove_item_atom, const int index) -> result<std::pair<JsonStore, Item>> {
            auto it = base_.item().children().begin();
            std::advance(it, index);
            if (it == base_.item().children().end())
                return make_error(xstudio_error::error, "Invalid index");
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            rp.delegate(caf::actor_cast<caf::actor>(this), remove_item_atom_v, it->uuid());
            return rp;
        },

        [=](remove_item_atom, const utility::Uuid &uuid) -> result<std::pair<JsonStore, Item>> {
            auto it = find_uuid(base_.item().children(), uuid);
            if (it == base_.item().end())
                return make_error(xstudio_error::error, "Invalid uuid");
            auto item = *it;
            demonitor(item.actor());
            actors_.erase(item.uuid());

            auto changes = base_.item().erase(it);
            auto more    = base_.item().refresh();
            if (not more.is_null())
                changes.insert(changes.begin(), more.begin(), more.end());

            // send(event_group_, event_atom_v, item_atom_v, changes, false);
            anon_send(history, history::log_atom_v, sysclock::now(), changes);

            send(this, utility::event_atom_v, change_atom_v);
            return std::make_pair(changes, item);
        },

        [=](erase_item_atom, const int index) -> result<JsonStore> {
            auto it = base_.item().children().begin();
            std::advance(it, index);
            if (it == base_.item().children().end())
                return make_error(xstudio_error::error, "Invalid index");
            auto rp = make_response_promise<JsonStore>();
            rp.delegate(caf::actor_cast<caf::actor>(this), erase_item_atom_v, it->uuid());
            return rp;
        },

        [=](erase_item_atom, const utility::Uuid &uuid) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            request(caf::actor_cast<caf::actor>(this), infinite, remove_item_atom_v, uuid)
                .then(
                    [=](const Item &item) mutable {
                        send_exit(item.actor(), caf::exit_reason::user_shutdown);
                        rp.deliver(true);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
            return rp;
        },

        // // code for playhead// get edit_list for all tracks/stacks..// this is temporary,
        // it'll
        // // need heavy changes..// also this only returns edit_lists for images, audio may be
        // // different..
        // [=](media::get_edit_list_atom, const Uuid &uuid) -> result<utility::EditList> {
        //     if (update_edit_list_) {
        //         std::vector<caf::actor> actors;
        //         for (const auto &i : base_.tracks())
        //             actors.push_back(actors_[i]);

        //         if (not actors.empty()) {
        //             auto rp = make_response_promise<utility::EditList>();

        //             fan_out_request<policy::select_all>(
        //                 actors, infinite, media::get_edit_list_atom_v, Uuid())
        //                 .await(
        //                     [=](std::vector<utility::EditList> sections) mutable {
        //                         edit_list_.clear();
        //                         for (const auto &i : base_.tracks()) {
        //                             for (const auto &ii : sections) {
        //                                 for (const auto &section : ii.section_list()) {
        //                                     const auto &[ud, rt, tc] = section;
        //                                     if (ud == i) {
        //                                         if (uuid.is_null())
        //                                             edit_list_.push_back(section);
        //                                         else
        //                                             edit_list_.push_back({uuid, rt, tc});
        //                                     }
        //                                 }
        //                             }
        //                         }
        //                         update_edit_list_ = false;
        //                         rp.deliver(edit_list_);
        //                     },
        //                     [=](error &err) mutable { rp.deliver(std::move(err)); });

        //             return rp;
        //         } else {
        //             edit_list_.clear();
        //             update_edit_list_ = false;
        //         }
        //     }

        //     return result<utility::EditList>(edit_list_);
        // },

        // [=](media::get_media_pointer_atom,
        //     const int logical_frame) -> result<media::AVFrameID> {
        //     if (base_.empty())
        //         return result<media::AVFrameID>(make_error(xstudio_error::error, "No
        //         media"));

        //     auto rp = make_response_promise<media::AVFrameID>();
        //     if (update_edit_list_) {
        //         request(actor_cast<caf::actor>(this), infinite, media::get_edit_list_atom_v)
        //             .then(
        //                 [=](const utility::EditList &) mutable {
        //                     deliver_media_pointer(logical_frame, rp);
        //                 },
        //                 [=](error &err) mutable { rp.deliver(std::move(err)); });
        //     } else {
        //         deliver_media_pointer(logical_frame, rp);
        //     }

        //     return rp;
        // },

        // [=](start_time_atom) -> utility::FrameRateDuration { return base_.start_time(); },

        // [=](utility::clear_atom) -> bool {
        //     base_.clear();
        //     for (const auto &i : actors_) {
        //         // this->leave(i.second);
        //         unlink_from(i.second);
        //         send_exit(i.second, caf::exit_reason::user_shutdown);
        //     }
        //     actors_.clear();
        //     return true;
        // },

        // [=](utility::event_atom, utility::change_atom) {
        //     update_edit_list_ = true;
        //     send(event_group_, utility::event_atom_v, utility::change_atom_v);
        // },

        // [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        // [=](utility::rate_atom) -> FrameRate { return base_.rate(); },

        // [=](utility::rate_atom, const FrameRate &rate) { base_.set_rate(rate); },

        // this operation isn't relevant ?
        [=](playlist::create_playhead_atom) -> UuidActor {
            if (playhead_)
                return playhead_;
            auto uuid  = utility::Uuid::generate();
            auto actor = spawn<playhead::PlayheadActor>(
                std::string("Timeline Playhead"), caf::actor_cast<caf::actor>(this), uuid);
            link_to(actor);
            playhead_ = UuidActor(uuid, actor);

            anon_send(actor, playhead::playhead_rate_atom_v, base_.rate());

            // this pushes this actor to the playhead as the 'source' that the
            // playhead will play
            send(
                actor,
                utility::event_atom_v,
                playhead::source_atom_v,
                std::vector<caf::actor>{caf::actor_cast<caf::actor>(this)});

            return playhead_;
        },
        [=](playlist::get_playhead_atom) {
            delegate(caf::actor_cast<caf::actor>(this), playlist::create_playhead_atom_v);
        },
        [=](playlist::get_change_event_group_atom) -> caf::actor {
            return change_event_group_;
        },

        [=](playlist::get_media_atom, const bool) -> UuidActorVector {
            return UuidActorVector(); // base_.item().find_all_uuid_actors(ItemType::IT_CLIP);
        },
        [=](playlist::get_media_atom) -> UuidActorVector {
            return UuidActorVector(); // base_.item().find_all_uuid_actors(ItemType::IT_CLIP);
        },

        [=](playlist::selection_actor_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(this);
        },

        [=](duration_atom, const int) {},

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const media::LogicalFrameRanges &ranges,
            const FrameRate &override_rate) -> caf::result<media::AVFrameIDs> {
            // spdlog::stopwatch sw;
            auto num_frames = 0;
            for (const auto &i : ranges)
                num_frames += (i.second - i.first) + 1;


            auto result = std::make_shared<media::AVFrameIDs>(num_frames);
            auto count  = std::make_shared<int>();
            *count      = 0;
            // spdlog::warn("{} {} {} {}", media_type, num_frames, start_frame,
            // override_rate.to_fps());

            caf::scoped_actor sys(system());

            auto item_tp = std::vector<std::optional<std::tuple<Item, utility::FrameRate>>>();
            item_tp.reserve(num_frames);

            for (const auto &r : ranges) {
                for (auto i = r.first; i <= r.second; i++) {
                    auto ii = base_.item().resolve_time(
                        FrameRate(i * override_rate.to_flicks()), media_type);
                    if (ii) {
                        item_tp.emplace_back(*ii);
                        (*count)++;
                    } else {
                        item_tp.emplace_back();
                    }
                }
            }

            // spdlog::error("resolve_time elapsed {:.3}", sw);

            //  only blank fraems
            auto bf = media::make_blank_frame(media_type);
            if (not *count) {
                for (auto i = 0; i < num_frames; i++)
                    (*result)[i] = bf;
                // spdlog::error("blank output elapsed {:.3}", sw);
                return *result;
            }

            auto rp = make_response_promise<media::AVFrameIDs>();

            auto start = 0;
            auto end   = 0;
            auto tps   = std::vector<FrameRate>();
            auto act   = caf::actor();

            for (auto i = 0; i < num_frames; i++) {
                auto item = item_tp[i];

                // dispatch on actor change
                if (not tps.empty() and (not item or std::get<0>(*item).actor() != act)) {
                    request(
                        act,
                        infinite,
                        media::get_media_pointer_atom_v,
                        media_type,
                        tps,
                        override_rate)
                        .then(
                            [=, s = start, e = end](const media::AVFrameIDs &mps) mutable {
                                for (auto ii = s; ii <= e; ii++) {
                                    (*result)[ii] = mps[ii - s];
                                    (*count)--;
                                    // spdlog::error("s {} e {} ii {} c {}", s, e, ii, *count);
                                    if (not *count) {
                                        rp.deliver(*result);
                                        // spdlog::error("get_media_pointers_atom elapsed
                                        // {:.3}", sw);
                                    }
                                }
                            },

                            [=, s = start, e = end](error &err) mutable {
                                for (auto ii = s; ii <= e; ii++) {
                                    (*result)[ii] = bf;
                                    (*count)--;
                                    // spdlog::error("s {} e {} ii {} c {}", s, e, ii, *count);
                                    if (not *count) {
                                        rp.deliver(*result);
                                        // spdlog::error("get_media_pointers_atom elapsed
                                        // {:.3}", sw);
                                    }
                                }
                            });

                    start = end = i;
                    tps.clear();
                    act = (item ? std::get<0>(*item).actor() : caf::actor());
                }

                if (not item) {
                    (*result)[i] = bf;
                } else {
                    if (tps.empty()) {
                        start = i;
                        act   = std::get<0>(*item).actor();
                    }
                    end = i;
                    tps.push_back(std::get<1>(*item));
                }
            }

            // catch all
            if (not tps.empty()) {
                request(
                    act,
                    infinite,
                    media::get_media_pointer_atom_v,
                    media_type,
                    tps,
                    override_rate)
                    .then(
                        [=, s = start, e = end](const media::AVFrameIDs &mps) mutable {
                            for (auto ii = s; ii <= e; ii++) {
                                (*result)[ii] = mps[ii - s];
                                (*count)--;
                                // spdlog::error("s {} e {} ii {} c {}", s, e, ii, *count);
                                if (not *count) {
                                    rp.deliver(*result);
                                    // spdlog::error("get_media_pointers_atom elapsed {:.3}",
                                    // sw);
                                }
                            }
                        },

                        [=, s = start, e = end](error &err) mutable {
                            for (auto ii = s; ii <= e; ii++) {
                                (*result)[ii] = bf;
                                (*count)--;
                                // spdlog::error("s {} e {} ii {} c {}", s, e, ii, *count);
                                if (not *count) {
                                    rp.deliver(*result);
                                    // spdlog::error("get_media_pointers_atom elapsed {:.3}",
                                    // sw);
                                }
                            }
                        });
            }

            // spdlog::error("get_media_pointers dispatched elapsed {:.3}", sw);
            return rp;
        },

        [=](media::get_edit_list_atom, const Uuid &uuid) -> result<utility::EditList> {
            auto el = utility::EditList(utility::ClipList({utility::EditListSection(
                base_.uuid(),
                base_.item().trimmed_frame_duration(),
                utility::Timecode(
                    base_.item().trimmed_frame_duration().frames(), base_.rate().to_fps()))}));
            return el;
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            if (not actors_.empty()) {
                auto rp = make_response_promise<JsonStore>();

                fan_out_request<policy::select_all>(
                    map_value_to_vec(actors_), infinite, serialise_atom_v)
                    .then(
                        [=](std::vector<JsonStore> json) mutable {
                            JsonStore jsn;
                            jsn["base"]   = base_.serialise();
                            jsn["actors"] = {};
                            for (const auto &j : json)
                                jsn["actors"]
                                   [static_cast<std::string>(j["base"]["container"]["uuid"])] =
                                       j;
                            rp.deliver(jsn);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

                return rp;
            }
            JsonStore jsn;
            jsn["base"]   = base_.serialise();
            jsn["actors"] = {};

            return result<JsonStore>(jsn);
        },


        // ***********************************************************
        //
        // The following message handlers mirror PlaylistSelectionActor
        // and allow this actor to mimic this actor so that it works
        // with the playhead
        //
        // ***********************************************************

        [=](playhead::delete_selection_from_playlist_atom) {},

        [=](media_hook::gather_media_sources_atom) {},

        [=](playhead::evict_selection_from_cache_atom) -> media::MediaKeyVector {
            return media::MediaKeyVector();
        },

        [=](playhead::get_selection_atom) -> UuidList { return UuidList{base_.uuid()}; },

        [=](playhead::get_selection_atom, caf::actor requester) {
            anon_send(
                requester,
                utility::event_atom_v,
                playhead::selection_changed_atom_v,
                UuidList{base_.uuid()});
        },

        [=](playhead::select_next_media_atom, const int skip_by) {},

        [=](playlist::select_all_media_atom) {},

        [=](playlist::select_media_atom, const UuidList &media_uuids) {},

        [=](playlist::select_media_atom) {},

        [=](playlist::select_media_atom, utility::Uuid media_uuid) {},

        [=](playhead::get_selected_sources_atom) -> std::vector<caf::actor> {
            std::vector<caf::actor> result;
            return result;
        },

        [=](session::get_playlist_atom) -> caf::actor {
            return caf::actor_cast<caf::actor>(playlist_);
        }

    );
}

void TimelineActor::add_item(const utility::UuidActor &ua) {
    // join_event_group(this, ua.second);
    scoped_actor sys{system()};

    try {
        auto grp =
            request_receive<caf::actor>(*sys, ua.actor(), utility::get_event_group_atom_v);
        auto joined = request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, this);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    monitor(ua.actor());
    actors_[ua.uuid()] = ua.actor();
}

void TimelineActor::on_exit() {
    for (const auto &i : actors_)
        send_exit(i.second, caf::exit_reason::user_shutdown);
}
