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

#include "xstudio/playhead/playhead.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_playhead(py::module_ &m) {
    py::enum_<playhead::CompareMode>(m, "CompareMode")
        .value("CM_STRING", playhead::CompareMode::CM_STRING)
        .value("CM_AB", playhead::CompareMode::CM_AB)
        .export_values();

    py::enum_<playhead::LoopMode>(m, "LoopMode")
        .value("LM_PLAY_ONCE", playhead::LoopMode::LM_PLAY_ONCE)
        .value("LM_LOOP", playhead::LoopMode::LM_LOOP)
        .value("LM_PING_PONG", playhead::LoopMode::LM_PING_PONG)
        .export_values();

    py::enum_<media::MediaType>(m, "MediaType")
        .value("MT_IMAGE", media::MediaType::MT_IMAGE)
        .value("MT_AUDIO", media::MediaType::MT_AUDIO)
        .export_values();

    py::enum_<media::MediaStatus>(m, "MediaStatus")
        .value("MS_ONLINE", media::MediaStatus::MS_ONLINE)
        .value("MS_MISSING", media::MediaStatus::MS_MISSING)
        .value("MS_CORRUPT", media::MediaStatus::MS_CORRUPT)
        .value("MS_UNSUPPORTED", media::MediaStatus::MS_UNSUPPORTED)
        .value("MS_UNREADABLE", media::MediaStatus::MS_UNREADABLE)
        .export_values();

    py::enum_<playhead::OverflowMode>(m, "OverflowMode")
        .value("OM_FAIL", playhead::OverflowMode::OM_FAIL)
        .value("OM_NULL", playhead::OverflowMode::OM_NULL)
        .value("OM_HOLD", playhead::OverflowMode::OM_HOLD)
        .value("OM_LOOP", playhead::OverflowMode::OM_LOOP)
        .export_values();
}