// SPDX-License-Identifier: Apache-2.0

#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::broadcast;
using namespace xstudio::utility;
using namespace caf;

static std::atomic<int> count{0};
static std::atomic<int> actor_count{0};

BroadcastActor::BroadcastActor(caf::actor_config &cfg, caf::actor owner)
    : caf::event_based_actor(cfg) {
    // count++;
    // actor_count++;
    if (owner) {
        monitor(owner, [this, addr = owner.address()](const error &) {
            owner_ = caf::actor_addr();
            quit();
        });
        owner_ = caf::actor_cast<caf::actor_addr>(owner);
    }
    init();
}

void BroadcastActor::monitor_subscriber(const caf::actor &actor) {
    auto act_addr = caf::actor_cast<caf::actor_addr>(actor);

    if (auto sit = monitor_.find(act_addr); sit == std::end(monitor_)) {
        monitor_[act_addr] = monitor(actor, [this, addr = actor.address()](const error &) {
            auto actor_addr = caf::actor_cast<caf::actor_addr>(addr);
            if (auto mit = monitor_.find(actor_addr); mit != std::end(monitor_))
                monitor_.erase(mit);

            if (actor_addr && subscribers_.count(actor_addr))
                subscribers_.erase(actor_addr);
        });
    }
}


// BroadcastActor::~BroadcastActor(){
// count--;
// spdlog::error("{} count {} actor_count {}", __PRETTY_FUNCTION__, count, actor_count);
// }

caf::message_handler BroadcastActor::default_event_handler() {
    return {[=](broadcast_down_atom, const caf::actor_addr &) {}};
}

void BroadcastActor::init() {
    // print_on_create(this, "BroadcastActor");
    // print_on_exit(this, "BroadcastActor");

    // set_default_handler(
    //     [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
    //         //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

    //         // spdlog::warn("Got broadcast from {} {}", to_string(current_sender()),
    //         //     to_string(msg)
    //         // );

    //         if (current_sender() == nullptr or not current_sender()) {
    //             for (const auto &i : subscribers_) {
    //                 // spdlog::warn("{} {} Anon Forward {}",
    //                 // to_string(caf::actor_cast<caf::actor>(this)),
    //                 // to_string(current_sender()),
    //                 to_string(caf::actor_cast<caf::actor>(i))); try {
    //                     mail(msg).send(caf::actor_cast<caf::actor>(i));
    //                 } catch (...) {
    //                 }
    //             }
    //         } else {
    //             for (const auto &i : subscribers_) {
    //                 // we need to send as if we were delegating..
    //                 try {
    //                     // spdlog::warn("{} {} Forward to {}",
    //                     // to_string(caf::actor_cast<caf::actor>(this)),
    //                     // to_string(current_sender()),
    //                     // to_string(caf::actor_cast<caf::actor>(i)));

    //                     send_as(
    //                         caf::actor_cast<caf::actor>(current_sender()),
    //                         caf::actor_cast<caf::actor>(i),
    //                         msg);
    //                 } catch (...) {
    //                 }
    //             }
    //         }
    //         return message{};
    //     });

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](bool) -> int {
            return int(subscribers_.size());
        },
        [=](leave_broadcast_atom) -> bool {
            auto subscriber = caf::actor_cast<caf::actor_addr>(current_sender());
            if (subscriber && subscribers_.count(subscriber)) {

                if (auto mit =
                        monitor_.find(caf::actor_cast<caf::actor_addr>(current_sender()));
                    mit != std::end(monitor_)) {
                    mit->second.dispose();
                    monitor_.erase(mit);
                }

                subscribers_.erase(subscriber);
                // spdlog::warn("subscriber leaving {} {}",
                // to_string(caf::actor_cast<caf::actor>(this)),to_string(current_sender()));
            }
            return true;
        },
        [=](leave_broadcast_atom, caf::actor sub) -> bool {
            auto subscriber = caf::actor_cast<caf::actor_addr>(sub);
            if (subscriber && subscribers_.count(subscriber)) {

                if (auto mit = monitor_.find(caf::actor_cast<caf::actor_addr>(sub));
                    mit != std::end(monitor_)) {
                    mit->second.dispose();
                    monitor_.erase(mit);
                }

                subscribers_.erase(subscriber);
                // spdlog::warn("subscriber leaving {} {}",
                // to_string(caf::actor_cast<caf::actor>(this)),to_string(sub));
            }
            return true;
        },
        [=](join_broadcast_atom) -> bool {
            auto subscriber = caf::actor_cast<caf::actor_addr>(current_sender());
            if (subscriber && not subscribers_.count(subscriber)) {
                monitor_subscriber(caf::actor_cast<caf::actor>(current_sender()));
                subscribers_.insert(subscriber);
                // spdlog::warn("new subscriber {} {} {}",
                // to_string(caf::actor_cast<caf::actor>(this)), to_string(current_sender()),
                // to_string(subscriber));
            }
            return true;
        },
        [=](join_broadcast_atom, caf::actor sub) -> bool {
            auto subscriber = caf::actor_cast<caf::actor_addr>(sub);

            if (subscriber && not subscribers_.count(subscriber)) {

                monitor_subscriber(sub);
                subscribers_.insert(subscriber);
                // spdlog::warn("new subscriber {} {} {}",
                // to_string(caf::actor_cast<caf::actor>(this)), to_string(sub),
                // to_string(subscriber));
            }
            return true;
        },
        [=](caf::message &msg) {
            //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

            // spdlog::warn("Got broadcast from {} {}", to_string(current_sender()),
            //     to_string(msg)
            // );

            if (current_sender() == nullptr or not current_sender()) {
                for (const auto &i : subscribers_) {
                    // spdlog::warn("{} {} Anon Forward {}",
                    // to_string(caf::actor_cast<caf::actor>(this)),
                    // to_string(current_sender()), to_string(caf::actor_cast<caf::actor>(i)));
                    try {
                        mail(msg).send(caf::actor_cast<caf::actor>(i));
                    } catch (...) {
                    }
                }
            } else {
                for (const auto &i : subscribers_) {
                    // we need to send as if we were delegating..
                    try {
                        // spdlog::warn("{} {} Forward to {}",
                        // to_string(caf::actor_cast<caf::actor>(this)),
                        // to_string(current_sender()),
                        // to_string(caf::actor_cast<caf::actor>(i)));

                        send_as(
                            caf::actor_cast<caf::actor>(current_sender()),
                            caf::actor_cast<caf::actor>(i),
                            msg);
                    } catch (...) {
                    }
                }
            }
            return message{};
        });
}

void BroadcastActor::on_exit() {
    // spdlog::warn("notify subscribers or shutdown");
    for (const auto &i : subscribers_) {
        try {
            auto sub = caf::actor_cast<caf::actor>(i);
            if (sub)
                anon_mail(broadcast_down_atom_v, caf::actor_cast<caf::actor_addr>(this))
                    .send(sub);
        } catch (...) {
        }
    }
    subscribers_.clear();
    // actor_count--;
    // spdlog::error("{} count {} actor_count {}", __PRETTY_FUNCTION__, count, actor_count);
}
