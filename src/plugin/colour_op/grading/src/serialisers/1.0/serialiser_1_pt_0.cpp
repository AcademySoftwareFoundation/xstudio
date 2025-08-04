// SPDX-License-Identifier: Apache-2.0

#include "grading_data_serialiser.hpp"
#include "grading_data.h"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;


class GradingDataSerialiser_1_pt_0 : public GradingDataSerialiser {

  public:
    GradingDataSerialiser_1_pt_0() = default;

    void _serialise(const GradingData *, nlohmann::json &) const override;
    void _deserialise(GradingData *, const nlohmann::json &) override;
};

RegisterGradingDataSerialiser(GradingDataSerialiser_1_pt_0, 1, 0)

    void GradingDataSerialiser_1_pt_0::_serialise(
        const GradingData *grading_data, nlohmann::json &d) const {

    d = *grading_data;
}

void GradingDataSerialiser_1_pt_0::_deserialise(
    GradingData *grading_data, const nlohmann::json &d) {

    *grading_data = d.template get<GradingData>();
}
