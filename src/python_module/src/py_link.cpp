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

#include "py_config.hpp"
#include "py_context.hpp"

using namespace xstudio;
namespace py = pybind11;

void py_link(py::module_ &m) {
    py::class_<caf::python::py_context>(m, "Link")
        .def(py::init<>())
        .def("connect_remote", &caf::python::py_context::connect_remote)
        .def("connect_local", &caf::python::py_context::connect_local)
        .def("disconnect", &caf::python::py_context::disconnect)
        .def("send", &caf::python::py_context::py_send, "Sends a message to an actor")
        .def(
            "request",
            &caf::python::py_context::py_request,
            "Sends a message to an actor, tracks response")
        .def(
            "send_exit",
            &caf::python::py_context::py_send_exit,
            "Sends exit message to an actor")
        .def(
            "dequeue_message",
            &caf::python::py_context::py_dequeue,
            "Receives the next message")
        .def(
            "dequeue_message_with_timeout",
            &caf::python::py_context::py_dequeue_with_timeout,
            "Receives the next message")
        .def(
            "tuple_from_message",
            &caf::python::py_context::py_tuple_from_wrapped_message,
            "Convert an XStudioExtensions.CafMessage to tuple")
        .def("spawn", &caf::python::py_context::py_spawn, "Spawn actor")
        .def("remote_spawn", &caf::python::py_context::py_remote_spawn, "Spawn remote actor")
        .def("join", &caf::python::py_context::py_join, "Join event group")
        .def("leave", &caf::python::py_context::py_leave, "Leave event group")
        .def("self", &caf::python::py_context::py_self, "Returns the global self handle")
        .def("remote", &caf::python::py_context::py_remote, "Returns the remote handle")
        .def("set_remote", &caf::python::py_context::py_set_remote, "Set the remote handle")
        .def("host", &caf::python::py_context::host, "Remote host")
        .def("port", &caf::python::py_context::port, "Remote port");
}