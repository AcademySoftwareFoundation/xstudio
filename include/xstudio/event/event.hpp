// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <caf/actor_system.hpp>
#include <caf/type_id.hpp>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace event {

    class Event {
      public:
        Event() = default;

        Event(
            const std::string progress_text,
            const int progress                    = 0,
            const int progress_minimum            = 0,
            const int progress_maximum            = 100,
            const std::set<utility::Uuid> targets = std::set<utility::Uuid>(),
            const utility::Uuid event             = utility::Uuid::generate())
            : progress_text_(std::move(progress_text)),
              progress_(progress),
              progress_minimum_(progress_minimum),
              progress_maximum_(progress_maximum),
              event_(std::move(event)),
              targets_(std::move(targets)) {}

        virtual ~Event() = default;

        [[nodiscard]] auto progress() const { return progress_; }
        [[nodiscard]] auto progress_minimum() const { return progress_minimum_; }
        [[nodiscard]] auto progress_maximum() const { return progress_maximum_; }
        [[nodiscard]] auto complete() const { return progress_ == progress_maximum_; }
        [[nodiscard]] auto event() const { return event_; }
        [[nodiscard]] auto targets() const { return targets_; }
        [[nodiscard]] auto contains(const utility::Uuid &value) const {
            return static_cast<bool>(targets_.count(value));
        }
        [[nodiscard]] auto progress_percentage() const {
            float percentage = 100.0;
            auto range       = progress_maximum_ - progress_minimum_;
            if (range > 0) {
                auto div   = 100.0f / static_cast<float>(range);
                percentage = div * static_cast<float>(progress_ - progress_minimum_);
            }

            return percentage;
        }

        [[nodiscard]] auto progress_text() const { return progress_text_; }
        [[nodiscard]] auto progress_text_percentage() const {
            return std::string(fmt::format(
                progress_text_, std::string(fmt::format("{:>5.1f}%", progress_percentage()))));
        }
        [[nodiscard]] auto progress_text_range() const {
            return std::string(fmt::format(
                progress_text_,
                std::string(fmt::format(
                    "{}/{}",
                    progress_ - progress_minimum_,
                    progress_maximum_ - progress_minimum_))));
        }

        template <class Inspector> friend bool inspect(Inspector &f, Event &x) {
            return f.object(x).fields(
                f.field("txt", x.progress_text_),
                f.field("val", x.progress_),
                f.field("min", x.progress_minimum_),
                f.field("max", x.progress_maximum_),
                f.field("evt", x.event_),
                f.field("tar", x.targets_));
        }

        void set_progress_text(const std::string value) { progress_text_ = std::move(value); }
        void set_progress(const int value) { progress_ = value; }
        void increment_progress(const int value = 1) {
            progress_ = std::min(progress_ + value, progress_maximum_);
        }
        void set_complete() { progress_ = progress_maximum_; }
        void set_progress_minimum(const int value) { progress_minimum_ = value; }
        void set_progress_maximum(const int value) { progress_maximum_ = value; }
        void set_event(const utility::Uuid value) { event_ = std::move(value); }
        void set_targets(const std::set<utility::Uuid> value) { targets_ = std::move(value); }
        void add_target(const utility::Uuid value) { targets_.emplace(value); }

      private:
        std::string progress_text_{"{}"};
        int progress_{0};
        int progress_minimum_{0};
        int progress_maximum_{0};
        utility::Uuid event_;
        std::set<utility::Uuid> targets_;
    };

    inline utility::Uuid send_event(caf::event_based_actor *act, const Event &event) {
        act->send(
            act->system().groups().get_local(global_event_group), utility::event_atom_v, event);
        return event.event();
    }
    inline utility::Uuid send_event(caf::blocking_actor *act, const Event &event) {
        act->send(
            act->system().groups().get_local(global_event_group), utility::event_atom_v, event);
        return event.event();
    }

} // namespace event
} // namespace xstudio
