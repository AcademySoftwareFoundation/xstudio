// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>
#include "xstudio/utility/json_store.hpp"

// If a pen stroke has thickness of 1, it will be 1 pixel thick agains
// an image that 3860 pixels in width.
#define PEN_STROKE_THICKNESS_SCALE 3860.0f


namespace xstudio {
namespace ui {
    namespace canvas {

        enum StrokeType { StrokeType_Pen, StrokeType_Brush, StrokeType_Erase };
        class Stroke;

        struct Stroke {

            // enum class Type { Unknown, Pen, Brush, Erase };

            struct Point {
                Imath::V2f pos;
                float pressure;

                bool operator==(const Point &o) const {
                    return pos == o.pos && pressure == o.pressure;
                }

                // Point(Imath::V2f pos, float pressure) :
                //     pos(pos), pressure(pressure)
                //     {}
            };

            float opacity{1.0f};
            float thickness{0.0f};
            float softness{0.0f};
            float size_sensitivity{0.0f};
            float opacity_sensitivity{0.0f};

            utility::ColourTriplet colour;
            // Type type{Type::Unknown};
            StrokeType type{StrokeType_Pen};
            std::vector<Point> points;

            static Stroke *
            Pen(const utility::ColourTriplet &colour,
                const float thickness,
                const float softness,
                const float opacity);

            static Stroke * Brush(
                const utility::ColourTriplet &colour,
                const float thickness,
                const float softness,
                const float opacity,
                const float size_sensitivity,
                const float opacity_sensitivity);

            static Stroke * Erase(const float thickness);

            bool operator==(const Stroke &o) const;

            std::string hash() const;

            // TODO: Below are shapes and should be extracted to dedicated types
            // Rendering them as stroke seems like an implementation details and
            // will probably not hold if we need filled shape for example.
            void make_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

            void make_circle(const Imath::V2f &origin, const float radius);

            void make_arrow(const Imath::V2f &start, const Imath::V2f &end);

            void make_line(const Imath::V2f &start, const Imath::V2f &end);

            void add_point(const Imath::V2f &pt, float pressure = 1.0f);
        };

        void from_json(const nlohmann::json &j, Stroke &s);
        void to_json(nlohmann::json &j, const Stroke &s);

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio

namespace std {
template <> struct hash<xstudio::ui::canvas::Stroke> {
    std::size_t operator()(const xstudio::ui::canvas::Stroke &item) const {
        std::hash<std::string> hasher;
        return hasher(item.hash());
    }
};
} // namespace std