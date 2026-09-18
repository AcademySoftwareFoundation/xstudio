// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/playlist/playlist.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::playlist;
using namespace xstudio::utility;

Playlist::Playlist(const std::string &name)
    : Container(name, "Playlist"),
      container_tree_(name, "Playlist", uuid()),
      media_rate_(timebase::k_flicks_24fps),
      playhead_rate_(timebase::k_flicks_24fps) {
    set_version(PROJECT_VERSION);
    set_file_version(PLAYLIST_FILE_VERSION);
}

Playlist::Playlist(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])),
      media_list_(static_cast<utility::JsonStore>(jsn["media"])) {
    media_rate_     = jsn["media_rate"];
    playhead_rate_  = jsn["playhead_rate"];
    container_tree_ = PlaylistTree(static_cast<utility::JsonStore>(jsn["container_tree"]));
    expanded_       = jsn.value("expanded", false);

    set_version(PROJECT_VERSION);
    set_file_version(PLAYLIST_FILE_VERSION, true);
}

JsonStore Playlist::serialise() const {
    JsonStore jsn;

    jsn["container"]     = Container::serialise();
    jsn["media_rate"]    = media_rate_;
    jsn["playhead_rate"] = playhead_rate_;

    // identify actors that are media..
    jsn["media"]          = media_list_.serialise();
    jsn["container_tree"] = container_tree_.serialise();
    jsn["expanded"]       = expanded_;
    return jsn;
}


std::optional<UuidVector> Playlist::container_children(const utility::Uuid &uuid) const {
    auto tmp = container_tree_.cfind(uuid);
    if (tmp) {
        return container_tree_.children_uuid(**tmp, true);
    }
    return {};
}

bool Playlist::insert_container(
    PlaylistTree container, const utility::Uuid &uuid_before, const bool into) {

    bool result = false;

    if (container.value().type() == "Playlist" or container.value().type() == "") {
        for (auto &i : container.children_) {
            result |= insert_container(i, uuid_before, into);
        }
    } else {
        result = container_tree_.insert(std::move(container), uuid_before, into);
    }

    return result;
}

std::optional<Uuid>
Playlist::insert_group(const std::string &name, const utility::Uuid &uuid_before) {
    return container_tree_.insert(PlaylistItem(name, "ContainerGroup"), uuid_before);
}

std::optional<Uuid> Playlist::insert_divider(
    const std::string &name, const utility::Uuid &uuid_before, const bool into) {
    return container_tree_.insert(PlaylistItem(name, "ContainerDivider"), uuid_before, into);
}

std::optional<Uuid> Playlist::insert_container(
    const PlaylistItem &container, const utility::Uuid &uuid_before, const bool into) {
    return container_tree_.insert(container, uuid_before, into);
}

bool Playlist::remove_container(const utility::Uuid &uuid) {
    return container_tree_.remove(uuid);
}

bool Playlist::move_container(
    const utility::Uuid &uuid, const utility::Uuid &uuid_before, const bool into) {
    return container_tree_.move(uuid, uuid_before, into);
}

bool Playlist::rename_container(const std::string &name, const utility::Uuid &uuid) {
    return container_tree_.rename(name, uuid);
}

bool Playlist::reflag_container(const std::string &flag, const utility::Uuid &uuid) {
    return container_tree_.reflag(flag, uuid);
}
