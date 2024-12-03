// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <Python.h>
#define PYBIND11_DETAILED_ERROR_MESSAGES

#include <pybind11/embed.h> // everything needed for embedding
#include <pybind11_json/pybind11_json.hpp>

#include "xstudio/embedded_python/embedded_python.hpp"
#include "xstudio/embedded_python/embedded_python_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::embedded_python;
using namespace xstudio::utility;
using namespace pybind11::literals;
namespace py = pybind11;

EmbeddedPython::EmbeddedPython(const std::string &name, EmbeddedPythonActor *parent)
    : Container(name, "EmbeddedPython"), parent_(parent) {
    s_instance_ = this;
}

EmbeddedPython::EmbeddedPython(const JsonStore &jsn, EmbeddedPythonActor *parent)
    : Container(static_cast<utility::JsonStore>(jsn["container"])), parent_(parent) {
    s_instance_ = this;
}

void EmbeddedPython::setup() {
    try {
        if (not Py_IsInitialized()) {
            spdlog::debug("py::initialize_interpreter");
            py::initialize_interpreter();
            inited_ = true;
        }

        if (Py_IsInitialized() and not setup_) {
            exec(R"(
import xstudio
from xstudio.connection import Connection
from xstudio.core import *
from xstudio.core.interactive import InteractiveSession
import signal
try:
    signal.signal(signal.SIGINT, signal.SIG_DFL)
except Exception as e:
    print (e)
xstudio_sessions = {}
        )");
            setup_ = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} Failed to setup API : {} ", __PRETTY_FUNCTION__, err.what());
    }
}

void EmbeddedPython::finalize() {
    try {
        message_handler_callbacks_.clear();
        message_conversion_function_.clear();
        if (Py_IsInitialized() and inited_ and PyGILState_Check()) {
            py::finalize_interpreter();
            inited_ = false;
            setup_  = false;
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} Failed to finalize_interpreter : {} ", __PRETTY_FUNCTION__, err.what());
    }
}


EmbeddedPython::~EmbeddedPython() { finalize(); }

void EmbeddedPython::disconnect() {

    message_handler_callbacks_.clear();
    try {
        exec(R"(
if 'XSTUDIO' in globals():
    XSTUDIO.disconnect()
    del XSTUDIO
)");
    } catch (const std::exception &err) {
        spdlog::warn("{} Failed to disconnect API : {} ", __PRETTY_FUNCTION__, err.what());
    }
}

bool EmbeddedPython::connect(const int port) {
    try {
        exec(R"(
XSTUDIO = Connection(
    debug=False
)
)");
        exec(
            R"(
XSTUDIO.connect_remote(
	"127.0.0.1",
	)" + std::to_string(port) +
            R"(
)
)");

        exec("XSTUDIO.load_python_plugins()");

    } catch (const std::exception &err) {
        spdlog::warn("{} Failed to init API : {} ", __PRETTY_FUNCTION__, err.what());
        return false;
    }
    return true;
}

bool EmbeddedPython::connect(caf::actor actor) {
    try {
        // spdlog::warn("connect 0");
        // spdlog::warn("connect 1");
        auto pyint = py::cast(1);
        // spdlog::warn("connect 1.5");
        auto pyactor = py::cast(actor);
        // spdlog::warn("connect 2.5");
        auto local = py::dict("actor"_a = pyactor);

        // spdlog::warn("connect 2");
        exec(R"(
XSTUDIO = Connection(
    debug=False
)
)");
        // spdlog::warn("connect 3");
        py::eval("XSTUDIO.connect_local(actor)", py::globals(), local);
        exec("XSTUDIO.load_python_plugins()");

    } catch (const py::cast_error &err) {
        spdlog::warn("{} Failed to init API : {} ", __PRETTY_FUNCTION__, err.what());
        return false;
    } catch (const std::exception &err) {
        spdlog::warn("{} Failed to init API : {} ", __PRETTY_FUNCTION__, err.what());
        return false;
    }
    return true;
}


void EmbeddedPython::hello() const { py::print("Hello, World!"); }

void EmbeddedPython::exec(const std::string &pystring) const { py::exec(pystring); }
void EmbeddedPython::eval_file(const std::string &pyfile) const { py::eval_file(pyfile); }

nlohmann::json EmbeddedPython::eval(const std::string &pystring) const {
    return py::eval(pystring);
}

nlohmann::json
EmbeddedPython::eval(const std::string &pystring, const nlohmann::json &locals) const {
    py::object pylocal = locals;

    return py::eval(pystring, py::globals(), pylocal);
}

nlohmann::json EmbeddedPython::eval_locals(const std::string &pystring) const {
    py::object pylocal;
    py::exec(pystring, py::globals(), pylocal);
    return pylocal;
}

nlohmann::json
EmbeddedPython::eval_locals(const std::string &pystring, const nlohmann::json &locals) const {
    py::object pylocal = locals;
    py::exec(pystring, py::globals(), pylocal);
    return pylocal;
}

JsonStore EmbeddedPython::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();

    return jsn;
}

utility::Uuid EmbeddedPython::create_session(const bool interactive) {
    if (interactive) {
        auto u           = utility::Uuid::generate();
        py::object scope = py::module::import("__main__").attr("__dict__");

        py::exec(
            "xstudio_sessions['" + to_string(u) + "'] = InteractiveSession(globals())", scope);
        py::exec(
            "xstudio_sessions['" + to_string(u) + "'].interact('xStudio','goodbye')", scope);
        sessions_.insert(u);
        return u;
    }

    return utility::Uuid();
}

bool EmbeddedPython::remove_session(const utility::Uuid &session_uuid) {
    if (sessions_.count(session_uuid)) {
        sessions_.erase(session_uuid);
        py::object scope = py::module::import("__main__").attr("__dict__");
        py::exec("del xstudio_sessions['" + to_string(session_uuid) + "']", scope);
        return true;
    }

    return false;
}

bool EmbeddedPython::input_session(
    const utility::Uuid &session_uuid, const std::string &input) {
    if (sessions_.count(session_uuid)) {
        std::string clean_input = replace_all(input, "'", R"(\')");
        py::object scope        = py::module::import("__main__").attr("__dict__");
        py::exec(
            "xstudio_sessions['" + to_string(session_uuid) + "'].interact_more('" +
                rtrim(clean_input) + "')",
            scope);
        return true;
    }

    return false;
}

bool EmbeddedPython::input_ctrl_c_session(const utility::Uuid &session_uuid) {
    if (sessions_.count(session_uuid)) {
        py::object scope = py::module::import("__main__").attr("__dict__");
        py::exec(
            "xstudio_sessions['" + to_string(session_uuid) + "'].keyboard_interrupt()", scope);
        return true;
    }

    return false;
}

void EmbeddedPython::add_message_callback(const py::tuple &cb_particulars) {

    try {

        if (cb_particulars.size() >= 2 || cb_particulars.size() <= 4) {

            auto i            = cb_particulars.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

            if (cb_particulars.size() >= 3) {
                // Let's say we want to receive event messages from a MediaActor.
                // We need to call 'join_broadcast' with the event_group actor
                // belonging to the MediaActor. However, in
                // push_caf_message_to_py_callbacks we need to resolve who sent
                // the message in order to call the correct python callback.
                // The 'sender' when a message comes via an event_group actor is
                // the owner (in this case the MediaActor). So here, the 3rd
                // argument is the owner of the event group and we use this to
                // set the actor address that we register the callback against.
                i++;
                auto parent_actor = (*i).cast<caf::actor>();
                addr              = caf::actor_cast<caf::actor_addr>(parent_actor);
            }

            if (cb_particulars.size() == 4) {
                // as an optional 4th argument, we take a function that will convert
                // a caf message into a tuple. This conversion function is provided
                // by the Link class in the xstuduio python module which we can't
                // get to from the C++ side here, hence we take a python function
                // that lets us run the conversion. Awkward.
                i++;
                auto convert_function = (*i).cast<py::function>();
                if (convert_function) {
                    message_conversion_function_[addr] = convert_function;
                }
            }

            const auto p = message_handler_callbacks_.find(addr);
            if (p != message_handler_callbacks_.end()) {
                auto q = p->second.begin();
                while (q != p->second.end()) {
                    if ((*q).is(callback_func)) {
                        return;
                    }
                    q++;
                }
            }

            message_handler_callbacks_[addr].push_back(callback_func);
            parent_->join_broadcast(remote_actor);

        } else {
            throw std::runtime_error(
                "Set message callback expecting tuple of size 2 or 3 "
                "(remote_event_group_actor, callack_func, (optional: parent_actor)).");
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void EmbeddedPython::remove_message_callback(const py::tuple &cb_particulars) {

    try {

        if (cb_particulars.size() == 2) {

            auto i            = cb_particulars.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

            const auto p = message_handler_callbacks_.find(addr);
            if (p != message_handler_callbacks_.end()) {
                auto q = p->second.begin();
                while (q != p->second.end()) {
                    if ((*q).is(callback_func)) {
                        q = p->second.erase(q);
                    } else {
                        q++;
                    }
                }
            }
            parent_->leave_broadcast(remote_actor);

        } else {
            throw std::runtime_error("Remove message callback expecting tuple of size 2 "
                                     "(remote_actor, callack_func).");
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void EmbeddedPython::run_callback_with_delay(const py::tuple &args) {

    try {

        if (args.size() == 2) {

            auto i             = args.begin();
            auto callback_func = (*i).cast<py::function>();
            i++;
            auto microseconds_delay = (*i).cast<int>();

            utility::Uuid id;
            for (auto &cb : delayed_callbacks_) {
                if (cb.second.is(callback_func)) {
                    id = cb.first;
                    break;
                }
            }
            if (id.is_null()) {
                id                     = utility::Uuid::generate();
                delayed_callbacks_[id] = callback_func;
            }
            parent_->delayed_callback(id, microseconds_delay);

        } else {
            throw std::runtime_error(
                "Set message callback expecting tuple of size 2 or 3 "
                "(remote_event_group_actor, callack_func, (optional: parent_actor)).");
        }

    } catch (std::exception &e) {
        // std::cerr << "E " << e.what() << "\n";
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void EmbeddedPython::run_callback(const utility::Uuid &id) {

    if (delayed_callbacks_.find(id) == delayed_callbacks_.end()) {
        spdlog::warn("{} : callback function not found.", __PRETTY_FUNCTION__);
        return;
    }

    // run the python callback!
    try {
        delayed_callbacks_[id]();
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void EmbeddedPython::s_add_message_callback(const py::tuple &cb_particulars) {
    if (s_instance_) {
        s_instance_->add_message_callback(cb_particulars);
    }
}

void EmbeddedPython::s_remove_message_callback(const py::tuple &cb_particulars) {
    if (s_instance_) {
        s_instance_->remove_message_callback(cb_particulars);
    }
}

void EmbeddedPython::s_run_callback_with_delay(const py::tuple &delayed_cb_args) {
    if (s_instance_) {
        s_instance_->run_callback_with_delay(delayed_cb_args);
    }
}


PYBIND11_EMBEDDED_MODULE(XStudioExtensions, m) {
    // `m` is a `py::module_` which is used to bind functions and classes
    m.def("run_callback_with_delay", &EmbeddedPython::s_run_callback_with_delay);
    m.def("add_message_callback", &EmbeddedPython::s_add_message_callback);
    m.def("remove_message_callback", &EmbeddedPython::s_remove_message_callback);
    py::class_<caf::message>(m, "CafMessage", py::module_local());
}

void EmbeddedPython::push_caf_message_to_py_callbacks(caf::actor sender, caf::message &m) {

    try {

        auto addr = caf::actor_cast<caf::actor_addr>(sender);

        const auto p = message_handler_callbacks_.find(addr);
        const auto q = message_conversion_function_.find(addr);
        if (p != message_handler_callbacks_.end()) {
            for (auto &func : p->second) {

                caf::message r = m;
                py::tuple msg(1);
                PyTuple_SetItem(msg.ptr(), 0, py::cast(r).release().ptr());

                if (q != message_conversion_function_.end()) {

                    // we have a py funct that will convert the caf message to
                    // a tuple
                    auto message_as_tuple = q->second(msg);
                    func(message_as_tuple);

                } else {

                    func(msg);
                }
            }
        }

    } catch (std::exception &e) {
        std::cerr << "PyError: " << e.what() << "\n";
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}
