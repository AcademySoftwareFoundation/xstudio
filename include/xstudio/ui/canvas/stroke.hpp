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

        enum StrokeType { StrokeType_Pen, StrokeType_Erase };

        struct Stroke {

            float opacity{1.0f};
            float thickness{0.0f};
            float softness{0.0f};
            utility::ColourTriplet colour;
            StrokeType type{StrokeType_Pen};
            std::vector<Imath::V2f> points;

            static Stroke
            Pen(const utility::ColourTriplet &colour,
                const float thickness,
                const float softness,
                const float opacity);

            static Stroke Erase(const float thickness);

            bool operator==(const Stroke &o) const;

            // TODO: Below are shapes and should be extracted to dedicated types
            // Rendering them as stroke seems like an implementation details and
            // will probably not hold if we need filled shape for example.
            void make_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

            void make_circle(const Imath::V2f &origin, const float radius);

            void make_arrow(const Imath::V2f &start, const Imath::V2f &end);

            void make_line(const Imath::V2f &start, const Imath::V2f &end);

            void add_point(const Imath::V2f &pt);

            std::vector<Imath::V2f> vertices() const;

          private:
            mutable std::vector<Imath::V2f> vertices_;
        };

        void from_json(const nlohmann::json &j, Stroke &s);
        void to_json(nlohmann::json &j, const Stroke &s);

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio