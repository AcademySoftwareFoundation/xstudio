// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <fmt/format.h>

namespace xstudio {
namespace utility {

    struct ColourTriplet {

        ColourTriplet()                       = default;
        ColourTriplet(const ColourTriplet &o) = default;
        ColourTriplet(const float _r, const float _g, const float _b) : r(_r), g(_g), b(_b) {}

        ColourTriplet &operator=(const ColourTriplet &o) = default;

        bool operator==(const ColourTriplet &o) const {
            return r == o.r && g == o.g && b == o.b;
        }
        bool operator!=(const ColourTriplet &o) const {
            return !(r == o.r && g == o.g && b == o.b);
        }

        float r = {0.0f}; // NOLINT
        float g = {0.0f}; // NOLINT
        float b = {0.0f}; // NOLINT

        float red() const { return r; }
        float green() const { return g; }
        float blue() const { return b; }

        void setRed(const float _r) { r = _r; }
        void setGreen(const float _g) { g = _g; }
        void setBlue(const float _b) { b = _b; }

        friend std::string to_string(const ColourTriplet &value);

        template <class Inspector> friend bool inspect(Inspector &f, ColourTriplet &x) {
            return f.object(x).fields(f.field("r", x.r), f.field("g", x.g), f.field("b", x.b));
        }
    };

    inline std::string to_string(const ColourTriplet &c) {
        return fmt::format("ColourTriplet({}, {}, {})", c.r, c.g, c.b);
    }


} // namespace utility
} // namespace xstudio