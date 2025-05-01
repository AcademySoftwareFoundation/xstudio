// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include <caf/actor_system.hpp>
#include <caf/blocking_actor.hpp>
#include <caf/scoped_actor.hpp>

namespace xstudio::json_store {

class JsonStoreHandler {
  public:
    JsonStoreHandler() = default;
    JsonStoreHandler(
        caf::event_based_actor *act,
        caf::actor event_group,
        const utility::Uuid &uuid              = utility::Uuid::generate(),
        const utility::JsonStore &json         = utility::JsonStore(),
        const std::chrono::milliseconds &delay = std::chrono::milliseconds());

    virtual ~JsonStoreHandler() = default;

    virtual caf::message_handler message_handler();
    static caf::message_handler default_event_handler();

    caf::actor json_actor() const { return json_store_; }

  private:
    caf::event_based_actor *actor_{nullptr};
    caf::actor event_group_;
    caf::actor json_store_;
};


} // namespace xstudio::json_store
