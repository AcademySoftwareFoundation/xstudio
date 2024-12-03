// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>
#include "xstudio/utility/json_store.hpp"


namespace xstudio {
namespace ui {
    namespace canvas {

        // Generic 4 sided polygon
        struct Quad {

            Imath::V2f bl;
            Imath::V2f tl;
            Imath::V2f tr;
            Imath::V2f br;

            utility::ColourTriplet colour;

            float softness{0.0f};
            float opacity{1.0f};

            bool invert{false};

            // Field used by Canvas to track shapes
            // Do not participate in equality tests
            uint32_t _id;

            bool operator==(const Quad &o) const;
        };

        void from_json(const nlohmann::json &j, Quad &s);
        void to_json(nlohmann::json &j, const Quad &s);

        struct Polygon {

            std::vector<Imath::V2f> points;

            utility::ColourTriplet colour;

            float softness{0.0f};
            float opacity{1.0f};

            bool invert{false};

            // Field used by Canvas to track shapes
            // Do not participate in equality tests
            uint32_t _id;

            bool operator==(const Polygon &o) const;
        };

        void from_json(const nlohmann::json &j, Polygon &s);
        void to_json(nlohmann::json &j, const Polygon &s);

        struct Ellipse {

            Imath::V2f center;
            Imath::V2f radius;
            float angle{0.0f};

            utility::ColourTriplet colour;

            float softness{0.0f};
            float opacity{1.0f};

            bool invert{false};

            // Field used by Canvas to track shapes
            // Do not participate in equality tests
            uint32_t _id;

            bool operator==(const Ellipse &o) const;
        };

        void from_json(const nlohmann::json &j, Ellipse &s);
        void to_json(nlohmann::json &j, const Ellipse &s);

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio