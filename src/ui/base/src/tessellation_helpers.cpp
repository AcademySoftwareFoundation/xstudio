// SPDX-License-Identifier: Apache-2.0
#include <Imath/ImathVec.h>
#include <algorithm>
#include <map>
#include <vector>

namespace xstudio::ui::tesslator {

struct TessellationVertex {

    TessellationVertex(TessellationVertex *n0, const Imath::V2f *pos, TessellationVertex *n1)
        : n0_(n0), pos_(pos), n1_(n1) {}
    TessellationVertex() = default;

    TessellationVertex *n0_{nullptr};
    TessellationVertex *n1_{nullptr};
    const Imath::V2f *pos_{nullptr};
    int index_{0};
    bool tessellated_{false};
};

inline bool neq(const Imath::V2f &a, const Imath::V2f &b) {

    return !(fabs(a.x - b.x) < 0.0001f && fabs(a.y - b.y) < 0.0001f);
}

inline float sign(const Imath::V2f &p1, const Imath::V2f &p2, const Imath::V2f &p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}


inline float tcross(const Imath::V2f &a, const Imath::V2f &b, const Imath::V2f &c) {

    Imath::V2f A = (b - a).normalized();
    Imath::V2f B = (c - b).normalized();
    return (A.x * B.y - A.y * B.x);
}

inline bool PointInTriangle(
    const Imath::V2f &pt, const Imath::V2f &v1, const Imath::V2f &v2, const Imath::V2f &v3) {
    if (&pt == &v1 || &pt == &v2 || &pt == &v3)
        return false;

    if (pt.x < std::min(v1.x, std::min(v2.x, v3.x)) ||
        pt.y < std::min(v1.y, std::min(v2.y, v3.y)) ||
        pt.x > std::max(v1.x, std::max(v2.x, v3.x)) ||
        pt.y > std::max(v1.y, std::max(v2.y, v3.y)))
        return false;

    bool b1, b2, b3;

    const float f1 = sign(pt, v1, v2);
    const float f2 = sign(pt, v2, v3);
    const float f3 = sign(pt, v3, v1);

    if (f1 == 0.0f || f2 == 0.0f || f3 == 0.0f)
        return true;

    b1 = f1 < 0.0f;
    b2 = f2 < 0.0f;
    b3 = f3 < 0.0f;

    return ((b1 == b2) && (b2 == b3));
}

inline bool inTri(const TessellationVertex &vtx, const std::vector<TessellationVertex> &vtxs) {

    for (const auto &i : vtxs) {

        if (!i.tessellated_ &&
            PointInTriangle(*(i.pos_), *(vtx.n0_->pos_), *(vtx.pos_), *(vtx.n1_->pos_))) {

            return true;
        }
    }

    return false;
}

inline bool p_test(const TessellationVertex &vtx) {

    return tcross(*(vtx.n0_->pos_), *(vtx.pos_), *(vtx.n1_->pos_)) <= 0.0f;
}

inline bool p_test2(const TessellationVertex &vtx) {

    return tcross(*(vtx.n0_->pos_), *(vtx.pos_), *(vtx.n1_->pos_)) >= 0.0f;
}

void makeTessellationVertices(
    std::vector<TessellationVertex> &vertex_pointers,
    const std::vector<Imath::V2f>::iterator shape_outline_begin,
    const std::vector<Imath::V2f>::iterator shape_outline_end) {

    const size_t num_vtx = std::distance(shape_outline_begin, shape_outline_end);
    vertex_pointers.clear();
    vertex_pointers.resize(num_vtx);

    int idx = 0;
    int j   = 0;
    for (auto i = shape_outline_begin; i != shape_outline_end; i++) {

        const auto *vtx_pos = (const Imath::V2f *)&(*i);

        if (idx && neq(*vtx_pos, *(vertex_pointers[idx - 1].pos_))) {

            vertex_pointers[idx].n0_          = &vertex_pointers[idx - 1];
            vertex_pointers[idx].pos_         = vtx_pos;
            vertex_pointers[idx].tessellated_ = false;
            vertex_pointers[idx].index_       = j;
            vertex_pointers[idx - 1].n1_      = &vertex_pointers[idx];
            idx++;

        } else if (!idx) {

            vertex_pointers[idx].pos_         = vtx_pos;
            vertex_pointers[idx].tessellated_ = false;
            vertex_pointers[idx].index_       = j;
            idx++;
        }
        j++;
    }

    vertex_pointers.resize(idx);

    if (vertex_pointers.size() <= 1)
        return;

    if (!neq(*(vertex_pointers.back().pos_), *(vertex_pointers[0].pos_))) {
        vertex_pointers.pop_back();
    }

    vertex_pointers.back().n1_ = &(vertex_pointers[0]);
    vertex_pointers[0].n0_     = &(vertex_pointers.back());
}

bool get_shape_winding_direction(
    const std::vector<Imath::V2f>::iterator shape_outline_begin,
    const std::vector<Imath::V2f>::iterator shape_outline_end) {

    // We need to work out whether the normal at the shape boundary (formed using
    // the order of vertices in shape_outline_vertices) points into the filled part
    // of the shape or the outside .... to work out the winding
    // order we find the boundary vertex that is leftmost (in screen space) and
    // compute the boundary normal for that vertex (just using the neighbouring
    // points either side in the boundary loop). We know that any point to the
    // left of that point *must* be outside the shape and hence we can work out
    // the general winding direction that is required for other parts of the
    // algorithm.

    std::vector<Imath::V2f>::iterator leftmost;
    float leftmost_vtx_pos = std::numeric_limits<float>::max();

    int idx = 0;
    for (auto i = shape_outline_begin; i != shape_outline_end; i++) {

        const float vtx_x_pos = (*i).x;
        if (vtx_x_pos < leftmost_vtx_pos) {
            leftmost         = i;
            leftmost_vtx_pos = vtx_x_pos;
        }
        idx++;
    }

    {
        // safety test to check that all vertices aren't the same - if they are (can happen
        // with shapes that have one CV) we need to skip the rest of the algorithm
        auto r = shape_outline_begin;
        while (*r == *shape_outline_begin && r != shape_outline_end) {
            r++;
        }
        if (r == shape_outline_end)
            return true;
    }

    auto leftmost0 = leftmost; // this will be vertex before lefmost in the loop
    auto leftmost1 = leftmost; // this will be vertex after the leftmost in the loop

    // step leftmost0 one backwards, accounting for going off start and
    // coming back at the end
    if (leftmost0 == shape_outline_begin) {
        leftmost0 = shape_outline_end;
        leftmost0--;
    } else {
        leftmost0--;
    }

    // step leftmost1 one forwards, accounting for going off end and
    // coming back at the start
    leftmost1++;
    if (leftmost1 == shape_outline_end) {
        leftmost1 = shape_outline_begin;
    }

    // quite simply, the 'direction of travel' in the y direction as
    // we walk through this leftmost vertex gives us the winding order
    return ((*leftmost - *leftmost0).normalized() - (*leftmost - *leftmost1).normalized()).y >
           0;
}


bool tessellate_vtx_loop_nil(
    std::vector<unsigned int> &result,
    const std::vector<Imath::V2f>::iterator shape_outline_begin,
    const std::vector<Imath::V2f>::iterator shape_outline_end,
    const int offset) {

    const bool clockwisw = get_shape_winding_direction(shape_outline_begin, shape_outline_end);

    // This algorithm is simple and pretty fast
    //
    // We make a list of vertiex pointers, each pointer also pointing to neighbouring
    // vertices in the loop.
    //
    // 1) We pick a vertex that has not been tesselated (they all begin as untesselated)
    // 2) We form a triangle by joining the vertex with it's neighbours to either side
    //     (from the set of untesselated vertices) in the loop
    // 3) We test the triangle to see if any vertices that have been tessalated end up
    // INSIDE
    //     the candidate triangle -
    //     - if this happens the triangle is not valid (because we must be at a concave
    //     segment of the loop) and we move on to the next vertex in the loop (returning to
    //     step 1)
    //     - if it does not happen, the triangle is added to the result and the vertex is
    //     marked as tesselated. The untesselated vertices that were neighbouring this
    //     vertex are adjusted so that they no longer point to the vertex that was just
    //     tesselated but instead to the next neighbouring vertex in the untesselated set.
    //
    // Try doing this with pencil and paper to see how it works!
    //

    std::vector<TessellationVertex> vertex_pointers;

    makeTessellationVertices(vertex_pointers, shape_outline_begin, shape_outline_end);

    int numUntessellatedVtxs = vertex_pointers.size();

    while (numUntessellatedVtxs > 2) {

        int idx1 = 0; // idx;
        int idx2 = 0;

        while (idx1 < numUntessellatedVtxs) {

            if (!vertex_pointers[idx2].tessellated_) {

                if ((clockwisw & p_test(vertex_pointers[idx2])) ||
                    (!clockwisw && p_test2(vertex_pointers[idx2]))) {

                    if (!inTri(vertex_pointers[idx2], vertex_pointers)) {

                        result.push_back(vertex_pointers[idx2].n0_->index_ + offset);
                        result.push_back(vertex_pointers[idx2].index_ + offset);
                        result.push_back(vertex_pointers[idx2].n1_->index_ + offset);

                        vertex_pointers[idx2].tessellated_ = true;
                        vertex_pointers[idx2].n0_->n1_     = vertex_pointers[idx2].n1_;
                        vertex_pointers[idx2].n1_->n0_     = vertex_pointers[idx2].n0_;
                        numUntessellatedVtxs--;
                        break;
                    }
                }

                idx1++;
            }
            idx2++;
        }
        if (idx1 == numUntessellatedVtxs) {
            break;
        }
    }

    return clockwisw;
}


inline bool distToPoint(
    float &r,
    const Imath::V2f &pt0,
    const Imath::V2f &pt1,
    const Imath::V2f &pt1_norm1,
    bool &parity) {
    const Imath::V2f a = pt0 - pt1;

    float result = a.length();
    if (result < r) {
        r      = result;
        parity = (a.x * pt1_norm1.x + a.y * pt1_norm1.y) < 0;
        return true;
    }
    return false;
}

inline float distToLine(
    const float r,
    const Imath::V2f &pt,
    const Imath::V2f &line_bg,
    const Imath::V2f &line_end,
    bool &parity) {

    if (line_bg == line_end)
        return 1e6f;

    const Imath::V2f bdbmin(std::min(line_bg.x, line_end.x), std::min(line_bg.y, line_end.y));
    const Imath::V2f bdbmax(std::max(line_bg.x, line_end.x), std::max(line_bg.y, line_end.y));

    if (pt.x < bdbmin.x && (bdbmin.x - pt.x) > r)
        return 1e6f;
    if (pt.x > bdbmax.x && (pt.x - bdbmax.x) > r)
        return 1e6f;
    if (pt.y < bdbmin.y && (bdbmin.y - pt.y) > r)
        return 1e6f;
    if (pt.y > bdbmax.y && (pt.y - bdbmax.y) > r)
        return 1e6f;

    float t = (pt.x - line_bg.x) * (line_end.x - line_bg.x) +
              (pt.y - line_bg.y) * (line_end.y - line_bg.y);

    const Imath::V2f a = pt - line_bg;
    const Imath::V2f L = (line_end - line_bg);
    const Imath::V2f b = L.normalized();

    const float dot = (a.x * b.x + a.y * b.y);

    float result;
    if (dot <= 0) {
        result = (pt - line_bg).length();
    } else if (dot > L.length()) {
        result = (pt - line_end).length();
    } else {
        Imath::V2f np = line_bg + b * dot;
        result        = (pt - np).length();
    }

    parity = (a.x * b.y - a.y * b.x) > 0;
    return result;
}

inline Imath::V2f
point_normal(const Imath::V2f prev_pt, const Imath::V2f pt, const Imath::V2f next_pt) {
    const Imath::V2f v0 = (pt - prev_pt).normalized();
    const Imath::V2f v1 = (next_pt - pt).normalized();

    return (Imath::V2f(v0.y, -v0.x) + Imath::V2f(v1.y, -v1.x)).normalized();
}


void make_glyph_sdf(
    std::vector<float> &buffer,
    const int buffer_width,
    const int buffer_height,
    const Imath::V2f bdb_bottom_left,
    const Imath::V2f bdb_top_right,
    const std::vector<Imath::V2f>::const_iterator shape_outline_begin,
    const std::vector<Imath::V2f>::const_iterator shape_outline_end) {

    // General note - this algorithm is not efficient at all, as we are testing
    // every pixel in the output buffer against every point and every line
    // in the shape outline. There must be a few ways to make this much faster
    // using bounding boxes and sorting of shape point positions.

    const float x_norm = float(buffer_width) / (bdb_top_right.x - bdb_bottom_left.x);
    const float y_norm = float(buffer_height) / (bdb_top_right.y - bdb_bottom_left.y);
    const float norm_x = 1.0f / x_norm;
    const float norm_y = 1.0f / y_norm;

    {

        // step 1 - compute the distance for each pixel to the nearest POINT
        // in the shape outline. We use the point normal to determine if the
        // pixel is considered inside or outside the outline loop.
        // Note that this on its own gives an incorrect SDF but it's needed to
        // correctly deal with pixels that are close to a sharp corner in the
        // shape outline

        auto p0 = shape_outline_begin; // the point before the one we are tessting
        auto p1 = p0;                  // the point we are testing
        p1++;
        auto p2 = p1; // the point after the one we are testing
        p2++;

        while (p0 != shape_outline_end) {

            if (p1 == shape_outline_end)
                p1 = shape_outline_begin;
            if (p2 == shape_outline_end)
                p2 = shape_outline_begin;

            Imath::V2f pt0 = (*p0 - bdb_bottom_left);
            Imath::V2f pt1 = (*p1 - bdb_bottom_left);
            Imath::V2f pt2 = (*p2 - bdb_bottom_left);

            pt0.x *= x_norm;
            pt0.y *= y_norm;
            pt1.x *= x_norm;
            pt1.y *= y_norm;
            pt2.x *= x_norm;
            pt2.y *= y_norm;

            const Imath::V2f point_norm = point_normal(pt0, pt1, pt2);

            if (pt0 != pt1 && pt1 != pt2) {

                Imath::V2f pixel_position(0.5f, 0.5f);
                float *buf_ptr = buffer.data();
                bool parity;
                for (int y = 0; y < buffer_height; ++y) {
                    pixel_position.x = 0.5f;
                    for (int x = 0; x < buffer_width; ++x) {
                        float r = fabs(*buf_ptr);
                        if (distToPoint(r, pixel_position, pt1, point_norm, parity)) {

                            if (parity)
                                *buf_ptr = -r;
                            else
                                *buf_ptr = r;
                        }
                        pixel_position.x++;
                        buf_ptr++;
                    }
                    pixel_position.y++;
                }
            }

            p0++;
            p1++;
            p2++;
        }
    }

    {

        // step 2 - compute the distance for each pixel to the each line that
        // makes up the outline loop

        auto p0 = shape_outline_begin;
        auto p1 = p0;
        p1++;

        while (p0 != shape_outline_end) {

            Imath::V2f line_start = (*p0) - bdb_bottom_left;
            Imath::V2f line_end   = (*p1) - bdb_bottom_left;

            line_start.x *= x_norm;
            line_start.y *= y_norm;
            line_end.x *= x_norm;
            line_end.y *= y_norm;

            Imath::V2f pixel_position(0.5f, 0.5f);
            float *buf_ptr = buffer.data();
            bool parity;
            for (int y = 0; y < buffer_height; ++y) {
                pixel_position.x = 0.5f;
                for (int x = 0; x < buffer_width; ++x) {
                    float r  = fabs(*buf_ptr);
                    float _r = distToLine(r, pixel_position, line_start, line_end, parity);
                    if (_r < r) {
                        if (parity)
                            *buf_ptr = _r;
                        else
                            *buf_ptr = -_r;
                    }
                    pixel_position.x++;
                    buf_ptr++;
                }
                pixel_position.y++;
            }
            p0++;
            p1++;
            if (p1 == shape_outline_end)
                p1 = shape_outline_begin;
        }
    }
}

} // namespace xstudio::ui::tesslator
