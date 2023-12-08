// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/utility/helpers.hpp"

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
      media_uuid_(std::move(media_uuid)) {}

Clip::Clip(const utility::JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))) {
    media_uuid_ = jsn.at("media_uuid");
}

utility::JsonStore Clip::serialise() const {
    utility::JsonStore jsn;

    jsn["container"]  = Container::serialise();
    jsn["item"]       = item_.serialise();
    jsn["media_uuid"] = media_uuid_;

    return jsn;
}
