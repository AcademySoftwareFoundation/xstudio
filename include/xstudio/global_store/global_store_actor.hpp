// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_config.hpp>
#include <caf/type_id.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace global_store {
    class GlobalStoreActor : public caf::event_based_actor {
      public:
        GlobalStoreActor(
            caf::actor_config &cfg,
            const std::string &name,
            const std::vector<GlobalStoreDef> &defs = gsds,
            std::string reg_value                   = global_store_registry);
        GlobalStoreActor(
            caf::actor_config &cfg,
            const std::string &name        = "GlobalStore",
            const utility::JsonStore &json = utility::JsonStore(),
            std::string reg_value          = global_store_registry);
        GlobalStoreActor(
            caf::actor_config &cfg,
            const utility::JsonStore &json,
            std::string reg_value = global_store_registry);
        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        inline static const std::string NAME = "GlobalStoreActor";
        void init();

      private:
        const std::string reg_value_;
        caf::behavior behavior_;
        GlobalStore base_;
    };
} // namespace global_store
} // namespace xstudio
