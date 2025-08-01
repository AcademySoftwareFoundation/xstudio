// SPDX-License-Identifier: Apache-2.0
#include <utility>

#include "grading_data.h"
#include "grading_data_serialiser.hpp"
#include "grading.h"

using namespace xstudio::ui::viewport;
using namespace xstudio;


void xstudio::ui::viewport::from_json(const nlohmann::json &j, Grade &g) {

    j.at("slope").get_to(g.slope);
    j.at("offset").get_to(g.offset);
    j.at("power").get_to(g.power);
    j.at("sat").get_to(g.sat);
}

void xstudio::ui::viewport::to_json(nlohmann::json &j, const Grade &g) {

    j["slope"]  = g.slope;
    j["offset"] = g.offset;
    j["power"]  = g.power;
    j["sat"]    = g.sat;
}

bool LayerData::identity() const { return (grade_ == Grade() && mask_.empty()); }

void xstudio::ui::viewport::from_json(const nlohmann::json &j, LayerData &l) {

    j.at("grade").get_to(l.grade_);
    j.at("mask_active").get_to(l.mask_active_);
    j.at("mask_editing").get_to(l.mask_editing_);
    j.at("mask").get_to(l.mask_);
}

void xstudio::ui::viewport::to_json(nlohmann::json &j, const LayerData &l) {

    j["grade"]        = l.grade_;
    j["mask_active"]  = l.mask_active_;
    j["mask_editing"] = l.mask_editing_;
    j["mask"]         = l.mask_;
}


GradingData::GradingData(const utility::JsonStore &s) : bookmark::AnnotationBase() {

    GradingDataSerialiser::deserialise(this, s);
}

utility::JsonStore GradingData::serialise(utility::Uuid &plugin_uuid) const {

    plugin_uuid = colour_pipeline::GradingTool::PLUGIN_UUID;
    return GradingDataSerialiser::serialise((const GradingData *)this);
}

bool GradingData::identity() const {

    return (layers_.empty() || (layers_.size() == 1 && layers_.front().identity()));
}

LayerData *GradingData::layer(size_t idx) {

    if (idx >= 0 && idx < layers_.size()) {
        return &layers_[idx];
    }
    return nullptr;
}

void GradingData::push_layer() { layers_.push_back(LayerData()); }

void GradingData::pop_layer() { layers_.pop_back(); }

void xstudio::ui::viewport::from_json(const nlohmann::json &j, GradingData &gd) {

    j.at("layers").get_to(gd.layers_);
}

void xstudio::ui::viewport::to_json(nlohmann::json &j, const GradingData &gd) {

    j["layers"] = gd.layers_;
}
