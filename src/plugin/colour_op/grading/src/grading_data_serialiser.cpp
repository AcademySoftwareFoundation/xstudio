// SPDX-License-Identifier: Apache-2.0
#include "grading_data_serialiser.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

std::map<long, std::shared_ptr<GradingDataSerialiser>> GradingDataSerialiser::serialisers;

static const std::string GRADING_VERSION_KEY("Grading Serialiser Version");

utility::JsonStore GradingDataSerialiser::serialise(const GradingData *grading_data) {

    if (serialisers.empty()) {
        throw std::runtime_error("No GradingData Serialisers registered.");
    }
    auto p = serialisers.rbegin();
    utility::JsonStore result;
    result[GRADING_VERSION_KEY] = p->first;
    result["Data"]              = nlohmann::json();
    p->second->_serialise(grading_data, result["Data"]);
    return result;
}

void GradingDataSerialiser::deserialise(
    GradingData *grading_data, const utility::JsonStore &data) {

    if (data.find(GRADING_VERSION_KEY) != data.end()) {
        const int sver = data[GRADING_VERSION_KEY].get<int>();
        if (serialisers.find(sver) != serialisers.end()) {
            serialisers[sver]->_deserialise(grading_data, data["Data"]);
        } else {
            throw std::runtime_error("Unknown GradingData serialiser version.");
        }
    } else {
        throw std::runtime_error("GradingDataSerialiser passed json data without \"GradingData "
                                 "Serialiser Version\".");
    }
}


void GradingDataSerialiser::register_serialiser(
    const unsigned char maj_ver,
    const unsigned char minor_ver,
    std::shared_ptr<GradingDataSerialiser> sptr) {
    int fver = maj_ver << 8 + minor_ver;
    assert(sptr);
    if (serialisers.find(fver) != serialisers.end()) {
        throw std::runtime_error("Attempt to register Annotation Serialiser with a used "
                                 "version number that is already used.");
    }
    serialisers[fver] = sptr;
}
