// SPDX-License-Identifier: Apache-2.0
#include "annotation_serialiser.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

std::map<long, std::shared_ptr<AnnotationSerialiser>> AnnotationSerialiser::serialisers;

static const std::string ANNO_VERSION_KEY("Annotation Serialiser Version");

utility::JsonStore AnnotationSerialiser::serialise(const Annotation *anno) {
    if (serialisers.empty()) {
        throw std::runtime_error("No Annotation Serialisers registered.");
    }
    auto p = serialisers.rbegin();
    utility::JsonStore result;
    result[ANNO_VERSION_KEY] = p->first;
    result["Data"]           = nlohmann::json();
    p->second->_serialise(anno, result["Data"]);
    return result;
}

void AnnotationSerialiser::deserialise(Annotation *anno, const utility::JsonStore &data) {

    if (data.find(ANNO_VERSION_KEY) != data.end()) {
        const int sver = data[ANNO_VERSION_KEY].get<int>();
        if (serialisers.find(sver) != serialisers.end()) {
            serialisers[sver]->_deserialise(anno, data["Data"]);
        } else {
            throw std::runtime_error("Unknown annotation serialiser version.");
        }
    } else {
        throw std::runtime_error(
            "AnnotationSerialiser passed json data without \"Annotation Serialiser Version\".");
    }
}


void AnnotationSerialiser::register_serialiser(
    const unsigned char maj_ver,
    const unsigned char minor_ver,
    std::shared_ptr<AnnotationSerialiser> sptr) {
    int fver = maj_ver << 8 + minor_ver;
    assert(sptr);
    if (serialisers.find(fver) != serialisers.end()) {
        throw std::runtime_error("Attempt to register Annotation Serialiser with a used "
                                 "version number that is already used.");
    }
    serialisers[fver] = sptr;
}
