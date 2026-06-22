// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/frame_rate.hpp"

namespace xstudio {
namespace utility {

    class FrameRateDuration {

      public:
        FrameRateDuration()
            : rate_(timebase::k_flicks_zero_seconds),
              duration_(timebase::k_flicks_zero_seconds) {}
        FrameRateDuration(const FrameRateDuration &) = default;
        FrameRateDuration(FrameRateDuration &&)      = default;
        explicit FrameRateDuration(const int frames, const double fps)
            : rate_(timebase::to_flicks(1.0 / fps)), duration_(rate_.count() * frames) {}
        explicit FrameRateDuration(const double seconds, FrameRate rat)
            : rate_(std::move(rat)), duration_(timebase::to_flicks(seconds)) {}
        explicit FrameRateDuration(const FrameRate dur, FrameRate rat)
            : rate_(std::move(rat)), duration_(dur.to_flicks()) {}
        explicit FrameRateDuration(const timebase::flicks flicks, FrameRate rat)
            : rate_(std::move(rat)), duration_(flicks) {}
        explicit FrameRateDuration(const int frames, FrameRate rat)
            : rate_(std::move(rat)), duration_(rate_.count() * frames) {}
        // FrameRateDuration (const int frames, const int fps) : rate_(1, den),
        // count_(timebase_ * frames) {} FrameRateDuration (const int frames, const int num,
        // const int den) : rate_(num, den), count_(timebase_ * frames) {}

        FrameRateDuration &operator=(const FrameRateDuration &) = default;
        FrameRateDuration &operator=(FrameRateDuration &&)      = default;

        FrameRateDuration &operator+=(const FrameRateDuration &);
        FrameRateDuration &operator-=(const FrameRateDuration &);

        FrameRateDuration operator-(const FrameRateDuration &);
        FrameRateDuration operator+(const FrameRateDuration &);

        [[nodiscard]] FrameRateDuration
        subtract_frames(const FrameRateDuration &, const bool remapped = true) const;
        [[nodiscard]] FrameRateDuration
        subtract_seconds(const FrameRateDuration &, const bool remapped = true) const;
        [[nodiscard]] FrameRateDuration
        add_frames(const FrameRateDuration &, const bool remapped = true) const;
        [[nodiscard]] FrameRateDuration
        add_seconds(const FrameRateDuration &, const bool remapped = true) const;

        /**
         *  @brief Compute the logical frame for a given timepoint.
         *
         *  @details Uses the frame rate to compute the frame corresponding to a time point,
         * as measured from frame=0, time=0.0. No rounding is applied in the computation, it
         * will return the number of whole frame periods divisible into seconds
         */
        [[nodiscard]] int frame(const timebase::flicks flicks) const;

        [[nodiscard]] int frame(const FrameRateDuration &rt, const bool remapped = true) const;
        [[nodiscard]] int frames(const FrameRate &override = FrameRate()) const;
        void set_frames(const int frames) { duration_ = rate_.to_flicks() * frames; }

        [[nodiscard]] double seconds(const FrameRate &override = FrameRate()) const;
        void set_seconds(double seconds) { duration_ = timebase::to_flicks(seconds); }

        [[nodiscard]] FrameRate rate() const { return rate_; }
        void set_rate(const FrameRate &rate, bool maintain_duration = false);

        [[nodiscard]] timebase::flicks duration(const FrameRate &override = FrameRate()) const;
        void set_duration(const timebase::flicks &flicks) { duration_ = flicks; }

        void step(const bool forward = true, const float velocity = 1.0f) {
            duration_ +=
                timebase::to_flicks(((forward ? 1 : -1)) * (rate_.to_seconds() * velocity));
        }

        // FrameRateDuration& operator+= (const FrameRateDuration& other);
        // FrameRateDuration& operator-= (const FrameRateDuration& other);
        // FrameRateDuration& operator*= (const FrameRateDuration& other);
        // FrameRateDuration& operator/= (const FrameRateDuration& other);
        bool operator==(const FrameRateDuration &other) const {
            return rate_.count() == other.rate_.count() and duration_ == other.duration_;
        }

        template <class Inspector> friend bool inspect(Inspector &f, FrameRateDuration &x) {
            return f.object(x).fields(f.field("rate", x.rate_), f.field("data", x.duration_));
        }

      public:
        FrameRate rate_;
        timebase::flicks duration_;
    };

    inline std::string to_string(const FrameRateDuration &v) {
        return std::to_string(v.rate_.to_fps()) + " " + std::to_string(v.frames());
    }
    void from_json(const nlohmann::json &j, FrameRateDuration &rt);
    void to_json(nlohmann::json &j, const FrameRateDuration &rt);
} // namespace utility
} // namespace xstudio