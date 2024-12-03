// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

// #include "xstudio/media/media_actor.hpp"
// #include "xstudio/atoms.hpp"
// #include "xstudio/playhead/playhead_actor.hpp"
// #include "xstudio/atoms.hpp"
// #include "xstudio/atoms.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace caf;

caf::actor TrackActor::deserialise(const utility::JsonStore &value, const bool replace_item) {
    auto key   = utility::Uuid(value.at("base").at("item").at("uuid"));
    auto actor = caf::actor();
    auto type  = value.at("base").at("container").at("type").get<std::string>();

    auto item = Item();

    if (type == "Clip") {
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

void TrackActor::deserialise() {
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
        default:
            break;
        }
    }
}

// trigger actor creation
void TrackActor::item_event_callback(const utility::JsonStore &event, Item &item) {

    switch (static_cast<ItemAction>(event.at("action"))) {
    case IA_INSERT: {
        // spdlog::warn("TrackActor IT_INSERT {}", event.dump(2));
        auto cuuid = utility::Uuid(event.at("item").at("uuid"));
        // spdlog::warn("{} {} {} {}", find_uuid(base_.item().children(), cuuid) !=
        // base_.item().cend(), actors_.count(cuuid), not event["blind"].is_null(),
        // event.dump(2)); needs to be child..

        // is it a direct child..
        auto child_item_it = find_uuid(base_.item().children(), cuuid);
        // spdlog::warn("{} {} {}", child_item_it != base_.item().cend(), not
        // actors_.count(cuuid), not event.at("blind").is_null());

        if (child_item_it != base_.item().end() and not actors_.count(cuuid) and
            not event.at("blind").is_null()) {
            // our child
            // spdlog::warn("RECREATE MATCH");

            // if(child_item_it->item_type() == IT_CLIP) {
            //     auto media_uuid = child_item_it->prop().value("media_uuid", Uuid());
            //     if(media_uuid) {
            //         // make sure media exists in timeline.

            //     }
            // }

            // spdlog::warn("child_item_it {}", child_item_it->item_type());
            // spdlog::warn("child_item_it {}", child_item_it->name());
            // spdlog::warn("child_item_it {}", child_item_it->prop().dump(2));

            auto actor = deserialise(utility::JsonStore(event.at("blind")), false);
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(actor)));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

            // iteraror becomes invalid..
            child_item_it->set_actor_addr(actor);


            // change item actor addr
            // spdlog::warn("TrackActor create
            // {}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

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
        // spdlog::warn("TrackActor IT_REMOVE {} {}", base_.item().name(), event.dump(2));

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

TrackActor::TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
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


TrackActor::TrackActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &pitem)
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

TrackActor::TrackActor(
    caf::actor_config &cfg,
    const std::string &name,
    const utility::FrameRate &rate,
    const media::MediaType media_type,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, rate, media_type, uuid, this) {
    base_.item().set_system(&system());
    base_.item().set_name(name);
    base_.item().bind_item_post_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });

    init();
}

TrackActor::TrackActor(caf::actor_config &cfg, const Item &item)
    : caf::event_based_actor(cfg), base_(item, this) {
    base_.item().set_system(&system());
    deserialise();
    init();
}

TrackActor::TrackActor(caf::actor_config &cfg, const Item &item, Item &nitem)
    : TrackActor(cfg, item) {
    nitem = base_.item().clone();
}


void TrackActor::on_exit() {
    for (const auto &i : actors_)
        send_exit(i.second, caf::exit_reason::user_shutdown);
}

caf::message_handler TrackActor::default_event_handler() {
    return caf::message_handler()
        .or_else(NotificationHandler::default_event_handler())
        .or_else(Container::default_event_handler());
}

caf::message_handler TrackActor::message_handler() {
    return caf::message_handler{
        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

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

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_atom) -> Item { return base_.item().clone(); },

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

        [=](item_atom, int index) -> result<Item> {
            if (static_cast<size_t>(index) >= base_.item().size()) {
                return make_error(xstudio_error::error, "Invalid index");
            }
            auto it = base_.item().begin();
            std::advance(it, index);
            return (*it).clone();
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

        [=](item_prop_atom, const utility::JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                send(base_.event_group(), event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](utility::rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](utility::rate_atom atom, const media::MediaType media_type) {
            delegate(caf::actor_cast<caf::actor>(this), atom);
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

        [=](insert_item_at_frame_atom,
            const int frame,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            insert_items_at_frame(rp, frame, uav);
            return rp;
        },

        [=](insert_item_atom,
            const int index,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            insert_items(rp, index, uav);
            return rp;
        },

        [=](insert_item_atom, const int index, const int frame, const UuidActorVector &uav)
            -> result<JsonStore> {
            auto rp     = make_response_promise<JsonStore>();
            auto tframe = base_.item().frame_at_index(index, frame);
            insert_items_at_frame(rp, tframe, uav);
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

        [=](remove_item_at_frame_atom,
            const int frame,
            const int duration,
            const bool add_gap,
            const bool collapse_gaps) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items_at_frame(rp, frame, duration, add_gap, collapse_gaps);
            return rp;
        },

        [=](remove_item_at_frame_atom, const int frame, const int duration, const bool add_gap)
            -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items_at_frame(rp, frame, duration, add_gap, true);
            return rp;
        },

        [=](remove_item_atom,
            const int index) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index, 1, false, true);
            return rp;
        },

        [=](remove_item_atom,
            const int index,
            const bool add_gap) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index, 1, add_gap, true);
            return rp;
        },

        [=](remove_item_atom, const int index, const int count, const bool add_gap)
            -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(rp, index, count, add_gap, true);
            return rp;
        },

        [=](remove_item_atom,
            const utility::Uuid &uuid,
            const bool add_gap) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                remove_items(rp, std::distance(base_.item().begin(), it), 1, add_gap, true);

            return rp;
        },

        [=](erase_item_at_frame_atom,
            const int frame,
            const int duration,
            const bool add_gap,
            const bool collapse_gaps) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items_at_frame(rp, frame, duration, add_gap, collapse_gaps);
            return rp;
        },

        [=](erase_item_at_frame_atom, const int frame, const int duration, const bool add_gap)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items_at_frame(rp, frame, duration, add_gap, true);
            return rp;
        },

        [=](erase_item_atom, const int index) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index, 1, false, true);
            return rp;
        },

        [=](erase_item_atom, const int index, const bool add_gap) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index, 1, add_gap, true);
            return rp;
        },

        [=](erase_item_atom, const int index, const int count, const bool add_gap)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(rp, index, count, add_gap, true);
            return rp;
        },

        [=](erase_item_atom,
            const utility::Uuid &uuid,
            const bool add_gap) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                erase_items(rp, std::distance(base_.item().begin(), it), 1, add_gap, true);

            return rp;
        },

        [=](split_item_at_frame_atom, const int frame) -> result<JsonStore> {
            auto rp          = make_response_promise<JsonStore>();
            auto split_point = base_.item().item_at_frame(frame);
            if (not split_point)
                rp.deliver(make_error(xstudio_error::error, "Invalid split frame"));
            else
                split_item(rp, split_point->first, split_point->second);

            return rp;
        },

        [=](split_item_atom, const int index, const int frame) -> result<JsonStore> {
            auto it = base_.item().children().begin();
            std::advance(it, index);
            if (it == base_.item().children().end())
                return make_error(xstudio_error::error, "Invalid index");
            auto rp = make_response_promise<JsonStore>();
            split_item(rp, it, frame);
            return rp;
        },

        [=](split_item_atom, const utility::Uuid &uuid, const int frame) -> result<JsonStore> {
            auto it = find_uuid(base_.item().children(), uuid);
            if (it == base_.item().end())
                return make_error(xstudio_error::error, "Invalid uuid");
            auto rp = make_response_promise<JsonStore>();
            split_item(rp, it, frame);
            return rp;
        },

        [=](move_item_at_frame_atom,
            const int frame,
            const int duration,
            const int dest_frame,
            const bool insert,
            const bool add_gap) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items_at_frame(rp, frame, duration, dest_frame, insert, add_gap);
            return rp;
        },

        [=](move_item_atom,
            const int src_index,
            const int count,
            const int dst_index,
            const bool add_gap) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items(rp, src_index, count, dst_index, add_gap);
            return rp;
        },

        [=](move_item_atom, const int src_index, const int count, const int dst_index)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items(rp, src_index, count, dst_index, false);
            return rp;
        },

        [=](move_item_atom,
            const utility::Uuid &src_uuid,
            const int count,
            const utility::Uuid &before_uuid) -> result<JsonStore> {
            // check src is valid.
            auto rp = make_response_promise<JsonStore>();

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
                        std::distance(base_.item().begin(), dit),
                        false);
            }

            return rp;
        },

        [=](utility::event_atom, utility::change_atom) {
            // update_edit_list_ = true;
            send(base_.event_group(), utility::event_atom_v, utility::change_atom_v);
        },

        [=](utility::event_atom, utility::name_atom, const std::string & /*name*/) {},

        [=](utility::duplicate_atom) -> result<UuidActor> {
            auto rp = make_response_promise<UuidActor>();
            JsonStore jsn;
            auto dup = base_.duplicate();
            dup.item().clear();

            jsn["base"]   = dup.serialise();
            jsn["actors"] = {};
            auto actor    = spawn<TrackActor>(jsn);

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

        [=](media::get_media_pointers_atom atom,
            const media::MediaType media_type,
            const utility::TimeSourceMode tsm,
            const utility::FrameRate &override_rate) -> caf::result<media::FrameTimeMapPtr> {
                
            // This is required by SubPlayhead actor to make the track
            // playable.
            return base_.item().get_all_frame_IDs(
                media_type,
                tsm,
                override_rate);

        }

    };
}


void TrackActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    // update_edit_list_ = true;

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

void TrackActor::add_item(const utility::UuidActor &ua) {
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


void TrackActor::split_item(
    caf::typed_response_promise<utility::JsonStore> rp,
    const Items::const_iterator &itemit,
    const int frame) {
    // validate frame is inside item..
    // validate item type..
    auto item = *itemit;
    if (item.item_type() == IT_GAP or item.item_type() == IT_CLIP) {
        auto trimmed_range = item.trimmed_range();
        auto orig_start    = trimmed_range.frame_start().frames();
        auto orig_duration = trimmed_range.frame_duration().frames();
        auto orig_end      = orig_start + orig_duration - 1;

        if (frame > orig_start and frame <= orig_end) {
            // duplicate item to split.
            request(item.actor(), infinite, utility::duplicate_atom_v)
                .await(
                    [=](const UuidActor &ua) mutable {
                        // adjust start frames if clip.
                        auto item1_range = trimmed_range;
                        auto item2_range = trimmed_range;

                        item1_range.set_duration(
                            (frame - item1_range.frame_start().frames()) * item1_range.rate());

                        if (item.item_type() != IT_GAP)
                            item2_range.set_start(FrameRate(item2_range.rate() * frame));
                        item2_range.set_duration(FrameRate(
                            item2_range.rate() *
                            (orig_duration - item1_range.frame_duration().frames())));

                        // set range on new item
                        request(
                            ua.actor(), infinite, timeline::active_range_atom_v, item2_range)
                            .await(
                                [=](const JsonStore &) mutable {
                                    // set range on old item
                                    request(
                                        item.actor(),
                                        infinite,
                                        timeline::active_range_atom_v,
                                        item1_range)
                                        .await(
                                            [=](const JsonStore &) mutable {
                                                // insert next to original.
                                                // we got a backlog of inflight events.
                                                // track will not be in sync
                                                insert_items(
                                                    rp,
                                                    static_cast<int>(
                                                        std::distance(
                                                            base_.item().cbegin(), itemit) +
                                                        1),
                                                    UuidActorVector({ua}));
                                            },
                                            [=](error &err) mutable {
                                                rp.deliver(std::move(err));
                                            });
                                    // insert next to original.
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });
        } else {
            // spdlog::warn("{} {} frame {} clip start {} clip end {}", __PRETTY_FUNCTION__,
            // "Invalid frame to split on", frame, orig_start, orig_end);
            rp.deliver(make_error(xstudio_error::error, "Invalid frame to split on"));
        }
    } else {
        rp.deliver(make_error(xstudio_error::error, "Invalid type to split"));
    }
}

void TrackActor::insert_items(
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

                scoped_actor sys{system()};

                // take ownership
                for (const auto &ua : uav)
                    add_item(ua);

                // find insertion point..
                auto it = std::next(base_.item().begin(), index);

                // insert items..
                // our list will be out of order..
                auto changes = JsonStore(R"([])"_json);
                for (const auto &ua : uav) {
                    // find item..
                    auto found = false;
                    for (const auto &i : items) {
                        if (ua.uuid() == i.uuid()) {
                            // we need to serialise item so undo redo can remove recreate it.
                            auto blind =
                                request_receive<JsonStore>(*sys, ua.actor(), serialise_atom_v);

                            auto tmp = base_.item().insert(it, i, blind);
                            changes.insert(changes.begin(), tmp.begin(), tmp.end());
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
                    changes.insert(changes.begin(), more.begin(), more.end());

                send(base_.event_group(), event_atom_v, item_atom_v, changes, false);

                rp.deliver(changes);
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void TrackActor::insert_items_at_frame(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int frame,
    const utility::UuidActorVector &uav) {
    auto item_frame = base_.item().item_at_frame(frame);

    if (not item_frame) {
        // off the end of the track..
        // insert gap to fill space..
        UuidActorVector uav_plus_gap;
        auto track_end = base_.item().trimmed_frame_start().frames() +
                         base_.item().trimmed_frame_duration().frames() - 1;
        auto filler   = frame - track_end;
        auto gap_uuid = utility::Uuid::generate();
        auto gap_actor =
            spawn<GapActor>("Gap", FrameRateDuration(filler, base_.item().rate()), gap_uuid);
        uav_plus_gap.push_back(UuidActor(gap_uuid, gap_actor));
        for (const auto &i : uav)
            uav_plus_gap.push_back(i);

        insert_items(rp, base_.item().size(), uav_plus_gap);
    } else {
        auto cit    = item_frame->first;
        auto cframe = item_frame->second;
        auto index  = std::distance(base_.item().cbegin(), cit);

        if (cframe == cit->trimmed_frame_start().frames()) {
            // simple insertion..
            insert_items(rp, index, uav);
        } else {
            // complex.. we need to split item
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                split_item_atom_v,
                static_cast<int>(index),
                static_cast<int>(cframe))
                .then(
                    [=](const JsonStore &) { insert_items_at_frame(rp, frame, uav); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
        }
    }
}

// find in / out points and split if inside clips
// build item/count value and pass to remove items
//  how do we wait for sync of state, from split children.. ?
void TrackActor::remove_items_at_frame(
    caf::typed_response_promise<std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
    const int frame,
    const int duration,
    const bool add_gap,
    const bool collapse_gaps) {

    auto in_point = base_.item().item_at_frame(frame);

    // spdlog::warn("{}",base_.item().size());

    if (not in_point)
        rp.deliver(make_error(xstudio_error::error, "Invalid frame"));
    else {
        if (in_point->second != in_point->first->trimmed_frame_start().frames()) {
            // split at in point, and recall function.
            // spdlog::warn("start split {} {}",
            // static_cast<int>(std::distance(base_.item().cbegin(), in_point->first)),
            // static_cast<int>(in_point->second));
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                split_item_atom_v,
                static_cast<int>(std::distance(base_.item().cbegin(), in_point->first)),
                static_cast<int>(in_point->second))
                .then(
                    [=](const JsonStore &) {
                        remove_items_at_frame(rp, frame, duration, add_gap, collapse_gaps);
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

        } else {
            // do we need to account for erasing off the end ?
            // split end point...
            auto out_point = base_.item().item_at_frame(frame + duration);
            if (out_point and
                out_point->second != out_point->first->trimmed_frame_start().frames()) {
                // split at in point, and recall function.
                // spdlog::warn("start end {} {}",
                // static_cast<int>(std::distance(base_.item().cbegin(), out_point->first)),
                // static_cast<int>(out_point->second));
                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    split_item_atom_v,
                    static_cast<int>(std::distance(base_.item().cbegin(), out_point->first)),
                    static_cast<int>(out_point->second))
                    .then(
                        [=](const JsonStore &) {
                            remove_items_at_frame(rp, frame, duration, add_gap, collapse_gaps);
                        },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            } else {
                // in and out split now remove items
                auto first_index = 0;
                auto last_index  = base_.item().size();
                if (in_point)
                    first_index = std::distance(base_.item().cbegin(), in_point->first);
                if (out_point)
                    last_index = std::distance(base_.item().cbegin(), out_point->first);

                // spdlog::warn("remove {} {} {}", first_index, last_index, last_index -
                // first_index);
                remove_items(rp, first_index, last_index - first_index, add_gap, collapse_gaps);
            }
        }
    }
}

std::pair<utility::JsonStore, std::vector<timeline::Item>> TrackActor::remove_items(
    const int index, const int count, const bool add_gap, const bool collapse_gaps) {
    std::vector<Item> items;
    JsonStore changes(R"([])"_json);

    if (index < 0 or index + count - 1 >= static_cast<int>(base_.item().size()))
        throw std::runtime_error("Invalid index / count");
    else {
        scoped_actor sys{system()};
        auto gap_size = FrameRateDuration();

        for (int i = index + count - 1; i >= index; i--) {
            auto it = std::next(base_.item().begin(), i);
            if (it != base_.item().end()) {
                auto item = *it;
                demonitor(item.actor());
                actors_.erase(item.uuid());

                // need to serialise actor..
                auto blind = request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);
                auto tmp   = base_.item().erase(it, blind);
                changes.insert(changes.end(), tmp.begin(), tmp.end());
                items.push_back(item.clone());

                if (add_gap) {
                    if (gap_size.frames()) {
                        gap_size += item.trimmed_frame_duration();
                    } else {
                        gap_size = item.trimmed_frame_duration();
                    }
                }
            }
        }

        // add gap and index isn't last entry.
        if (gap_size.frames() and index != static_cast<int>(base_.item().size())) {
            JsonStore gap_changes(R"([])"_json);
            auto uuid = utility::Uuid::generate();
            auto gap  = spawn<GapActor>("GAP", gap_size, uuid);
            // take ownership
            add_item(UuidActor(uuid, gap));
            // where we're going to insert gap..
            auto it    = std::next(base_.item().begin(), index);
            auto item  = request_receive<Item>(*sys, gap, item_atom_v);
            auto blind = request_receive<JsonStore>(*sys, gap, serialise_atom_v);

            auto tmp = base_.item().insert(it, item, blind);

            changes.insert(changes.end(), tmp.begin(), tmp.end());
        }

        if (collapse_gaps) {
            auto more = merge_gaps();
            if (not more.is_null())
                changes.insert(changes.end(), more.begin(), more.end());
        } else {
            auto more = base_.item().refresh();
            if (not more.is_null())
                changes.insert(changes.end(), more.begin(), more.end());
        }
    }
    std::reverse(items.begin(), items.end());
    return std::make_pair(changes, items);
}


void TrackActor::remove_items(
    caf::typed_response_promise<std::pair<utility::JsonStore, std::vector<timeline::Item>>> rp,
    const int index,
    const int count,
    const bool add_gap,
    const bool collapse_gaps) {

    try {
        auto result = remove_items(index, count, add_gap, collapse_gaps);
        send(base_.event_group(), event_atom_v, item_atom_v, result.first, false);
        rp.deliver(result);
    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void TrackActor::erase_items_at_frame(
    caf::typed_response_promise<JsonStore> rp,
    const int frame,
    const int duration,
    const bool add_gap,
    const bool collapse_gaps) {

    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        remove_item_at_frame_atom_v,
        frame,
        duration,
        add_gap,
        collapse_gaps)
        .then(
            [=](const std::pair<JsonStore, std::vector<Item>> &hist_item) mutable {
                for (const auto &i : hist_item.second)
                    send_exit(i.actor(), caf::exit_reason::user_shutdown);
                rp.deliver(hist_item.first);
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
}

void TrackActor::erase_items(
    caf::typed_response_promise<JsonStore> rp,
    const int index,
    const int count,
    const bool add_gap,
    const bool collapse_gaps) {

    try {
        auto result = remove_items(index, count, add_gap, collapse_gaps);

        // spdlog::warn("{}", result.first.dump(2));

        send(base_.event_group(), event_atom_v, item_atom_v, result.first, false);
        for (const auto &i : result.second)
            send_exit(i.actor(), caf::exit_reason::user_shutdown);
        rp.deliver(result.first);

    } catch (const std::exception &err) {
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}


void TrackActor::move_items(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int src_index,
    const int count,
    const int dst_index,
    const bool add_gap,
    const bool replace_with_gap) {

    // this has to deal with gap removal / consolidation

    if (dst_index == src_index or not count)
        rp.deliver(make_error(xstudio_error::error, "Invalid Move"));
    else {
        auto sit = std::next(base_.item().begin(), src_index);
        auto eit = std::next(sit, count);
        auto dit = std::next(base_.item().begin(), dst_index);

        // collect size of possible gap.
        auto gap_size = FrameRateDuration();
        if (add_gap or replace_with_gap) {
            for (auto it = sit; it != eit; it++) {
                if (gap_size.frames()) {
                    gap_size += it->trimmed_frame_duration();
                } else {
                    gap_size = it->trimmed_frame_duration();
                }
            }
        }

        // move done.
        auto changes = base_.item().splice(dit, base_.item().children(), sit, eit);

        // deal with gaps..
        auto index = src_index;
        if (dst_index < src_index)
            index += count;

        if (index < static_cast<int>(base_.item().size())) {
            scoped_actor sys{system()};

            if (add_gap and gap_size.frames()) {

                JsonStore gap_changes(R"([])"_json);

                auto uuid = utility::Uuid::generate();
                auto gap  = spawn<GapActor>("GAP", gap_size, uuid);
                // take ownership
                add_item(UuidActor(uuid, gap));
                // where we're going to insert gap..
                auto it    = std::next(base_.item().begin(), index);
                auto item  = request_receive<Item>(*sys, gap, item_atom_v);
                auto blind = request_receive<JsonStore>(*sys, gap, serialise_atom_v);

                auto tmp = base_.item().insert(it, item, blind);
                changes.insert(changes.end(), tmp.begin(), tmp.end());
            }
        }

        if (replace_with_gap) {
            // remove moved entries, and insert gap there..
            auto adjusted_dst = dst_index;
            if (dst_index > src_index)
                adjusted_dst -= count;

            auto more = remove_items(adjusted_dst, count, true, false);
            for (const auto &i : more.second) {
                send_exit(i.actor(), caf::exit_reason::user_shutdown);
            }
            changes.insert(changes.end(), more.first.begin(), more.first.end());
        }


        auto more = merge_gaps();
        if (not more.is_null())
            changes.insert(changes.end(), more.begin(), more.end());

        // end gap code...
        more = base_.item().refresh();
        if (not more.is_null())
            changes.insert(changes.begin(), more.begin(), more.end());

        send(base_.event_group(), event_atom_v, item_atom_v, changes, false);

        rp.deliver(changes);
    }
}

void TrackActor::move_items_at_frame(
    caf::typed_response_promise<utility::JsonStore> rp,
    const int frame_,
    const int duration_,
    const int dest_frame_,
    const bool insert_,
    const bool add_gap_,
    const bool replace_with_gap_) {

    // this is gonna be complex..
    // validate input
    auto frame            = frame_;
    auto duration         = duration_;
    auto dest_frame       = dest_frame_;
    auto insert           = insert_;
    auto add_gap          = add_gap_;
    auto replace_with_gap = replace_with_gap_;

    auto overwrite_self_start = dest_frame > frame and dest_frame < frame + duration;
    auto overwrite_self_end =
        dest_frame + duration > frame and dest_frame + duration < frame + duration;
    auto overwrite_self = overwrite_self_start or overwrite_self_end;

    if (frame_ == dest_frame_) {
        return rp.deliver(make_error(xstudio_error::error, "Invalid start frame"));
    }

    if (overwrite_self_end) {
        // invert logic..
        frame            = dest_frame_;
        duration         = frame_ - dest_frame_;
        dest_frame       = frame_ + duration_;
        insert           = true;
        add_gap          = false;
        replace_with_gap = true;

        // handle moving off beginning of track
        // spdlog::warn("overwrite_self_end");
        // spdlog::warn("{} {} {}", frame_, duration_, dest_frame_);

        if (frame < 0) {
            // spdlog::warn("{} {} {}", frame, duration, dest_frame);
            duration += -frame - 1;
            // dest_frame += frame;
            frame = 0;
        }

        // spdlog::warn("{} {} {}", frame, duration, dest_frame);
    } else if (overwrite_self_start) {
        dest_frame = frame_;
        frame      = frame_ + duration_;
        duration   = dest_frame_ - frame_;

        // spdlog::warn("overwrite_self_start");
        // spdlog::warn("{} {} {}", frame_, duration_, dest_frame_);
        // spdlog::warn("{} {} {}", frame, duration, dest_frame);
        insert           = true;
        add_gap          = false;
        replace_with_gap = true;

        // if were overwriting the end of the track we need to pad it.
        // use track trimmed duration to work out how big a gap we need,
        auto track_duration = base_.item().trimmed_frame_duration().frames();
        auto gap_duration   = (frame + duration) - track_duration;
        if (gap_duration > 0) {
            // insert gap on track end.
            auto gap_uuid  = utility::Uuid::generate();
            auto gap_actor = spawn<GapActor>(
                "Gap", FrameRateDuration(gap_duration, base_.item().rate()), gap_uuid);

            // spdlog::warn("INSERT DEST GAP {}", gap_duration);

            // insert_items(base_.item().size(), uav_plus_gap, rp);
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                insert_item_atom_v,
                Uuid(),
                UuidActorVector({UuidActor(gap_uuid, gap_actor)}))
                .then(
                    [=](const JsonStore &) mutable {
                        move_items_at_frame(
                            rp,
                            frame_,
                            duration_,
                            dest_frame_,
                            insert_,
                            add_gap_,
                            replace_with_gap_);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return;
        }
        // spdlog::warn("gap_duration {}", gap_duration);
    }

    auto start = base_.item().item_at_frame(frame);

    if (not start)
        rp.deliver(make_error(xstudio_error::error, "Invalid start frame"));
    else {
        // spdlog::warn(
        //     "check source start {} {} {}",
        //     frame,
        //     start->first->trimmed_frame_start().frames(),
        //     start->second);

        // split at start ?
        if (start and start->first->trimmed_frame_start().frames() != start->second) {
            // split start item
            // spdlog::warn(
            //     "split source index {} at frame {} (start)",
            //     static_cast<int>(std::distance(base_.item().cbegin(), start->first)),
            //     start->second);
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                split_item_atom_v,
                static_cast<int>(std::distance(base_.item().cbegin(), start->first)),
                start->second)
                .then(
                    [=](const JsonStore &) mutable {
                        move_items_at_frame(
                            rp, frame, duration, dest_frame, insert, add_gap, replace_with_gap);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

        } else {
            // split at end frame ?
            auto end = base_.item().item_at_frame(frame + duration);
            // if (end)
            //     spdlog::warn(
            //         "check source end {} {} {}",
            //         frame + duration,
            //         end->first->trimmed_frame_start().frames(),
            //         end->second);
            // else
            //     spdlog::warn("check source end {}", frame + duration);

            if (end and end->first->trimmed_frame_start().frames() != end->second) {
                // split end item
                // spdlog::warn(
                //     "split source index {} at frame {} (end)",
                //     static_cast<int>(std::distance(base_.item().cbegin(), end->first)),
                //     end->second);

                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    split_item_atom_v,
                    static_cast<int>(std::distance(base_.item().cbegin(), end->first)),
                    end->second)
                    .then(
                        [=](const JsonStore &) mutable {
                            move_items_at_frame(
                                rp,
                                frame,
                                duration,
                                dest_frame,
                                insert,
                                add_gap,
                                replace_with_gap);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });
            } else {
                // move to frame should insert gap, but we might need to split..
                // either split or inject end..
                // dest might be off end of track which is still valid..
                auto dest = base_.item().item_at_frame(dest_frame);

                // if (dest)
                //     spdlog::warn(
                //         "check dest start {} {} {}",
                //         dest_frame,
                //         dest->first->trimmed_frame_start().frames(),
                //         dest->second);

                if (dest and dest->first->trimmed_frame_start().frames() != dest->second) {
                    // spdlog::warn(
                    //     "split dest index {} at frame {} (start)",
                    //     static_cast<int>(std::distance(base_.item().cbegin(), dest->first)),
                    //     dest->second);

                    // split dest start
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        split_item_atom_v,
                        static_cast<int>(std::distance(base_.item().cbegin(), dest->first)),
                        dest->second)
                        .then(
                            [=](const JsonStore &) mutable {
                                move_items_at_frame(
                                    rp,
                                    frame,
                                    duration,
                                    dest_frame,
                                    insert,
                                    add_gap,
                                    replace_with_gap);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } else if (
                    not dest and base_.item().trimmed_frame_duration().frames() != dest_frame) {
                    // check for off end as we'll need gap..
                    auto track_end = base_.item().trimmed_frame_start().frames() +
                                     base_.item().trimmed_frame_duration().frames() - 1;
                    auto filler    = dest_frame - track_end - 1;
                    auto gap_uuid  = utility::Uuid::generate();
                    auto gap_actor = spawn<GapActor>(
                        "Gap", FrameRateDuration(filler, base_.item().rate()), gap_uuid);

                    // spdlog::warn("INSERT GAP {}", filler);

                    // insert_items(base_.item().size(), uav_plus_gap, rp);
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        insert_item_atom_v,
                        Uuid(),
                        UuidActorVector({UuidActor(gap_uuid, gap_actor)}))
                        .then(
                            [=](const JsonStore &) mutable {
                                move_items_at_frame(
                                    rp,
                                    frame,
                                    duration,
                                    dest_frame,
                                    true,
                                    add_gap,
                                    replace_with_gap);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } else {
                    // finally ready..
                    if (insert or
                        (not dest and
                         base_.item().trimmed_frame_duration().frames() == dest_frame)) {

                        auto index     = std::distance(base_.item().cbegin(), start->first);
                        auto end_index = end ? std::distance(base_.item().cbegin(), end->first)
                                             : base_.item().size();
                        auto count =
                            end_index - std::distance(base_.item().cbegin(), start->first);
                        auto dst = dest ? std::distance(base_.item().cbegin(), dest->first)
                                        : base_.item().size();

                        // spdlog::warn("move_items index {} count {} dst {}", index, count,
                        // dst);

                        move_items(rp, index, count, dst, add_gap, replace_with_gap);
                    } else {
                        // we need to remove material at destnation
                        // we may need to split at dst+duration
                        auto dst_end = base_.item().item_at_frame(dest_frame + duration);

                        // if(dst_end)
                        //     spdlog::warn(
                        //         "check dest end {} {} {}",
                        //         dest_frame + duration,
                        //         dst_end->first->trimmed_frame_start().frames(),
                        //         dst_end->second);

                        if (dst_end and
                            dst_end->first->trimmed_frame_start().frames() != dst_end->second) {
                            // we need to split..
                            // split dest..
                            // spdlog::warn(
                            //     "split dest index {} at frame {} (end)",
                            //     static_cast<int>(
                            //         std::distance(base_.item().cbegin(), dst_end->first)),
                            //     dst_end->second);

                            request(
                                caf::actor_cast<caf::actor>(this),
                                infinite,
                                split_item_atom_v,
                                static_cast<int>(
                                    std::distance(base_.item().cbegin(), dst_end->first)),
                                dst_end->second)
                                .then(
                                    [=](const JsonStore &) mutable {
                                        move_items_at_frame(
                                            rp,
                                            frame,
                                            duration,
                                            dest_frame,
                                            insert,
                                            add_gap,
                                            replace_with_gap);
                                    },
                                    [=](error &err) mutable { rp.deliver(std::move(err)); });
                        } else {
                            // move and prune..
                            int move_from_frame = frame;
                            int move_to_frame   = dest_frame;

                            // if we remove from in front of start we need to adjust move
                            // ranges.
                            if (dest_frame < frame) {
                                move_from_frame -= duration;
                            }
                            // spdlog::warn("ERASE {} {}", dest_frame, duration);

                            // replace source with gap ?
                            request(
                                caf::actor_cast<caf::actor>(this),
                                infinite,
                                erase_item_at_frame_atom_v,
                                dest_frame,
                                duration,
                                false,
                                false)
                                .then(
                                    [=](const JsonStore &) mutable {
                                        move_items_at_frame(
                                            rp,
                                            move_from_frame,
                                            duration,
                                            move_to_frame,
                                            true,
                                            add_gap,
                                            replace_with_gap);
                                    },
                                    [=](error &err) mutable { rp.deliver(std::move(err)); });
                        }
                    }
                }
            }
        }
    }
}

utility::JsonStore TrackActor::merge_gaps() {
    JsonStore changes(R"([])"_json);
    scoped_actor sys{system()};

    // purge trailing gaps also handles track of gap
    while (not base_.item().empty() and base_.item().back().item_type() == IT_GAP) {
        // prune gaps off end of track.
        // spdlog::warn("ERASE TRAILING GAP {}", base_.item().size() - 1);

        auto it   = std::next(base_.item().begin(), base_.item().size() - 1);
        auto item = *it;
        demonitor(item.actor());
        actors_.erase(item.uuid());

        // need to serialise actor..
        auto blind = request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);
        auto tmp   = base_.item().erase(it, blind);
        send_exit(item.actor(), caf::exit_reason::user_shutdown);
        changes.insert(changes.end(), tmp.begin(), tmp.end());
    }

    auto gap_count    = 0;
    auto gap_duration = FrameRateDuration();
    // look for multiple sequential gaps.
    int index = static_cast<int>(base_.item().size()) - 1;

    auto merge_gaps_ = [&](auto it) mutable {
        if (gap_count) {
            if (gap_count > 1) {
                // spdlog::warn("merging {}", gap_count);
                scoped_actor sys{system()};

                auto uuid = utility::Uuid::generate();
                auto gap  = spawn<GapActor>("GAP", gap_duration, uuid);

                // take ownership
                add_item(UuidActor(uuid, gap));

                // where we're going to insert gap..
                auto item  = request_receive<Item>(*sys, gap, item_atom_v);
                auto blind = request_receive<JsonStore>(*sys, gap, serialise_atom_v);

                auto tmp = base_.item().insert(it, item, blind);
                changes.insert(changes.end(), tmp.begin(), tmp.end());

                while (gap_count) {
                    // get distance from start.
                    auto gip = std::next(it, gap_count - 1);

                    // spdlog::warn("remove index {}", std::distance(base_.item().begin(),
                    // gip));

                    auto item = *gip;
                    demonitor(item.actor());
                    actors_.erase(item.uuid());

                    // need to serialise actor..
                    auto blind =
                        request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);
                    auto tmp = base_.item().erase(gip, blind);
                    send_exit(item.actor(), caf::exit_reason::user_shutdown);
                    changes.insert(changes.end(), tmp.begin(), tmp.end());

                    gap_count--;
                }
            } else
                gap_count = 0;
        }
    };

    for (; index >= 0; index--) {
        auto it = std::next(base_.item().begin(), index);

        if (it->item_type() == IT_GAP) {
            if (not gap_count)
                gap_duration = it->trimmed_frame_duration();
            else
                gap_duration += it->trimmed_frame_duration();

            gap_count++;
        } else {
            merge_gaps_(std::next(it, 1));
        }
    }

    merge_gaps_(base_.item().begin());

    auto more = base_.item().refresh();
    if (not more.is_null())
        changes.insert(changes.end(), more.begin(), more.end());

    return changes;
}

// iterate over track removing surplus gaps
void TrackActor::merge_gaps(caf::typed_response_promise<utility::JsonStore> rp) {
    auto changes = merge_gaps();

    if (changes.size())
        send(base_.event_group(), event_atom_v, item_atom_v, changes, false);

    rp.deliver(changes);
}