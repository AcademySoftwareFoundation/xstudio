// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <caf/logger.hpp>
#include <caf/actor_registry.hpp>

#include <Python.h>

#include <pybind11/pybind11.h> // everything needed for embedding
#include <pybind11/embed.h>    // everything needed for embedding
#include <pybind11/stl.h>
#include <tuple>
#include <filesystem>

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

namespace fs = std::filesystem;
namespace py = pybind11;

#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC visibility push(hidden)
#else
#pragma warning(push, hidden)
#endif

namespace {
/* This actor prints python output to the shell until something else (like
the Python interpreter UI) starts up */
class PythonOutputToShellActor : public caf::event_based_actor {
  public:
    PythonOutputToShellActor(caf::actor_config &cfg, caf::actor py_event_group)
        : caf::event_based_actor(cfg), py_event_group_(py_event_group) {

        utility::join_broadcast(this, py_event_group_);

        behavior_.assign(
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {},

            [=](utility::event_atom,
                embedded_python::python_output_atom,
                const utility::Uuid &uuid,
                const std::tuple<std::string, std::string> &output) {
                // if we mail an event group with a bool it will send bnack the
                // number of subscribers it has
                mail(true)
                    .request(py_event_group_, infinite)
                    .then(
                        [=](int num_subscribers) {
                            if (num_subscribers > 1) {
                                // someone else is listenining to python output (probably)
                                // the Python interpreter UI. We can close
                                send_exit(this, caf::exit_reason::user_shutdown);
                            } else {
                                // print to logs
                                spdlog::info(
                                    "Python Output:\n\n{}{}\n",
                                    std::get<0>(output),
                                    std::get<1>(output));
                            }
                        },
                        [=](caf::error &err) {});
            },

            [=](utility::event_atom, utility::change_atom) {},

            [=](utility::event_atom, utility::name_atom, const std::string &) {});
    }

    caf::behavior behavior_;
    caf::actor py_event_group_;

    caf::behavior make_behavior() override { return behavior_; }
};

} // namespace

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
        act->mail(
               utility::event_atom_v, python_output_atom_v, uuid, std::make_tuple(stdo, stde))
            .send(grp);
    }
}

void py_print(const std::string &e) {
    // Note . on Windows, using py::arg or _a literal is causing seg-fault
    // but using a dict to set kwargs on py::print works fine.
    auto py_stderr = py::module::import("sys").attr("stderr").cast<py::object>();
#ifdef _WIN32
    py::dict d;
    d["file"] = py_stderr;
    py::print(e, d);
#else
    py::print(e, "file"_a = py_stderr);
#endif
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


#pragma GCC visibility pop


EmbeddedPythonActor::EmbeddedPythonActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
    : caf::blocking_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"]), this) {

    init();
}

EmbeddedPythonActor::EmbeddedPythonActor(caf::actor_config &cfg, const std::string &name)
    : caf::blocking_actor(cfg), base_(name, this) {

    init();
}

static void *s_run(void *self) {
    ((EmbeddedPythonActor *)self)->main_loop();
    return nullptr;
}
void EmbeddedPythonActor::act() {
#ifdef __apple__
    // Note: on MacOS, the default stack size for subthreads is 512K.
    // We are seeing bus error when xstudio api module is imported.
    // I suspect there is some problem with interdependency between
    // our .py files causing some recursion that eventually halts
    // on Linux but blows up on Mac with the smaller stack,
    // but not sure about that. It could just be that the stack
    // is not an appropriate size for running python interpreter
    // in.
    pthread_t _thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t stacksize;

    // increase stack to 10x default
    pthread_attr_setstacksize(&attr, 524288 * 10);

    pthread_create(&_thread, &attr, s_run, this);
    pthread_join(_thread, NULL);
#else
    main_loop();
#endif
}

void EmbeddedPythonActor::main_loop() {
    bool running = true;

    data_.set_origin(true);
    data_.bind_send_event_func([&](auto &&PH1, auto &&PH2) {
        auto event     = JsonStore(std::forward<decltype(PH1)>(PH1));
        auto undo_redo = std::forward<decltype(PH2)>(PH2);
        mail(utility::event_atom_v, json_store::sync_atom_v, data_uuid_, event)
            .send(event_group_);
    });

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(prefs.get_group(j));
        update_preferences(j);
    } catch (...) {
    }

    base_.setup();

    auto py_output_to_shell = spawn<PythonOutputToShellActor>(event_group_);

    // data_.insert_rows(
    //     0, 1,
    //     R"([{
    //         "name": "test",
    //         "path": "file:///u/al/.config/DNEG/xstudio/snippets/test.py"
    //     }])"_json
    // );

    {

        py::gil_scoped_release release;

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
                py::gil_scoped_acquire gil;

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
                        py_print(e.what());
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                    send_output(this, event_group_, out);
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                return result;
            },

            [=](connect_atom, const int port) -> bool {
                py::gil_scoped_acquire gil;

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
                        py_print(e.what());
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                    send_output(this, event_group_, out);
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                CAF_SET_LOGGER_SYS(&system());

                return result;
            },

            [=](session::export_atom) -> result<std::vector<std::string>> {
                // get otio supported export formats.
                auto result = std::vector<std::string>({"otio"});

                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");

                py::gil_scoped_acquire gil;

                std::string error;

                try {
                    PyStdErrOutStreamRedirect out{};
                    try {
                        // Import the OTIO Python module.
                        auto p_module =
                            PyObjectRef(PyImport_ImportModule("opentimelineio.adapters"));
                        auto p_suffixes_with_defined_adapters = PyObjectRef(
                            PyObject_GetAttrString(p_module, "suffixes_with_defined_adapters"));
                        auto p_suffixes_with_defined_adapters_args =
                            PyObjectRef(PyTuple_New(2));

                        PyTuple_SetItem(p_suffixes_with_defined_adapters_args, 0, Py_False);
                        PyTuple_SetItem(p_suffixes_with_defined_adapters_args, 1, Py_True);

                        auto p_suffixes = PyObjectRef(PyObject_CallObject(
                            p_suffixes_with_defined_adapters,
                            p_suffixes_with_defined_adapters_args));

                        PyObject *repr = PyObject_Repr(p_suffixes);
                        if (repr) {
                            PyObject *str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
                            if (str) {
                                const char *bytes = PyBytes_AS_STRING(str);

                                for (const auto &i : resplit(
                                         std::string(bytes), std::regex{"\\{'|'\\}|',\\s'"})) {
                                    if (not i.empty())
                                        result.push_back(i);
                                }
                                // something like .. {'otiod', 'edl', 'mb', 'ma', 'kdenlive',
                                // 'otio', 'otioz', 'ale', 'm3u8', 'xml', 'aaf', 'fcpxml',
                                // 'svg', 'xges', 'rv'}

                                Py_XDECREF(str);
                            }
                            Py_XDECREF(repr);
                        }

                        std::sort(result.begin(), result.end());
                        return result;
                    } catch (py::error_already_set &err) {
                        err.restore();
                        error = err.what();
                        py_print(error);
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        error = err.what();
                    }
                    // send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    error = err.what();
                }

                return make_error(xstudio_error::error, error);
            },

            // downgrade_manifest = otio.versioning.fetch_map("OTIO_CORE", "0.15.0")
            // otio.adapters.write_to_file(
            //     sc,
            //     "/path/to/file.otio",
            //     target_schema_versions=downgrade_manifest
            // )
            [=](session::export_atom,
                const std::string &otio_str,
                const caf::uri &upath,
                const std::string &type,
                const std::string &target_schema) -> result<bool> {
                // get otio supported export formats.
                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");

                py::gil_scoped_acquire gil;
                std::string error;

                try {
                    PyStdErrOutStreamRedirect out{};
                    // Import the OTIO Python module.
                    auto path = uri_to_posix_path(upath);

                    py::object read_from_string =
                        py::module_::import("opentimelineio.adapters").attr("read_from_string");
                    py::object write_to_file =
                        py::module_::import("opentimelineio.adapters").attr("write_to_file");

                    py::object p_timeline = read_from_string(otio_str);

                    if (target_schema.empty()) {
                        write_to_file(p_timeline, path);
                    } else {
                        py::object fetchmap =
                            py::module_::import("opentimelineio.versioning").attr("fetch_map");
                        py::object vmap = fetchmap("OTIO_CORE", target_schema);
                        py::object result =
                            write_to_file(p_timeline, path, "target_schema_versions"_a = vmap);
                    }

                    return true;
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    error = err.what();
                }

                return make_error(xstudio_error::error, error);
            },

            // import otio file return as otio xml string.
            // if already native format should be quick..
            [=](session::import_atom, const caf::uri &path) -> result<std::string> {
                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");

                py::gil_scoped_acquire gil;

                std::string error;

                try {
                    PyStdErrOutStreamRedirect out{};
                    try {
                        const std::string file_name_normalized = uri_to_posix_path(path);
                        py::object read_from_file =
                            py::module_::import("opentimelineio.adapters")
                                .attr("read_from_file");
                        py::object p_timeline = read_from_file(file_name_normalized);
                        py::str jsn           = p_timeline.attr("to_json_string")();
                        return jsn;
                    } catch (py::error_already_set &err) {
                        err.restore();
                        error = err.what();
                        py_print(error);
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        error = err.what();
                    }

                    // send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    error = err.what();
                }

                return make_error(xstudio_error::error, error);
            },

            [=](python_create_session_atom, const bool interactive) -> result<utility::Uuid> {
                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");

                py::gil_scoped_acquire gil;

                std::string error;

                try {
                    PyStdErrOutStreamRedirect out{};
                    auto session_uuid = base_.create_session(interactive);
                    send_output(this, event_group_, out, session_uuid);
                    return session_uuid;
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
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

                py::gil_scoped_acquire gil;

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
                        py_print(error);
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        error = err.what();
                    }

                    send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
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

                py::gil_scoped_acquire gil;

                try {
                    return JsonStore(base_.eval(pystring, locals));
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
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

                py::gil_scoped_acquire gil;

                try {
                    PyStdErrOutStreamRedirect out{};
                    try {
                        base_.eval_file(uri_to_posix_path(pyfile));
                    } catch (py::error_already_set &e) {
                        e.restore();
                        py_print(e.what());
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }

                    send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](python_eval_locals_atom, const std::string &pystring) -> result<JsonStore> {
                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");
                py::gil_scoped_acquire gil;

                try {
                    return JsonStore(base_.eval_locals(pystring));
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
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

                py::gil_scoped_acquire gil;

                try {
                    return JsonStore(base_.eval_locals(pystring, locals));
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
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

                py::gil_scoped_acquire gil;

                try {
                    PyStdErrOutStreamRedirect out{};
                    try {
                        base_.exec(pystring);
                    } catch (py::error_already_set &e) {
                        e.restore();
                        py_print(e.what());
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }

                    send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", e.what());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](python_remove_session_atom, const utility::Uuid &uuid) -> result<bool> {
                if (not base_.enabled())
                    return make_error(xstudio_error::error, "EmbeddedPython disabled");
                py::gil_scoped_acquire gil;

                try {
                    return base_.remove_session(uuid);
                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
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
                py::gil_scoped_acquire gil;

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
                        py_print(error);
                        // get console back to input mode..
                        auto result = base_.input_session(uuid, "");
                        send_output(this, event_group_, out, uuid);
                        return result;
                        // spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                        // return make_error(xstudio_error::error, err.what());
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
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
                py::gil_scoped_acquire gil;

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
                        py_print(error);
                        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, "Python error", error);
                        return make_error(xstudio_error::error, err.what());
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    send_output(this, event_group_, out, uuid);
                } catch (py::error_already_set &err) {
                    err.restore();
                    error = err.what();
                    py_print(error);
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

            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {
                if (uuid == data_uuid_)
                    data_.process_event(event, true, false, false);
            },

            [=](json_store::sync_atom) -> UuidVector { return UuidVector({data_uuid_}); },

            [=](media::rescan_atom) { refresh_snippets(snippet_paths_); },

            [=](save_atom, const caf::uri &path, const std::string &content) -> result<bool> {
                auto result = true;
                try {
                    std::ofstream myfile;
                    myfile.open(uri_to_posix_path(path));
                    myfile << content << std::endl;
                    myfile.close();
                    refresh_snippets(snippet_paths_);
                    result = true;
                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
                return result;
            },

            [=](json_store::sync_atom, const Uuid &uuid) -> JsonStore {
                if (uuid == data_uuid_)
                    return data_.as_json();

                return JsonStore();
            },

            [=](json_store::update_atom,
                const JsonStore & /*change*/,
                const std::string & /*path*/,
                const JsonStore &full) {
                return mail(json_store::update_atom_v, full)
                    .delegate(actor_cast<caf::actor>(this));
            },

            [=](json_store::update_atom, const JsonStore &j) mutable {
                try {
                    update_preferences(j);
                } catch (...) {
                }
            },

            [=](caf::actor caller) {
                // call and respond used by EmbeddedPythonUI so the Python
                // setup does not block the startup of the GUI
                anon_mail(actor_cast<caf::actor>(this)).send(caller);
            },

            [=](bool) {},

            [=](embedded_python::python_exec_atom,
                const std::string &plugin_name,
                const std::string &method_name,
                const utility::JsonStore &packed_args) -> result<utility::JsonStore> {
                py::gil_scoped_acquire gil;

                try {
                    auto g = py::globals();
                    if (g.contains(py::str("XSTUDIO"))) {

                        py::object xstudio_link = g["XSTUDIO"];
                        py::object plugin =
                            xstudio_link.attr("get_named_python_plugin_instance")(plugin_name);
                        py::object result;
                        py::object json_py_module = py::module_::import("json");

                        // convert the json args to python args (dict or array)
                        py::object args_list = json_py_module.attr("loads")(packed_args.dump());

                        if (packed_args.is_object() && !packed_args.is_array()) {

                            auto p = packed_args.cbegin();
                            while (p != packed_args.cend()) {
                                if (p.value().is_string()) {
                                    const std::string v = p.value().get<std::string>();
                                    // could be an actor as a string. Try to convert to actual actor here. This
                                    // saves us doing string_to_actor in Python code, which currently fails
                                    // on MacOS due to reasons not understood. But ... it's pretty ugly as 
                                    // we are trying EVERY string. Need a way to IDENTIFY actor as string before
                                    // running actor_from_string.
                                    caf::actor actor = utility::actor_from_string(system(), v);
                                    if (actor) {
                                        py::str s(p.key());
                                        args_list[s] = py::cast(actor);
                                    }
                                }
                                p++;
                            }
                            result = plugin.attr(method_name.c_str())(**args_list);

                        } else if (packed_args.is_array()) {
                            // if args were provided as an array, separate out into
                            // unpacked args
                            py::tuple args;
                            args  = py::tuple(py::len(args_list));
                            int j = 0;
                            for (auto i = args_list.begin(); i != args_list.end(); ++i) {
                                (*i).inc_ref();
                                PyTuple_SetItem(args.ptr(), static_cast<int>(j++), (*i).ptr());
                            }
                            result = plugin.attr(method_name.c_str())(*args);
                        } else {
                            result = plugin.attr(method_name.c_str())(**args_list);
                        }
                        return result.cast<utility::JsonStore>();
                    } else {
                        throw std::runtime_error("Couln't import XSTUDIO module.");
                    }

                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    return make_error(xstudio_error::error, e.what());

                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
            },

            [=](embedded_python::python_exec_atom,
                const utility::Uuid &plugin_uuid,
                const std::string method_name,
                const utility::JsonStore &packed_args) -> result<utility::JsonStore> {
                py::gil_scoped_acquire gil;

                try {
                    auto g = py::globals();
                    if (g.contains(py::str("XSTUDIO"))) {
                        py::object xstudio_link = g["XSTUDIO"];
                        py::object plugin =
                            xstudio_link.attr("get_plugin_instance")(plugin_uuid);
                        py::tuple args;
                        if (packed_args.is_array()) {
                            py::object json_py_module = py::module_::import("json");
                            py::object args_list =
                                json_py_module.attr("loads")(packed_args.dump());
                            args  = py::tuple(py::len(args_list));
                            int j = 0;
                            for (auto i = args_list.begin(); i != args_list.end(); ++i) {
                                (*i).inc_ref();
                                PyTuple_SetItem(args.ptr(), static_cast<int>(j++), (*i).ptr());
                            }
                        }
                        py::object result = plugin.attr(method_name.c_str())(*args);
                        return result.cast<utility::JsonStore>();
                    } else {
                        throw std::runtime_error("Couln't import XSTUDIO module.");
                    }

                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    return make_error(xstudio_error::error, e.what());

                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
            },

            [=](ui::viewport::hud_settings_atom,
                std::vector<caf::actor> media_actors) -> result<utility::JsonStore> {
                // this message handler is used by the offscreen viewport to retrieve
                // display data for Python HUD overlay plugins. We pass the HUD Plugins
                // the full set of on-screen media actors. They return Json data for
                // each media item, which xstudio then uses to initialise special
                // QML properties that the HUD Plugin QML items use at draw time.
                py::gil_scoped_acquire gil;
                try {
                    auto g = py::globals();
                    if (g.contains(py::str("XSTUDIO"))) {
                        py::object xstudio_link = g["XSTUDIO"];
                        py::list mact;
                        for (auto &m : media_actors) {
                            if (m)
                                mact.append(m);
                            else
                                mact.append(py::none());
                        }
                        py::args args = py::make_tuple(mact);
                        py::object result =
                            xstudio_link.attr("hud_plugins_onscreen_data")(*args);
                        return result.cast<utility::JsonStore>();

                    } else {
                        throw std::runtime_error("Couln't import XSTUDIO module.");
                    }

                } catch (py::error_already_set &e) {
                    e.restore();
                    py_print(e.what());
                    return make_error(xstudio_error::error, e.what());

                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
            },

            [&](exit_msg &em) {
                if (em.reason) {
                    if (base_.enabled()) {
                        py::gil_scoped_acquire gil;
                        base_.disconnect();
                    }
                    fail_state(std::move(em.reason));
                    running = false;
                    system().registry().erase(embedded_python_registry);
                }
            });
    }

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

    mail(broadcast::join_broadcast_atom_v, caf::actor_cast<caf::actor>(this))
        .send(broadcast_group);
}

void EmbeddedPythonActor::leave_broadcast(caf::actor broadcast_group) {

    mail(broadcast::leave_broadcast_atom_v, caf::actor_cast<caf::actor>(this))
        .send(broadcast_group);
}

void EmbeddedPythonActor::join_broadcast(
    caf::actor plugin_base_actor, const std::string &plugin_name) {

    auto plugin_manager = system().registry().template get<caf::actor>(plugin_manager_registry);

    mail(plugin_manager::spawn_plugin_base_atom_v, plugin_name)
        .request(plugin_manager, infinite)
        .receive(
            [=](caf::actor plugin_actor) {
                mail(broadcast::join_broadcast_atom_v, caf::actor_cast<caf::actor>(this))
                    .send(plugin_actor);
            },
            [=](caf::error &e) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
            });
}


void EmbeddedPythonActor::update_preferences(const utility::JsonStore &j) {
    auto paths = preference_value<JsonStore>(j, "/core/python/snippets/path");
    auto snips = std::vector<caf::uri>();

    for (const auto &path : paths)
        snips.emplace_back(posix_path_to_uri(expand_envvars(path)));

    if (snips != snippet_paths_) {
        refresh_snippets(snips);
        snippet_paths_ = snips;
    }
}


void EmbeddedPythonActor::refresh_snippets(const std::vector<caf::uri> &paths) {
    auto data = R"([])"_json;

    for (const auto &i : paths) {
        for (const auto &j : refresh_snippet(fs::path(uri_to_posix_path(i)), "Snippets"))
            data.push_back(j);
    }

    // sort results..
    std::sort(
        data.begin(), data.end(), [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
            return std::string(
                       a.at("menu_path").get<std::string>() + a.at("name").get<std::string>()) <
                   std::string(
                       b.at("menu_path").get<std::string>() + b.at("name").get<std::string>());
        });

    data_.reset_data(data);
}

nlohmann::json EmbeddedPythonActor::refresh_snippet(
    const std::filesystem::path &path,
    const std::string &menu_path_,
    const std::string &snippet_type) {
    auto result = R"([])"_json;

    if (!fs::is_directory(path))
        return result;

    try {
        for (const auto &entry : fs::directory_iterator(path)) {
            if (fs::is_regular_file(entry.status()) and entry.path().extension() == ".py") {
                auto item =
                    R"({"id": null, "type": "script", "snippet_type": null, "name": null, "script_path":null})"_json;
                item["id"]           = Uuid::generate();
                item["snippet_type"] = snippet_type.empty() ? "Application" : snippet_type;
                item["name"]         = title_case(entry.path().stem().string());
                item["script_path"]  = to_string(posix_path_to_uri(entry.path().string()));
                item["menu_path"]    = menu_path_;
                result.push_back(item);
            } else if (fs::is_directory(entry.status())) {
                auto change_type = snippet_type;
                auto menu_path =
                    menu_path_.empty()
                        ? title_case(entry.path().stem().string())
                        : menu_path_ + "|" + title_case(entry.path().stem().string());

                if (change_type.empty()) {
                    if (entry.path().stem() == "media") {
                        change_type = "Media";
                        menu_path   = "";
                    } else if (entry.path().stem() == "playlist") {
                        change_type = "Playlist";
                        menu_path   = "";
                    } else if (entry.path().stem() == "sequence") {
                        change_type = "Sequence";
                        menu_path   = "";
                    } else if (entry.path().stem() == "track") {
                        change_type = "Track";
                        menu_path   = "";
                    } else if (entry.path().stem() == "clip") {
                        change_type = "Clip";
                        menu_path   = "";
                    } else {
                        change_type = "Application";
                    }
                }

                for (const auto &i : refresh_snippet(entry.path(), menu_path, change_type))
                    result.push_back(i);

                // auto item = R"({"id": null, "type": "script","name": null,
                // "children":[]})"_json; item["id"] = Uuid::generate(); item["name"] =
                // entry.path().stem().string(); item["children"] =
                // refresh_snippet(entry.path()); result.push_back(item);
            }
        }

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}
void EmbeddedPythonActor::delayed_callback(utility::Uuid &cb_id, const int microseconds_delay) {
    anon_mail(cb_id)
        .delay(std::chrono::microseconds(microseconds_delay))
        .send(caf::actor_cast<caf::actor>(this), weak_ref);
}
