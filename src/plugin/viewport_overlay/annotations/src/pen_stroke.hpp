// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include <Imath/ImathVec.h>


// If a pen stroke has thickness of 1, it will be 1 pixel thick agains
// an image that 3860 pixels in width.
#define PEN_STROKE_THICKNESS_SCALE 3860.0f

namespace xstudio {
namespace ui {
    namespace viewport {

        class PenStroke {

          public:
            PenStroke(
                const utility::ColourTriplet &colour,
                const float thickness,
                const float opacity);
            PenStroke(const float thickness);
            PenStroke(const PenStroke &o) = default;
            PenStroke() {}

            PenStroke &operator=(const PenStroke &o) = default;

            bool operator==(const PenStroke &o) const {
                return (
                    points_ == o.points_ && opacity_ == o.opacity_ &&
                    thickness_ == o.thickness_ && is_erase_stroke_ == o.is_erase_stroke_ &&
                    colour_ == o.colour_);
            }

            void add_point(const Imath::V2f &pt);

            int fetch_render_data(std::vector<Imath::V2f> &vertices);

            void make_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

            void make_circle(const Imath::V2f &origin, const float radius);

            void make_arrow(const Imath::V2f &start, const Imath::V2f &end);

            void make_line(const Imath::V2f &start, const Imath::V2f &end);

            /*int fetch_render_data(
              const float depth,
              std::vector<Imath::V3f> &vertices,
              std::vector<float> &vertices_distance_from_line_centre,
              const bool fetch_fat_line_data);*/

            std::vector<Imath::V2f> points_;
            float opacity_        = {1.0f};
            float thickness_      = {0.0f};
            bool is_erase_stroke_ = {false};
            utility::ColourTriplet colour_;

          private:
            /*void make_render_data(
              std::vector<Imath::V3f> &vtxs,
              std::vector<float> &vtxs_dist_from_centre,
              const float thickness_delta) const;

            std::vector<Imath::V3f> vertices_;
            std::vector<float> vertex_dist_from_line_centre_;

            std::vector<Imath::V3f> fat_line_vertices_;
            std::vector<float> fat_line_vertex_dist_from_line_centre_;

            float vertex_depth_;*/

            friend class AnnotationsRenderer;
            friend class AnnotationSerialiser;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio