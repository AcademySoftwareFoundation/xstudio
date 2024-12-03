// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/gap.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Gap::Gap(
    const std::string &name,
    const utility::FrameRateDuration &duration,
    const utility::Uuid &_uuid,
    const caf::actor &actor)
    : Container(name, "Gap", _uuid),
      item_(
          ItemType::IT_GAP,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor)),
          utility::FrameRange(FrameRateDuration(0, duration.rate()), duration),
          utility::FrameRange(FrameRateDuration(0, duration.rate()), duration)) {
    item_.set_name(name);
}

Gap::Gap(const utility::JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))) {}

Gap::Gap(const Item &item, const caf::actor &actor)
    : Container(item.name(), "Gap", item.uuid()), item_(item.clone()) {
    item_.set_actor_addr(caf::actor_cast<caf::actor_addr>(actor));
}

utility::JsonStore Gap::serialise() const {
    utility::JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise();

    return jsn;
}


Gap Gap::duplicate() const {
    utility::JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"] = dup_container.serialise();
    jsn["item"]      = dup_item.serialise();

    return Gap(jsn);
}
