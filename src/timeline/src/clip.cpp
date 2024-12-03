// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::timeline;
using namespace xstudio;

Clip::Clip(
    const std::string name,
    const utility::Uuid &_uuid,
    const caf::actor &actor,
    const utility::Uuid media_uuid)
    : Container(std::move(name), "Clip", _uuid),
      item_(
          ItemType::IT_CLIP,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor))),
      media_uuid_(std::move(media_uuid)) {
    item_.set_name(name);

    auto jsn          = R"({"media_uuid": null})"_json;
    jsn["media_uuid"] = media_uuid_;
    item_.set_prop(utility::JsonStore(jsn));
}

Clip::Clip(const utility::JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))) {

    // hack for old data
    if (jsn.count("media_uuid")) {
        media_uuid_       = jsn.at("media_uuid");
        auto jsn          = R"({"media_uuid": null})"_json;
        jsn["media_uuid"] = media_uuid_;
        item_.set_prop(utility::JsonStore(jsn));
    } else {
        media_uuid_ = item_.prop().at("media_uuid");
    }
}

Clip::Clip(const Item &item, const caf::actor &actor)
    : Container(item.name(), "Clip", item.uuid()), item_(item.clone()) {
    media_uuid_ = item_.prop().value("media_uuid", utility::Uuid());
    item_.set_actor_addr(caf::actor_cast<caf::actor_addr>(actor));
}


Clip Clip::duplicate() const {
    utility::JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"] = dup_container.serialise();
    jsn["item"]      = dup_item.serialise();

    return Clip(jsn);
}

utility::JsonStore Clip::serialise() const {
    utility::JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise();

    return jsn;
}
