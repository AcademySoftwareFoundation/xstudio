// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

// If a pen stroke has thickness of 1, it will be 1 pixel thick agains
// an image that 3840 pixels in width.
#define PEN_STROKE_THICKNESS_SCALE 3840.0f


namespace xstudio {
namespace ui {
    namespace canvas {

        enum StrokeType { StrokeType_Pen, StrokeType_Brush, StrokeType_Erase };
        class Stroke;

        struct Stroke {

            // enum class Type { Unknown, Pen, Brush, Erase };

            Stroke() : _id(utility::Uuid::generate()) {}
            Stroke(const Stroke &o) = default;

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

            static Stroke *
            Pen(const utility::ColourTriplet &colour,
                const float thickness,
                const float softness,
                const float opacity);

            static Stroke *Brush(
                const utility::ColourTriplet &colour,
                const float thickness,
                const float softness,
                const float opacity,
                const float size_sensitivity,
                const float opacity_sensitivity);

            static Stroke *Erase(const float thickness);

            bool operator==(const Stroke &o) const;

            // TODO: Below are shapes and should be extracted to dedicated types
            // Rendering them as stroke seems like an implementation details and
            // will probably not hold if we need filled shape for example.
            void make_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

            void make_circle(const Imath::V2f &origin, const float radius);

            void make_arrow(const Imath::V2f &start, const Imath::V2f &end);

            void make_line(const Imath::V2f &start, const Imath::V2f &end);

            void add_point(const Imath::V2f &pt, float pressure = 1.0f);

            const std::vector<Point> &points() const { return _points; }

            bool fade(const float fade_amount);

            [[nodiscard]] float opacity() const { return _opacity; }
            [[nodiscard]] float thickness() const { return _thickness; }
            [[nodiscard]] float softness() const { return _softness; }
            [[nodiscard]] float size_sensitivity() const { return _size_sensitivity; }
            [[nodiscard]] float opacity_sensitivity() const { return _opacity_sensitivity; }
            [[nodiscard]] StrokeType type() const { return _type; }
            [[nodiscard]] size_t hash() const { return _hash; }
            const utility::ColourTriplet &colour() const { return _colour; }
            const utility::Uuid &id() const { return _id; }

            friend void from_json(const nlohmann::json &j, Stroke &s);
            friend void to_json(nlohmann::json &j, const Stroke &s);

          private:
            void update_hash(const bool update_with_last_point_only = false);

            size_t _hash{0};
            float _opacity{1.0f};
            float _thickness{0.0f};
            float _softness{0.0f};
            float _size_sensitivity{0.0f};
            float _opacity_sensitivity{0.0f};
            utility::Uuid _id;

            utility::ColourTriplet _colour;
            // Type type{Type::Unknown};
            StrokeType _type{StrokeType_Pen};
            std::vector<Point> _points;
        };

        void from_json(const nlohmann::json &j, Stroke &s);
        void to_json(nlohmann::json &j, const Stroke &s);


    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio

/*namespace std {
template <> struct hash<xstudio::ui::canvas::Stroke> {
    std::size_t operator()(const xstudio::ui::canvas::Stroke &item) const {
        std::hash<std::string> hasher;
        return hasher(item.hash());
    }
};
} // namespace std*/