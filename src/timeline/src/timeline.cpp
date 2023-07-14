// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/timeline.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::timeline;
using namespace xstudio::utility;

Timeline::Timeline(const std::string &name, const utility::Uuid &_uuid, const caf::actor &actor)
    : Container(name, "Timeline", _uuid),
      item_(
          ItemType::IT_TIMELINE,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor))) {}

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
