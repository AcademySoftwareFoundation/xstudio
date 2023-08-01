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

#include "xstudio/utility/remote_session_file.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_remote_session_file(py::module_ &m) {
    py::class_<utility::RemoteSessionFile>(
        m, "RemoteSessionFile", "RemoteSessionFile contains instance of a remote session.")
        .def(py::init<std::string>(), "Create remote session.")
        .def("path", &utility::RemoteSessionFile::path)
        .def("session_name", &utility::RemoteSessionFile::session_name)
        .def("filename", &utility::RemoteSessionFile::filename)
        .def("filepath", &utility::RemoteSessionFile::filepath)
        .def("host", &utility::RemoteSessionFile::host)
        .def("port", &utility::RemoteSessionFile::port)
        .def("pid", &utility::RemoteSessionFile::pid)
        .def("sync", &utility::RemoteSessionFile::sync);

    py::class_<utility::RemoteSessionManager>(m, "RemoteSessionManager")
        .def(py::init<std::string>())
        .def("first_api", &utility::RemoteSessionManager::first_api)
        .def("first_sync", &utility::RemoteSessionManager::first_sync)
        .def("find", &utility::RemoteSessionManager::find)
        .def("__len__", &utility::RemoteSessionManager::size)
        .def("empty", &utility::RemoteSessionManager::empty);
}