// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/timeline.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::timeline;
using namespace xstudio::utility;

Timeline::Timeline(
    const std::string &name,
    const utility::FrameRate &rate,
    const utility::Uuid &_uuid,
    const caf::actor &actor)
    : Container(name, "Timeline", _uuid),
      item_(
          ItemType::IT_TIMELINE,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor)),
          {},
          utility::FrameRange(FrameRateDuration(0, rate))) {
    item_.set_name(name);
}

Timeline::Timeline(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))),
      media_list_(static_cast<utility::JsonStore>(jsn.at("media"))) {}

JsonStore Timeline::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise(1);
    jsn["media"]     = media_list_.serialise();

    return jsn;
}

Timeline Timeline::duplicate() const {
    utility::JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"] = dup_container.serialise();
    jsn["item"]      = dup_item.serialise(1);
    jsn["media"]     = media_list_.serialise();

    return Timeline(jsn);
}