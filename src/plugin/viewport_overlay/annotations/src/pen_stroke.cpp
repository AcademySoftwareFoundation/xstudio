// SPDX-License-Identifier: Apache-2.0
#include "pen_stroke.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

#define CIRC_PTS 16

namespace {
/*static const struct CircPts {

    std::vector <Imath::V3f> pts0;
    std::vector <Imath::V3f> pts1;

    CircPts(const int n) {
        for (int i = 0; i < n; ++i) {
            pts0.push_back(Imath::V3f(
                cos(float(i)*M_PI*2.0f/float(n)),
                sin(float(i)*M_PI*2.0f/float(n)),
                0.0f
                ));
            pts1.push_back(Imath::V3f(
                cos(float(i+1)*M_PI*2.0f/float(n)),
                sin(float(i+1)*M_PI*2.0f/float(n)),
                0.0f
                ));

        }
    }

} s_circ_pts(CIRC_PTS);*/

static const struct CircPts {

    std::vector<Imath::V2f> pts_;

    CircPts(const int n) {
        for (int i = 0; i < n + 1; ++i) {
            pts_.emplace_back(Imath::V2f(
                cos(float(i) * M_PI * 2.0f / float(n)),
                sin(float(i) * M_PI * 2.0f / float(n))));
        }
    }

} s_circ_pts(48);

} // namespace

PenStroke::PenStroke(
    const utility::ColourTriplet &colour, const float thickness, const float opacity)
    : thickness_(thickness), colour_(colour), opacity_(opacity) {}

PenStroke::PenStroke(const float thickness)
    : thickness_(thickness),
      colour_(1.0f, 1.0f, 1.0f),
      // opacity_(1.0f),
      is_erase_stroke_(true) {}

void PenStroke::add_point(const Imath::V2f &pt) {
    if (!(!points_.empty() && points_.back() == pt)) {
        points_.emplace_back(pt);
    }
}

inline float cross(const Imath::V3f &a, const Imath::V3f &b) { return a.x * b.y - b.x * a.y; }

inline float dot(const Imath::V3f &a, const Imath::V3f &b) { return a.x * b.x + b.y * a.y; }

inline bool line_intersection(
    const Imath::V3f &q,
    const Imath::V3f &q1,
    const Imath::V3f &p,
    const Imath::V3f &p1,
    Imath::V3f &pos) {

    if (q1 == p || p1 == q)
        return false;

    const Imath::V3f r = p1 - p;
    const Imath::V3f s = q1 - q;

    float cr = cross(r, s);
    if (cr == 0.0f)
        return false;

    float t = cross(q - p, s) / cr;
    float u = cross(q - p, r) / cr;

    if (t > 0.0f && t < 1.0f && u > 0.0f && u < 1.0f) {

        pos = p + r * t;
        return true;
    }
    return false;
}

int PenStroke::fetch_render_data(std::vector<Imath::V2f> &vertices) {
    if (!points_.empty()) {
        vertices.insert(vertices.end(), points_.begin(), points_.end());
        vertices.push_back(points_.back()); // repeat last point to make end 'cap'
        return points_.size() + 1;
    } else {
        return 0;
    }
}

void PenStroke::make_square(const Imath::V2f &corner1, const Imath::V2f &corner2) {
    points_ = std::vector<Imath::V2f>(
        {Imath::V2f(corner1.x, corner1.y),
         Imath::V2f(corner2.x, corner1.y),
         Imath::V2f(corner2.x, corner2.y),
         Imath::V2f(corner1.x, corner2.y),
         Imath::V2f(corner1.x, corner1.y)});
}

void PenStroke::make_circle(const Imath::V2f &origin, const float radius) {

    points_.clear();
    for (const auto &pt : s_circ_pts.pts_) {
        points_.push_back(origin + pt * radius);
    }
}

void PenStroke::make_arrow(const Imath::V2f &start, const Imath::V2f &end) {

    Imath::V2f v;
    if (start == end) {
        v = Imath::V2f(1.0f, 0.0f) * thickness_ * 4.0f;
    } else {
        v = (start - end).normalized() * std::max(thickness_ * 4.0f, 0.01f);
    }
    const Imath::V2f t(v.y, -v.x);

    points_.clear();
    points_.push_back(start);
    points_.push_back(end);
    points_.push_back(end + v + t);
    points_.push_back(end);
    points_.push_back(end + v - t);
}

void PenStroke::make_line(const Imath::V2f &start, const Imath::V2f &end) {
    points_.clear();
    points_.push_back(start);
    points_.push_back(end);
}