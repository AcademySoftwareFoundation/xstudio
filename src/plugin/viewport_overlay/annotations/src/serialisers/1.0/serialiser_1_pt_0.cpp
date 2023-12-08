// SPDX-License-Identifier: Apache-2.0
#include "annotation_serialiser.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

class AnnotationSerialiser_1_pt_0 : public AnnotationSerialiser {

  public:
    AnnotationSerialiser_1_pt_0() = default;

    void _serialise(const Annotation *, nlohmann::json &) const override;
    void _deserialise(Annotation *anno, const nlohmann::json &data) override;

    void serialise_pen_stroke(const PenStroke &s, nlohmann::json &o) const;
    void serialise_caption(const std::shared_ptr<Caption> &capt, nlohmann::json &o) const;
};

RegisterAnnotationSerialiser(AnnotationSerialiser_1_pt_0, 1, 0)

    void AnnotationSerialiser_1_pt_0::_serialise(
        const Annotation *anno, nlohmann::json &d) const {
    d["pen_strokes"] = nlohmann::json::array();
    for (const auto &stroke : anno->strokes_) {
        d["pen_strokes"].emplace_back(nlohmann::json());
        nlohmann::json &s = d["pen_strokes"].back();
        serialise_pen_stroke(stroke, s);
    }
    d["captions"] = nlohmann::json::array();
    for (const auto &caption : anno->captions_) {
        d["captions"].emplace_back(nlohmann::json());
        nlohmann::json &s = d["captions"].back();
        serialise_caption(caption, s);
    }
}

void AnnotationSerialiser_1_pt_0::serialise_pen_stroke(
    const PenStroke &s, nlohmann::json &o) const {
    // TODO: pack data into bytes and convert to ASCII legal characters. Will be *much* more
    // compact than json text encoding
    /*std::vector <uint8_t> stroke_data();
    size_t sz = 2 // num of points as a short
      + s.points_.size() * sizeof(half) * 2 // points data as half float
      + sizeof(float) // opacity
      + sizeof(float) // thickness
      + sizeof(bool) // is_erase_stroke
      + 3*sizeof(float); // RGB values;

    stroke_data.resize(sz);

    uint8_t *d = stroke_data.data();

    short n = s.points_.size();
    memcpy(d, &n, 2);
    d += 2;

    for (auto & pt: s.points_) {
      half h = pt.x;
      memcpy(d, &h, sizeof(half));
      d += sizeof(half);
      h = pt.y;
      memcpy(d, &h, sizeof(half));
      d += sizeof(half);
    }

    memcpy(d, &(s.opacity_), sizeof(float));
    d += sizeof(float);

    memcpy(d, &(s.thickness_), sizeof(float));
    d += sizeof(float);

    memcpy(d, &(s.is_erase_stroke_), sizeof(bool));
    d += sizeof(bool);

    memcpy(d, &(s.colour_.r), sizeof(float));
    d += sizeof(float);
    memcpy(d, &(s.colour_.g), sizeof(float));
    d += sizeof(float);
    memcpy(d, &(s.colour_.b), sizeof(float));
    d += sizeof(float);*/

    std::vector<float> pts;
    pts.reserve(s.points_.size());
    for (auto &pt : s.points_) {
        pts.push_back(pt.x);
        pts.push_back(pt.y);
    }
    o["points"]          = pts;
    o["opacity"]         = s.opacity_;
    o["thickness"]       = s.thickness_;
    o["is_erase_stroke"] = s.is_erase_stroke_;
    if (!s.is_erase_stroke_) {
        o["r"] = s.colour_.r;
        o["g"] = s.colour_.g;
        o["b"] = s.colour_.b;
    }
}

void AnnotationSerialiser_1_pt_0::serialise_caption(
    const std::shared_ptr<Caption> &capt, nlohmann::json &o) const {

    o["text"]          = capt->text_;
    o["position"]      = capt->position_;
    o["wrap_width"]    = capt->wrap_width_;
    o["font_size"]     = capt->font_size_;
    o["font_name"]     = capt->font_name_;
    o["colour"]        = capt->colour_;
    o["opacity"]       = capt->opacity_;
    o["justification"] = static_cast<int>(capt->justification_);
    o["outline"]       = false;
}


void AnnotationSerialiser_1_pt_0::_deserialise(Annotation *anno, const nlohmann::json &data) {

    if (data.contains("pen_strokes") && data["pen_strokes"].is_array()) {

        const auto &d = data["pen_strokes"];
        for (const auto &o : d)
            if (o["is_erase_stroke"].get<bool>()) {
                PenStroke stroke(o["thickness"].get<float>());
                if (o.contains("points") && o["points"].is_array()) {
                    auto p = o["points"].begin();
                    while (p != o["points"].end()) {
                        auto x = p.value().get<float>();
                        p++;
                        auto y = p.value().get<float>();
                        p++;
                        stroke.add_point(Imath::V2f(x, y));
                    }
                }
                anno->strokes_.push_back(std::move(stroke));
            } else {
                PenStroke stroke(
                    utility::ColourTriplet(
                        o["r"].get<float>(), o["g"].get<float>(), o["b"].get<float>()),
                    o["thickness"].get<float>(),
                    o["opacity"].get<float>());
                if (o.contains("points") && o["points"].is_array()) {
                    auto p = o["points"].begin();
                    while (p != o["points"].end()) {
                        auto x = p.value().get<float>();
                        p++;
                        auto y = p.value().get<float>();
                        p++;
                        stroke.add_point(Imath::V2f(x, y));
                    }
                }
                anno->strokes_.push_back(std::move(stroke));
            }
    }

    if (data.contains("captions") && data["captions"].is_array()) {

        const auto &d = data["captions"];
        for (const auto &o : d) {
            try {
                auto capt = std::make_shared<Caption>(
                    o["position"].get<Imath::V2f>(),
                    o["wrap_width"].get<float>(),
                    o["font_size"].get<float>(),
                    o["colour"].get<utility::ColourTriplet>(),
                    o["opacity"].get<float>(),
                    static_cast<ui::Justification>(o["justification"].get<int>()),
                    o["font_name"].get<std::string>());

                capt->text_ = o["text"].get<std::string>();
                anno->captions_.push_back(std::move(capt));
            } catch (std::exception &e) {
            }
        }
    }
}
