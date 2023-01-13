// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/tag/tag_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::tag;
using namespace nlohmann;
using namespace caf;

namespace {} // namespace


TagActor::TagActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {

    init();
}

TagActor::TagActor(caf::actor_config &cfg, const utility::Uuid &uuid)
    : caf::event_based_actor(cfg) {
    base_.set_uuid(uuid);

    init();
}

caf::message_handler TagActor::default_event_handler() {
    return {
        [=](utility::event_atom, remove_tag_atom, const utility::Uuid &) {},
        [=](utility::event_atom, add_tag_atom, const Tag &) {},
    };
}

void TagActor::init() {
    print_on_create(this, base_);
    print_on_exit(this, base_);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        base_.make_ignore_error_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        base_.make_last_changed_event_handler(event_group_, this),

        [=](get_tag_atom, const utility::Uuid &id) -> result<Tag> {
            auto tag = base_.get_tag(id);
            if (tag)
                return *tag;
            return make_error(xstudio_error::error, "Invalid uuid");
        },
        [=](get_tags_atom, const utility::Uuid &link_id) -> std::vector<Tag> {
            return base_.get_tags(link_id);
        },
        [=](get_tags_atom) -> std::vector<Tag> { return base_.get_tags(); },

        [=](add_tag_atom, const Tag &tag) -> utility::Uuid {
            auto replaced = base_.add_tag(tag);

            if (replaced)
                send(event_group_, utility::event_atom_v, remove_tag_atom_v, replaced->id());

            send(event_group_, utility::event_atom_v, add_tag_atom_v, tag);

            if (tag.persistent())
                base_.send_changed(event_group_, this);

            return tag.id();
        },

        [=](remove_tag_atom, const utility::Uuid &id) -> bool {
            auto result = base_.remove_tag(id);
            if (result) {
                send(event_group_, utility::event_atom_v, remove_tag_atom_v, id);
                base_.send_changed(event_group_, this);
            }
            return result;
        },

        [=](utility::serialise_atom) -> JsonStore {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return jsn;
        });
}
