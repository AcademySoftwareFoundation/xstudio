// SPDX-License-Identifier: Apache-2.0
#pragma once

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

#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/uuid.hpp"

namespace caf::python {

namespace py = pybind11;

class py_config;

class py_context : public py_config {
  public:
    py_context(int argc = 0, char **argv = nullptr);

    virtual ~py_context() {
        // shutdown system
        disconnect();
    }

    std::optional<message> py_build_message(const py::args &xs);
    void py_send(const py::args &xs);
    void py_join(const py::args &xs);
    void py_leave(const py::args &xs);
    uint64_t py_request(const py::args &xs);
    void py_send_exit(const py::args &xs);
    py::tuple
    tuple_from_message(const message_id mid, const strong_actor_ptr sender, const message &msg);
    py::tuple py_tuple_from_wrapped_message(const py::args &xs);
    py::tuple py_dequeue();
    py::tuple py_dequeue_with_timeout(xstudio::utility::absolute_receive_timeout timeout);
    void py_run_xstudio_message_loop();
    void py_add_message_callback(const py::args &xs);
    void py_remove_message_callback(const py::args &xs);

    actor py_self() { return self_; }
    actor py_remote() { return remote_; }
    actor py_spawn(const py::args &xs);
    actor py_remote_spawn(const py::args &xs);

    void py_set_remote(actor act) { remote_ = act; }
    void disconnect();
    // xstudio::utility::version_atom version_atom() { return
    // xstudio::utility::version_atom_v;
    // }

    bool connect_remote(std::string host, uint16_t port);
    bool connect_local(caf::actor actor);
    std::string host() { return host_; }
    uint16_t port() { return port_; }

  public:
    std::string host_;
    uint16_t port_;

    void call_func();

    // Caf is fussy about how systems are created and passed around. The
    // python module might be running in an embedded python interpreter
    // in the xstudio application in which case we want to use the caf
    // system that already exists as part of the application. If this
    // python module is being run external to an xstudio application,
    // we need a local system. We create a local system anyway, but only
    // use it if there isn't already an application level. See
    // ActorSystemSingleton class for more.
    actor_system py_local_system_;

    // we use this reference, which either references py_local_system_
    // or another (application provided system) that was started before
    // the python module is instanced.
    actor_system &system_;

    scoped_actor self_;
    actor remote_;
    py::function my_func;
    std::thread my_thread;

    std::map<caf::actor_addr, std::vector<py::function>> message_handler_callbacks_;
    std::map<caf::actor_addr, py::function> message_conversion_function_;
    std::map<xstudio::utility::Uuid, py::function> delayed_callbacks_;
};
} // namespace caf::python
