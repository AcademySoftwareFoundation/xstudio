// SPDX-License-Identifier: Apache-2.0

#include "annotation_serialiser.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;


class AnnotationSerialiser_1_pt_0 : public AnnotationSerialiser {

  public:
    AnnotationSerialiser_1_pt_0() = default;

    void _serialise(const Annotation *, nlohmann::json &) const override;
    void _deserialise(Annotation *anno, const nlohmann::json &data) override;
};

RegisterAnnotationSerialiser(AnnotationSerialiser_1_pt_0, 1, 0)

    void AnnotationSerialiser_1_pt_0::_serialise(
        const Annotation *anno, nlohmann::json &d) const {

    // std::cerr << "Serialising using Annotation Serializer version 1\n";

    d = anno->canvas();
}

void AnnotationSerialiser_1_pt_0::_deserialise(Annotation *anno, const nlohmann::json &data) {

    // std::cerr << "Deserialising using Annotation Serializer version 1\n";
    anno->canvas() = data.template get<Canvas>();
}
