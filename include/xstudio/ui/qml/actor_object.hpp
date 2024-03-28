// SPDX-License-Identifier: Apache-2.0
/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <caf/actor_companion.hpp>
#include <caf/config.hpp>
#include <caf/make_actor.hpp>
#include <caf/message_handler.hpp>
#include <caf/scheduled_actor.hpp>
#include "caf/detail/shared_spinlock.hpp"

#include <caf/scoped_execution_unit.hpp>

CAF_PUSH_WARNINGS
#include <QApplication>
#include <QEvent>
#include <QDebug>
CAF_POP_WARNINGS

#include "xstudio/atoms.hpp"
#include "xstudio/utility/logging.hpp"

namespace caf::mixin {

// QEvent::User == 1000
template <typename Base, int EventId = static_cast<int>(FIRST_CUSTOM_ID)>
class actor_object : public Base {
  public:
    /// A shared lockable.
    using lock_type = detail::shared_spinlock;

    struct event_type : public QEvent {
        mailbox_element_ptr mptr;
        event_type(mailbox_element_ptr ptr)
            : QEvent(static_cast<QEvent::Type>(EventId)), mptr(std::move(ptr)) {
            // nop
        }
    };

    template <typename... Ts> actor_object(Ts &&...xs) : Base(std::forward<Ts>(xs)...) {
        // nop
    }

    ~actor_object() override {
        alive_ = false;
        this->disconnect();
        if (companion_)
            self()->cleanup(error{}, &execution_unit_);
    }

    void init(actor_system &system) {
        alive_ = true;
        execution_unit_.system_ptr(&system);
        companion_ = actor_cast<strong_actor_ptr>(system.spawn<actor_companion>());

        self()->on_enqueue([=](mailbox_element_ptr ptr) {
            std::lock_guard<lock_type> guard(lock_);

            if (self() == nullptr or not alive_) {
                // spdlog::warn("{} companion is null on_enqueue", __PRETTY_FUNCTION__);
            } else {
                if (ptr) {
                    qApp->postEvent(this, new event_type(std::move(ptr)));
                } else {
                    spdlog::warn("{} null message ptr!", __PRETTY_FUNCTION__);
                }
            }
        });

        self()->on_exit([=] {
            spdlog::warn("{}", __PRETTY_FUNCTION__);
            // close widget if actor companion dies
            // this->close();
        });
    }

    template <class F> void set_message_handler(F pfun) { self()->become(pfun(self())); }

    /// Terminates the actor companion and closes this widget.
    void quit_and_close(error exit_state = error{}) {
        self()->quit(std::move(exit_state));
        // this->close();
    }

    bool event(QEvent *event) override {
        if (event->type() == static_cast<QEvent::Type>(EventId)) {
            auto ptr = dynamic_cast<event_type *>(event);
            if (ptr && alive_) {
                try {
                    switch (self()->activate(&execution_unit_, *(ptr->mptr))) {
                    case caf::scheduled_actor::activation_result::success:
                    case caf::scheduled_actor::activation_result::terminated:
                    case caf::scheduled_actor::activation_result::skipped:
                    case caf::scheduled_actor::activation_result::dropped:
                    default:
                        break;
                    }
                } catch (...) {
                }
                return true;
            }
        }
        return Base::event(event);
    }

    [[nodiscard]] actor as_actor() const {
        CAF_ASSERT(companion_);
        return actor_cast<actor>(companion_);
    }

    [[nodiscard]] actor_companion *self() const {
        using bptr = abstract_actor *;  // base pointer
        using dptr = actor_companion *; // derived pointer
        return companion_ ? static_cast<dptr>(actor_cast<bptr>(companion_)) : nullptr;
    }

  private:
    scoped_execution_unit execution_unit_;
    strong_actor_ptr companion_{nullptr};
    bool alive_{false};
    // guards access to handler_
    lock_type lock_;
};

} // namespace caf::mixin
