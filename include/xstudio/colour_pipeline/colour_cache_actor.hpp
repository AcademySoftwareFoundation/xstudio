// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <string>

#include <caf/all.hpp>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"

namespace xstudio {
namespace colour_pipeline {
    class GlobalColourCacheActor : public caf::event_based_actor {
      public:
        GlobalColourCacheActor(caf::actor_config &cfg);
        ~GlobalColourCacheActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "GlobalColourCacheActor";

        caf::behavior behavior_;
        utility::TimeCache<std::string, ColourOperationDataPtr> cache_;
    };
} // namespace colour_pipeline
} // namespace xstudio
