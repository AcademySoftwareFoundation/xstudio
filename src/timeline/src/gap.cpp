// SPDX-License-Identifier: Apache-2.0

#include "xstudio/timeline/gap.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Gap::Gap(
    const std::string &name,
    const FrameRateDuration &duration,
    const Uuid &_uuid,
    const caf::actor &actor)
    : Container(name, "Gap", _uuid),
      item_(
          ItemType::IT_GAP,
          UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor)),
          FrameRange(FrameRateDuration(0, duration.rate()), duration),
          FrameRange(FrameRateDuration(0, duration.rate()), duration)) {
    item_.set_name(name);
}

Gap::Gap(const JsonStore &jsn)
    : Container(static_cast<JsonStore>(jsn.at("container"))),
      item_(static_cast<JsonStore>(jsn.at("item"))) {}

Gap::Gap(const Item &item, const caf::actor &actor)
    : Container(item.name(), "Gap", item.uuid()), item_(item.clone()) {
    item_.set_actor_addr(caf::actor_cast<caf::actor_addr>(actor));
}

JsonStore Gap::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise();

    return jsn;
}


Gap Gap::duplicate() const {
    JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"] = dup_container.serialise();
    jsn["item"]      = dup_item.serialise();

    return Gap(jsn);
}
