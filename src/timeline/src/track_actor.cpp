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

    if (type == "Clip") {
        auto item = Item();
        actor     = spawn<ClipActor>(static_cast<utility::JsonStore>(value), item);
        add_item(UuidActor(key, actor));
        if (replace_item) {
            auto itemit = find_uuid(base_.item().children(), key);
            (*itemit)   = item;
        }
    } else if (type == "Gap") {
        auto item = Item();
        actor     = spawn<GapActor>(static_cast<utility::JsonStore>(value), item);
        add_item(UuidActor(key, actor));
        if (replace_item) {
            auto itemit = find_uuid(base_.item().children(), key);
            (*itemit)   = item;
        }
    } else if (type == "Stack") {
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
void TrackActor::item_event_callback(const utility::JsonStore &event, Item &item) {

    switch (static_cast<ItemAction>(event.at("action"))) {
    case IT_INSERT: {
        // spdlog::warn("TrackActor IT_INSERT {}", event.dump(2));
        auto cuuid = utility::Uuid(event.at("item").at("uuid"));
        // spdlog::warn("{} {} {} {}", find_uuid(base_.item().children(), cuuid) !=
        // base_.item().cend(), actors_.count(cuuid), not event["blind"].is_null(),
        // event.dump(2)); needs to be child..

        // is it a direct child..
        auto child_item_it = find_uuid(base_.item().children(), cuuid);
        // spdlog::warn("{} {} {}", child_item_it != base_.item().cend(), not
        // actors_.count(cuuid), not event.at("blind").is_null());

        if (child_item_it != base_.item().cend() and not actors_.count(cuuid) and
            not event.at("blind").is_null()) {
            // our child
            // spdlog::warn("RECREATE MATCH");

            auto actor = deserialise(utility::JsonStore(event.at("blind")), false);
            add_item(UuidActor(cuuid, actor));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(actor)));
            // spdlog::warn("{}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));
            child_item_it->set_actor_addr(actor);
            // change item actor addr
            // spdlog::warn("TrackActor create
            // {}",to_string(caf::actor_cast<caf::actor_addr>(child_item_it->actor())));

            // item actor_addr will be wrong.. in ancestors
            // send special update..
            send(
                event_group_,
                event_atom_v,
                item_atom_v,
                child_item_it->make_actor_addr_update(),
                true);
        }
    } break;

    case IT_REMOVE: {
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
    base_.item().bind_item_event_func([this](const utility::JsonStore &event, Item &item) {
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
    base_.item().bind_item_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });
    pitem = base_.item();

    init();
}

TrackActor::TrackActor(
    caf::actor_config &cfg,
    const std::string &name,
    const media::MediaType media_type,
    const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, media_type, uuid, this) {
    base_.item().set_system(&system());
    base_.item().set_name(name);
    base_.item().bind_item_event_func([this](const utility::JsonStore &event, Item &item) {
        item_event_callback(event, item);
    });

    init();
}

void TrackActor::on_exit() {
    for (const auto &i : actors_)
        send_exit(i.second, caf::exit_reason::user_shutdown);
}

void TrackActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    // update_edit_list_ = true;

    set_down_handler([=](down_msg &msg) {
        // if a child dies we won't have enough information to recreate it.
        // we still need to report it up the chain though.
        for (auto it = std::begin(actors_); it != std::end(actors_); ++it) {
            if (msg.source == it->second) {

                // spdlog::warn("detected death {}", to_string(it->second));
                demonitor(it->second);
                actors_.erase(it);

                // remove from base.
                auto it = find_actor_addr(base_.item().children(), msg.source);

                if (it != base_.item().end()) {
                    auto jsn  = base_.item().erase(it);
                    auto more = base_.item().refresh();
                    if (not more.is_null())
                        jsn.insert(jsn.begin(), more.begin(), more.end());

                    send(event_group_, event_atom_v, item_atom_v, jsn, false);
                }

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

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},

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

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_atom) -> Item { return base_.item(); },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            request(caf::actor_cast<caf::actor>(this), infinite, utility::serialise_atom_v)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item()));
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
            return *it;
        },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                send(event_group_, event_atom_v, item_atom_v, jsn, false);
            return jsn;
        },

        [=](active_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<utility::FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> utility::FrameRange { return base_.item().trimmed_range(); },

        // handle child change events.
        [=](event_atom, item_atom, const JsonStore &update, const bool hidden) {
            if (base_.item().update(update)) {
                auto more = base_.item().refresh();
                if (not more.is_null()) {
                    more.insert(more.begin(), update.begin(), update.end());
                    send(event_group_, event_atom_v, item_atom_v, more, hidden);
                    return;
                }
            }

            send(event_group_, event_atom_v, item_atom_v, update, hidden);
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
            insert_items_at_frame(frame, uav, rp);
            return rp;
        },

        [=](insert_item_atom,
            const int index,
            const UuidActorVector &uav) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            insert_items(index, uav, rp);
            return rp;
        },

        [=](insert_item_atom, const int index, const int frame, const UuidActorVector &uav)
            -> result<JsonStore> {
            auto rp     = make_response_promise<JsonStore>();
            auto tframe = base_.item().frame_at_index(index, frame);
            insert_items_at_frame(tframe, uav, rp);
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
                insert_items(index, uav, rp);

            return rp;
        },


        [=](remove_item_at_frame_atom,
            const int frame,
            const int duration) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items_at_frame(frame, duration, rp);
            return rp;
        },

        [=](remove_item_atom,
            const int index) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(index, 1, rp);
            return rp;
        },

        [=](remove_item_atom,
            const int index,
            const int count) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();
            remove_items(index, count, rp);
            return rp;
        },

        [=](remove_item_atom,
            const utility::Uuid &uuid) -> result<std::pair<JsonStore, std::vector<Item>>> {
            auto rp = make_response_promise<std::pair<JsonStore, std::vector<Item>>>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                remove_items(std::distance(base_.item().begin(), it), 1, rp);

            return rp;
        },

        [=](erase_item_at_frame_atom,
            const int frame,
            const int duration) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items_at_frame(frame, duration, rp);
            return rp;
        },

        [=](erase_item_atom, const int index) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(index, 1, rp);
            return rp;
        },

        [=](erase_item_atom, const int index, const int count) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            erase_items(index, count, rp);
            return rp;
        },

        [=](erase_item_atom, const utility::Uuid &uuid) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();

            auto it = find_uuid(base_.item().children(), uuid);

            if (it == base_.item().end())
                rp.deliver(make_error(xstudio_error::error, "Invalid uuid"));

            if (rp.pending())
                erase_items(std::distance(base_.item().begin(), it), 1, rp);

            return rp;
        },

        [=](split_item_at_frame_atom, const int frame) -> result<JsonStore> {
            auto rp          = make_response_promise<JsonStore>();
            auto split_point = base_.item().item_at_frame(frame);
            if (not split_point)
                rp.deliver(make_error(xstudio_error::error, "Invalid split frame"));
            else
                split_item(split_point->first, split_point->second, rp);

            return rp;
        },

        [=](split_item_atom, const int index, const int frame) -> result<JsonStore> {
            auto it = base_.item().children().begin();
            std::advance(it, index);
            if (it == base_.item().children().end())
                return make_error(xstudio_error::error, "Invalid index");
            auto rp = make_response_promise<JsonStore>();
            split_item(it, frame, rp);
            return rp;
        },

        [=](split_item_atom, const utility::Uuid &uuid, const int frame) -> result<JsonStore> {
            auto it = find_uuid(base_.item().children(), uuid);
            if (it == base_.item().end())
                return make_error(xstudio_error::error, "Invalid uuid");
            auto rp = make_response_promise<JsonStore>();
            split_item(it, frame, rp);
            return rp;
        },

        [=](move_item_at_frame_atom,
            const int frame,
            const int duration,
            const int dest_frame,
            const bool insert) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items_at_frame(frame, duration, dest_frame, insert, rp);
            return rp;
        },

        [=](move_item_atom, const int src_index, const int count, const int dst_index)
            -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            move_items(src_index, count, dst_index, rp);
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
                        std::distance(base_.item().begin(), sitb),
                        count,
                        std::distance(base_.item().begin(), dit),
                        rp);
            }

            return rp;
        },

        [=](utility::event_atom, utility::change_atom) {
            // update_edit_list_ = true;
            send(event_group_, utility::event_atom_v, utility::change_atom_v);
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
        }


        // code for playhead// get edit_list for all tracks/stacks..// this is temporary, it'll
        // need heavy changes..// also this only returns edit_lists for images, audio may be
        // different..
        // [=](media::get_edit_list_atom, const Uuid &uuid) -> result<utility::EditList> {
        //     if (update_edit_list_) {
        //         std::vector<caf::actor> actors;
        //         for (const auto &i : base_.clips())
        //             actors.push_back(actors_[i]);

        //         if (not actors.empty()) {
        //             auto rp = make_response_promise<utility::EditList>();

        //             fan_out_request<policy::select_all>(
        //                 actors, infinite, media::get_edit_list_atom_v, Uuid())
        //                 .await(
        //                     [=](std::vector<utility::EditList> sections) mutable {
        //                         edit_list_.clear();
        //                         for (const auto &i : base_.clips()) {
        //                             for (const auto &ii : sections) {
        //                                 if (not ii.section_list().empty()) {
        //                                     const auto &[ud, rt, tc] = ii.section_list()[0];
        //                                     if (ud == i) {
        //                                         if (uuid.is_null())
        //                                             edit_list_.push_back(ii.section_list()[0]);
        //                                         else
        //                                             edit_list_.push_back({uuid, rt, tc});
        //                                         break;
        //                                     }
        //                                 }
        //                             }
        //                         }
        //                         update_edit_list_ = false;
        //                         auto edit_list    = edit_list_;
        //                         if (not uuid.is_null())
        //                             edit_list.set_uuid(uuid);
        //                         else
        //                             edit_list.set_uuid(base_.uuid());

        //                         rp.deliver(edit_list);
        //                     },
        //                     [=](error &err) mutable { rp.deliver(std::move(err)); });

        //             return rp;
        //         } else {
        //             edit_list_.clear();
        //             update_edit_list_ = false;
        //         }
        //     }

        //     auto edit_list = edit_list_;
        //     if (not uuid.is_null())
        //         edit_list.set_uuid(uuid);
        //     else
        //         edit_list.set_uuid(base_.uuid());

        //     return result<utility::EditList>(edit_list);
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


    );
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
    const Items::const_iterator &itemit,
    const int frame,
    caf::typed_response_promise<utility::JsonStore> rp) {
    // validate frame is inside item..
    // validate item type..
    auto item = *itemit;
    if (item.item_type() == IT_GAP or item.item_type() == IT_CLIP) {
        auto trimmed_range = item.trimmed_range();
        auto orig_start    = trimmed_range.frame_start().frames();
        auto orig_duration = trimmed_range.frame_duration().frames();
        auto orig_end      = orig_start + orig_duration - 1;

        if (frame > orig_start and frame < orig_end) {
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
                                                rp.delegate(
                                                    caf::actor_cast<caf::actor>(this),
                                                    insert_item_atom_v,
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
            rp.deliver(make_error(xstudio_error::error, "Invalid frame to split on"));
        }
    } else {
        rp.deliver(make_error(xstudio_error::error, "Invalid type to split"));
    }
}

void TrackActor::insert_items(
    const int index,
    const UuidActorVector &uav,
    caf::typed_response_promise<utility::JsonStore> rp) {
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

                send(event_group_, event_atom_v, item_atom_v, changes, false);

                rp.deliver(changes);
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void TrackActor::insert_items_at_frame(
    const int frame,
    const utility::UuidActorVector &uav,
    caf::typed_response_promise<utility::JsonStore> rp) {
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

        insert_items(base_.item().size(), uav_plus_gap, rp);
    } else {
        auto cit    = item_frame->first;
        auto cframe = item_frame->second;
        auto index  = std::distance(base_.item().cbegin(), cit);

        if (cframe == cit->trimmed_frame_start().frames()) {
            // simple insertion..
            insert_items(index, uav, rp);
        } else {
            // complex.. we need to split item
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                split_item_atom_v,
                static_cast<int>(index),
                static_cast<int>(cframe))
                .then(
                    [=](const JsonStore &) { insert_items_at_frame(frame, uav, rp); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
        }
    }
}

// find in / out points and split if inside clips
// build item/count value and pass to remove items
//  how do we wait for sync of state, from split children.. ?
void TrackActor::remove_items_at_frame(
    const int frame,
    const int duration,
    caf::typed_response_promise<std::pair<utility::JsonStore, std::vector<timeline::Item>>>
        rp) {

    auto in_point = base_.item().item_at_frame(frame);

    if (not in_point)
        rp.deliver(make_error(xstudio_error::error, "Invalid frame"));
    else {
        if (in_point->second != in_point->first->trimmed_frame_start().frames()) {
            // split at in point, and recall function.
            request(
                caf::actor_cast<caf::actor>(this),
                infinite,
                split_item_atom_v,
                static_cast<int>(std::distance(base_.item().cbegin(), in_point->first)),
                static_cast<int>(in_point->second))
                .then(
                    [=](const JsonStore &) { remove_items_at_frame(frame, duration, rp); },
                    [=](const caf::error &err) mutable { rp.deliver(err); });

        } else {
            // split end point...
            auto out_point = base_.item().item_at_frame(frame + duration);
            if (out_point->second != out_point->first->trimmed_frame_start().frames()) {
                // split at in point, and recall function.
                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    split_item_atom_v,
                    static_cast<int>(std::distance(base_.item().cbegin(), out_point->first)),
                    static_cast<int>(out_point->second))
                    .then(
                        [=](const JsonStore &) { remove_items_at_frame(frame, duration, rp); },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            } else {
                // in and out split now remove items
                auto first_index = std::distance(base_.item().cbegin(), in_point->first);
                auto last_index  = std::distance(base_.item().cbegin(), out_point->first);
                remove_items(first_index, last_index - first_index, rp);
            }
        }
    }
}

void TrackActor::remove_items(
    const int index,
    const int count,
    caf::typed_response_promise<std::pair<utility::JsonStore, std::vector<timeline::Item>>>
        rp) {

    std::vector<Item> items;
    JsonStore changes(R"([])"_json);

    if (index < 0 or index + count - 1 >= static_cast<int>(base_.item().size()))
        rp.deliver(make_error(xstudio_error::error, "Invalid index / count"));
    else {
        scoped_actor sys{system()};
        for (int i = index + count - 1; i >= index; i--) {
            auto it = std::next(base_.item().begin(), i);
            if (it != base_.item().end()) {
                auto item = *it;
                demonitor(item.actor());
                actors_.erase(item.uuid());

                // need to serialise actor..
                auto blind = request_receive<JsonStore>(*sys, item.actor(), serialise_atom_v);
                auto tmp   = base_.item().erase(it, blind);

                changes.insert(changes.begin(), tmp.begin(), tmp.end());
                items.push_back(item);
            }
        }

        // reverse order as we deleted back to front.
        std::reverse(items.begin(), items.end());

        auto more = base_.item().refresh();
        if (not more.is_null())
            changes.insert(changes.begin(), more.begin(), more.end());

        send(event_group_, event_atom_v, item_atom_v, changes, false);

        rp.deliver(std::make_pair(changes, items));
    }
}

void TrackActor::erase_items_at_frame(
    const int frame, const int duration, caf::typed_response_promise<JsonStore> rp) {

    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        remove_item_at_frame_atom_v,
        frame,
        duration)
        .then(
            [=](const std::pair<JsonStore, std::vector<Item>> &hist_item) mutable {
                for (const auto &i : hist_item.second)
                    send_exit(i.actor(), caf::exit_reason::user_shutdown);
                rp.deliver(hist_item.first);
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
}

void TrackActor::erase_items(
    const int index, const int count, caf::typed_response_promise<JsonStore> rp) {

    request(caf::actor_cast<caf::actor>(this), infinite, remove_item_atom_v, index, count)
        .then(
            [=](const std::pair<JsonStore, std::vector<Item>> &hist_item) mutable {
                for (const auto &i : hist_item.second)
                    send_exit(i.actor(), caf::exit_reason::user_shutdown);
                rp.deliver(hist_item.first);
            },
            [=](error &err) mutable { rp.deliver(std::move(err)); });
}


void TrackActor::move_items(
    const int src_index,
    const int count,
    const int dst_index,
    caf::typed_response_promise<utility::JsonStore> rp) {


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

        send(event_group_, event_atom_v, item_atom_v, changes, false);
        rp.deliver(changes);
    }
}

void TrackActor::move_items_at_frame(
    const int frame,
    const int duration,
    const int dest_frame,
    const bool insert,
    caf::typed_response_promise<utility::JsonStore> rp) {

    // don't support moving in to move range..
    if (dest_frame >= frame and dest_frame <= frame + duration)
        rp.deliver(make_error(xstudio_error::error, "Invalid move"));
    else {
        // this is gonna be complex..
        // validate input
        auto start = base_.item().item_at_frame(frame);

        if (not start)
            rp.deliver(make_error(xstudio_error::error, "Invalid start frame"));
        else {
            // split at start ?
            if (start->first->trimmed_frame_start().frames() != start->second) {
                // split start item
                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    split_item_atom_v,
                    static_cast<int>(std::distance(base_.item().cbegin(), start->first)),
                    start->second)
                    .then(
                        [=](const JsonStore &) mutable {
                            move_items_at_frame(frame, duration, dest_frame, insert, rp);
                        },
                        [=](error &err) mutable { rp.deliver(std::move(err)); });

            } else {
                // split at end frame ?
                auto end = base_.item().item_at_frame(frame + duration);

                if (end->first->trimmed_frame_start().frames() != end->second) {
                    // split end item
                    request(
                        caf::actor_cast<caf::actor>(this),
                        infinite,
                        split_item_atom_v,
                        static_cast<int>(std::distance(base_.item().cbegin(), end->first)),
                        end->second)
                        .then(
                            [=](const JsonStore &) mutable {
                                move_items_at_frame(frame, duration, dest_frame, insert, rp);
                            },
                            [=](error &err) mutable { rp.deliver(std::move(err)); });
                } else {
                    // move to frame should insert gap, but we might need to split..
                    // either split or inject end..
                    // dest might be off end of track which is still valid..
                    auto dest = base_.item().item_at_frame(dest_frame);

                    if (dest and dest->first->trimmed_frame_start().frames() != dest->second) {

                        // split dest..
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            split_item_atom_v,
                            static_cast<int>(std::distance(base_.item().cbegin(), dest->first)),
                            dest->second)
                            .then(
                                [=](const JsonStore &) mutable {
                                    move_items_at_frame(
                                        frame, duration, dest_frame, insert, rp);
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    } else if (
                        not dest and
                        base_.item().trimmed_frame_duration().frames() != dest_frame) {

                        // check for off end as we'll need gap..
                        auto track_end = base_.item().trimmed_frame_start().frames() +
                                         base_.item().trimmed_frame_duration().frames() - 1;
                        auto filler    = dest_frame - track_end - 1;
                        auto gap_uuid  = utility::Uuid::generate();
                        auto gap_actor = spawn<GapActor>(
                            "Gap", FrameRateDuration(filler, base_.item().rate()), gap_uuid);

                        // insert_items(base_.item().size(), uav_plus_gap, rp);
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            insert_item_atom_v,
                            static_cast<int>(base_.item().size()),
                            UuidActorVector({UuidActor(gap_uuid, gap_actor)}))
                            .then(
                                [=](const JsonStore &) mutable {
                                    move_items_at_frame(
                                        frame, duration, dest_frame, insert, rp);
                                },
                                [=](error &err) mutable { rp.deliver(std::move(err)); });
                    } else {
                        // finally ready..
                        if (insert or
                            (not dest and
                             base_.item().trimmed_frame_duration().frames() == dest_frame)) {
                            auto index = std::distance(base_.item().cbegin(), start->first);
                            auto count = std::distance(base_.item().cbegin(), end->first) -
                                         std::distance(base_.item().cbegin(), start->first);
                            auto dst = dest ? std::distance(base_.item().cbegin(), dest->first)
                                            : base_.item().size();

                            move_items(index, count, dst, rp);
                        } else {
                            // we need to remove material at destnation
                            // we may need to split at dst+duration
                            auto dst_end = base_.item().item_at_frame(dest_frame + duration);
                            if (dst_end) {
                                // we need to split..
                                // split dest..
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
                                                frame, duration, dest_frame, insert, rp);
                                        },
                                        [=](error &err) mutable {
                                            rp.deliver(std::move(err));
                                        });
                            } else {
                                // move and prune..
                                int move_from_frame = frame;
                                int move_to_frame   = dest_frame + duration;

                                // if we remove from in front of start we need to adjust move
                                // ranges.
                                if (dest_frame < frame) {
                                    move_from_frame -= duration;
                                    move_to_frame -= duration;
                                }

                                request(
                                    caf::actor_cast<caf::actor>(this),
                                    infinite,
                                    erase_item_at_frame_atom_v,
                                    static_cast<int>(dest_frame + duration),
                                    duration)
                                    .then(
                                        [=](const JsonStore &) mutable {
                                            move_items_at_frame(
                                                move_from_frame,
                                                duration,
                                                move_to_frame,
                                                false,
                                                rp);
                                        },
                                        [=](error &err) mutable {
                                            rp.deliver(std::move(err));
                                        });
                            }
                        }
                    }
                }
            }
        }
    }
}


// void TrackActor::deliver_media_pointer(
//     const int logical_frame, caf::typed_response_promise<media::AVFrameID> rp) {
//     // should be able to use edit_list ?
//     try {
//         int clip_frame = 0;
//         utility::EditListSection clip_at_frame =
//             edit_list_.media_frame(logical_frame, clip_frame);
//         // send request media atom..
//         request(
//             actors_[clip_at_frame.media_uuid_],
//             infinite,
//             media::get_media_pointer_atom_v,
//             clip_frame)
//             .then(
//                 [=](const media::AVFrameID &mp) mutable { rp.deliver(mp); },
//                 [=](error &err) mutable { rp.deliver(std::move(err)); });

//     } catch (const std::exception &e) {
//         rp.deliver(make_error(xstudio_error::error, e.what()));
//     }
// }
