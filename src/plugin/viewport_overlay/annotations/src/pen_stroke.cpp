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
      opacity_(1.0f),
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
    vertices.insert(vertices.end(), points_.begin(), points_.end());
    return points_.size();
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

/*int PenStroke::fetch_render_data(
      const float depth,
      std::vector<Imath::V3f> &vertices,
      std::vector<float> &vertices_distance_from_line_centre,
      const bool fetch_fat_line_data)
{

    if (vertices_.empty()) {

        make_render_data(
            vertices_,
            vertex_dist_from_line_centre_,
            0.0f);

        make_render_data(
            fat_line_vertices_,
            fat_line_vertex_dist_from_line_centre_,
            100.0f/PEN_STROKE_THICKNESS_SCALE);

        vertex_depth_ = -1e6f;
    }

    if (depth != vertex_depth_) {
        vertex_depth_ = depth;
        for (auto & t: vertices_) {
            t.z = vertex_depth_;
        }
        for (auto & t: fat_line_vertices_) {
            t.z = vertex_depth_;
        }
    }

    int rt = 0;
    if (fetch_fat_line_data) {

        vertices.reserve(vertices.size() + std::distance(fat_line_vertices_.begin(),
fat_line_vertices_.end())); vertices.insert(vertices.end(), fat_line_vertices_.begin(),
fat_line_vertices_.end());

        vertices_distance_from_line_centre.reserve(
            vertices_distance_from_line_centre.size() + std::distance(
                fat_line_vertex_dist_from_line_centre_.begin(),
                fat_line_vertex_dist_from_line_centre_.end())
                );
        vertices_distance_from_line_centre.insert(
            vertices_distance_from_line_centre.end(),
            fat_line_vertex_dist_from_line_centre_.begin(),
            fat_line_vertex_dist_from_line_centre_.end()
            );

        rt = fat_line_vertex_dist_from_line_centre_.size();

    } else {

        vertices.reserve(vertices.size() + std::distance(vertices_.begin(), vertices_.end()));
        vertices.insert(vertices.end(), vertices_.begin(), vertices_.end());

        vertices_distance_from_line_centre.reserve(
            vertices_distance_from_line_centre.size() + std::distance(
                vertex_dist_from_line_centre_.begin(),
                vertex_dist_from_line_centre_.end())
                );
        vertices_distance_from_line_centre.insert(
            vertices_distance_from_line_centre.end(),
            vertex_dist_from_line_centre_.begin(),
            vertex_dist_from_line_centre_.end()
            );
        rt = vertex_dist_from_line_centre_.size();

    }
    return rt;

}*/

/*
void PenStroke::make_render_data(
    std::vector<Imath::V3f> &vtxs,
    std::vector<float> &vtxs_dist_from_centre,
    const float thickness_delta) const {

    vtxs.clear();
    vtxs.reserve(points_.size()*(3*CIRC_PTS + 6) + 3*CIRC_PTS);
    vtxs_dist_from_centre.clear();
    vtxs_dist_from_centre.reserve(points_.size()*(3*CIRC_PTS + 6) + 3*CIRC_PTS);
    const float thickness = thickness_delta + thickness_;

    auto ptr0 = points_.begin();
    auto ptr1 = points_.begin();
    ptr1++;
    if (ptr0 == points_.end()) {
        return;
    } else if (ptr1 == points_.end()) {
        const Imath::V3f & p0 = *ptr0;
        for (int i = 0; i < CIRC_PTS; ++i) {

            vtxs.push_back(p0);
            vtxs_dist_from_centre.push_back(0.0f);
            vtxs.push_back(p0 + s_circ_pts.pts0[i]*thickness);
            vtxs_dist_from_centre.push_back(thickness);
            vtxs.push_back(p0 + s_circ_pts.pts1[i]*thickness);
            vtxs_dist_from_centre.push_back(thickness);
        }
    }

    while (ptr1 != points_.end()) {

        const Imath::V3f & p0 = *ptr0;
        const Imath::V3f & p1 = *ptr1;

        for (int i = 0; i < CIRC_PTS; ++i) {

            vtxs.push_back(p0);
            vtxs_dist_from_centre.push_back(0.0f);
            vtxs.push_back(p0 + s_circ_pts.pts0[i]*thickness);
            vtxs_dist_from_centre.push_back(thickness);
            vtxs.push_back(p0 + s_circ_pts.pts1[i]*thickness);
            vtxs_dist_from_centre.push_back(thickness);
        }

        Imath::V3f v = p1 - p0;
        Imath::V3f t = v.normalized()*thickness;
        Imath::V3f tr(-t.y,t.x,0.0f);

        Imath::V3f q0 = p0+tr;
        Imath::V3f q1 = q0+v;
        Imath::V3f q2 = q1-tr*2;
        Imath::V3f q3 = q2-v;

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(q0);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(q1);
        vtxs_dist_from_centre.push_back(thickness);

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(q1);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(p1);
        vtxs_dist_from_centre.push_back(0.0);

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(q3);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(q2);
        vtxs_dist_from_centre.push_back(thickness);

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(p1);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(q2);
        vtxs_dist_from_centre.push_back(thickness);

        ptr0++;
        ptr1++;
    }

    // end point
    const Imath::V3f & p0 = *ptr0;
    for (int i = 0; i < CIRC_PTS; ++i) {

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(p0 + s_circ_pts.pts0[i]*thickness);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(p0 + s_circ_pts.pts1[i]*thickness);
        vtxs_dist_from_centre.push_back(thickness);

    }

}*/

/*void PenStroke::make_render_data(
    std::vector<Imath::V3f> &vtxs,
    std::vector<float> &vtxs_dist_from_centre,
    const float thickness_delta) const {

    std::cerr << "make_render_data in\n";
    auto ptr0 = points_.begin();
    auto ptr1 = points_.begin();
    ptr1++;
    auto ptr2 = ptr1;
    ptr2++;

    vtxs.clear();
    vtxs.reserve(points_.size()*(3*CIRC_PTS + 6) + 3*CIRC_PTS);
    vtxs_dist_from_centre.clear();
    vtxs_dist_from_centre.reserve(points_.size()*(3*CIRC_PTS + 6) + 3*CIRC_PTS);

    if (ptr0 == points_.end() || ptr1 == points_.end() || ptr2 == points_.end()) {
           std::cerr << "make_render_data out 0\n";

        return;
    }

    const float thickness = thickness_delta + thickness_;

    Imath::V3f R0, R1, R2, R3, S0, S1, S2, S3, PR0, PR1;
    Imath::V3f tr1, v1;
    {
        const Imath::V3f & p0 = *ptr0;
        const Imath::V3f & p1 = *ptr1;
        const Imath::V3f & p2 = *ptr2;

        v1 = p1 - p0;
        Imath::V3f t1 = v1.normalized()*thickness;
        tr1 = Imath::V3f(-t1.y,t1.x,0.0f);

        Imath::V3f v2 = p2 - p1;
        Imath::V3f t2 = v2.normalized()*thickness;
        Imath::V3f tr2(-t2.y,t2.x,0.0f);

        R0 = p0-tr1;
        R1 = p0+tr1;
        R2 = p1+tr1;
        R3 = p1-tr1;

        PR0 = p0-tr1;
        PR1 = p0+tr1;

    }

    while (ptr2 != points_.end()) {

        const Imath::V3f & p0 = *ptr0;
        const Imath::V3f & p1 = *ptr1;
        const Imath::V3f & p2 = *ptr2;

        Imath::V3f v2 = p2 - p1;
        if (v2.length() < 0.0001) {
            ptr0++;
            ptr1++;
            ptr2++;
            std::cerr << ".";
            continue;
        }
        Imath::V3f t2 = v2.normalized()*thickness;
        Imath::V3f tr2(-t2.y,t2.x,0.0f);

        S0 = p1-tr2;
        S1 = p1+tr2;
        S2 = p2+tr2;
        S3 = p2-tr2;

        float ang = acos(dot(v2.normalized(), v1.normalized()));
        bool clockwise_elbow = false;
        Imath::V3f Q1, Q2;

        if (!line_intersection(R0, R3, S0, S3, Q1)) {

            Imath::V3f tr = tr1;
            int nstep = std::min(int(fabs(ang*180/M_PI)/20.0f)+1,8);
            if (nstep) {
                const float cos_step = cos((ang)/nstep);
                const float sin_step = sin((ang)/nstep);
                for (int i = 0; i < nstep; ++i) {
                    vtxs.push_back(p1);
                    vtxs_dist_from_centre.push_back(0.0);
                    vtxs.push_back(p1 - tr);
                    vtxs_dist_from_centre.push_back(thickness);
                    Imath::V3f tr2(cos_step*tr.x - sin_step*tr.y, sin_step*tr.x + cos_step*tr.y,
0.0f); vtxs.push_back(p1 - tr2); vtxs_dist_from_centre.push_back(thickness); tr = tr2;
                }
            }
            Q1 = R3;
            clockwise_elbow = true;

        }

        if (!line_intersection(R1, R2, S1, S2, Q2)) {
            Imath::V3f tr = tr1;
            int nstep = std::min(int(fabs(ang*180/M_PI)/20.0f)+1,8);
            if (nstep) {
                const float cos_step = cos(-(ang)/nstep);
                const float sin_step = sin(-(ang)/nstep);
                for (int i = 0; i < nstep; ++i) {
                    vtxs.push_back(p1 + tr);
                    vtxs_dist_from_centre.push_back(thickness);
                    vtxs.push_back(p1);
                    vtxs_dist_from_centre.push_back(0.0);
                    Imath::V3f tr2(cos_step*tr.x - sin_step*tr.y, sin_step*tr.x + cos_step*tr.y,
0.0f); vtxs.push_back(p1 + tr2); vtxs_dist_from_centre.push_back(thickness); tr = tr2;
                }
            }
            Q2 = R2;

        }

        tr1 = tr2;
        v1 = v2;

        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(PR1);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(Q2);
        vtxs_dist_from_centre.push_back(thickness);

        vtxs.push_back(Q2);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(p1);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);

        vtxs.push_back(PR0);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(p0);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(p1);
        vtxs_dist_from_centre.push_back(0.0);

        vtxs.push_back(p1);
        vtxs_dist_from_centre.push_back(0.0);
        vtxs.push_back(PR0);
        vtxs_dist_from_centre.push_back(thickness);
        vtxs.push_back(Q1);
        vtxs_dist_from_centre.push_back(thickness);

        if (clockwise_elbow) {
            PR0 = S0;
            PR1 = Q2;
        } else {
            PR0 = Q1;
            PR1 = S1;
        }

        R0=S0;
        R1=S1;
        R2=S2;
        R3=S3;

        ptr0++;
        ptr1++;
        ptr2++;
    }
    std::cerr << "make_render_data out\n";

}*/