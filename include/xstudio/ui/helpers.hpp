// SPDX-License-Identifier: Apache-2.0
#pragma once

// Use hardware fused multiply-add if supported
#ifndef __GNUC__
#pragma STDC FENV_ACCESS ON
#endif

#include <cmath>
#include <Imath/ImathMatrix.h>

namespace xstudio {
namespace utility {

    // map a normalized linear value between 0 and 1 to a
    // normalized exp value between min and max
    inline float lin_to_exp(float lin_value, float exp) {
        // y = 100 * pow((x / 100), a)  for various 0 < a

        return std::pow(lin_value, exp);

        // float log_val_normalized = std::log10(9 * lin_value + 1);

        // return log_val_normalized * (log_max - log_min) + log_min;
    }

    // map a normalized linear value between 0 and 1 to a
    // normalized log value between log_min and log_max
    inline float lin_to_log(float lin_value, float log_min, float log_max) {

        float log_val_normalized = std::log10(9 * lin_value + 1);

        return log_val_normalized * (log_max - log_min) + log_min;
    }

    // Linear interpolation of two floats
    // Use fused multiply-add if compilers support it
    inline float flerpf(float v0, float v1, float t) { return fmaf(t, v1, fmaf(-t, v0, v0)); }

    // Calculate the approximation of the norm of a vector.
    // Uses the simple hypethenous calculation explained here:
    // https://math.stackexchange.com/a/1351711
    inline float approximate_norm(float x, float y) {

        constexpr float RATIO = 3.0f / 7.0f;

        // GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79700
        // Cannot use fbasf, available from GCC 14. FIX when released
        x = std::abs(x);
        y = std::abs(y);

        return x > y ? (x + y * RATIO) : (y + x * RATIO);
    }

    inline Imath::M44f remove_matrix_rotation(const Imath::M44f &in) {

        // For text display, we don't want to apply the image rotation but we still
        // want the scale and translate. Here's my hokey way to do that:
        Imath::V4f a(0.0f, 0.0f, 0.0f, 1.0f);
        Imath::V4f b(1.0f, 0.0f, 0.0f, 1.0f);
        Imath::V4f c(0.0f, 1.0f, 0.0f, 1.0f);
        a *= in;
        b *= in;
        c *= in;
        float x_scale =
            (Imath::V2f(b.x / b.w, b.y / b.w) - Imath::V2f(a.x / a.w, a.y / a.w)).length();
        float y_scale =
            (Imath::V2f(c.x / c.w, c.y / c.w) - Imath::V2f(a.x / a.w, a.y / a.w)).length();
        Imath::M44f result;
        result.setTranslation(Imath::V3f(a.x / a.w, a.y / a.w, 0.0f));
        result.scale(Imath::V3f(x_scale, -y_scale, 1.0f));
        return result;
    }


} // namespace utility
} // namespace xstudio