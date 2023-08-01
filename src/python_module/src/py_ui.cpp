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

#include "xstudio/ui/keyboard.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_ui(py::module_ &m) {
    py::enum_<ui::KeyboardModifier>(m, "KeyboardModifier")
        .value("NoModifier", ui::KeyboardModifier::NoModifier)
        .value("ShiftModifier", ui::KeyboardModifier::ShiftModifier)
        .value("ControlModifier", ui::KeyboardModifier::ControlModifier)
        .value("MetaModifier", ui::KeyboardModifier::MetaModifier)
        .value("KeypadModifier", ui::KeyboardModifier::KeypadModifier)
        .value("GroupSwitchModifier", ui::KeyboardModifier::GroupSwitchModifier)
        .value("ZoomActionModifier", ui::KeyboardModifier::ZoomActionModifier)
        .value("PanActionModifier", ui::KeyboardModifier::PanActionModifier)
        .export_values();
}