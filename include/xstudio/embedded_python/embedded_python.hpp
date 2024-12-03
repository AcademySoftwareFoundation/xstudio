// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <list>
#include <memory>
#define PYBIND11_DETAILED_ERROR_MESSAGES

#include <pybind11/embed.h> // everything needed for embedding
#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/exports.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace embedded_python {
    namespace py = pybind11;

    class EmbeddedPythonActor;

    class DLL_PUBLIC EmbeddedPython : public utility::Container {
      public:
        EmbeddedPython(
            const std::string &name = "EmbeddedPython", EmbeddedPythonActor *parent = nullptr);
        EmbeddedPython(const utility::JsonStore &jsn, EmbeddedPythonActor *parent = nullptr);

        ~EmbeddedPython() override;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] bool enabled() const { return Py_IsInitialized(); }

        void setup();

        void hello() const;
        void exec(const std::string &pystring) const;
        void eval_file(const std::string &pyfile) const;
        [[nodiscard]] nlohmann::json eval(const std::string &pystring) const;
        [[nodiscard]] nlohmann::json
        eval(const std::string &pystring, const nlohmann::json &locals) const;

        [[nodiscard]] nlohmann::json eval_locals(const std::string &pystring) const;
        [[nodiscard]] nlohmann::json
        eval_locals(const std::string &pystring, const nlohmann::json &locals) const;

        bool connect(const int port);
        bool connect(caf::actor actor);
        void disconnect();

        utility::Uuid create_session(const bool interactive);
        bool remove_session(const utility::Uuid &session_uuid);
        bool input_session(const utility::Uuid &session_uuid, const std::string &input);
        bool input_ctrl_c_session(const utility::Uuid &session_uuid);

        void push_caf_message_to_py_callbacks(caf::actor sender, caf::message &m);

        void add_message_callback(const py::tuple &xs);
        void remove_message_callback(const py::tuple &xs);
        void run_callback_with_delay(const py::tuple &args);
        void run_callback(const utility::Uuid &id);

        static void s_add_message_callback(const py::tuple &xs);
        static void s_remove_message_callback(const py::tuple &xs);
        static void s_run_callback_with_delay(const py::tuple &delayed_cb_args);

        void finalize();

      private:
        // py::function cb_;
        std::map<caf::actor_addr, std::vector<py::function>> message_handler_callbacks_;
        std::map<caf::actor_addr, py::function> message_conversion_function_;
        std::map<utility::Uuid, py::function> delayed_callbacks_;

        EmbeddedPythonActor *parent_;

        inline static EmbeddedPython *s_instance_ = nullptr;
        std::set<utility::Uuid> sessions_;
        bool inited_{false};
        bool setup_{false};
        // std::map<utility::Uuid, std::pair<py::object, py::object>> sessions_;
    };
} // namespace embedded_python
} // namespace xstudio