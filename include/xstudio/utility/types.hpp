// SPDX-License-Identifier: Apache-2.0
#pragma once

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
    };

} // namespace utility
} // namespace xstudio