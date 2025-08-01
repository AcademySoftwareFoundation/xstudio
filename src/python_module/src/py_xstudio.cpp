// SPDX-License-Identifier: Apache-2.0
#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "py_opaque.hpp"

#include <caf/all.hpp>
#include <caf/config.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include "pybind11_json/pybind11_json.hpp"
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/embed.h>

CAF_POP_WARNINGS

#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/serialise_headers.hpp"
#include "xstudio/media_reader/pixel_info.hpp"

using namespace xstudio;
namespace py = pybind11;

namespace caf::python {
py::tuple py_parse_posix_path(const std::string &path) {
    utility::FrameList fl;
    caf::uri _uri = utility::parse_cli_posix_path(path, fl, true);
    py::tuple result(2);

    PyTuple_SetItem(result.ptr(), 0, py::cast(std::move(_uri)).release().ptr());
    PyTuple_SetItem(result.ptr(), 1, py::cast(std::move(fl)).release().ptr());
    return result;
}
} // namespace caf::python

#include "py_config.hpp"

void py_remote_session_file(py::module_ &);
void py_playhead(py::module_ &);
void py_plugin(py::module_ &);
void py_utility(py::module_ &);
void py_ui(py::module_ &);
void py_link(py::module_ &);

PYBIND11_MODULE(__pybind_xstudio, m) {

    ACTOR_INIT_GLOBAL_META()
    caf::core::init_global_meta_objects();
    caf::io::middleman::init_global_meta_objects();

    caf::python::py_config p{};
    m.doc() = "Pybind11 xStudio module";
    m.def("parse_posix_path", &caf::python::py_parse_posix_path, R"(
        Parse path for use.

        Args:
            path(str): Path to file

        Returns:
            result(tuple(URI,FrameList)): result.
    )");
    m.def("remote_session_path", &utility::remote_session_path, "Return path to session files");

    py::enum_<session::ExportFormat>(m, "ExportFormat")
        .value("EF_UNDEFINED", session::ExportFormat::EF_UNDEFINED)
        .value("EF_CSV", session::ExportFormat::EF_CSV)
        .value("EF_LAST", session::ExportFormat::EF_LAST)
        .export_values();

    py::enum_<timeline::ItemType>(m, "ItemType")
        .value("IT_NONE", timeline::ItemType::IT_NONE)
        .value("IT_GAP", timeline::ItemType::IT_GAP)
        .value("IT_CLIP", timeline::ItemType::IT_CLIP)
        .value("IT_AUDIO_TRACK", timeline::ItemType::IT_AUDIO_TRACK)
        .value("IT_VIDEO_TRACK", timeline::ItemType::IT_VIDEO_TRACK)
        .value("IT_STACK", timeline::ItemType::IT_STACK)
        .value("IT_TIMELINE", timeline::ItemType::IT_TIMELINE)
        .export_values();

    py::enum_<global::StatusType>(m, "StatusType")
        .value("ST_NONE", global::StatusType::ST_NONE)
        .value("ST_BUSY", global::StatusType::ST_BUSY)
        .export_values();

    py::enum_<module::Attribute::Role>(m, "AttributeRole")
        .value("Type", module::Attribute::Role::Type)
        .value("Enabled", module::Attribute::Role::Enabled)
        .value("Activated", module::Attribute::Role::Activated)
        .value("Title", module::Attribute::Role::Title)
        .value("AbbrTitle", module::Attribute::Role::AbbrTitle)
        .value("StringChoices", module::Attribute::Role::StringChoices)
        .value("AbbrStringChoices", module::Attribute::Role::AbbrStringChoices)
        .value("StringChoicesEnabled", module::Attribute::Role::StringChoicesEnabled)
        .value("ToolTip", module::Attribute::Role::ToolTip)
        .value("CustomMessage", module::Attribute::Role::CustomMessage)
        .value("IntegerMin", module::Attribute::Role::IntegerMin)
        .value("IntegerMax", module::Attribute::Role::IntegerMax)
        .value("FloatScrubMin", module::Attribute::Role::FloatScrubMin)
        .value("FloatScrubMax", module::Attribute::Role::FloatScrubMax)
        .value("FloatScrubStep", module::Attribute::Role::FloatScrubStep)
        .value("FloatScrubSensitivity", module::Attribute::Role::FloatScrubSensitivity)
        .value("FloatDisplayDecimals", module::Attribute::Role::FloatDisplayDecimals)
        .value("DisabledValue", module::Attribute::Role::DisabledValue)
        .value("Value", module::Attribute::Role::Value)
        .value("DefaultValue", module::Attribute::Role::DefaultValue)
        .value("AbbrValue", module::Attribute::Role::AbbrValue)
        .value("UuidRole", module::Attribute::Role::UuidRole)
        .value("Groups", module::Attribute::Role::Groups)
        .value("MenuPaths", module::Attribute::Role::MenuPaths)
        .value("ToolbarPosition", module::Attribute::Role::ToolbarPosition)
        .value("OverrideValue", module::Attribute::Role::OverrideValue)
        .value("SerializeKey", module::Attribute::Role::SerializeKey)
        .value("QmlCode", module::Attribute::Role::QmlCode)
        .value("PreferencePath", module::Attribute::Role::PreferencePath)
        .value("InitOnlyPreferencePath", module::Attribute::Role::InitOnlyPreferencePath)
        .value("FontSize", module::Attribute::Role::FontSize)
        .value("FontFamily", module::Attribute::Role::FontFamily)
        .value("TextAlignment", module::Attribute::Role::TextAlignment)
        .value("TextContainerBox", module::Attribute::Role::TextContainerBox)
        .value("Colour", module::Attribute::Role::Colour)
        .value("HotkeyUuid", module::Attribute::Role::HotkeyUuid)
        .export_values();

    py_remote_session_file(m);
    py_playhead(m);
    py_plugin(m);
    py_ui(m);
    py_utility(m);
    py_link(m);

    p.py_init(m);
}
