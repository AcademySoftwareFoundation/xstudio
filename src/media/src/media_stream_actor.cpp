// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <chrono>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/json_store/json_store_handler.hpp"
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
    jsn_handler_ = JsonStoreHandler(
        dynamic_cast<caf::event_based_actor *>(this),
        base_.event_group(),
        utility::Uuid::generate(),
        not jsn.count("store") or jsn["store"].is_null()
            ? JsonStore()
            : static_cast<JsonStore>(jsn["store"]));

    init();
}

MediaStreamActor::MediaStreamActor(
    caf::actor_config &cfg,
    const StreamDetail &detail,
    const utility::Uuid &uuid,
    const JsonStore &meta)
    : caf::event_based_actor(cfg), base_(detail) {

    jsn_handler_ = JsonStoreHandler(
        dynamic_cast<caf::event_based_actor *>(this),
        base_.event_group(),
        utility::Uuid::generate(),
        meta);

    if (not uuid.is_null())
        base_.set_uuid(uuid);

    init();
}

caf::message_handler MediaStreamActor::message_handler() {
    return caf::message_handler{
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](get_media_type_atom) -> MediaType { return base_.media_type(); },

        [=](get_stream_detail_atom) -> StreamDetail { return base_.detail(); },

        [=](const StreamDetail &detail) { base_.set_detail(detail); },

        [=](json_store::set_json_atom atom, const JsonStore &json) {
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json).delegate(jsn_handler_.json_actor());
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json, path).delegate(jsn_handler_.json_actor());
        },

        [=](media::pixel_aspect_atom, const double new_aspect) {
            StreamDetail detail  = base_.detail();
            detail.pixel_aspect_ = new_aspect;
            base_.set_detail(detail);
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
        },

        [=](utility::duplicate_atom) -> result<UuidActor> {
            // clone ourself..
            auto rp = make_response_promise<UuidActor>();

            mail(json_store::get_json_atom_v, "")
                .request(jsn_handler_.json_actor(), infinite)
                .then([=](const JsonStore &meta) mutable {
                    const auto uuid  = utility::Uuid::generate();
                    const auto actor = spawn<MediaStreamActor>(base_.detail(), uuid, meta);
                    rp.deliver(UuidActor(uuid, actor));
                });

            return rp;
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            auto rp = make_response_promise<JsonStore>();
            mail(json_store::get_json_atom_v, "")
                .request(jsn_handler_.json_actor(), infinite)
                .then([=](const JsonStore &meta) mutable {
                    JsonStore jsn;
                    jsn["store"] = meta;
                    jsn["base"]  = base_.serialise();

                    rp.deliver(jsn);
                });

            return rp;
        }};
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
}
