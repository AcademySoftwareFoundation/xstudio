// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

// If a pen stroke has thickness of 1, it will be 1 pixel thick agains
// an image that 3840 pixels in width.
#define PEN_STROKE_THICKNESS_SCALE 3840.0f


namespace xstudio::ui::canvas {

enum StrokeType {
    StrokeType_Pen,
    StrokeType_Brush,
    StrokeType_Erase,
    StrokeType_BurnAdd,
    StrokeType_BurnMult,
    StrokeType_DodgeAdd,
    StrokeType_DodgeMult
};

// clang-format off
        NLOHMANN_JSON_SERIALIZE_ENUM(StrokeType, {
            {StrokeType_Pen,       "Pen"},
            {StrokeType_Brush,     "Brush"},
            {StrokeType_Erase,     "Erase"},
            {StrokeType_BurnAdd,   "BurnAdd"},
            {StrokeType_BurnMult,  "BurnMult"},
            {StrokeType_DodgeAdd,  "DodgeAdd"},
            {StrokeType_DodgeMult, "DodgeMult"},
        })
// clang-format on

struct Stroke {

    // enum class Type { Unknown, Pen, Brush, Erase };

    Stroke() : _id(to_string(utility::Uuid::generate())) {}
    Stroke(const Stroke &o) = default;

    struct Point {
        Imath::V2f pos;
        float size_pressure;
        float opacity_pressure;

        bool operator==(const Point &o) const {
            return pos == o.pos && size_pressure == o.size_pressure &&
                   opacity_pressure == o.opacity_pressure;
        }
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

    static Stroke *BurnAdd(const float intensity, const float thickness, const float softness);

    static Stroke *BurnMult(const float intensity, const float thickness, const float softness);

    static Stroke *DodgeAdd(const float intensity, const float thickness, const float softness);

    static Stroke *
    DodgeMult(const float intensity, const float thickness, const float softness);

    bool operator==(const Stroke &o) const;

    // TODO: Below are shapes and should be extracted to dedicated types
    // Rendering them as stroke seems like an implementation details and
    // will probably not hold if we need filled shape for example.
    void make_square(const Imath::V2f &corner1, const Imath::V2f &corner2);

    void make_circle(const Imath::V2f &origin, const float radius);

    void make_arrow(const Imath::V2f &start, const Imath::V2f &end);

    void make_line(const Imath::V2f &start, const Imath::V2f &end);

    void set_opacity(const float o) { _opacity = o; }
    void set_colour(const utility::ColourTriplet &c) { _colour = c; }

    void
    add_point(const Imath::V2f &pt, float size_pressure = 1.0f, float opacity_pressure = 1.0f);
    void add_points(const std::vector<Point> &points);

    void set_id(const std::string &id) { _id = id; }

    [[nodiscard]] const std::vector<Point> &points() const { return _points; }

    bool fade(const float fade_amount);

    [[nodiscard]] float opacity() const { return _opacity; }
    [[nodiscard]] float thickness() const { return _thickness; }
    [[nodiscard]] float softness() const { return _softness; }
    [[nodiscard]] float size_sensitivity() const { return _size_sensitivity; }
    [[nodiscard]] float opacity_sensitivity() const { return _opacity_sensitivity; }
    [[nodiscard]] StrokeType type() const { return _type; }
    [[nodiscard]] float intensity() const { return _colour.r; }
    [[nodiscard]] int blend_mode() const;
    [[nodiscard]] size_t hash() const { return _hash; }
    [[nodiscard]] const utility::ColourTriplet &colour() const { return _colour; }
    [[nodiscard]] const std::string &id() const { return _id; }

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
    std::string _id;

    utility::ColourTriplet _colour;
    StrokeType _type{StrokeType_Pen};
    std::vector<Point> _points;
};

void from_json(const nlohmann::json &j, Stroke &s);
void to_json(nlohmann::json &j, const Stroke &s);

} // namespace xstudio::ui::canvas

/*namespace std {
template <> struct hash<xstudio::ui::canvas::Stroke> {
    std::size_t operator()(const xstudio::ui::canvas::Stroke &item) const {
        std::hash<std::string> hasher;
        return hasher(item.hash());
    }
};
} // namespace std*/