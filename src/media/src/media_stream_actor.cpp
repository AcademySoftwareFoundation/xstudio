// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>

#include <chrono>
#include <tuple>
#include <Imath/ImathMatrixAlgo.h>

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

        [=](transform_matrix_atom, const Imath::M44f &tform) -> bool {
            if (tform != base_.transform()) {
                base_.set_transform(tform);
                base_.send_changed();
                mail(utility::event_atom_v, utility::change_atom_v).send(base_.event_group());
            }
            return true;
        },

        [=](transform_matrix_atom) -> Imath::M44f { return base_.transform(); },

        [=](media::rotation_atom, const float r) -> bool {
            apply_auto_rotation(r);
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return true;
        },

        [=](media::rotation_atom) -> float {
            float r = 0.0f;
            try {
                const Imath::M44f tform = base_.transform();
                Imath::V3f rot;
                Imath::extractEulerXYZ(tform, rot);
                r = -rot.z * 180.0f / M_PI;
            } catch (...) {
            }
            return r;
        },

        [=](json_store::set_json_atom atom, const JsonStore &json) {
            if (json.contains(
                    nlohmann::json::json_pointer("/metadata/stream/@/side_data/rotation"))) {

                apply_auto_rotation(
                    json.get("/metadata/stream/@/side_data/rotation").get<float>());
            }
            // metadata changed - need to broadcast an update
            base_.send_changed();
            mail(utility::event_atom_v, change_atom_v).send(base_.event_group());
            return mail(atom, json).delegate(jsn_handler_.json_actor());
        },

        [=](json_store::set_json_atom atom, const JsonStore &json, const std::string &path) {
            // metadata changed - need to broadcast an update
            if (path == "/metadata/stream/@" &&
                json.contains(nlohmann::json::json_pointer("/side_data/rotation"))) {
                apply_auto_rotation(json.get("/side_data/rotation").get<float>());
            }
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

void MediaStreamActor::apply_auto_rotation(const float rotation_degrees) {

    Imath::M44f m;
    if (rotation_degrees != 0.0) {

        // IMath coord sys is weird I think ... not right handed. Hence
        // negation here.
        m.rotate(Imath::V3f(0.0f, 0.0f, -rotation_degrees * M_PI / 180.0f));

        // after applying a rotation, we need to apply an additional
        // scale so that the viewport 'fit' (Width, Height, Best etc.)
        // will work correctly. xSTUDIO's coordinate system will by
        // default scale an image so that it's left edge is at x=-1.0
        // and right edge is at x=1.0. The transform matrix is applied *after*
        // the xSTUDIO coordinate system has been enetered, however. So if you
        // have an image of aspect 16:9, say, and it's rotated by 90 degrees with
        // no scaling, then the right edge will now not be at x=1.0 but at x = 9/16....

        // make virtual corners of the un-rotated image
        const float a =
            float(base_.detail().resolution_.y) / float(base_.detail().resolution_.x);
        std::array<Imath::V4f, 4> corners = {
            Imath::V4f(1.0f, a, 0.0f, 1.0f),
            Imath::V4f(1.0f, -a, 0.0f, 1.0f),
            Imath::V4f(-1.0f, a, 0.0f, 1.0f),
            Imath::V4f(-1.0f, -a, 0.0f, 1.0f)};
        // apply the rotation, find out where the image corners end up
        float rightmost = 0.0f;
        for (int i = 0; i < 4; ++i) {
            Imath::V4f v = corners[i] * m;
            if (v.w != 0.0f) {
                // take the max x-position of any of the corners
                rightmost = std::max(rightmost, v.x / v.w);
            }
        }
        // use the max x-position of the transformed corners to re-scale the
        // image so that the rotated corners 'fit' between -1.0 and 1.0 in the
        // xSTUDIO viewport coordinate system
        m.scale(Imath::V3f(1.0f / rightmost, 1.0f / rightmost, 1.0f));
    }

    if (m != base_.transform()) {
        base_.set_transform(m);
        mail(utility::event_atom_v, change_atom_v, media::rotation_atom_v, rotation_degrees)
            .send(base_.event_group());
    }
}
