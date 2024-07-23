// SPDX-License-Identifier: Apache-2.0
#include <caf/io/middleman.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"

using namespace xstudio;
using namespace xstudio::caf_utility;
using namespace caf;

caf_config::caf_config(int argc, char **argv) : caf::actor_system_config() {
    auto error = parse(argc, argv);
    if (error) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(error));
    }
    load<io::middleman>();
}
