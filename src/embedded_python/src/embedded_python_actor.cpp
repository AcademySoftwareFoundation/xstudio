// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/logger.hpp>

#include <pybind11/pybind11.h> // everything needed for embedding
#include <pybind11/embed.h>    // everything needed for embedding
#include <pybind11/stl.h>
#include <tuple>

#ifdef BUILD_OTIO
#include <opentimelineio/timeline.h>
#endif

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/embedded_python/embedded_python_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::embedded_python;
using namespace nlohmann;
using namespace caf;
using namespace pybind11::literals;

namespace py   = pybind11;

#ifdef BUILD_OTIO
namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;
#endif


#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC visibility push(hidden)
#else
#pragma warning(push, hidden)
#endif

class PyStdErrOutStreamRedirect {
  public:
    PyStdErrOutStreamRedirect() {
        auto sysm      = py::module::import("sys");
        stdout_        = sysm.attr("stdout");
        stderr_        = sysm.attr("stderr");
        auto stringio  = py::module::import("io").attr("StringIO");
        stdout_buffer_ = stringio(); // Other filelike object can be used here as well, such as
                                     // objects created by pybind11
        stderr_buffer_      = stringio();
        sysm.attr("stdout") = stdout_buffer_;
        sysm.attr("stderr") = stderr_buffer_;
    }
    std::string stdout_string() {
        stdout_buffer_.attr("seek")(0);
        return py::str(stdout_buffer_.attr("read")());
    }
    std::string stderr_string() {
        stderr_buffer_.attr("seek")(0);
        return py::str(stderr_buffer_.attr("read")());
    }
    ~PyStdErrOutStreamRedirect() {
        auto sysm           = py::module::import("sys");
        sysm.attr("stdout") = stdout_;
        sysm.attr("stderr") = stderr_;
    }

  private:
    py::object stdout_;
    py::object stderr_;
    py::object stdout_buffer_;
    py::object stderr_buffer_;
};

void send_output(
    caf::blocking_actor *act,
    caf::actor &grp,
    PyStdErrOutStreamRedirect &output,
    const utility::Uuid &uuid = utility::Uuid()) {

    auto stdo = output.stdout_string();
    auto stde = output.stderr_string();

    if (not stdo.empty() or not stde.empty()) {
        act->send(
            grp,
            utility::event_atom_v,
            python_output_atom_v,
            uuid,
            std::make_tuple(stdo, stde));
    }
}

class PyObjectRef {
  public:
    PyObjectRef(PyObject *obj) : obj_(obj) {
        if (!obj_) {
            throw std::runtime_error("Python error");
        }
    }

    ~PyObjectRef() { Py_XDECREF(obj_); }

    PyObject *obj_ = nullptr;

    operator PyObject *() const { return obj_; }
};

#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC visibility pop
#else
#pragma warning(pop)
#endif

EmbeddedPythonActor::EmbeddedPythonActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::blocking_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"]), this) {

    init();
}

EmbeddedPythonActor::EmbeddedPythonActor(caf::actor_config &cfg, const std::string &name)
    : caf::blocking_actor(cfg), base_(name, this) {

    init();
}

void EmbeddedPythonActor::act() {
    bool running = true;

    base_.setup();

    receive_while(running)(
        base_.make_set_name_handler(event_group_, this),
        base_.make_get_name_handler(),
        base_.make_last_changed_getter(),
        base_.make_last_changed_setter(event_group_, this),
        base_.make_last_changed_event_handler(event_group_, this),
        base_.make_get_uuid_handler(),
        base_.make_get_type_handler(),
        make_get_event_group_handler(event_group_),
        base_.make_get_detail_handler(this, event_group_),
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](connect_atom, caf::actor actor) -> bool {
            // spdlog::warn("connect actor 1");
            bool result = false;
            try {
                PyStdErrOutStreamRedirect out;
                try {
                    if (actor) {
                        // spdlog::warn("connect actor 2");
                        result = base_.connect(actor);
                    } else {
                        base_.disconnect();
                    }
                } catch (py::error_already_set &e) {
                    e.restore();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(e.what(), "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
                send_output(this, event_group_, out);
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
        },

        [=](connect_atom, const int port) -> bool {
            bool result = false;
            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    base_.setup();
                    if (port) {
                        result = base_.connect(port);
                    } else {
                        base_.disconnect();
                    }
                } catch (py::error_already_set &e) {
                    e.restore();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(e.what(), "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
                send_output(this, event_group_, out);
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            CAF_SET_LOGGER_SYS(&system());

            return result;
        },

#ifdef BUILD_OTIO

        // import otio file return as otio xml string.
        // if already native format should be quick..
        [=](session::import_atom, const caf::uri &path) -> result<std::string> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            std::string error;

            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    otio::ErrorStatus error_status;

                    auto p_module =
                        PyObjectRef(PyImport_ImportModule("opentimelineio.adapters"));
                    // Read the timeline into Python.
                    auto p_read_from_file =
                        PyObjectRef(PyObject_GetAttrString(p_module, "read_from_file"));
                    auto p_read_from_file_args = PyObjectRef(PyTuple_New(1));


                    const std::string file_name_normalized = uri_to_posix_path(path);
                    auto p_read_from_file_arg              = PyUnicode_FromStringAndSize(
                        file_name_normalized.c_str(), file_name_normalized.size());
                    if (!p_read_from_file_arg) {
                        throw std::runtime_error("cannot create arg");
                    }
                    PyTuple_SetItem(p_read_from_file_args, 0, p_read_from_file_arg);
                    auto p_timeline = PyObjectRef(
                        PyObject_CallObject(p_read_from_file, p_read_from_file_args));

                    // Convert the Python timeline into a string and use that to create a C++
                    // timeline.
                    auto p_to_json_string =
                        PyObjectRef(PyObject_GetAttrString(p_timeline, "to_json_string"));
                    auto p_json_string =
                        PyObjectRef(PyObject_CallObject(p_to_json_string, nullptr));

                    return PyUnicode_AsUTF8AndSize(p_json_string, nullptr);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(error, "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    error = err.what();
                }

                // send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &err) {
                err.restore();
                error          = err.what();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(error, "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                error = err.what();
            }

            return make_error(xstudio_error::error, error);
        },

#endif BUILD_OTIO

        [=](python_create_session_atom, const bool interactive) -> result<utility::Uuid> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            std::string error;

            try {
                PyStdErrOutStreamRedirect out{};
                auto session_uuid = base_.create_session(interactive);
                send_output(this, event_group_, out, session_uuid);
                return session_uuid;
            } catch (py::error_already_set &err) {
                err.restore();
                error          = err.what();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(error, "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                return make_error(xstudio_error::error, err.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_eval_atom,
            const std::string &pystring,
            const utility::Uuid &uuid) -> result<JsonStore> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            std::string error;

            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    auto r = base_.eval(pystring);
                    send_output(this, event_group_, out, uuid);
                    return JsonStore(r);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(error, "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    error = err.what();
                }

                send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &err) {
                err.restore();
                error          = err.what();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(error, "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                error = err.what();
            }
            return make_error(xstudio_error::error, error);
        },

        [=](python_eval_atom,
            const std::string &pystring,
            const JsonStore &locals) -> result<JsonStore> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            try {
                return JsonStore(base_.eval(pystring, locals));
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                return make_error(xstudio_error::error, e.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_eval_file_atom, const caf::uri &pyfile, const utility::Uuid &uuid) {
            if (not base_.enabled())
                return;

            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    base_.eval_file(uri_to_posix_path(pyfile));
                } catch (py::error_already_set &e) {
                    e.restore();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(e.what(), "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](python_eval_locals_atom, const std::string &pystring) -> result<JsonStore> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            try {
                return JsonStore(base_.eval_locals(pystring));
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                return make_error(xstudio_error::error, e.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_eval_locals_atom,
            const std::string &pystring,
            const JsonStore &locals) -> result<JsonStore> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");
            try {
                return JsonStore(base_.eval_locals(pystring, locals));
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                return make_error(xstudio_error::error, e.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_exec_atom, const std::string &pystring, const utility::Uuid &uuid) {
            if (not base_.enabled())
                return;
            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    base_.exec(pystring);
                } catch (py::error_already_set &e) {
                    e.restore();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(e.what(), "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        },

        [=](python_remove_session_atom, const utility::Uuid &uuid) -> result<bool> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            try {
                return base_.remove_session(uuid);
            } catch (py::error_already_set &e) {
                e.restore();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(e.what(), "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                return make_error(xstudio_error::error, e.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_session_input_atom,
            const utility::Uuid &uuid,
            const std::string &input) -> result<bool> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");

            std::string error;

            try {
                PyStdErrOutStreamRedirect out{};
                try {
                    auto result = base_.input_session(uuid, input);
                    send_output(this, event_group_, out, uuid);
                    return result;
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(error, "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                    return make_error(xstudio_error::error, err.what());
                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
                send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &err) {
                err.restore();
                error          = err.what();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(error, "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                return make_error(xstudio_error::error, err.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](python_session_interrupt_atom, const utility::Uuid &uuid) -> result<bool> {
            if (not base_.enabled())
                return make_error(xstudio_error::error, "EmbeddedPython disabled");
            std::string error;

            try {

                PyStdErrOutStreamRedirect out{};

                try {
                    auto result = base_.input_ctrl_c_session(uuid);
                    send_output(this, event_group_, out, uuid);
                    return result;
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    auto py_stderr =
                        py::module::import("sys").attr("stderr").cast<py::object>();
                    py::print(error, "file"_a = py_stderr);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                    return make_error(xstudio_error::error, err.what());
                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
                send_output(this, event_group_, out, uuid);
            } catch (py::error_already_set &err) {
                err.restore();
                error          = err.what();
                auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
                py::print(error, "file"_a = py_stderr);
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                return make_error(xstudio_error::error, err.what());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return make_error(xstudio_error::error, "Unknown error");
        },

        [=](utility::serialise_atom) -> result<JsonStore> {
            JsonStore jsn;
            jsn["base"] = base_.serialise();
            return result<JsonStore>(jsn);
        },

        [&](exit_msg &em) {
            if (em.reason) {
                if (base_.enabled())
                    base_.disconnect();
                fail_state(std::move(em.reason));
                running = false;
            }
        },
        others >> [=](message &msg) -> skippable_result {
            auto sender = caf::actor_cast<caf::actor>(current_sender());
            base_.push_caf_message_to_py_callbacks(sender, msg);
            return message{};

            // return sec::unexpected_message;
        });
    base_.finalize();
}

void EmbeddedPythonActor::init() {
    spdlog::debug("Created EmbeddedPythonActor {}", base_.name());
    print_on_exit(this, "EmbeddedPythonActor");

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    system().registry().put(embedded_python_registry, this);
}

void EmbeddedPythonActor::join_broadcast(caf::actor broadcast_group) {

    send(broadcast_group, broadcast::join_broadcast_atom_v, caf::actor_cast<caf::actor>(this));
}

void EmbeddedPythonActor::join_broadcast(
    caf::actor plugin_base_actor, const std::string &plugin_name) {

    auto plugin_manager = system().registry().template get<caf::actor>(plugin_manager_registry);

    request(plugin_manager, infinite, plugin_manager::spawn_plugin_base_atom_v, plugin_name)
        .receive(
            [=](caf::actor plugin_actor) {
                send(
                    plugin_actor,
                    broadcast::join_broadcast_atom_v,
                    caf::actor_cast<caf::actor>(this));
            },
            [=](caf::error &e) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
            });
}
