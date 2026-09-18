// SPDX-License-Identifier: Apache-2.0
#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wattributes"
#endif
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

#include "xstudio/utility/enums.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_utility(py::module_ &m) {
    py::enum_<utility::TimeSourceMode>(m, "TimeSourceMode")
        .value("TSM_FIXED", utility::TimeSourceMode::FIXED)
        .value("TSM_REMAPPED", utility::TimeSourceMode::REMAPPED)
        .value("TSM_DYNAMIC", utility::TimeSourceMode::DYNAMIC)
        .export_values();

    py::enum_<utility::NotificationType>(m, "NotificationType")
        .value("NT_UNKNOWN", utility::NotificationType::NT_UNKNOWN)
        .value("NT_INFO", utility::NotificationType::NT_INFO)
        .value("NT_WARN", utility::NotificationType::NT_WARN)
        .value("NT_PROCESSING", utility::NotificationType::NT_PROCESSING)
        .value("NT_PROGRESS_RANGE", utility::NotificationType::NT_PROGRESS_RANGE)
        .value("NT_PROGRESS_PERCENTAGE", utility::NotificationType::NT_PROGRESS_PERCENTAGE)
        .export_values();
}
