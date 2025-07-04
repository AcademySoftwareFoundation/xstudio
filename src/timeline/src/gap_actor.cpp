// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

GapActor::GapActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);

    init();
}

GapActor::GapActor(caf::actor_config &cfg, const JsonStore &jsn, Item &pitem)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn.at("base"))) {
    base_.item().set_actor_addr(this);

    pitem = base_.item().clone();
    init();
}

GapActor::GapActor(
    caf::actor_config &cfg,
    const std::string &name,
    const FrameRateDuration &duration,
    const Uuid &uuid)
    : caf::event_based_actor(cfg), base_(name, duration, uuid, this) {

    base_.item().set_name(name);
    init();
}

GapActor::GapActor(caf::actor_config &cfg, const Item &item)
    : caf::event_based_actor(cfg), base_(item, this) {
    init();
}

GapActor::GapActor(caf::actor_config &cfg, const Item &item, Item &nitem)
    : GapActor(cfg, item) {
    nitem = base_.item().clone();
}

caf::message_handler GapActor::message_handler() {
    return caf::message_handler{
        [=](item_lock_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_locked(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_name_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_name(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_flag_atom, const std::string &value) -> JsonStore {
            auto jsn = base_.item().set_flag(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_type_atom) -> ItemType { return base_.item().item_type(); },

        [=](plugin_manager::enable_atom, const bool value) -> JsonStore {
            auto jsn = base_.item().set_enabled(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](active_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_active_range(fr);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](available_range_atom, const FrameRange &fr) -> JsonStore {
            auto jsn = base_.item().set_available_range(fr);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const bool merge) -> JsonStore {
            auto p = base_.item().prop();
            p.update(value);
            auto jsn = base_.item().set_prop(p);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value) -> JsonStore {
            auto jsn = base_.item().set_prop(value);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](item_prop_atom, const JsonStore &value, const std::string &path) -> JsonStore {
            auto prop = base_.item().prop();
            try {
                auto ptr = nlohmann::json::json_pointer(path);
                prop.at(ptr).update(value);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
            auto jsn = base_.item().set_prop(prop);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },
        [=](rate_atom) -> FrameRate { return base_.item().rate(); },

        [=](rate_atom atom, const media::MediaType media_type) {
            return mail(atom).delegate(caf::actor_cast<caf::actor>(this));
        },

        [=](rate_atom,
            const utility::FrameRate &new_rate,
            const bool force_media_rate_to_match) -> bool { return true; },

        [=](item_prop_atom) -> JsonStore { return base_.item().prop(); },

        [=](active_range_atom) -> std::optional<FrameRange> {
            return base_.item().active_range();
        },

        [=](available_range_atom) -> std::optional<FrameRange> {
            return base_.item().available_range();
        },

        [=](trimmed_range_atom) -> FrameRange { return base_.item().trimmed_range(); },

        [=](trimmed_range_atom,
            const FrameRange &avail,
            const FrameRange &active) -> JsonStore {
            auto jsn = base_.item().set_range(avail, active);
            if (not jsn.is_null())
                mail(event_atom_v, item_atom_v, jsn, false).send(base_.event_group());
            return jsn;
        },

        [=](trimmed_range_atom,
            const FrameRange &avail,
            const FrameRange &active,
            const bool silent) -> JsonStore { return base_.item().set_range(avail, active); },

        [=](link_media_atom, const UuidActorMap &, const bool) -> bool { return true; },

        [=](item_atom) -> Item { return base_.item().clone(); },

        [=](item_atom, const bool with_state) -> result<std::pair<JsonStore, Item>> {
            auto rp = make_response_promise<std::pair<JsonStore, Item>>();
            mail(serialise_atom_v)
                .request(caf::actor_cast<caf::actor>(this), infinite)
                .then(
                    [=](const JsonStore &jsn) mutable {
                        rp.deliver(std::make_pair(jsn, base_.item().clone()));
                    },
                    [=](const caf::error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().undo(hist);
            return true;
        },

        [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
            base_.item().redo(hist);
            return true;
        },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](duplicate_atom) -> UuidActor {
            JsonStore jsn;
            auto dup    = base_.duplicate();
            jsn["base"] = dup.serialise();

            auto actor = spawn<GapActor>(jsn);
            return UuidActor(dup.uuid(), actor);
        },

        [=](playhead::source_atom,
            const UuidUuidMap &swap,
            const UuidActorMap &media) -> result<bool> { return true; },

        [=](serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        }};
}


void GapActor::init() {
    print_on_create(this, base_.name());
    print_on_exit(this, base_.name());
}
