// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media_reader/media_reader.hpp"

namespace xstudio::conform {
class ConformWorkerActor : public caf::event_based_actor {
  public:
    ConformWorkerActor(caf::actor_config &cfg);
    ~ConformWorkerActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }
    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ConformWorkerActor";
    caf::behavior behavior_;
};

class ConformManagerActor : public caf::event_based_actor {
  public:
    ConformManagerActor(
        caf::actor_config &cfg, const utility::Uuid uuid = utility::Uuid::generate());
    ~ConformManagerActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }
    void on_exit() override;
    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ConformManagerActor";
    caf::behavior behavior_;
    utility::Uuid uuid_;
    caf::actor event_group_;
    std::vector<std::string> tasks_;
};

} // namespace xstudio::conform
