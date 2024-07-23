// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <chrono>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media;
using namespace xstudio::media_metadata;
using namespace caf;

MediaStreamActor::MediaStreamActor(caf::actor_config &cfg, const JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<JsonStore>(jsn["base"])) {
    if (not jsn.count("store") or jsn["store"].is_null()) {
        json_store_ = spawn<json_store::JsonStoreActor>(utility::Uuid::generate());
    } else {
        json_store_ = spawn<json_store::JsonStoreActor>(
            utility::Uuid::generate(), static_cast<JsonStore>(jsn["store"]));
    }
    link_to(json_store_);

    init();
}

MediaStreamActor::MediaStreamActor(
    caf::actor_config &cfg, const StreamDetail &detail, const utility::Uuid &uuid)
    : caf::event_based_actor(cfg), base_(detail) {
    if (not uuid.is_null())
        base_.set_uuid(uuid);

    json_store_ = spawn<json_store::JsonStoreActor>(utility::Uuid::generate());
    link_to(json_store_);

    init();
}

void MediaStreamActor::init() {
    // only parial..
    // spdlog::debug(
    //     "Created MediaStreamActor {} {} {}",
    //     base_.name(),
    //     to_readable_string(base_.media_type()),
    //     to_string(base_.duration()));
    print_on_create(this, base_);
    print_on_exit(this, base_);

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

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

        [=](get_media_type_atom) -> MediaType { return base_.media_type(); },

        [=](get_stream_detail_atom) -> StreamDetail { return base_.detail(); },

        [=](const StreamDetail &detail) { base_.set_detail(detail); },

        [=](json_store::get_json_atom _get_atom, const std::string &path) {
            return delegate(json_store_, _get_atom, path);
        },

        [=](utility::duplicate_atom) -> UuidActor {
            // clone ourself..
            const auto uuid  = utility::Uuid::generate();
            const auto actor = spawn<MediaStreamActor>(base_.detail(), uuid);
            return UuidActor(uuid, actor);
        },

        [=](utility::get_group_atom _get_group_atom) {
            return delegate(json_store_, _get_group_atom);
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            request(json_store_, infinite, json_store::get_json_atom_v, "")
                .then([=](const JsonStore &meta) mutable {
                    JsonStore jsn;
                    jsn["store"] = meta;
                    jsn["base"]  = base_.serialise();

                    rp.deliver(jsn);
                });

            return rp;
        });
}
