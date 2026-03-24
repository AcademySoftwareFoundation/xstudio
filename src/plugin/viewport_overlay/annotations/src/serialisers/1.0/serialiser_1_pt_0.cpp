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

    nlohmann::json converted = data;

    if (converted.contains("pen_strokes")) {
        for (auto &stroke : converted["pen_strokes"]) {

            if (!stroke.contains("softness")) {
                stroke["softness"] = 0.0f;
            }

            stroke["size_sensitivity"]    = 0.0f;
            stroke["opacity_sensitivity"] = 0.0f;

            const auto &old_points = stroke["points"];

            // In the older JSON format, thikness was added at each point
            if (old_points.size() >= 3) {
                stroke["thickness"] = old_points[2].get<float>();
            } else {
                stroke["thickness"] = 10;
            }

            nlohmann::json new_points = nlohmann::json::array();

            for (size_t i = 0; i + 2 < old_points.size(); i += 3) {
                float x = old_points[i].get<double>();
                float y = old_points[i + 1].get<double>();

                new_points.push_back(x);
                new_points.push_back(y);
                new_points.push_back(1.0f);
                new_points.push_back(1.0f);
            }

            stroke["points"] = std::move(new_points);
        }
    }

    // std::cout << converted.dump(2) << std::endl;

    // Now deserialise the converted JSON
    anno->canvas() = converted.template get<Canvas>();
}
