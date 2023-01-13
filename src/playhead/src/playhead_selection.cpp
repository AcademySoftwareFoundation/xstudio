// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/media/media.hpp"
#include "xstudio/playhead/playhead_selection.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::playhead;
using namespace xstudio::media;
using namespace xstudio::utility;

PlayheadSelection::PlayheadSelection(const std::string &name, const std::string &type)
    : Container(name, type) {}

PlayheadSelection::PlayheadSelection(const JsonStore &jsn)
    : Container(static_cast<JsonStore>(jsn["container"])),
      selected_media_uuids_(static_cast<JsonStore>(jsn["image_items"])) {
    monitored_uuid_ = jsn["monitored_uuid"];
}

JsonStore PlayheadSelection::serialise() const {
    JsonStore jsn;

    jsn["container"]      = Container::serialise();
    jsn["image_items"]    = selected_media_uuids_.serialise();
    jsn["monitored_uuid"] = monitored_uuid_;

    return jsn;
}

UuidVector PlayheadSelection::items_vec() const {
    UuidVector result;
    result.insert(
        result.begin(),
        selected_media_uuids_.uuids().begin(),
        selected_media_uuids_.uuids().end());
    return result;
}

void PlayheadSelection::insert_item(const Uuid &uuid, const Uuid &uuid_before) {
    if (not selected_media_uuids_.contains(uuid)) {
        selected_media_uuids_.insert(uuid, uuid_before);
    }
}

bool PlayheadSelection::remove_item(const Uuid &uuid) {
    return selected_media_uuids_.remove(uuid);
}

bool PlayheadSelection::move_item(const Uuid &uuid, const Uuid &uuid_before) {
    return selected_media_uuids_.move(uuid, uuid_before);
}

void PlayheadSelection::clear() { selected_media_uuids_.clear(); }

bool PlayheadSelection::contains(const Uuid &uuid) const {
    return selected_media_uuids_.contains(uuid);
}
