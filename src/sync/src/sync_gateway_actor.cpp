// SPDX-License-Identifier: Apache-2.0
#include <caf/io/all.hpp>
#include <caf/policy/select_all.hpp>
#include <tuple>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/sync/sync_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::sync;
using namespace xstudio::utility;
using namespace xstudio::global_store;

SyncGatewayActor::SyncGatewayActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    init();
}

void SyncGatewayActor::init() {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created SyncGatewayActor");
    print_on_exit(this, "SyncGatewayActor");

    system().registry().put(sync_gateway_registry, this);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](authorise_connection_atom _req, const Uuid &lock, const std::string &key) {
            delegate(
                system().registry().template get<caf::actor>(sync_gateway_manager_registry),
                _req,
                lock,
                key);
        },

        [=](global::get_api_mode_atom) -> std::string { return "GATEWAY"; },

        [=](request_connection_atom _req) {
            delegate(
                system().registry().template get<caf::actor>(sync_gateway_manager_registry),
                _req);
        });
}

void SyncGatewayActor::on_exit() { system().registry().erase(sync_gateway_registry); }
