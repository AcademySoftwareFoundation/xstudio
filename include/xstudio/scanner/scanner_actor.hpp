// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

namespace xstudio {
namespace scanner {
    class ScannerActor : public caf::event_based_actor {
      public:
        ScannerActor(caf::actor_config &cfg);

        ~ScannerActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ScannerActor";
        caf::behavior behavior_;
    };

} // namespace scanner
} // namespace xstudio