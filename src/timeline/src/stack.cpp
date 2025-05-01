// SPDX-License-Identifier: Apache-2.0

#include "xstudio/timeline/stack.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Stack::Stack(
    const std::string &name, const FrameRate &rate, const Uuid &uuid_, const caf::actor &actor)
    : Container(name, "Stack", uuid_),
      item_(
          ItemType::IT_STACK,
          UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor)),
          {},
          FrameRange(FrameRateDuration(0, rate))) {
    item_.set_name(name);
}

Stack::Stack(const JsonStore &jsn)
    : Container(static_cast<JsonStore>(jsn.at("container"))),
      item_(static_cast<JsonStore>(jsn.at("item"))) {}

Stack::Stack(const Item &item, const caf::actor &actor)
    : Container(item.name(), "Stack", item.uuid()), item_(item.clone()) {
    item_.set_actor_addr(caf::actor_cast<caf::actor_addr>(actor));
}

JsonStore Stack::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise(1);

    return jsn;
}

Stack Stack::duplicate() const {
    JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"] = dup_container.serialise();
    jsn["item"]      = dup_item.serialise(1);

    return Stack(jsn);
}