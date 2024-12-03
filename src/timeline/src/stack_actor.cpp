// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/timeline_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

caf::actor StackActor::deserialise(const utility::JsonStore &value, const bool replace_item) {
    auto key   = utility::Uuid(value.at("base").at("item").at("uuid"));
    auto actor = caf::actor();
    auto type  = value.at("base").at("container").at("type").get<std::string>();

    auto item = Item();

    if (type == "Track") {
        actor = spawn<TrackActor>(static_cast<utility::JsonStore>(value), item);
    } else if (type == "Clip") {
        actor = spawn<ClipActor>(static_cast<utility::JsonStore>(value), item);
    } else if (type == "Gap") {
        actor = spawn<GapActor>(static_cast<utility::JsonStore>(value), item);
    } else if (type == "Stack") {
        actor = spawn<StackActor>(static_cast<utility::JsonStore>(value), item);
    }

    if (actor) {
        add_item(UuidActor(key, actor));
        if (replace_item) {
            auto itemit = find_uuid(base_.item().children(), key);
            if (itemit != base_.item().end()) {
                (*itemit) = item;
            } else {
                spdlog::warn(
                    "{} Invalid item to replace {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(key),
                    value.dump(2));
            }
        }
    }

    return actor;
}

void StackActor::deserialise() {
    for (auto &i : base_.item().children()) {
        switch (i.item_type()) {
        case IT_CLIP: {
            auto actor = spawn<ClipActor>(i, i);
            add_item(UuidActor(i.uuid(), actor));
        } break;
        case IT_GAP: {
            auto actor = spawn<GapActor>(i, i);
            add_item(UuidActor(i.uuid(), actor));
        } break;
        case IT_STACK: {
            auto actor = spawn<StackActor>(i, i);
            add_item(UuidActor(i.uuid(), actor));
        } break;
        case IT_AUDIO_TRACK:
        case IT_VIDEO_TRACK: {
            auto actor = spawn<TrackActor>(i, i);
            add_item(UuidActor(i.uuid(), actor));
        } break;
        default:
            break;
        }
    }
}


StackActor::StackActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn.at("base"))) {

    base_.item().set_actor_addr(this);

    for (const auto &[key, value] : jsn.at("actors").items()) {
        try {
            deserialise(value, true);
        } catch (const std::exception &e) {
            spdlog::error("{}", e.what());
        }
    }
    base_.item().set_system(&system());
    base_.item().bind_item_post_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });

    init();
}

StackActor::StackActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &pitem)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn.at("base"))) {

    base_.item().set_actor_addr(this);

    for (const auto &[key, value] : jsn.at("actors").items()) {
        try {
            deserialise(value, true);
        } catch (const std::exception &e) {
            spdlog::error("{}", e.what());
        }
    }
    base_.item().set_system(&system());
    base_.item().bind_item_post_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });
    pitem = base_.item().clone();

    init();
}

StackActor::StackActor(
    caf::actor_config &cfg,
    const std::string &name,
    const utility::FrameRate &rate,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, rate, uuid, this) {

    base_.item().set_system(&system());
    base_.item().set_name(name);
    base_.item().bind_item_post_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });
    init();
}

StackActor::StackActor(caf::actor_config &cfg, const Item &item)
    : caf::event_based_actor(cfg), base_(item, this) {
    base_.item().set_system(&system());
    deserialise();
    init();
}

StackActor::StackActor(caf::actor_config &cfg, const Item &item, Item &nitem)
    : StackActor(cfg, item) {
    nitem = base_.item().clone();
}

void StackActor::on_exit() {
    for (const auto &i : actors_)
        send_exit(i.second, caf::exit_reason::user_shutdown);
}

// trigger actor creation
void StackActor::item_event_callback(const utility::JsonStore &event, Item &item) {

    switch (static_cast<ItemAction>(event.at("action"))) {
    case IA_INSERT: {
        auto cuuid = utility::Uuid(event.at("item").at("uuid"));
        // spdlog::warn("{} {} {} {}", find_uuid(base_.item().children(), cuuid) !=
        // base_.item().cend(), actors_.count(cuuid), not event["blind"].is_null(),
        // event.dump(2)); needs to be child..
        auto child_item_it = find_uuid(base_.item().children(), cuuid);
        if (child_item_it != base_.item().end() and not actors_.count(cuuid) and
            not event.at("blind").is_null()) {
            // our child
            // spdlog::warn("RECREATE MATCH");

            auto actor = deserialise(utility::JsonStore(event.at("blind")), false);
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(actor)));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));
            child_item_it->set_actor_addr(actor);
            // change item actor addr
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

            // item actor_addr will be wrong.. in ancestors
            // send special update..
            send(
                base_.event_group(),
                event_atom_v,
                item_atom_v,
                child_item_it->make_actor_addr_update(),
                true);
        }
    } break;

    case IA_REMOVE: {
        auto cuuid = utility::Uuid(event.at("item_uuid"));
        // child destroyed
        if (actors_.count(cuuid)) {
            // spdlog::warn("destroy
            // {}",to_string(caf::actor_cast<caf::actor_addr>(actors_[cuuid])));
            demonitor(actors_[cuuid]);
            send_exit(actors_[cuuid], caf::exit_reason::user_shutdown);
            actors_.erase(cuuid);
        }
    } break;

    case IA_LOCK:
    case IA_ENABLE:
    case IA_ACTIVE:
    case IA_RANGE:
    case IA_AVAIL:
    case IA_SPLICE:
    case IA_ADDR:
    case IA_NONE:
    default:
        break;
    }
}

caf::message_handler StackActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
            // should we handle child down ?
        },

        [=](utility::event_atom, notification_atom, const JsonStore &) {},

        [=](link_media_atom, const UuidActorMap &media, const bool force) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (actors_.empty()) {
                rp.deliver(true);
            } else {
                // pool direct children for state.
                fan_out_request<policy::select_all>(
                    map_value_to_vec(actors_), infinite, link_media_atom_v, media, force)
                    .await(
                        [=](std::vector<bool> items) mutable { rp.deliver(true); },
                        [=](error &err) mutable {
                            spdlog::warn(
                                "{} {} {}",
                                __PRETTY_FUNCTION__,
                                to_string(err),
                                base_.item().name());
                            rp.deliver(false);
                        });
            }
            return rp;
        },

        [=](item_marker_atom, insert_item_atom, const Marker &value) -> JsonStore {
            auto markers = base_.item().markers();
            markers.push_back(value);
            auto jsn = base_.item().set_markers(markers);

            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);

            return jsn;
        },

        [=](item_marker_atom, insert_item_atom, const std::vector<Marker> &value) -> JsonStore {
            auto markers = base_.item().markers();
            markers.insert(markers.end(), value.begin(), value.end());
            auto jsn = base_.item().set_markers(markers);

            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);

            return jsn;
        },

        [=](item_marker_atom, const std::vector<Marker> &markers) -> JsonStore {
            auto jsn = base_.item().set_markers(markers);

            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);

            return jsn;
        },

        [=](utility::rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](utility::rate_atom atom, const media::MediaType media_type) {
            delegate(caf::actor_cast<caf::actor>(this), atom);
        },

        [=](item_marker_atom) -> std::vector<Marker> {
            std::vector<Marker> result(
                base_.item().markers().begin(), base_.item().markers().end());
            return result;
        },

        [=](item_atom) -> Item { return base_.item().clone(); },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            request(caf::actor_cast<caf::actor>(this), infinite, utility::serialise_atom_v)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item().clone()));
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_lock_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_locked(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_atom, int index) -> result<Item> {
            if (static_cast<size_t>(index) >= base_.item().size()) {
                return make_error(xstudio_error::error, "Invalid index");
            }
            auto it = base_.item().cbegin();
            std::advance(it, index);
            return (*it).clone();
        },

        [=](item_prop_atom, const utility::JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_prop_atom,
            const utility::JsonStore &value,
            const std::string &path) -> JsonStore {
            auto prop = base_.item().prop();
            try {
                auto ptr = nlohmann::json::json_pointer(path);
                prop.at(ptr).update(value);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            auto jsn = base_.item().set_prop(prop);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> utility::FrameRange { return base_.item().trimmed_range(); },

        // should these be reflected upward ?
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

        // check events processes
        [=](item_atom, event_atom, const std::set<utility::Uuid> &events) -> bool {
            auto result = true;
            for (const auto &i : events) {
                if (not events_processed_.contains(i)) {
                    result = false;
                    break;
                }
            }
            return result;
        },

        // handle child change events.
        [=](event_atom, item_atom, const JsonStore &update, const bool hidden) {
            auto event_ids = base_.item().update(update);
            if (not event_ids.empty()) {
                events_processed_.insert(event_ids.begin(), event_ids.end());
                auto more = base_.item().refresh();
                if (not more.is_null()) {
                    more.insert(more.begin(), update.begin(), update.end());
                    send(base_.event_group(), event_atom_v, item_atom_v, more, hidden);
                    return;
                }
            }

            send(base_.event_group(), event_atom_v, item_atom_v, update, hidden);
        },

        [=](insert_item_atom,
            const int index,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            insert_items(rp, index, uav);
            return rp;
        },

        [=](insert_item_atom,
            const utility::Uuid &before_uuid,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto index = base_.item().size();
            // find index. for uuid
            if (not before_uuid.is_null()) {
                auto it = find_uuid(base_.item().children(), before_uuid);
                if (it == base_.item().end())
                    rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));
                else
                    index = std::distance(base_.item().begin(), it);
            }

            if (rp.pending())
                insert_items(rp, index, uav);

            return rp;
        },

        [=](move_item_atom, const int src_index, const int count, const int dst_index)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items(rp, src_index, count, dst_index);
            return rp;
        },

        [=](move_item_atom,
            const utility::Uuid &src_uuid,
            const int count,
            const utility::Uuid &before_uuid) -> result<JsonStore> {
            // check src is valid.
            auto rp   = make_response_promise<JsonStore>();
            auto sitb = find_uuid(base_.item().children(), src_uuid);
            if (sitb == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid src uuid"));


            if (rp.pending()) {
                auto dit = base_.item().children().end();
                if (not before_uuid.is_null()) {
                    dit = find_uuid(base_.item().children(), before_uuid);
                    if (dit == base_.item().end())
                        rp.deliver(make_error(xstudio_error::error, "Invalid dst uuid"));
                }
                if (rp.pending())
                    move_items(
                        rp,
                        std::distance(base_.item().begin(), sitb),
                        count,
                        std::distance(base_.item().begin(), dit));
            }

            return rp;
        },

        [=](utility::clear_atom) -> result<bool> {
            auto rp = make_response_promise<bool>();
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                remove_item_atom_v,
                0,
                (int)base_.item().size(),
                true)
                .then(
                    [=](const std::pair<JsonStore, std::vector<Item>> &) mutable {
                        rp.deliver(true);
                    },
                    [=](caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](remove_item_atom,
            const int index,
            const bool) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index);
            return rp;
        },

        [=](remove_item_atom, const int index, const int count, const bool)
            -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index, count);
            return rp;
        },

        [=](remove_item_atom,
            const utility::Uuid &uuid,
            const bool) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                remove_items(rp, std::distance(base_.item().begin(), it));

            return rp;
        },

        [=](erase_item_atom, const int index, const bool) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index);
            return rp;
        },

        [=](erase_item_atom, const int index, const int count, const bool)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index, count);
            return rp;
        },

        [=](erase_item_atom, const utility::Uuid &uuid, const bool) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                erase_items(rp, std::distance(base_.item().begin(), it));

            return rp;
        },

        [=](playhead::source_atom,
            const UuidUuidMap &swap,
            const utility::UuidActorMap &media) -> result<bool> {
            auto rp = make_response_promise<bool>();

            if (actors_.empty()) {
                rp.deliver(true);
            } else {
                fan_out_request<policy::select_all>(
                    map_value_to_vec(actors_), infinite, playhead::source_atom_v, swap, media)
                    .await(
                        [=](std::vector<bool> items) mutable { rp.deliver(true); },
                        [=](error &err) mutable {
                            spdlog::warn(
                                "{} {} {}",
                                __PRETTY_FUNCTION__,
                                to_string(err),
                                base_.item().name());
                            rp.deliver(err);
                        });
            }

            return rp;
        },

        [=](utility::duplicate_atom) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            JsonStore jsn;
            auto dup = base_.duplicate();
            dup.item().clear();

            jsn["base"]   = dup.serialise();
            jsn["actors"] = {};
            auto actor    = spawn<StackActor>(jsn);

            if (actors_.empty()) {
                rp.deliver(UuidActor(dup.uuid(), actor));
            } else {
                // duplicate all children and relink against items.
                scoped_actor sys{system()};

                for (const auto &i : base_.children()) {
                    auto ua = request_receive<UuidActor>(
                        *sys, actors_[i.uuid()], utility::duplicate_atom_v);
                    request_receive<JsonStore>(
                        *sys, actor, insert_item_atom_v, -1, UuidActorVector({ua}));
                }
                rp.deliver(UuidActor(dup.uuid(), actor));
            }

            return rp;
        },

        [=](utility::event_atom, utility::change_atom) {
            send(base_.event_group(), utility::event_atom_v, utility::change_atom_v);
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
        }};
}

void StackActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    set_down_handler([=](down_msg &msg) {
        // if a child dies we won't have enough information to recreate it.
        // we still need to report it up the chain though.
        for (auto it = std::begin(actors_); it != std::end(actors_); ++it) {
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

                    send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
                }
                break;
            }
        }
    });
}

void StackActor::add_item(const utility::UuidActor &ua) {
    // joining must be in sync
    scoped_actor sys{system()};

    try {
        auto grp =
            request_receive<caf::actor>(*sys, ua.actor(), utility::get_event_group_atom_v);
        auto joined = request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, this);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // join_event_group(this, ua.second);
    monitor(ua.actor());
    actors_[ua.uuid()] = ua.actor();
}

void StackActor::insert_items(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int index,
    const UuidActorVector &uav) {
    // validate items can be inserted.
    fan_out_request<policy::select_all>(vector_to_caf_actor_vector(uav), infinite, item_atom_v)
        .then(
            [=](std::vector<Item> items) mutable {
                // items are valid for insertion ?
                for (const auto &i : items) {
                    if (not base_.item().valid_child(i))
                        return rp.deliver(
                            make_error(xstudio_error::error, "Invalid child type"));
                }

                // take ownership
                for (const auto &ua : uav)
                    add_item(ua);

                // find insertion point..
                auto it = std::next(base_.item().begin(), index);

                // insert items..
                // our list will be out of order..
                auto changes = JsonStore(R"([])"_json);
                for (auto uit = uav.rbegin(); uit != uav.rend(); ++uit) {
                    // find item..
                    auto found = false;
                    for (const auto &i : items) {
                        if (uit->uuid() == i.uuid()) {
                            auto tmp = base_.item().insert(it, i);
                            it       = std::next(base_.item().begin(), index);
                            changes.insert(changes.end(), tmp.begin(), tmp.end());
                            found = true;
                            break;
                        }
                    }

                    if (not found) {
                        spdlog::error("item not found for insertion");
                    }
                }

                // add changes to stack
                auto more = base_.item().refresh();
                if (not more.is_null())
                    changes.insert(changes.end(), more.begin(), more.end());

                send(base_.event_group(), event_atom_v, item_atom_v, changes, false);
                rp.deliver(changes);
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void StackActor::remove_items(
    caf::typed_response_promise<std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
    const int index,
    const int count) {

    try {
        rp.deliver(remove_items(index, count));
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void StackActor::erase_items(
    caf::typed_response_promise<JsonStore> rp, const int index, const int count) {

    try {
        auto result = remove_items(index, count);
        for (const auto &i : result.second)
            send_exit(i.actor(), caf::exit_reason::user_shutdown);
        rp.deliver(result.first);

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

std::pair<utility::JsonStore, std::vector<timeline::Item>>
StackActor::remove_items(const int index, const int count) {
    std::vector<Item> items;
    JsonStore changes(R"([])"_json);

    if (index < 0 or index + count - 1 >= static_cast<int>(base_.item().size()))
        throw std::runtime_error("Invalid index / count");
    else {
        scoped_actor sys{system()};

        for (int i = index + count - 1; i >= index; i--) {
            auto it = std::next(base_.item().begin(), i);
            if (it != base_.item().end()) {
                auto item = *it;
                demonitor(item.actor());
                actors_.erase(item.uuid());

                auto blind = request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);

                auto tmp = base_.item().erase(it, blind);
                changes.insert(changes.end(), tmp.begin(), tmp.end());
                items.push_back(item.clone());
            }
        }

        auto more = base_.item().refresh();
        if (not more.is_null())
            changes.insert(changes.begin(), more.begin(), more.end());

        send(base_.event_group(), event_atom_v, item_atom_v, changes, false);
    }

    return std::make_pair(changes, items);
}


void StackActor::move_items(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int src_index,
    const int count,
    const int dst_index) {

    // don't allow mixing audio / video tracks ?

    if (dst_index == src_index or not count)
        rp.deliver(make_error(xstudio_error::error, "Invalid Move"));
    else {
        auto sit = std::next(base_.item().begin(), src_index);
        auto eit = std::next(sit, count);
        auto dit = std::next(base_.item().begin(), dst_index);

        auto changes = base_.item().splice(dit, base_.item().children(), sit, eit);
        auto more    = base_.item().refresh();
        if (not more.is_null())
            changes.insert(changes.begin(), more.begin(), more.end());

        send(base_.event_group(), event_atom_v, item_atom_v, changes, false);
        rp.deliver(changes);
    }
}
