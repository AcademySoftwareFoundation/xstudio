// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <filesystem>

#include "xstudio/embedded_python/embedded_python.hpp"
#include "xstudio/utility/exports.hpp"
#include "xstudio/utility/json_store_sync.hpp"
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
        void leave_broadcast(caf::actor act);
        void delayed_callback(utility::Uuid &cb_id, const int microseconds_delay);

      private:
        inline static const std::string NAME = "EmbeddedPythonActor";

        void act() override;
        void init();

        void refresh_snippets(const std::vector<caf::uri> &paths);
        nlohmann::json refresh_snippet(
            const std::filesystem::path &path,
            const std::string &menu_path    = "",
            const std::string &snippet_type = "");
        void update_preferences(const utility::JsonStore &);

      private:
        EmbeddedPython base_;
        caf::actor event_group_;

        // stores information on conforming actions.
        utility::JsonStoreSync data_;
        utility::Uuid data_uuid_{utility::Uuid::generate()};
        std::vector<caf::uri> snippet_paths_;
    };
} // namespace embedded_python
} // namespace xstudio
