// SPDX-License-Identifier: Apache-2.0
#include <caf/io/all.hpp>
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/sync/sync_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::sync;
using namespace xstudio::utility;

SyncActor::SyncActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) { init(); }

void SyncActor::init() {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created SyncActor");
    print_on_exit(this, "SyncActor");

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](global::get_api_mode_atom) -> std::string { return "SYNC"; },

        [=](global::get_application_mode_atom _req) {
            delegate(system().registry().template get<caf::actor>(global_registry), _req);
        });
}

void SyncActor::on_exit() {}
