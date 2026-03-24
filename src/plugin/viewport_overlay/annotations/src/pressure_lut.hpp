// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>


namespace xstudio {
namespace ui {
    namespace viewport {

        /**
         * PressureLUT Class
         *
         * A pressure mapping class that supports both direct LUT loading and bezier curve
         * generation.
         *
         * Usage:
         *   PressureLUT lut;
         *
         *   // Map a pressure value
         *   float output = lut.map(0.5f);
         *
         *   // Set LUT directly
         *   std::vector<float> data = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
         *   lut.set_lut(data);
         *
         *   // Or generate from bezier curve
         *   PressureLUT::Curve curve;
         *   curve.xSat = 0.5f;
         *   curve.P1 = {0.25f, 0.0f};
         *   curve.P2 = {-0.15f, 0.0f};
         *   lut.set_curve(curve);
         */

        class PressureLUT {
          public:
            enum class Mode { Curve, LUT };

            struct Point {
                float dx = 0.0f;
                float dy = 0.0f;
            };

            struct Curve {
                float xSat = 0.75f;
                Point P1   = {0.75f, 0.0f};
                Point P2   = {-0.25f, 0.0f};
            };

            struct BezierPoint {
                float x;
                float y;
            };

            // Constructor
            explicit PressureLUT(size_t size = 512) : lut_(size), mode_(Mode::Curve) {
                bake_lut();
            }

            // ============================================================
            // PUBLIC API - MAPPING
            // ============================================================

            /**
             * Map an input pressure value (0-1) to output (0-1) using the LUT
             */
            float map(float pressure) const {
                float p        = std::clamp(pressure, 0.0f, 1.0f);
                float x        = p * (lut_.size() - 1);
                size_t i       = static_cast<size_t>(std::floor(x));
                float t        = x - i;
                size_t nextIdx = std::min(i + 1, lut_.size() - 1);
                return lut_[i] * (1.0f - t) + lut_[nextIdx] * t;
            }

            // ============================================================
            // PUBLIC API - LUT
            // ============================================================

            /**
             * Get a copy of the current LUT
             */
            std::vector<float> get_lut() const { return lut_; }

            /**
             * Get pointer to LUT data (for direct access)
             */
            const float *get_lut_data() const { return lut_.data(); }

            /**
             * Set the LUT directly from a vector
             * This switches mode to 'LUT' (curve parameters become stale)
             */
            void set_lut(const std::vector<float> &lut) {
                if (lut.size() < 2) {
                    throw std::invalid_argument("PressureLUT: LUT must have at least 2 values");
                }
                lut_  = lut;
                mode_ = Mode::LUT;
            }

            /**
             * Set the LUT from raw array
             */
            void set_lut(const float *data, size_t size) {
                if (size < 2) {
                    throw std::invalid_argument("PressureLUT: LUT must have at least 2 values");
                }
                lut_.assign(data, data + size);
                mode_ = Mode::LUT;
            }

            /**
             * Get the current LUT size
             */
            size_t get_size() const { return lut_.size(); }

            /**
             * Set LUT size and regenerate from curve (only works in curve mode)
             */
            void set_size(size_t size) {
                lut_.resize(std::max(size_t(2), size));
                if (mode_ == Mode::Curve) {
                    bake_lut();
                }
            }

            // ============================================================
            // PUBLIC API - CURVE
            // ============================================================

            /**
             * Get the current bezier curve parameters
             */
            Curve get_curve() const { return curve_; }

            /**
             * Set bezier curve parameters and regenerate LUT
             * This switches mode to 'Curve'
             */
            void set_curve(const Curve &curve) {
                curve_.xSat  = std::clamp(curve.xSat, 0.01f, 1.0f);
                curve_.P1.dx = curve.P1.dx;
                curve_.P1.dy = curve.P1.dy;
                curve_.P2.dx = curve.P2.dx;
                curve_.P2.dy = curve.P2.dy;

                // Enforce constraints
                curve_.P1.dx = std::clamp(curve_.P1.dx, 0.0f, curve_.xSat - 0.001f);
                curve_.P1.dy = std::clamp(curve_.P1.dy, 0.0f, 1.0f);
                curve_.P2.dx = std::clamp(curve_.P2.dx, -curve_.xSat, -0.001f);
                curve_.P2.dy = std::clamp(curve_.P2.dy, -1.0f, 0.0f);

                mode_ = Mode::Curve;

                bake_lut();
            }

            /**
             * Get the current mode
             */
            Mode get_mode() const { return mode_; }

            // ============================================================
            // BEZIER MATH
            // ============================================================

            /**
             * Evaluate the bezier curve at parameter t (0-1)
             * Returns {x, y} in curve space (both 0-1)
             */
            BezierPoint eval_bezier(float t) const {
                float u   = 1.0f - t;
                float p2x = curve_.xSat + curve_.P2.dx;
                float p2y = 1.0f + curve_.P2.dy;

                BezierPoint result;
                result.x = 3.0f * u * u * t * curve_.P1.dx + 3.0f * u * t * t * p2x +
                           t * t * t * curve_.xSat;
                result.y = 3.0f * u * u * t * curve_.P1.dy + 3.0f * u * t * t * p2y + t * t * t;
                return result;
            }

            /**
             * Sample the curve at a given x value (0-1)
             * Returns y value (0-1)
             */
            float sample_curve(float x) const {
                if (x >= curve_.xSat)
                    return 1.0f;
                if (x <= 0.0f)
                    return 0.0f;

                // Binary search for t that gives us the target x
                float lo = 0.0f, hi = 1.0f, t = 0.5f;
                for (int i = 0; i < 16; i++) {
                    t = (lo + hi) * 0.5f;
                    if (eval_bezier(t).x < x) {
                        lo = t;
                    } else {
                        hi = t;
                    }
                }
                return std::clamp(eval_bezier(t).y, 0.0f, 1.0f);
            }

          private:
            void bake_lut() {
                for (size_t i = 0; i < lut_.size(); i++) {
                    float x = static_cast<float>(i) / (lut_.size() - 1);
                    lut_[i] = sample_curve(x);
                }
            }

            std::vector<float> lut_;
            Curve curve_;
            Mode mode_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
