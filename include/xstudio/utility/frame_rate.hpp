// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <chrono>
#include <flicks.hpp>
#include <nlohmann/json.hpp>

#include "xstudio/utility/enums.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace utility {

    class FrameRate : public timebase::flicks {
      public:
        FrameRate(const timebase::flicks &flicks = timebase::k_flicks_zero_seconds)
            : timebase::flicks(flicks) {}
        FrameRate(const double seconds) : FrameRate(timebase::to_flicks(seconds)) {}

        FrameRate(const FrameRate &) = default;
        FrameRate(FrameRate &&)      = default;
        ~FrameRate()                 = default;

        // double to_double() const;
        [[nodiscard]] std::chrono::microseconds to_microseconds() const {
            return std::chrono::duration_cast<std::chrono::microseconds>(*this);
        }

        // [[nodiscard]] std::chrono::nanoseconds::rep count() const { return count(); }
        [[nodiscard]] double to_seconds() const { return timebase::to_seconds(*this); }
        [[nodiscard]] double to_fps() const {
            double fps = 0.0;
            if (count())
                fps = 1.0 / timebase::to_seconds(*this);
            return fps;
        }
        [[nodiscard]] timebase::flicks to_flicks() const { return *this; }

        FrameRate &operator=(const FrameRate &) = default;
        FrameRate &operator=(FrameRate &&)      = default;

        // Rational& operator+= (const Rational& other);
        // Rational& operator-= (const Rational& other);
        // Rational& operator*= (const Rational& other);
        // FrameRate &operator/=(const FrameRate &other) {
        //     *this /= other;
        //     return *this;
        // }
        // bool operator==(const FrameRate &other) const { return *this == other; }

        // bool operator!=(const FrameRate &other) const { return data_ != other.data_; }

        operator bool() const { return count() != 0; }

        template <class Inspector> friend bool inspect(Inspector &f, FrameRate &x) {
            return f.object(x).fields(f.field("flick", static_cast<timebase::flicks &>(x)));
        }

        // template <class Inspector> friend bool inspect(Inspector &f, FrameRate &x) {
        //     return f.object(x).fields(f.field("flicks", x));
        // }
    };

    inline std::string to_string(const FrameRate &v) { return std::to_string(v.to_fps()); }


    // [[gnu::pure]] inline FrameRate operator/(FrameRate a, const FrameRate &b) {
    //     a /= b;
    //     return a;
    // }

    inline void from_json(const nlohmann::json &j, FrameRate &r) { r = timebase::flicks(j); }

    inline void to_json(nlohmann::json &j, const FrameRate &r) { j = r.count(); }
} // namespace utility
} // namespace xstudio