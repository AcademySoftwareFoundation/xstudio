// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/utility/notification_handler.hpp"

namespace xstudio::utility {

void to_json(nlohmann::json &j, const Notification &n) {
    j["uuid"] = n.uuid_;
    switch (n.type_) {
    case NT_INFO:
        j["type"] = "INFO";
        break;
    case NT_WARN:
        j["type"] = "WARN";
        break;
    case NT_PROGRESS_RANGE:
        j["type"] = "PROGRESS_RANGE";
        break;
    case NT_PROGRESS_PERCENTAGE:
        j["type"] = "PROGRESS_PERCENTAGE";
        break;
    case NT_PROCESSING:
        j["type"] = "PROCESSING";
        break;
    default:
        j["type"] = "UNKNOWN";
        break;
    }
    j["expires"] = n.expires_;

    if (n.type_ == NT_PROGRESS_RANGE) {
        j["min_progress"]     = n.progress_minimum_;
        j["max_progress"]     = n.progress_maximum_;
        j["progress"]         = n.progress_;
        j["progress_percent"] = n.progress_percentage();
        j["text"]             = n.progress_text_range();
    } else if (n.type_ == NT_PROGRESS_PERCENTAGE) {
        j["min_progress"]     = n.progress_minimum_;
        j["max_progress"]     = n.progress_maximum_;
        j["progress"]         = n.progress_;
        j["progress_percent"] = n.progress_percentage();
        j["text"]             = n.progress_text_percentage();
    } else {
        j["text"] = n.text_;
    }
}

caf::message_handler NotificationHandler::default_event_handler() {
    return {[=](utility::event_atom, notification_atom, const JsonStore &) {}};
}

caf::message_handler
NotificationHandler::message_handler(caf::event_based_actor *act, caf::actor event_group) {
    actor_       = act;
    event_group_ = event_group;

    return caf::message_handler(
        {[=](notification_atom) -> JsonStore { return digest(); },
         [=](notification_atom, const Uuid &uuid) -> bool {
             auto result = remove_notification(uuid);
             if (result)
                 actor_->send(
                     event_group_, utility::event_atom_v, notification_atom_v, digest());
             return result;
         },
         [=](notification_atom, const bool check_expire) {
             if (check_expired()) {
                 actor_->send(
                     event_group_, utility::event_atom_v, notification_atom_v, digest());
                 utility::sys_time_point next_expires = next_expire();
                 if (next_expires != utility::sys_time_point()) {
                     if (next_expires > utility::sysclock::now())
                         act->delayed_anon_send(
                             act,
                             next_expires - utility::sysclock::now(),
                             notification_atom_v,
                             true);
                     else
                         act->anon_send(act, notification_atom_v, true);
                 }
             }
         },
         [=](notification_atom, const Notification &notification) -> bool {
             auto result = add_update_notification(notification);
             if (result) {
                 actor_->send(
                     event_group_, utility::event_atom_v, notification_atom_v, digest());
                 utility::sys_time_point next_expires = next_expire();
                 if (next_expires != utility::sys_time_point()) {
                     if (next_expires > utility::sysclock::now())
                         act->delayed_anon_send(
                             act,
                             next_expires - utility::sysclock::now(),
                             notification_atom_v,
                             true);
                     else
                         act->anon_send(act, notification_atom_v, true);
                 }
             }
             return result;
         },
         [=](notification_atom, const Uuid &uuid, const float progress) -> bool {
             auto result = update_progress(uuid, progress);
             if (result)
                 actor_->send(
                     event_group_, utility::event_atom_v, notification_atom_v, digest());
             return result;
         }});
}

utility::sys_time_point NotificationHandler::next_expire() const {
    utility::sys_time_point result;

    if (not notifications_.empty())
        result = notifications_.front().expires_;

    for (const auto &i : notifications_) {
        if (i.expires_ < result)
            result = i.expires_;
    }

    return result;
}


bool NotificationHandler::add_update_notification(const Notification &notification) {
    auto result = false;

    for (auto it = notifications_.begin(); not result and it != notifications_.end(); ++it) {
        if (it->uuid_ == notification.uuid_) {
            result = true;
            *it    = notification;
            it->update_expires();
        }
    }

    if (not result) {
        result = true;
        notifications_.push_back(notification);
        notifications_.back().update_expires();
    }

    return result;
}


JsonStore NotificationHandler::digest() const {
    auto result = R"([])"_json;

    for (const auto &i : notifications_)
        result.push_back(i);

    return result;
}

bool NotificationHandler::check_expired() {
    auto now    = utility::sysclock::now();
    auto result = false;
    auto it     = notifications_.begin();

    while (it != notifications_.end()) {
        if (it->expires_ < now) {
            result = true;
            it     = notifications_.erase(it);
        } else
            it++;
    }

    return result;
}

bool NotificationHandler::remove_notification(const Uuid &uuid) {
    auto result = false;

    for (auto it = notifications_.begin(); not result and it != notifications_.end(); ++it) {
        if (it->uuid_ == uuid) {
            result = true;
            it     = notifications_.erase(it);
        }
    }

    return result;
}

bool NotificationHandler::update_progress(const Uuid &uuid, const float value) {
    auto result = false;

    for (auto it = notifications_.begin(); not result and it != notifications_.end(); ++it) {
        if (it->uuid_ == uuid) {
            if (it->progress_ != value) {
                result        = true;
                it->progress_ = value;
            }
            break;
        }
    }

    return result;
}


} // namespace xstudio::utility
