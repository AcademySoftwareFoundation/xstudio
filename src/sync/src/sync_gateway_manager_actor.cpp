// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <fmt/format.h>
#include <iostream>
#include <thread>
#include <tuple>

#include <caf/io/all.hpp>
#include <caf/policy/select_all.hpp>

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
using namespace std::chrono_literals;

SyncGatewayManagerActor::SyncGatewayManagerActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    init();
}

void SyncGatewayManagerActor::init() {
    // launch global actors..
    // preferences first..
    // this will need more configuration
    spdlog::debug("Created SyncGatewayManagerActor");
    print_on_exit(this, "SyncGatewayManagerActor");

    system().registry().put(sync_gateway_manager_registry, this);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        for (auto it = std::begin(clients_); it != std::end(clients_); ++it) {
            if (msg.source == it->second) {
                spdlog::debug("Remove sync actor {}", to_string(it->first));
                demonitor(it->second);
                clients_.erase(it);
                break;
            }
        }
    });

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](authorise_connection_atom,
            const Uuid &lock,
            const std::string &key) -> caf::result<caf::actor> {
            if (not lock_key_.count(lock))
                return make_error(
                    xstudio_error::error, fmt::format("Invalid lock {}", to_string(lock)));
            if (clients_.count(lock))
                return make_error(
                    xstudio_error::error, fmt::format("Already connected {}", to_string(lock)));

            if (lock_key_[lock] != key) {
                lock_key_.erase(lock);
                return make_error(
                    xstudio_error::error,
                    fmt::format("Invalid key {} {}, lock invalidated.", to_string(lock), key));
            }
            lock_key_.erase(lock);
            auto act = spawn<SyncActor>();
            monitor(act);
            clients_.insert({lock, act});
            return act;
        },

        [=](get_sync_atom) -> caf::actor {
            Uuid lock = Uuid::generate();
            auto act  = spawn<SyncActor>();
            monitor(act);
            clients_.insert({lock, act});
            return act;
        },

        [=](request_connection_atom) -> Uuid {
            Uuid lock = Uuid::generate();
            lock_key_.insert({lock, std::string("key")});
            // stop client hammering..
            std::this_thread::sleep_for(1s);
            return lock;
        });
}

void SyncGatewayManagerActor::on_exit() {
    system().registry().erase(sync_gateway_manager_registry);
}
