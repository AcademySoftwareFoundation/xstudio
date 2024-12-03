// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <list>

#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/enums.hpp"

namespace xstudio::utility {

class NotificationHandler;

class Notification {
  public:
    Notification()          = default;
    virtual ~Notification() = default;

    Notification(
        const std::string text,
        const NotificationType type                = NT_INFO,
        const float progress                       = 0.0,
        const float progress_minimum               = 0.0,
        const float progress_maximum               = 100.0,
        const std::chrono::milliseconds expires_in = std::chrono::minutes(10),
        caf::event_based_actor *host               = nullptr,
        const caf::actor &destination              = caf::actor())
        : text_(std::move(text)),
          type_(type),
          progress_(progress),
          progress_minimum_(progress_minimum),
          progress_maximum_(progress_maximum),
          expires_in_(std::move(expires_in)),
          host_(host),
          destination_(destination) {
        send();
    }

    static Notification InfoNotification(
        caf::event_based_actor *host,
        const caf::actor &destination,
        const std::string text,
        const std::chrono::milliseconds expires_in = std::chrono::seconds(10)) {
        return Notification(text, NT_INFO, 0.0, 0.0, 100.0, expires_in, host, destination);
    }

    static Notification InfoNotification(
        const std::string text,
        const std::chrono::milliseconds expires_in = std::chrono::seconds(10)) {
        return Notification(text, NT_INFO, 0.0, 0.0, 100.0, expires_in);
    }

    static Notification WarnNotification(
        caf::event_based_actor *host,
        const caf::actor &destination,
        const std::string text,
        const std::chrono::milliseconds expires_in = std::chrono::seconds(10)) {
        return Notification(text, NT_WARN, 0.0, 0.0, 100.0, expires_in, host, destination);
    }

    static Notification WarnNotification(
        const std::string text,
        const std::chrono::milliseconds expires_in = std::chrono::seconds(10)) {
        return Notification(text, NT_WARN, 0.0, 0.0, 100.0, expires_in);
    }

    static Notification ProcessingNotification(
        caf::event_based_actor *host, const caf::actor &destination, const std::string text) {
        return Notification(
            text, NT_PROCESSING, 0.0, 0.0, 100.0, std::chrono::minutes(10), host, destination);
    }

    static Notification ProcessingNotification(const std::string text) {
        return Notification(text, NT_PROCESSING);
    }

    static Notification ProgressRangeNotification(
        caf::event_based_actor *host,
        const caf::actor &destination,
        const std::string text,
        const float progress                       = 0.0,
        const float progress_minimum               = 0.0,
        const float progress_maximum               = 100.0,
        const std::chrono::milliseconds expires_in = std::chrono::minutes(10)) {
        return Notification(
            text,
            NT_PROGRESS_RANGE,
            progress,
            progress_minimum,
            progress_maximum,
            expires_in,
            host,
            destination);
    }

    static Notification ProgressRangeNotification(
        const std::string text,
        const float progress                       = 0.0,
        const float progress_minimum               = 0.0,
        const float progress_maximum               = 100.0,
        const std::chrono::milliseconds expires_in = std::chrono::minutes(10)) {
        return Notification(
            text, NT_PROGRESS_RANGE, progress, progress_minimum, progress_maximum, expires_in);
    }

    static Notification ProgressPercentageNotification(
        const std::string text,
        const float progress                       = 0.0,
        const std::chrono::milliseconds expires_in = std::chrono::minutes(10)) {
        return Notification(text, NT_PROGRESS_PERCENTAGE, progress, 0.0, 100.0, expires_in);
    }

    static Notification ProgressPercentageNotification(
        caf::event_based_actor *host,
        const caf::actor &destination,
        const std::string text,
        const float progress                       = 0.0,
        const std::chrono::milliseconds expires_in = std::chrono::minutes(10)) {
        return Notification(
            text, NT_PROGRESS_PERCENTAGE, progress, 0.0, 100.0, expires_in, host, destination);
    }

    template <class Inspector> friend bool inspect(Inspector &f, Notification &x) {
        return f.object(x).fields(
            f.field("uid", x.uuid_),
            f.field("exp", x.expires_),
            f.field("exi", x.expires_in_),
            f.field("min", x.progress_minimum_),
            f.field("max", x.progress_maximum_),
            f.field("pro", x.progress_),
            f.field("typ", x.type_),
            f.field("txt", x.text_));
    }

    friend void to_json(nlohmann::json &j, const Notification &n);
    friend class NotificationHandler;

    [[nodiscard]] Uuid uuid() const { return uuid_; }
    [[nodiscard]] std::string text() const { return text_; }
    [[nodiscard]] float progress() const { return progress_; }
    [[nodiscard]] float progress_maximum() const { return progress_maximum_; }
    [[nodiscard]] float progress_minimum() const { return progress_minimum_; }
    [[nodiscard]] utility::sys_time_point expires() const { return expires_; }
    [[nodiscard]] std::chrono::milliseconds expires_in() const { return expires_in_; }
    [[nodiscard]] NotificationType type() const { return type_; }
    [[nodiscard]] float progress_percentage() const {
        float percentage = 100.0f;
        auto range       = progress_maximum_ - progress_minimum_;
        if (range > 0) {
            auto div   = 100.0f / static_cast<float>(range);
            percentage = div * static_cast<float>(progress_ - progress_minimum_);
        }

        return percentage;
    }

    void uuid(const Uuid &value) { uuid_ = value; }

    void text(const std::string &value) {
        text_ = value;
        send();
    }

    void send() {
        if (host_ and destination_)
            host_->anon_send(destination_, notification_atom_v, *this);
    }

    void progress(const float value) {
        progress_ = std::min(progress_maximum_, std::max(value, progress_minimum_));
        if (host_ and destination_)
            host_->anon_send(destination_, notification_atom_v, uuid_, progress_);
    }

    void progress_maximum(const float value) {
        progress_maximum_ = value;
        send();
    }

    void progress_minimum(const float value) {
        progress_minimum_ = value;
        send();
    }

    void type(const NotificationType value) {
        type_ = value;
        send();
    }

    void expires_in(const std::chrono::milliseconds value) {
        expires_in_ = value;
        send();
    }

    void remove() {
        if (host_ and destination_)
            host_->anon_send(destination_, notification_atom_v, uuid_);
    }

    void update_expires() { expires_ = utility::sysclock::now() + expires_in_; }

    [[nodiscard]] std::string progress_text_percentage() const {
        return std::string(fmt::format(
            fmt::runtime(text_), std::string(fmt::format("{:>5.1f}%", progress_percentage()))));
    }

    [[nodiscard]] std::string progress_text_range() const {
        return std::string(fmt::format(
            fmt::runtime(text_),
            std::string(fmt::format(
                "{}/{}",
                progress_ - progress_minimum_,
                progress_maximum_ - progress_minimum_))));
    }


  private:
    Uuid uuid_{Uuid::generate()};
    std::chrono::milliseconds expires_in_;
    utility::sys_time_point expires_;
    float progress_minimum_{0.0};
    float progress_maximum_{100.0};
    float progress_{0.0};
    std::string text_;
    NotificationType type_{NT_INFO};
    caf::event_based_actor *host_{nullptr};
    caf::actor destination_;
};

class NotificationHandler {
  public:
    NotificationHandler() = default;
    ;

    virtual ~NotificationHandler() = default;

    virtual caf::message_handler
    message_handler(caf::event_based_actor *act, caf::actor event_group);
    static caf::message_handler default_event_handler();

  private:
    [[nodiscard]] JsonStore digest() const;
    bool check_expired();
    bool remove_notification(const Uuid &uuid);
    bool update_progress(const Uuid &uuid, const float value);
    bool add_update_notification(const Notification &notification);
    utility::sys_time_point next_expire() const;
    caf::event_based_actor *actor_{nullptr};
    caf::actor event_group_;

    std::list<Notification> notifications_;

    //  	utility::sys_time_point next_expire_ {utility::sysclock::now()};
};

void to_json(nlohmann::json &j, const Notification &n);

} // namespace xstudio::utility