// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_system_config.hpp>

namespace xstudio::caf_utility {
class caf_config : public caf::actor_system_config {
  public:
    caf_config(int argc, char **argv);
};
} // namespace xstudio::caf_utility
