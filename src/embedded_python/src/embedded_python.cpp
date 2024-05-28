// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
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

//EmbeddedPython *EmbeddedPython::s_instance_ = nullptr;

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

        if (cb_particulars.size() == 2) {

            auto i            = cb_particulars.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

            message_handler_callbacks_[addr].push_back(callback_func);
            parent_->join_broadcast(remote_actor);

        } else {

            if (cb_particulars.size() != 3) {
                throw std::runtime_error("Set message callback expecting tuple of size 3 "
                                         "(remote_actor, callack_func, py_plugin_name).");
            }
            auto i            = cb_particulars.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            i++;
            auto plugin_name = (*i).cast<std::string>();
            auto addr        = caf::actor_cast<caf::actor_addr>(remote_actor);

            message_handler_callbacks_[addr].push_back(callback_func);
            parent_->join_broadcast(remote_actor, plugin_name);
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void EmbeddedPython::s_add_message_callback(const py::tuple &cb_particulars) {
    if (s_instance_) {
        s_instance_->add_message_callback(cb_particulars);
    }
}

PYBIND11_EMBEDDED_MODULE(XStudioExtensions, m) {
    // `m` is a `py::module_` which is used to bind functions and classes
    m.def("add_message_callback", &EmbeddedPython::s_add_message_callback);
    py::class_<caf::message>(m, "CafMessage", py::module_local());
}

void EmbeddedPython::push_caf_message_to_py_callbacks(caf::actor sender, caf::message &m) {

    try {

        caf::message r = m;
        py::tuple result(1);
        PyTuple_SetItem(result.ptr(), 0, py::cast(r).release().ptr());

        auto addr = caf::actor_cast<caf::actor_addr>(sender);

        for (auto &p : message_handler_callbacks_) {
            for (auto &func : p.second) {
                func(result);
            }
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}
