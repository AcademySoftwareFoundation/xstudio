// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <flicks.hpp>
#include <nlohmann/json.hpp>

#include "xstudio/utility/enums.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/frame_rate_and_duration.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace utility {

    class FrameRange {
      public:
        FrameRange() = default;
        FrameRange(const FrameRateDuration &duration)
            : rate_(duration.rate()), duration_(duration.duration()) {}

        FrameRange(const FrameRate start, const FrameRate duration, const FrameRate rate)
            : start_(std::move(start)),
              duration_(std::move(duration)),
              rate_(std::move(rate)) {}

        FrameRange(const FrameRateDuration &start, const FrameRateDuration &duration)
            : rate_(duration.rate()),
              start_(start.duration()),
              duration_(duration.duration()) {}
        ~FrameRange() = default;

        [[nodiscard]] FrameRateDuration frame_start() const {
            return FrameRateDuration(start_, rate_);
        }
        [[nodiscard]] FrameRateDuration frame_duration() const {
            return FrameRateDuration(duration_, rate_);
        }

        [[nodiscard]] FrameRate rate() const { return rate_; }
        [[nodiscard]] FrameRate start() const { return start_; }
        [[nodiscard]] FrameRate duration() const { return duration_; }

        // ignoring rate...
        [[nodiscard]] FrameRange intersect(const FrameRange &fr) const {
            auto result = fr;

            // adjust start
            if (result.start_ < start_) {
                result.duration_ -=
                    std::max((start_ - result.start_), timebase::k_flicks_zero_seconds);
                result.start_ = start_;
            }

            if (result.start_ >= start_ + duration_) {
                result.duration_ = timebase::k_flicks_zero_seconds;
                result.start_    = start_ + duration_;
            }

            // adjust end
            if (result.start_ + result.duration_ > start_ + duration_) {
                result.duration_ -= (result.start_ + result.duration_) - (start_ + duration_);
            }

            return result;
        }

        void set_rate(const FrameRate &rate) { rate_ = rate; }
        void set_start(const FrameRate &start) { start_ = start; }
        void set_duration(const FrameRate &duration) { duration_ = duration; }

        template <class Inspector> friend bool inspect(Inspector &f, FrameRange &x) {
            return f.object(x).fields(
                f.field("rate", x.rate_),
                f.field("start", x.start_),
                f.field("duration", x.duration_));
        }

        friend void from_json(const nlohmann::json &j, FrameRange &r);
        friend void to_json(nlohmann::json &j, const FrameRange &r);

        bool operator==(const FrameRange &other) const {
            return rate_ == other.rate_ and start_ == other.start_ and
                   duration_ == other.duration_;
        }
        bool operator!=(const FrameRange &other) const {
            return rate_ != other.rate_ or start_ != other.start_ or
                   duration_ != other.duration_;
        }

      private:
        FrameRate rate_{timebase::k_flicks_24fps};
        FrameRate start_{timebase::k_flicks_zero_seconds};
        FrameRate duration_{timebase::k_flicks_zero_seconds};
    };

    inline void from_json(const nlohmann::json &j, FrameRange &r) {
        j.at("rate").get_to(r.rate_);
        j.at("start").get_to(r.start_);
        j.at("duration").get_to(r.duration_);
    }

    inline void to_json(nlohmann::json &j, const FrameRange &r) {
        j = nlohmann::json{{"rate", r.rate_}, {"start", r.start_}, {"duration", r.duration_}};
    }

} // namespace utility
} // namespace xstudio
