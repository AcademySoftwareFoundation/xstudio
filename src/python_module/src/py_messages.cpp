// SPDX-License-Identifier: Apache-2.0
#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wattributes"
#endif

// #include <caf/all.hpp>
// #include <caf/config.hpp>
// #include <caf/io/all.hpp>

#include "py_opaque.hpp"

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/playlist/playlist.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/remote_session_file.hpp"
#include "xstudio/utility/serialise_headers.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/frame_range.hpp"

#include "py_config.hpp"

namespace caf::python {

extern void register_abs_class(py::module &m, const std::string &name);
extern void register_uuid_class(py::module &m, const std::string &name);
extern void register_plugindetail_class(py::module &m, const std::string &name);
extern void register_playlisttree_class(py::module &m, const std::string &name);
extern void register_thumbnailbuffer_class(py::module &m, const std::string &name);
extern void register_streamdetail_class(py::module &m, const std::string &name);
extern void register_timecode_class(py::module &m, const std::string &name);
extern void register_mediareference_class(py::module &m, const std::string &name);
extern void register_bookmark_detail_class(py::module &m, const std::string &name);
extern void register_mediapointer_class(py::module &m, const std::string &name);
extern void register_mediakey_class(py::module &m, const std::string &name);
extern void register_jsonstore_class(py::module &m, const std::string &name);
extern void register_FrameRate_class(py::module &m, const std::string &name);
extern void register_group_down_msg(py::module &m, const std::string &name);
extern void register_URI_class(py::module &m, const std::string &name);
extern void register_FrameRateDuration_class(py::module &m, const std::string &name);
extern void register_uuid_actor_class(py::module &m, const std::string &name);
extern void register_uuid_actor_vector_class(py::module &m, const std::string &name);
extern void register_uuidvec_class(py::module &m, const std::string &name);
extern void register_item_class(py::module &m, const std::string &name);
extern void register_frame_range_class(py::module &m, const std::string &name);
extern void register_frame_list_class(py::module &m, const std::string &name);

using namespace xstudio;

void py_config::add_messages() {
    add_message_type<caf::group_down_msg>(
        "group_down_msg", "caf::group_down_msg", &register_group_down_msg);
    add_message_type<caf::uri>("URI", "caf::uri", &register_URI_class);
    add_message_type<media::MediaType>("MediaType", "xstudio::media::MediaType", nullptr);
    add_message_type<media::StreamDetail>(
        "StreamDetail", "xstudio::media::StreamDetail", &register_streamdetail_class);
    add_message_type<plugin_manager::PluginDetail>(
        "PluginDetail", "xstudio::plugin_manager::PluginDetail", &register_plugindetail_class);
    add_message_type<std::vector<plugin_manager::PluginDetail>>(
        "PluginDetailVec", "std::vector<xstudio::plugin_manager::PluginDetail>", nullptr);
    add_message_type<playhead::CompareMode>(
        "CompareMode", "xstudio::playhead::CompareMode", nullptr);
    add_message_type<global::StatusType>("StatusType", "xstudio::global::StatusType", nullptr);
    add_message_type<playhead::LoopMode>("LoopMode", "xstudio::playhead::LoopMode", nullptr);
    add_message_type<playhead::OverflowMode>(
        "OverflowMode", "xstudio::playhead::OverflowMode", nullptr);
    add_message_type<media::MediaStatus>("MediaStatus", "xstudio::media::MediaStatus", nullptr);
    add_message_type<media::MediaKey>("MediaKey", "xstudio::media::MediaKey", nullptr);
    add_message_type<session::ExportFormat>(
        "ExportFormat", "xstudio::session::ExportFormat", nullptr);

    add_message_type<std::tuple<std::string, std::string>>(
        "std::tuple<std::string, std::string>",
        "std::tuple<std::string, std::string>",
        nullptr);

    add_message_type<utility::UuidActor>(
        "UuidActor", "xstudio::utility::UuidActor", &register_uuid_actor_class);
    add_message_type<std::vector<media::MediaKey>>(
        "MediaKeyVec", "std::vector<xstudio::media::MediaKey>", nullptr);
    add_message_type<std::pair<utility::Uuid, utility::UuidActor>>(
        "UuidUuidActor",
        "std::pair<xstudio::utility::Uuid, xstudio::utility::UuidActor>",
        nullptr);
    add_message_type<std::tuple<int, double, utility::Timecode>>(
        "std::tuple<int, double, Timecode>", "std::tuple<int, double, Timecode>", nullptr);
    add_message_type<std::vector<utility::MediaReference>>(
        "MediaReferenceVec", "std::vector<xstudio::utility::MediaReference>", nullptr);
    add_message_type<std::pair<caf::uri, std::filesystem::file_time_type>>(
        "std::pair<caf::uri, std::filesystem::file_time_type>",
        "std::pair<caf::uri, std::filesystem::file_time_type>",
        nullptr);

    add_message_type<std::pair<std::string, std::vector<std::byte>>>(
        "std::pair<std::string, std::vector<std::byte>>",
        "std::pair<std::string, std::vector<std::byte>>",
        nullptr);

    add_message_type<std::vector<std::pair<int, int>>>(
        "std::vector<std::pair<int, int>>", "std::vector<std::pair<int, int>>", nullptr);

    add_message_type<timebase::flicks>("timebase::flicks", "timebase::flicks", nullptr);

    add_message_type<std::vector<std::tuple<xstudio::utility::Uuid, std::string, int, int>>>(
        "std::vector<std::tuple<xstudio::utility::Uuid, std::string, int, int>>",
        "std::vector<std::tuple<xstudio::utility::Uuid, std::string, int, int>>",
        nullptr);

    add_message_type<timebase::flicks>(
        "xstudio::utility::time_point", "xstudio::utility::time_point", nullptr);

    add_message_type<std::vector<utility::Uuid>>(
        "UuidVec", "std::vector<xstudio::utility::Uuid>", &register_uuidvec_class);
    add_message_type<utility::absolute_receive_timeout>(
        "absolute_receive_timeout",
        "xstudio::utility::absolute_receive_timeout",
        &register_abs_class);
    add_message_type<utility::FrameList>(
        "FrameList", "xstudio::utility::FrameList", &register_frame_list_class);
    add_message_type<utility::FrameRate>(
        "FrameRate", "xstudio::utility::FrameRate", &register_FrameRate_class);
    add_message_type<utility::FrameRateDuration>(
        "FrameRateDuration",
        "xstudio::utility::FrameRateDuration",
        &register_FrameRateDuration_class);
    add_message_type<utility::JsonStore>(
        "JsonStore", "xstudio::utility::JsonStore", &register_jsonstore_class);
    add_message_type<utility::MediaReference>(
        "MediaReference", "xstudio::utility::MediaReference", &register_mediareference_class);

    add_message_type<bookmark::BookmarkDetail>(
        "BookmarkDetail", "xstudio::bookmark::BookmarkDetail", &register_bookmark_detail_class);

    add_message_type<media::AVFrameID>(
        "AVFrameID", "xstudio::media::AVFrameID", &register_mediapointer_class);
    add_message_type<media::MediaKey>(
        "MediaKey", "xstudio::media::MediaKey", &register_mediakey_class);
    add_message_type<utility::PlaylistTree>(
        "PlaylistTree", "xstudio::utility::PlaylistTree", &register_playlisttree_class);
    add_message_type<utility::Timecode>(
        "Timecode", "xstudio::utility::Timecode", &register_timecode_class);
    add_message_type<utility::TimeSourceMode>(
        "TimeSourceMode", "xstudio::utility::TimeSourceMode", nullptr);
    add_message_type<utility::Uuid>("Uuid", "xstudio::utility::Uuid", &register_uuid_class);

    add_message_type<std::vector<xstudio::utility::UuidActor>>(
        "UuidActorVec", "xstudio::utility::UuidActorVector", &register_uuid_actor_vector_class);

    add_message_type<std::shared_ptr<xstudio::thumbnail::ThumbnailBuffer>>(
        "ThumbnailBufferPtr", "std::shared_ptr<xstudio::thumbnail::ThumbnailBuffer>", nullptr);

    add_message_type<xstudio::thumbnail::ThumbnailBuffer>(
        "ThumbnailBuffer",
        "xstudio::thumbnail::ThumbnailBuffer",
        &register_thumbnailbuffer_class);

    add_message_type<xstudio::timeline::Item>(
        "Item", "xstudio::timeline::Item", &register_item_class);

    add_message_type<xstudio::utility::FrameRange>(
        "FrameRange", "xstudio::utility::FrameRange", &register_frame_range_class);

    add_message_type<std::pair<xstudio::utility::JsonStore, xstudio::timeline::Item>>(
        "std::pair<xstudio::utility::JsonStore, xstudio::timeline::Item>",
        "std::pair<xstudio::utility::JsonStore, xstudio::timeline::Item>",
        nullptr);
}
} // namespace caf::python
