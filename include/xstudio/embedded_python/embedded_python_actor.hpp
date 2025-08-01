// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/embedded_python/embedded_python.hpp"
#include "xstudio/utility/exports.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace embedded_python {

    class DLL_PUBLIC EmbeddedPythonActor : public caf::blocking_actor {
      public:
        EmbeddedPythonActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        EmbeddedPythonActor(caf::actor_config &cfg, const std::string &name);
        ~EmbeddedPythonActor() override = default;
        const char *name() const override { return NAME.c_str(); }

        void join_broadcast(caf::actor act, const std::string &plugin_name);

        void join_broadcast(caf::actor act);

      private:
        inline static const std::string NAME = "EmbeddedPythonActor";

        void act() override;
        void init();

      private:
        EmbeddedPython base_;
        caf::actor event_group_;
    };
} // namespace embedded_python
} // namespace xstudio
