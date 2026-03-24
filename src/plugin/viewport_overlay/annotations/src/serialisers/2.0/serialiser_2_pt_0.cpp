// SPDX-License-Identifier: Apache-2.0

#include "annotation_serialiser.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

using namespace xstudio;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;


class AnnotationSerialiser_2_pt_0 : public AnnotationSerialiser {

  public:
    AnnotationSerialiser_2_pt_0() = default;

    void _serialise(const Annotation *, nlohmann::json &) const override;
    void _deserialise(Annotation *anno, const nlohmann::json &data) override;
};

RegisterAnnotationSerialiser(AnnotationSerialiser_2_pt_0, 2, 0)

    void AnnotationSerialiser_2_pt_0::_serialise(
        const Annotation *anno, nlohmann::json &d) const {

    // std::cerr << "Serialising using Annotation Serializer version 2\n";

    d = anno->canvas();
}

void AnnotationSerialiser_2_pt_0::_deserialise(Annotation *anno, const nlohmann::json &data) {

    // std::cerr << "Deserialising using Annotation Serializer version 2\n";

    nlohmann::json converted = data;

    if (converted.contains("pen_strokes")) {
        for (auto &stroke : converted["pen_strokes"]) {
            // Collect keys that start with '_'
            std::vector<std::string> keys_to_rename;
            for (auto &[key, value] : stroke.items()) {
                if (!key.empty() && key[0] == '_') {
                    keys_to_rename.push_back(key);
                }
            }

            // Rename them without the leading underscore
            for (const auto &key : keys_to_rename) {
                stroke[key.substr(1)] = std::move(stroke[key]);
                stroke.erase(key);
            }

            // Expand points: x, y, p -> x, y, p * size_sens, p * opacity_sens
            float size_sens    = stroke.value("size_sensitivity", 1.0f);
            float opacity_sens = stroke.value("opacity_sensitivity", 1.0f);

            const auto &old_points    = stroke["points"];
            nlohmann::json new_points = nlohmann::json::array();

            for (size_t i = 0; i + 2 < old_points.size(); i += 3) {
                float x = old_points[i].get<double>();
                float y = old_points[i + 1].get<double>();
                float p = old_points[i + 2].get<double>();

                new_points.push_back(x);
                new_points.push_back(y);
                new_points.push_back(std::pow(p, size_sens));
                new_points.push_back(std::pow(p, opacity_sens));
            }

            stroke["points"] = std::move(new_points);
        }
    }

    // std::cout << converted.dump(2) << std::endl;

    // Now deserialise the converted JSON
    anno->canvas() = converted.template get<Canvas>();
}
