// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

namespace xstudio::scanner {
class ScanHelperActor : public caf::event_based_actor {
  public:
    ScanHelperActor(caf::actor_config &cfg);

    ~ScanHelperActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ScanHelperActor";
    caf::behavior behavior_;

    std::map<caf::uri, std::pair<std::string, uintmax_t>> cache_;
};

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

} // namespace xstudio::scanner
