// SPDX-License-Identifier: Apache-2.0
#include "py_opaque.hpp"


#include <caf/all.hpp>
#include <caf/config.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include "pybind11_json/pybind11_json.hpp"
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

CAF_POP_WARNINGS

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playlist/playlist.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/frame_range.hpp"


namespace py = pybind11;

using namespace xstudio;
namespace caf::python {

void register_group_down_msg(py::module &m, const std::string &name) {
    py::class_<caf::group_down_msg>(m, name.c_str()).def(py::init<>());
}

void register_playlistitem_class(py::module &m, const std::string &name) {
    // auto str_impl = [](const utility::PlaylistItem& x) { return to_string(x); };
    py::class_<utility::PlaylistItem>(m, name.c_str())
        .def(py::init<>())
        // .def("__str__", str_impl)
        .def("name", &utility::PlaylistItem::name)
        .def("type", &utility::PlaylistItem::type)
        .def("flag", &utility::PlaylistItem::flag)
        .def("uuid", &utility::PlaylistItem::uuid);
}

void register_abs_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::absolute_receive_timeout &x) { return to_string(x); };
    py::class_<utility::absolute_receive_timeout>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<int>())
        .def("__str__", str_impl);
}

void register_uuid_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::Uuid &x) { return to_string(x); };
    py::class_<utility::Uuid>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<const std::string &>())
        .def("generate", &utility::Uuid::generate)
        .def("generate_in_place", &utility::Uuid::generate_in_place)
        .def("is_null", &utility::Uuid::is_null)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__str__", str_impl);
}

void register_plugindetail_class(py::module &m, const std::string &name) {
    py::class_<plugin_manager::PluginDetail>(m, name.c_str())
        .def(py::init<>())
        .def("name", [](const plugin_manager::PluginDetail &x) { return x.name_; })
        .def("type", [](const plugin_manager::PluginDetail &x) { return x.type_; })
        .def("enabled", [](const plugin_manager::PluginDetail &x) { return x.enabled_; })
        .def("path", [](const plugin_manager::PluginDetail &x) { return x.path_; })
        .def("author", [](const plugin_manager::PluginDetail &x) { return x.author_; })
        .def(
            "description", [](const plugin_manager::PluginDetail &x) { return x.description_; })
        .def("version", [](const plugin_manager::PluginDetail &x) { return x.version_; })
        .def("uuid", [](const plugin_manager::PluginDetail &x) { return x.uuid_; });
}


void register_playlistcontainer_class(py::module &m, const std::string &name) {
    auto str_impl = [](const xstudio::utility::UuidTree<xstudio::utility::PlaylistItem> &x) {
        return to_string(x);
    };
    py::class_<xstudio::utility::UuidTree<xstudio::utility::PlaylistItem>>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        // .def("name", &PlaylistTree::name)
        // .def("type", &PlaylistTree::type)
        // .def("flag", &PlaylistTree::flag)
        // .def("value_uuid", &PlaylistTree::value_uuid)
        // .def("empty", &PlaylistTree::empty)
        // .def("size", &PlaylistTree::size)
        // .def("children", &PlaylistTree::children)
        // .def("value", &UuidTree::value)
        .def(
            "name",
            [](const xstudio::utility::UuidTree<xstudio::utility::PlaylistItem> &x) {
                return x.value().name();
            })
        .def(
            "value_uuid",
            [](const xstudio::utility::UuidTree<xstudio::utility::PlaylistItem> &x) {
                return x.value().uuid();
            })
        .def("uuid", [](const xstudio::utility::UuidTree<xstudio::utility::PlaylistItem> &x) {
            return x.uuid();
        });
}


void register_playlisttree_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::PlaylistTree &x) { return to_string(x); };
    py::class_<utility::PlaylistTree>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        .def("name", &utility::PlaylistTree::name)
        .def("type", &utility::PlaylistTree::type)
        .def("flag", &utility::PlaylistTree::flag)
        .def("value_uuid", &utility::PlaylistTree::value_uuid)
        .def("empty", &utility::PlaylistTree::empty)
        .def("size", &utility::PlaylistTree::size)
        .def("children", &utility::PlaylistTree::children)
        // .def("value", &UuidTree::value)
        .def("uuid", &utility::PlaylistTree::uuid);
}

void register_thumbnailbuffer_class(py::module &m, const std::string &name) {
    py::class_<thumbnail::ThumbnailBuffer, std::shared_ptr<thumbnail::ThumbnailBuffer>>(
        m, name.c_str())
        .def(py::init<>())
        .def("width", &thumbnail::ThumbnailBuffer::width)
        .def("height", &thumbnail::ThumbnailBuffer::height)
        .def("data", &thumbnail::ThumbnailBuffer::data);
}

void register_streamdetail_class(py::module &m, const std::string &name) {
    auto str_impl = [](const media::StreamDetail &x) { return to_string(x); };
    py::class_<media::StreamDetail>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        .def("name", [](const media::StreamDetail &x) { return x.name_; })
        .def("key_format", [](const media::StreamDetail &x) { return x.key_format_; })
        .def("duration", [](const media::StreamDetail &x) { return x.duration_; })
        .def(
            "resolution",
            [](const media::StreamDetail &x) {
                return std::vector<int>({x.resolution_.x, x.resolution_.y});
            })
        .def("pixel_aspect", [](const media::StreamDetail &x) { return x.pixel_aspect_; })
        .def("media_type", [](const media::StreamDetail &x) { return x.media_type_; });
}


void register_timecode_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::Timecode &x) { return to_string(x); };
    py::class_<utility::Timecode>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        .def("hours", [](const utility::Timecode &x) { return x.hours(); })
        .def("minutes", [](const utility::Timecode &x) { return x.minutes(); })
        .def("seconds", [](const utility::Timecode &x) { return x.seconds(); })
        .def("frames", [](const utility::Timecode &x) { return x.frames(); })
        .def("framerate", [](const utility::Timecode &x) { return x.framerate(); })
        .def("dropframe", [](const utility::Timecode &x) { return x.dropframe(); })
        .def("total_frames", [](const utility::Timecode &x) { return x.total_frames(); });
}

void register_mediareference_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::MediaReference &x) { return to_string(x); };
    py::class_<utility::MediaReference>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        .def("container", &utility::MediaReference::container)
        .def("frame_count", &utility::MediaReference::frame_count)
        .def(
            "seconds",
            &utility::MediaReference::seconds,
            "Duration in seconds",
            py::arg("override") = utility::FrameRate())
        .def("duration", &utility::MediaReference::duration)
        .def("rate", &utility::MediaReference::rate)
        .def("set_rate", &utility::MediaReference::set_rate)
        .def(
            "uri",
            py::overload_cast<>(&utility::MediaReference::uri, py::const_),
            "URI of mediareference")

        .def("uri_from_frame", &utility::MediaReference::uri_from_frame)
        .def("timecode", &utility::MediaReference::timecode)
        .def("offset", &utility::MediaReference::offset)
        .def("set_offset", &utility::MediaReference::set_offset)
        .def("frame", [](const utility::MediaReference &x, int i) {
            auto f = x.frame(i);
            if (f)
                return *f;
            throw pybind11::value_error("Out of range");
        });
}

void register_bookmark_detail_class(py::module &m, const std::string &name) {
    auto str_impl = [](const bookmark::BookmarkDetail &x) { return to_string(x); };
    py::class_<bookmark::BookmarkDetail>(m, name.c_str())
        .def(py::init<>())
        .def("__str__", str_impl)
        .def_readonly("uuid", &bookmark::BookmarkDetail::uuid_)
        .def_readwrite("enabled", &bookmark::BookmarkDetail::enabled_)
        .def_readwrite("has_focus", &bookmark::BookmarkDetail::has_focus_)
        .def_readwrite("visible", &bookmark::BookmarkDetail::visible_)
        .def_readwrite("start", &bookmark::BookmarkDetail::start_)
        .def_readwrite("duration", &bookmark::BookmarkDetail::duration_)
        .def_readwrite("author", &bookmark::BookmarkDetail::author_)
        .def_readwrite("owner", &bookmark::BookmarkDetail::owner_)
        .def_readwrite("note", &bookmark::BookmarkDetail::note_)
        .def_readwrite("created", &bookmark::BookmarkDetail::created_)
        .def_readonly("has_note", &bookmark::BookmarkDetail::has_note_)
        .def_readonly("has_annotation", &bookmark::BookmarkDetail::has_annotation_);
}

void register_mediapointer_class(py::module &m, const std::string &name) {
    auto str_impl = [](const media::AVFrameID &) { return "AVFrameID"; };
    py::class_<media::AVFrameID>(m, name.c_str()).def(py::init<>()).def("__str__", str_impl);
}

void register_mediakey_class(py::module &m, const std::string &name) {
    auto str_impl = [](const media::MediaKey &x) { return to_string(x); };
    py::class_<media::MediaKey>(m, name.c_str()).def(py::init<>()).def("__str__", str_impl);
}

void register_jsonstore_class(py::module &m, const std::string &name) {
    auto str_impl             = [](const utility::JsonStore &x) { return to_string(x); };
    auto get_preferences_impl = [](const utility::JsonStore &x,
                                   const std::set<std::string> &context =
                                       std::set<std::string>()) {
        return global_store::get_preferences(x, context);
    };
    auto get_preference_values_impl = [](const utility::JsonStore &x,
                                         const std::set<std::string> &context =
                                             std::set<std::string>()) {
        return global_store::get_preference_values(x, context);
    };

    py::class_<utility::JsonStore>(m, name.c_str())
        .def(py::init())
        .def(py::init<const nlohmann::json &>())
        .def("__str__", str_impl)
        .def(
            "get",
            py::overload_cast<const std::string &>(
                &utility::JsonStore::get<nlohmann::json>, py::const_),
            "Get json.",
            py::arg("path") = "")
        .def(
            "set",
            py::overload_cast<const nlohmann::json &, const std::string &>(
                &utility::JsonStore::set),
            "Set json.",
            py::arg("json"),
            py::arg("path") = "")
        .def("clear", &utility::JsonStore::clear)
        .def("dump", &utility::JsonStore::dump, "dump json", py::arg("pad") = 0)
        .def("is_null", &utility::JsonStore::is_null)
        .def("remove", &utility::JsonStore::remove)
        .def("empty", &utility::JsonStore::empty)
        .def("get_preferences", get_preferences_impl)
        .def("get_values", get_preference_values_impl);
}

void register_FrameRate_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::FrameRate &x) { return to_string(x); };
    py::class_<utility::FrameRate>(m, name.c_str())
        .def(py::init<>())
        .def(py::init([](const double &fps) { return utility::FrameRate(1.0f / fps); }))
        .def("seconds", &utility::FrameRate::to_seconds)
        .def("fps", &utility::FrameRate::to_fps)
        .def("__str__", str_impl);
}

void register_URI_class(py::module &m, const std::string &name) {
    auto str_impl = [](const caf::uri &x) { return to_string(x); };
    py::class_<caf::uri>(m, name.c_str())
        .def(py::init<>())
        .def(py::init([](const std::string &uri_str) {
            auto uri = caf::make_uri(uri_str);
            if (uri)
                return *uri;

            throw pybind11::value_error("Invalid uri");
        }))
        // .def(py::init<const std::string>())
        .def("empty", &caf::uri::empty)
        .def("__str__", str_impl);
}

void register_FrameRateDuration_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::FrameRateDuration &x) { return to_string(x); };
    py::class_<utility::FrameRateDuration>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<int, utility::FrameRate>())
        .def(
            "frames",
            &utility::FrameRateDuration::frames,
            py::arg("rate") = utility::FrameRate())
        .def(
            "seconds",
            &utility::FrameRateDuration::seconds,
            py::arg("rate") = utility::FrameRate())
        .def("set_frames", &utility::FrameRateDuration::set_frames)
        .def("rate", &utility::FrameRateDuration::rate)
        .def("__str__", str_impl);
}

void register_actor_class(py::module &m, const std::string &name) {
    auto str_impl = [](const caf::actor &x) { return to_string(x); };
    py::class_<caf::actor>(m, name.c_str()).def(py::init()).def("__str__", str_impl);
}

void register_group_class(py::module &m, const std::string &name) {
    auto str_impl = [](const caf::group &x) { return to_string(x); };
    py::class_<caf::group>(m, name.c_str()).def(py::init()).def("__str__", str_impl);
}

void register_addr_class(py::module &m, const std::string &name) {
    auto str_impl = [](const caf::actor_addr &x) { return to_string(x); };
    py::class_<caf::actor_addr>(m, name.c_str()).def(py::init()).def("__str__", str_impl);
}

void register_uuid_actor_class(py::module &m, const std::string &name) {
    // auto str_impl = [](const std::vector<xstudio::utility::Uuid>& x) { return
    // to_string(x);
    // };
    py::class_<xstudio::utility::UuidActor>(m, name.c_str())
        .def(py::init())
        .def(py::init<xstudio::utility::Uuid, caf::actor>())
        .def_readonly("uuid", &xstudio::utility::UuidActor::uuid_)
        .def_readonly("actor", &xstudio::utility::UuidActor::actor_);
}


void register_uuid_actor_vector_class(py::module &m, const std::string &name) {
    py::class_<std::vector<xstudio::utility::UuidActor>>(m, name.c_str())
        .def(py::init())
        // .def("__str__", str_impl)
        .def("clear", &std::vector<xstudio::utility::UuidActor>::clear)
        .def(
            "push_back",
            py::overload_cast<const xstudio::utility::UuidActor &>(
                &std::vector<xstudio::utility::UuidActor>::push_back))
        .def("pop_back", &std::vector<xstudio::utility::UuidActor>::pop_back)
        .def("__len__", &std::vector<xstudio::utility::UuidActor>::size)
        .def(
            "__getitem__",
            [](const std::vector<xstudio::utility::UuidActor> &s, size_t i) {
                if (i >= s.size()) {
                    throw py::index_error();
                }
                return s[i];
            })
        .def(
            "__iter__",
            [](std::vector<xstudio::utility::UuidActor> &v) {
                return py::make_iterator(v.begin(), v.end());
            },
            py::keep_alive<0, 1>());
}

void register_uuidvec_class(py::module &m, const std::string &name) {
    // auto str_impl = [](const std::vector<xstudio::utility::Uuid>& x) { return
    // to_string(x);
    // };
    py::class_<std::vector<xstudio::utility::Uuid>>(m, name.c_str())
        .def(py::init())
        // .def("__str__", str_impl)
        .def("clear", &std::vector<xstudio::utility::Uuid>::clear)
        .def(
            "push_back",
            py::overload_cast<const xstudio::utility::Uuid &>(
                &std::vector<xstudio::utility::Uuid>::push_back))
        .def("pop_back", &std::vector<xstudio::utility::Uuid>::pop_back)
        .def("__len__", &std::vector<xstudio::utility::Uuid>::size)
        .def(
            "__iter__",
            [](std::vector<xstudio::utility::Uuid> &v) {
                return py::make_iterator(v.begin(), v.end());
            },
            py::keep_alive<0, 1>());
}

void register_item_class(py::module &m, const std::string &name) {
    // auto str_impl = [](const timeline::Item &x) { return to_string(x); };
    py::class_<timeline::Item>(m, name.c_str())
        .def(py::init<>())
        .def("uuid", &timeline::Item::uuid)
        .def("item_type", &timeline::Item::item_type)
        .def("actor_addr", &timeline::Item::actor_addr)
        .def("actor", &timeline::Item::actor)
        .def("uuid_actor", &timeline::Item::uuid_actor)
        .def("enabled", &timeline::Item::enabled)
        .def("transparent", &timeline::Item::transparent)

        .def("rate", &timeline::Item::rate)
        .def("name", &timeline::Item::name)
        .def("flag", &timeline::Item::flag)
        .def("prop", &timeline::Item::prop)

        .def("active_range", &timeline::Item::active_range)
        .def("active_duration", &timeline::Item::active_duration)
        .def("active_start", &timeline::Item::active_start)

        .def("available_range", &timeline::Item::available_range)
        .def("available_duration", &timeline::Item::available_duration)
        .def("available_start", &timeline::Item::available_start)

        .def("trimmed_range", &timeline::Item::trimmed_range)
        .def("trimmed_duration", &timeline::Item::trimmed_duration)
        .def("trimmed_start", &timeline::Item::trimmed_start)

        .def(
            "resolve_time",
            &timeline::Item::resolve_time,
            py::arg("time")       = utility::FrameRate(),
            py::arg("media_type") = media::MediaType::MT_IMAGE,
            py::arg("focus")      = utility::UuidSet())

        .def("children", py::overload_cast<>(&timeline::Item::children), "Get children")
        .def("__len__", [](timeline::Item &v) { return v.size(); });
    // .def("__str__", str_impl);
}

void register_frame_range_class(py::module &m, const std::string &name) {
    py::class_<utility::FrameRange>(m, name.c_str())
        .def(py::init())
        .def(py::init<xstudio::utility::FrameRateDuration>())
        .def(py::init<
             xstudio::utility::FrameRateDuration,
             xstudio::utility::FrameRateDuration>())
        .def("rate", &utility::FrameRange::rate)
        .def("start", &utility::FrameRange::start)
        .def("duration", &utility::FrameRange::duration)
        .def("frame_start", &utility::FrameRange::frame_start)
        .def("frame_duration", &utility::FrameRange::frame_duration)

        .def("set_rate", &utility::FrameRange::set_rate, py::arg("rate") = utility::FrameRate())
        .def(
            "set_start",
            &utility::FrameRange::set_start,
            py::arg("start") = utility::FrameRate())
        .def(
            "set_duration",
            &utility::FrameRange::set_duration,
            py::arg("duration") = utility::FrameRate());
}

void register_frame_list_class(py::module &m, const std::string &name) {
    auto str_impl = [](const utility::FrameList &x) { return to_string(x); };
    py::class_<utility::FrameList>(m, name.c_str())
        .def(py::init<>())
        .def(py::init<std::string>())
        .def("__str__", str_impl);
}

} // namespace caf::python
