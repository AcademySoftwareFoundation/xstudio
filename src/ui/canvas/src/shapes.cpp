// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/canvas/shapes.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio;


bool Quad::operator==(const Quad &o) const {
    return (
        bl == o.bl && tl == o.tl && tr == o.tr && br == o.br && colour == o.colour &&
        softness == o.softness && opacity == o.opacity && invert == o.invert);
}

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Quad &q) {

    j.at("bl").get_to(q.bl);
    j.at("tl").get_to(q.tl);
    j.at("tr").get_to(q.tr);
    j.at("br").get_to(q.br);
    j.at("colour").get_to(q.colour);
    j.at("softness").get_to(q.softness);
    j.at("opacity").get_to(q.opacity);
    j.at("invert").get_to(q.invert);
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Quad &q) {

    j["bl"]       = q.bl;
    j["tl"]       = q.tl;
    j["tr"]       = q.tr;
    j["br"]       = q.br;
    j["colour"]   = q.colour;
    j["softness"] = q.softness;
    j["opacity"]  = q.opacity;
    j["invert"]   = q.invert;
}

bool Polygon::operator==(const Polygon &o) const {
    return (
        points == o.points && colour == o.colour && softness == o.softness &&
        opacity == o.opacity && invert == o.invert);
}

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Polygon &q) {

    j.at("points").get_to(q.points);
    j.at("colour").get_to(q.colour);
    j.at("softness").get_to(q.softness);
    j.at("opacity").get_to(q.opacity);
    j.at("invert").get_to(q.invert);
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Polygon &q) {

    j["points"]   = q.points;
    j["colour"]   = q.colour;
    j["softness"] = q.softness;
    j["opacity"]  = q.opacity;
    j["invert"]   = q.invert;
}

bool Ellipse::operator==(const Ellipse &o) const {
    return (
        center == o.center && radius == o.radius && angle == o.angle && colour == o.colour &&
        softness == o.softness && opacity == o.opacity && invert == o.invert);
}

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Ellipse &e) {

    j.at("center").get_to(e.center);
    j.at("radius").get_to(e.radius);
    j.at("angle").get_to(e.angle);
    j.at("colour").get_to(e.colour);
    j.at("softness").get_to(e.softness);
    j.at("opacity").get_to(e.opacity);
    j.at("invert").get_to(e.invert);
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Ellipse &e) {

    j["center"]   = e.center;
    j["radius"]   = e.radius;
    j["angle"]    = e.angle;
    j["colour"]   = e.colour;
    j["softness"] = e.softness;
    j["opacity"]  = e.opacity;
    j["invert"]   = e.invert;
}