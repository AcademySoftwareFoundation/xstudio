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

inline size_t __hash_combine(float _lhs, size_t rhs) {
    uint32_t v = *(reinterpret_cast<uint32_t *>(&_lhs));
    size_t lhs = size_t(v);
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

} // anonymous namespace

void Stroke::update_hash(const bool update_with_last_point_only) {

    if (update_with_last_point_only && _points.size()) {
        _hash = __hash_combine(_points.back().pos.x, _hash);
        _hash = __hash_combine(_points.back().pos.y, _hash);
        _hash = __hash_combine(_points.back().size_pressure, _hash);
        _hash = __hash_combine(_points.back().opacity_pressure, _hash);
    } else {
        _hash = 0;
        _hash = __hash_combine(_thickness, _hash);
        _hash = __hash_combine(_softness, _hash);
        _hash = __hash_combine(_opacity, _hash);
        _hash = __hash_combine(_size_sensitivity, _hash);
        _hash = __hash_combine(_opacity_sensitivity, _hash);
        _hash = __hash_combine(_colour.red(), _hash);
        _hash = __hash_combine(_colour.red(), _hash);
        _hash = __hash_combine(_colour.green(), _hash);
        _hash = __hash_combine(_colour.blue(), _hash);
        for (const auto &point : _points) {
            _hash = __hash_combine(point.pos.x, _hash);
            _hash = __hash_combine(point.pos.y, _hash);
            _hash = __hash_combine(point.size_pressure, _hash);
            _hash = __hash_combine(point.opacity_pressure, _hash);
        }
    }
}

Stroke *Stroke::Pen(
    const utility::ColourTriplet &_colour,
    const float _thickness,
    const float _softness,
    const float _opacity) {

    Stroke *s               = new Stroke;
    s->_thickness           = _thickness;
    s->_softness            = _softness;
    s->_colour              = _colour;
    s->_opacity             = _opacity;
    s->_size_sensitivity    = 0.0f;
    s->_opacity_sensitivity = 0.0f;
    s->_type                = StrokeType_Pen;
    s->update_hash();

    return s;
}

Stroke *Stroke::Brush(
    const utility::ColourTriplet &_colour,
    const float _thickness,
    const float _softness,
    const float _opacity,
    const float _size_sensitivity,
    const float _opacity_sensitivity) {

    Stroke *s               = new Stroke;
    s->_thickness           = _thickness;
    s->_softness            = _softness;
    s->_colour              = _colour;
    s->_opacity             = _opacity;
    s->_size_sensitivity    = _size_sensitivity;
    s->_opacity_sensitivity = _opacity_sensitivity;
    s->_type                = StrokeType_Brush;
    s->update_hash();

    return s;
}

Stroke *Stroke::Erase(const float _thickness) {

    Stroke *s               = new Stroke;
    s->_thickness           = _thickness;
    s->_softness            = 0.0f;
    s->_colour              = {1.0f, 1.0f, 1.0f};
    s->_opacity             = 1.0f;
    s->_size_sensitivity    = 1.0f;
    s->_opacity_sensitivity = 1.0f;
    s->_type                = StrokeType_Erase;
    s->update_hash();

    return s;
}

bool Stroke::operator==(const Stroke &o) const {
    return (
        _opacity == o._opacity && _thickness == o._thickness && _softness == o._softness &&
        _colour == o._colour && _size_sensitivity == o._size_sensitivity &&
        _opacity_sensitivity == o._opacity_sensitivity && _type == o._type &&
        _points == o._points);
}

void Stroke::make_line(const Imath::V2f &start, const Imath::V2f &end) {

    _points.clear();
    add_point(start);
    add_point(end);
}

void Stroke::make_square(const Imath::V2f &corner1, const Imath::V2f &corner2) {

    _points.clear();
    add_point(Imath::V2f(corner1.x, corner1.y));
    add_point(Imath::V2f(corner2.x, corner1.y));
    add_point(Imath::V2f(corner2.x, corner2.y));
    add_point(Imath::V2f(corner1.x, corner2.y));
    add_point(Imath::V2f(corner1.x, corner1.y));
}

void Stroke::make_circle(const Imath::V2f &origin, const float radius) {

    _points.clear();
    for (const auto &pt : s_circ_pts.pts_) {
        add_point(origin + pt * radius);
    }
}

void Stroke::make_arrow(const Imath::V2f &start, const Imath::V2f &end) {

    Imath::V2f v;
    if (start == end) {
        v = Imath::V2f(1.0f, 0.0f) * _thickness * 4.0f;
    } else {
        v = (start - end).normalized() * std::max(_thickness * 4.0f, 0.01f);
    }
    const Imath::V2f t(v.y, -v.x);

    _points.clear();
    add_point(start);
    add_point(end);
    add_point(end + v + t);
    add_point(end);
    add_point(end + v - t);
}

void Stroke::add_points(const std::vector<Point> &points) {
    _points.insert(_points.end(), points.begin(), points.end());
    update_hash(true);
}

void Stroke::add_point(const Imath::V2f &pt, float size_pressure, float opacity_pressure) {

    if ((_points.empty() || _points.back().pos != pt)) {
        _points.emplace_back(pt, size_pressure, opacity_pressure);
    }

    update_hash(true);
}

bool Stroke::fade(const float fade_amount) {
    bool is_invisible = true;
    for (auto &point : _points) {
        point.opacity_pressure = std::max(0.0f, point.opacity_pressure - fade_amount);
        if (point.opacity_pressure)
            is_invisible = false; // at least one point has non-zero opacity ... don't erase
    }
    update_hash();
    return is_invisible;
}

// Read V3 format
void xstudio::ui::canvas::from_json(const nlohmann::json &j, Stroke &s) {

    j.at("opacity").get_to(s._opacity);
    j.at("thickness").get_to(s._thickness);
    j.at("softness").get_to(s._softness);
    j.at("size_sensitivity").get_to(s._size_sensitivity);
    j.at("opacity_sensitivity").get_to(s._opacity_sensitivity);

    s._type = j.at("is_erase_stroke").get<bool>() ? StrokeType_Erase : StrokeType_Pen;

    s._colour =
        utility::ColourTriplet{j.value("r", 1.0f), j.value("g", 1.0f), j.value("b", 1.0f)};


    s.update_hash();

    if (j.contains("points") && j["points"].is_array()) {
        auto it = j["points"].begin();
        while (it != j["points"].end()) {
            auto x                = (it++)->get<float>();
            auto y                = (it++)->get<float>();
            auto size_pressure    = (it++)->get<float>();
            auto opacity_pressure = (it++)->get<float>();

            s.add_point(Imath::V2f(x, y), size_pressure, opacity_pressure);
        }
    }
}

// Write V3 format
void xstudio::ui::canvas::to_json(nlohmann::json &j, const Stroke &s) {

    j = nlohmann::json{
        {"opacity", s._opacity},
        {"thickness", s._thickness},
        {"softness", s._softness},
        {"is_erase_stroke", s._type == StrokeType_Erase},
        {"size_sensitivity", s._size_sensitivity},
        {"opacity_sensitivity", s._opacity_sensitivity}};

    if (s._type != StrokeType_Erase) {
        j["r"] = s._colour.r;
        j["g"] = s._colour.g;
        j["b"] = s._colour.b;
    }

    std::vector<float> pts;
    pts.reserve(s._points.size() * 3);
    for (auto &pt : s._points) {
        pts.push_back(pt.pos.x);
        pts.push_back(pt.pos.y);
        pts.push_back(pt.size_pressure);
        pts.push_back(pt.opacity_pressure);
    }

    j["points"] = pts;
}
