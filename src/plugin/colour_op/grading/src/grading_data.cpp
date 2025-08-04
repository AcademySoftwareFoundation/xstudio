// SPDX-License-Identifier: Apache-2.0
#include <utility>

#include "grading_data.h"
#include "grading_data_serialiser.hpp"
#include "grading.h"

using namespace xstudio::ui::viewport;
using namespace xstudio;


void xstudio::ui::viewport::from_json(const nlohmann::json &j, Grade &g) {

    j.at("slope").get_to(static_cast<std::array<double, 4> &>(g.slope));
    j.at("offset").get_to(static_cast<std::array<double, 4> &>(g.offset));
    j.at("power").get_to(static_cast<std::array<double, 4> &>(g.power));
    j.at("sat").get_to(g.sat);

    if (j.contains("exposure"))
        j.at("exposure").get_to(g.exposure);
    if (j.contains("contrast"))
        j.at("contrast").get_to(g.contrast);
}

void xstudio::ui::viewport::to_json(nlohmann::json &j, const Grade &g) {

    j["slope"]    = g.slope;
    j["offset"]   = g.offset;
    j["power"]    = g.power;
    j["sat"]      = g.sat;
    j["exposure"] = g.exposure;
    j["contrast"] = g.contrast;
}

GradingData::GradingData(const utility::JsonStore &s) : bookmark::AnnotationBase() {

    GradingDataSerialiser::deserialise(this, s);
}

utility::JsonStore GradingData::serialise(utility::Uuid &plugin_uuid) const {

    plugin_uuid = colour_pipeline::GradingTool::PLUGIN_UUID;
    return GradingDataSerialiser::serialise((const GradingData *)this);
}

bool GradingData::identity() const { return (grade_ == Grade() && mask_.empty()); }

void xstudio::ui::viewport::from_json(const nlohmann::json &j, GradingData &l) {

    if (j.contains("colour_space"))
        j.at("colour_space").get_to(l.colour_space_);

    j.at("grade").get_to(l.grade_);
    j.at("mask_active").get_to(l.mask_active_);
    j.at("mask_editing").get_to(l.mask_editing_);
    j.at("mask").get_to(l.mask_);
}

void xstudio::ui::viewport::to_json(nlohmann::json &j, const GradingData &l) {

    j["colour_space"] = l.colour_space_;
    j["grade"]        = l.grade_;
    j["mask_active"]  = l.mask_active_;
    j["mask_editing"] = l.mask_editing_;
    j["mask"]         = l.mask_;
}