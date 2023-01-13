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
    py::enum_<plugin_manager::PluginType>(m, "PluginType")
        .value("PT_CUSTOM", plugin_manager::PluginType::PT_CUSTOM)
        .value("PT_MEDIA_READER", plugin_manager::PluginType::PT_MEDIA_READER)
        .value("PT_MEDIA_HOOK", plugin_manager::PluginType::PT_MEDIA_HOOK)
        .value("PT_MEDIA_METADATA", plugin_manager::PluginType::PT_MEDIA_METADATA)
        .value("PT_COLOUR_MANAGEMENT", plugin_manager::PluginType::PT_COLOUR_MANAGEMENT)
        .value("PT_DATA_SOURCE", plugin_manager::PluginType::PT_DATA_SOURCE)
        .value("PT_UTILITY", plugin_manager::PluginType::PT_UTILITY)
        .export_values();
}