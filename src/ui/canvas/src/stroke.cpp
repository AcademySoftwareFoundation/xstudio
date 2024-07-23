// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/canvas/stroke.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio;

namespace {

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

} // anonymous namespace


Stroke Stroke::Pen(
    const utility::ColourTriplet &colour,
    const float thickness,
    const float softness,
    const float opacity) {

    Stroke s;
    s.thickness = thickness;
    s.softness  = softness;
    s.colour    = colour;
    s.opacity   = opacity;
    s.type      = StrokeType_Pen;
    return s;
}

Stroke Stroke::Erase(const float thickness) {

    Stroke s;
    s.thickness = thickness;
    s.softness  = 0.0f;
    s.colour    = {1.0f, 1.0f, 1.0f};
    s.opacity   = 1.0f;
    s.type      = StrokeType_Erase;
    return s;
}

bool Stroke::operator==(const Stroke &o) const {
    return (
        opacity == o.opacity && thickness == o.thickness && softness == o.softness &&
        colour == o.colour && type == o.type && points == o.points);
}

void Stroke::make_square(const Imath::V2f &corner1, const Imath::V2f &corner2) {

    points = std::vector<Imath::V2f>(
        {Imath::V2f(corner1.x, corner1.y),
         Imath::V2f(corner2.x, corner1.y),
         Imath::V2f(corner2.x, corner2.y),
         Imath::V2f(corner1.x, corner2.y),
         Imath::V2f(corner1.x, corner1.y)});
}

void Stroke::make_circle(const Imath::V2f &origin, const float radius) {

    points.clear();
    for (const auto &pt : s_circ_pts.pts_) {
        points.push_back(origin + pt * radius);
    }
}

void Stroke::make_arrow(const Imath::V2f &start, const Imath::V2f &end) {

    Imath::V2f v;
    if (start == end) {
        v = Imath::V2f(1.0f, 0.0f) * thickness * 4.0f;
    } else {
        v = (start - end).normalized() * std::max(thickness * 4.0f, 0.01f);
    }
    const Imath::V2f t(v.y, -v.x);

    points.clear();
    points.push_back(start);
    points.push_back(end);
    points.push_back(end + v + t);
    points.push_back(end);
    points.push_back(end + v - t);
}

void Stroke::make_line(const Imath::V2f &start, const Imath::V2f &end) {

    points.clear();
    points.push_back(start);
    points.push_back(end);
}

void Stroke::add_point(const Imath::V2f &pt) {

    if (!(!points.empty() && points.back() == pt)) {
        points.emplace_back(pt);
    }
}

std::vector<Imath::V2f> Stroke::vertices() const {

    std::vector<Imath::V2f> result;

    if (!points.empty()) {
        result = points;
        result.push_back(points.back()); // repeat last point to make end 'cap'
    }

    return result;
}

// Note the below is slightly more complex than it could because
// we try to maintain bakward compatibility with previous format.

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Stroke &s) {

    j.at("opacity").get_to(s.opacity);
    j.at("thickness").get_to(s.thickness);

    if (j.contains("softness")) {
        j.at("softness").get_to(s.softness);
    }

    s.type = j["is_erase_stroke"].get<bool>() ? StrokeType_Erase : StrokeType_Pen;
    s.colour =
        utility::ColourTriplet{j.value("r", 1.0f), j.value("g", 1.0f), j.value("b", 1.0f)};

    if (j.contains("points") && j["points"].is_array()) {
        auto it = j["points"].begin();
        while (it != j["points"].end()) {
            auto x = it++.value().get<float>();
            auto y = it++.value().get<float>();
            s.add_point(Imath::V2f(x, y));
        }
    }
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Stroke &s) {

    j = nlohmann::json{
        {"opacity", s.opacity},
        {"thickness", s.thickness},
        {"softness", s.softness},
        {"is_erase_stroke", s.type == StrokeType_Erase}};

    std::vector<float> pts;
    pts.reserve(s.points.size() * 2);
    for (auto &pt : s.points) {
        pts.push_back(pt.x);
        pts.push_back(pt.y);
    }
    j["points"] = pts;

    if (s.type != StrokeType_Erase) {
        j["r"] = s.colour.r;
        j["g"] = s.colour.g;
        j["b"] = s.colour.b;
    }
}