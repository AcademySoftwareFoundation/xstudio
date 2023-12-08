// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/stack.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Stack::Stack(const std::string &name, const utility::Uuid &uuid_, const caf::actor &actor)
    : Container(name, "Stack", uuid_),
      item_(
          ItemType::IT_STACK,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor))) {}

Stack::Stack(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))) {}

JsonStore Stack::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["item"]      = item_.serialise(1);

    return jsn;
}
