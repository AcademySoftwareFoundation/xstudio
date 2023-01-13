// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::broadcast;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

class TestActor : public caf::event_based_actor {
  public:
    TestActor(caf::actor_config &cfg, caf::actor broadcast)
        : caf::event_based_actor(cfg), broadcast_(std::move(broadcast)) {
        init();
    }
    ~TestActor() override = default;

    const char *name() const override { return NAME.c_str(); }
    void on_exit() override {}

  private:
    inline static const std::string NAME = "TestActor";

    void init() {
        spdlog::warn("TestActor {}", to_string(caf::actor_cast<caf::actor>(this)));

        request(broadcast_, infinite, join_broadcast_atom_v)
            .then(
                [=](const bool) { send(broadcast_, xstudio::global_store::autosave_atom_v); },
                [=](const caf::error &) {});

        behavior_.assign(
            [=](xstudio::global_store::autosave_atom) {
                spdlog::warn(
                    "TestActor got autosave_atom broadcast from {}",
                    to_string(current_sender()));
            },
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
                spdlog::warn("broadcast down recieved");
            },
            [=](xstudio::broadcast::leave_broadcast_atom) {
                request(broadcast_, infinite, leave_broadcast_atom_v)
                    .then(
                        [=](const bool) { spdlog::warn("TestActor leave_broadcast_atom_v"); },
                        [=](const caf::error &) {});
            },
            [=](xstudio::broadcast::join_broadcast_atom) {
                request(broadcast_, infinite, join_broadcast_atom_v)
                    .then(
                        [=](const bool) { spdlog::warn("TestActor join_broadcast_atom"); },
                        [=](const caf::error &) {});
            });
    }

    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
    caf::actor broadcast_;
};


TEST(BroadcastActorTest, Test) {
    fixture f;
    start_logger();
    auto b = f.self->spawn<BroadcastActor>();
    auto c = f.self->spawn<TestActor>(b);
    auto d = f.self->spawn<TestActor>(b);

    f.self->anon_send(c, xstudio::broadcast::leave_broadcast_atom_v);
    f.self->send_exit(d, caf::exit_reason::user_shutdown);

    f.self->anon_send(b, xstudio::global_store::autosave_atom_v);

    f.self->anon_send(c, xstudio::broadcast::join_broadcast_atom_v);
    f.self->anon_send(b, xstudio::global_store::autosave_atom_v);
}
