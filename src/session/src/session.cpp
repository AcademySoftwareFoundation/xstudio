// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/session/session.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::session;
using namespace xstudio::utility;

Session::Session(const std::string &name)
    : Container(name, "Session"),
      playlists_("New Session", "Session"),
      media_rate_(timebase::k_flicks_24fps),
      playhead_rate_(timebase::k_flicks_24fps) {}

Session::Session(const JsonStore &jsn, caf::uri filepath)
    : Container(static_cast<utility::JsonStore>(jsn["container"])),
      filepath_(std::move(filepath)) {

    session_file_mtime_    = get_file_mtime(filepath_);
    media_rate_            = jsn["media_rate"];
    playhead_rate_         = jsn["playhead_rate"];
    playlists_             = PlaylistTree(static_cast<utility::JsonStore>(jsn["playlists"]));
    current_playlist_uuid_ = jsn.value("current_playlist_uuid", utility::Uuid());
    viewed_playlist_uuid_  = jsn.value("viewed_playlist_uuid", utility::Uuid());
}

JsonStore Session::serialise() const {
    JsonStore jsn;

    jsn["container"]     = Container::serialise();
    jsn["media_rate"]    = media_rate_;
    jsn["playhead_rate"] = playhead_rate_;

    // identify actors that are media..
    jsn["playlists"]             = playlists_.serialise();
    jsn["current_playlist_uuid"] = current_playlist_uuid_;
    jsn["viewed_playlist_uuid"]  = viewed_playlist_uuid_;

    return jsn;
}

void Session::set_filepath(const caf::uri &path) {
    filepath_           = path;
    session_file_mtime_ = get_file_mtime(filepath_);
}

std::optional<UuidVector> Session::container_children(const utility::Uuid &uuid) const {
    auto tmp = playlists_.cfind(uuid);
    if (tmp) {
        return playlists_.children_uuid(**tmp, true);
    }
    return {};
}


std::optional<Uuid>
Session::insert_group(const std::string &name, const utility::Uuid &uuid_before) {
    return playlists_.insert(PlaylistItem(name, "ContainerGroup"), uuid_before);
}

std::optional<Uuid> Session::insert_divider(
    const std::string &name, const utility::Uuid &uuid_before, const bool into) {
    return playlists_.insert(PlaylistItem(name, "ContainerDivider"), uuid_before, into);
}

std::optional<Uuid> Session::insert_container(
    const PlaylistItem &container, const utility::Uuid &uuid_before, const bool into) {
    return playlists_.insert(container, uuid_before, into);
}

bool Session::insert_container(
    PlaylistTree container, const utility::Uuid &uuid_before, const bool into) {
    return playlists_.insert(std::move(container), uuid_before, into);
}

// bool Session::insert_container(const utility::Uuid &uuid, const utility::Uuid &uuid_before,
// const bool into) { 	return playlists_.insert(uuid, uuid_before, into);
// }

bool Session::remove_container(const utility::Uuid &uuid) { return playlists_.remove(uuid); }

bool Session::move_container(
    const utility::Uuid &uuid, const utility::Uuid &uuid_before, const bool into) {
    return playlists_.move(uuid, uuid_before, into);
}

bool Session::rename_container(const std::string &name, const utility::Uuid &uuid) {
    return playlists_.rename(name, uuid);
}

bool Session::reflag_container(const std::string &flag, const utility::Uuid &uuid) {
    return playlists_.reflag(flag, uuid);
}
