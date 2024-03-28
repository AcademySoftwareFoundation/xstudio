// SPDX-License-Identifier: Apache-2.0
#pragma GCC diagnostic ignored "-Wattributes"

#include "py_opaque.hpp"

// CAF_PUSH_WARNINGS
#include "pybind11_json/pybind11_json.hpp"
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
// CAF_POP_WARNINGS

#include "xstudio/plugin_manager/plugin_manager.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_plugin(py::module_ &m) {
    py::enum_<plugin_manager::PluginFlags>(m, "PluginFlags")
        .value("PF_CUSTOM", plugin_manager::PluginFlags::PF_CUSTOM)
        .value("PF_MEDIA_READER", plugin_manager::PluginFlags::PF_MEDIA_READER)
        .value("PF_MEDIA_HOOK", plugin_manager::PluginFlags::PF_MEDIA_HOOK)
        .value("PF_MEDIA_METADATA", plugin_manager::PluginFlags::PF_MEDIA_METADATA)
        .value("PF_COLOUR_MANAGEMENT", plugin_manager::PluginFlags::PF_COLOUR_MANAGEMENT)
        .value("PF_COLOUR_OPERATION", plugin_manager::PluginFlags::PF_COLOUR_OPERATION)
        .value("PF_DATA_SOURCE", plugin_manager::PluginFlags::PF_DATA_SOURCE)
        .value("PF_VIEWPORT_OVERLAY", plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY)
        .value("PF_HEAD_UP_DISPLAY", plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY)
        .value("PF_UTILITY", plugin_manager::PluginFlags::PF_UTILITY)
        .value("PF_CONFORM", plugin_manager::PluginFlags::PF_CONFORM)
        .export_values();
}