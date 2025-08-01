// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>

#include "xstudio/utility/json_store.hpp"
#include "xstudio/ui/font.hpp"


namespace xstudio {
namespace ui {
    namespace canvas {

        struct Caption {

            // JSON serialisation requires default constructible types
            Caption() = default;

            Caption(
                const Imath::V2f position,
                const float wrap_width,
                const float font_size,
                const utility::ColourTriplet colour,
                const float opacity,
                const Justification justification,
                const std::string font_name,
                const utility::ColourTriplet background_colour,
                const float background_opacity);

            bool operator==(const Caption &o) const;

            void modify_text(const std::string &t, std::string::const_iterator &cursor);

            Imath::Box2f bounding_box() const;

            std::vector<float> vertices() const;

            std::string hash() const;

            std::string text;
            Imath::V2f position;
            float wrap_width;
            float font_size;
            std::string font_name;
            utility::ColourTriplet colour{utility::ColourTriplet(1.0f, 1.0f, 1.0f)};
            float opacity;
            Justification justification;
            utility::ColourTriplet background_colour = {
                utility::ColourTriplet(0.0f, 0.0f, 0.0f)};
            float background_opacity;

          private:
            std::string caption_hash() const;
            void update_vertices() const;

            mutable std::string hash_;
            mutable Imath::Box2f bounding_box_;
            mutable std::vector<float> vertices_;
        };

        void from_json(const nlohmann::json &j, Caption &c);
        void to_json(nlohmann::json &j, const Caption &c);

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio
