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

#ifdef WIN32
#else
#include <dlfcn.h>
#include <codecvt>
#endif

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

            PyConfig config;
            PyConfig_InitPythonConfig(&config);

#ifndef _WIN32
            // Since we're running embedded python, we need to set the PYTHONHOME
            // correctly at runtime. We can use dladdr to get to the filesystem
            // location of the Py_IsInitialized symbol, say, to get the path of
            // the python.dylib - this should be in the same location as the
            // rest of the python installation
            Dl_info info;
            // Some of the libc headers miss `const` in `dladdr(const void*, Dl_info*)`
            const int res = dladdr((void *)(&Py_IsInitialized), &info);
            if (res) {
                auto p = fs::path(info.dli_fname);
                std::string python_home;
                if (p.string().find("Contents/Frameworks") != std::string::npos) {
                    // String match will happen On MacOS install, here python
                    // installation is in Frameworks colder in the app bundle
                    python_home = p.parent_path();
                } else {
                    // Otherwise, we jump up twice to get above the 'lib' folder
                    // where python310.so is installed, as python home should
                    // be the parent folder of where the main python DSO is
                    // installed
                    python_home = p.parent_path().parent_path();
                }
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                std::wstring wstr = converter.from_bytes(python_home.data());
                PyConfig_SetString(&config, &config.home, wstr.data());

                std::string xstudio_python_path;
                auto pythonpath_env = get_env("PYTHONPATH");
                if (pythonpath_env) {
                    xstudio_python_path = *pythonpath_env + ":";
                }
                xstudio_python_path += utility::xstudio_resources_dir("python/lib/");
                wstr = converter.from_bytes(xstudio_python_path.data());
                PyConfig_SetString(&config, &config.pythonpath_env, wstr.data());
            }
#else
            // Win32 python home setup. Not 100% sure about this, I can't find any docs on how
            // python should be packaged with an application that embeds python.
            auto p = fs::path(
                fs::path(utility::xstudio_root()).parent_path().parent_path().string() +
                "\\bin\\python3");
            PyConfig_SetBytesString(&config, &config.home, p.string().data());
            /*std::string xstudio_python_path;
            auto pythonpath_env = get_env("PYTHONPATH");
            if (pythonpath_env) {
                xstudio_python_path = *pythonpath_env + ";";
            }
            xstudio_python_path +=
            fs::path(utility::xstudio_resources_dir("plugin-python")).string();
            PyConfig_SetBytesString(&config, &config.pythonpath_env,
            xstudio_python_path.data());*/


#endif

            py::initialize_interpreter(&config);
            inited_ = true;
        }

        if (Py_IsInitialized() and not setup_) {
            exec(R"(
import site
import sys
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
        py::dict local;
        local["actor"] = pyactor;
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
        std::string clean_input = rtrim(input);
        py::object scope        = py::module::import("__main__").attr("__dict__");
        py::exec(
            "xstudio_sessions['" + to_string(session_uuid) + "'].interact_more(\"\"\"" +
                clean_input + "\"\"\")",
            scope);
        return true;
    } else if (session_uuid.is_null()) {
        std::string clean_input = rtrim(input);
        py::object scope        = py::module::import("__main__").attr("__dict__");
        py::exec(clean_input, scope);
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