// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/media/enums.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/frame_range.hpp"

using namespace xstudio;
using namespace xstudio::media;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Track::Track(
    const std::string &name,
    const utility::FrameRate &rate,
    const media::MediaType media_type,
    const utility::Uuid &_uuid,
    const caf::actor &actor)
    : Container(name, "Track", _uuid),
      media_type_(media_type),
      item_(
          media_type == MediaType::MT_IMAGE ? ItemType::IT_VIDEO_TRACK
                                            : ItemType::IT_AUDIO_TRACK,
          utility::UuidActorAddr(uuid(), caf::actor_cast<caf::actor_addr>(actor)),
          {},
          utility::FrameRange(FrameRateDuration(0, rate))) {
    item_.set_name(name);
}

Track::Track(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn.at("container"))),
      item_(static_cast<utility::JsonStore>(jsn.at("item"))) {
    media_type_ = jsn.at("media_type");
}

Track::Track(const Item &item, const caf::actor &actor)
    : Container(item.name(), "Track", item.uuid()), item_(item.clone()) {
    item_.set_actor_addr(caf::actor_cast<caf::actor_addr>(actor));
    media_type_ =
        (item_.item_type() == ItemType::IT_VIDEO_TRACK ? MediaType::MT_IMAGE
                                                       : MediaType::MT_AUDIO);
}

Track Track::duplicate() const {
    utility::JsonStore jsn;

    auto dup_container = Container::duplicate();
    auto dup_item      = item_;
    dup_item.set_uuid(dup_container.uuid());

    jsn["container"]  = dup_container.serialise();
    jsn["media_type"] = media_type_;
    jsn["item"]       = dup_item.serialise(1);

    return Track(jsn);
}

JsonStore Track::serialise() const {
    JsonStore jsn;

    jsn["container"]  = Container::serialise();
    jsn["media_type"] = media_type_;
    jsn["item"]       = item_.serialise(1);

    return jsn;
}

void Track::set_media_type(const media::MediaType media_type) { media_type_ = media_type; }
