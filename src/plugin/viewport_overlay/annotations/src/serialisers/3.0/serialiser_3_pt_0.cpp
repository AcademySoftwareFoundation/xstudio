// SPDX-License-Identifier: Apache-2.0

#include "annotation_serialiser.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;


class AnnotationSerialiser_3_pt_0 : public AnnotationSerialiser {

  public:
    AnnotationSerialiser_3_pt_0() = default;

    void _serialise(const Annotation *, nlohmann::json &) const override;
    void _deserialise(Annotation *anno, const nlohmann::json &data) override;
};

RegisterAnnotationSerialiser(AnnotationSerialiser_3_pt_0, 3, 0)

    void AnnotationSerialiser_3_pt_0::_serialise(
        const Annotation *anno, nlohmann::json &d) const {

    // std::cerr << "Serialising using Annotation Serializer version 3\n";

    d = anno->canvas();
}

void AnnotationSerialiser_3_pt_0::_deserialise(Annotation *anno, const nlohmann::json &data) {

    nlohmann::json converted = data;

    if (converted.contains("pen_strokes")) {
        for (auto &stroke : converted["pen_strokes"]) {
            // V3 → V4: convert "is_erase_stroke" + r/g/b → "type" + "colour"
            bool is_erase  = stroke.value("is_erase_stroke", false);
            stroke["type"] = is_erase ? "Erase" : "Brush";
            stroke.erase("is_erase_stroke");

            float r          = stroke.value("r", 1.0f);
            float g          = stroke.value("g", 1.0f);
            float b          = stroke.value("b", 1.0f);
            stroke["colour"] = {r, g, b};
            stroke.erase("r");
            stroke.erase("g");
            stroke.erase("b");
        }
    }

    anno->canvas() = converted.template get<Canvas>();
}
