// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/canvas/caption.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio;

Caption::Caption(
    const Imath::V2f position,
    const float wrap_width,
    const float font_size,
    const utility::ColourTriplet colour,
    const float opacity,
    const Justification justification,
    const std::string font_name,
    const utility::ColourTriplet background_colour,
    const float background_opacity)
    : position(position),
      wrap_width(wrap_width),
      font_size(font_size),
      colour(colour),
      opacity(opacity),
      justification(justification),
      font_name(std::move(font_name)),
      background_colour(background_colour),
      background_opacity(background_opacity) {}

bool Caption::operator==(const Caption &o) const {

    return (
        text == o.text && position == o.position && wrap_width == o.wrap_width &&
        font_size == o.font_size && font_name == o.font_name && colour == o.colour &&
        opacity == o.opacity && justification == o.justification &&
        background_colour == o.background_colour && background_opacity == o.background_opacity);
}

void Caption::modify_text(const std::string &t, std::string::const_iterator &cursor) {

    if (t.size() != 1) {
        return;
    }

    if (cursor < text.cbegin() || cursor > text.cend()) {
        cursor = text.cend();
    }

    const char ascii_code = t.c_str()[0];

    const int cpos = std::distance<std::string::const_iterator>(text.cbegin(), cursor);

    // N.B. - calling text.begin() invalidates 'cursor' as the string data gets copied
    // to writeable buffer (or something). Maybe the way I use a string iterator for
    // the caption cursor is bad.
    auto cr = text.begin();

    std::advance(cr, cpos);

    if (ascii_code == 127) {
        // delete
        if (cr != text.end()) {
            cr = text.erase(cr);
        }
    } else if (ascii_code == 8) {
        // backspace
        if (text.size() && cr != text.begin()) {
            auto p = cr;
            p--;
            cr = text.erase(p);
        }
    } else if (ascii_code >= 32 || ascii_code == '\r' || ascii_code == '\n') {
        // printable character
        cr = text.insert(cr, ascii_code);
        cr++;
    }
    cursor = cr;

    update_vertices();
}

Imath::Box2f Caption::bounding_box() const {

    update_vertices();
    return bounding_box_;
}

std::vector<float> Caption::vertices() const {

    update_vertices();
    return vertices_;
}

std::string Caption::hash() const {

    std::string hash;
    hash += text;
    hash += std::to_string(position.x);
    hash += std::to_string(position.y);
    hash += std::to_string(wrap_width);
    hash += std::to_string(font_size);
    hash += std::to_string((int)justification);

    return hash;
}

void Caption::update_vertices() const {
    const std::string curr_hash = hash();

    if (curr_hash != hash_) {
        bounding_box_ =
            SDFBitmapFont::font_by_name(font_name)->precompute_text_rendering_vertex_layout(
                vertices_, text, position, wrap_width, font_size, justification, 1.0f);
        hash_ = curr_hash;
    }
}

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Caption &c) {

    j.at("text").get_to(c.text);
    j.at("position").get_to(c.position);
    j.at("wrap_width").get_to(c.wrap_width);
    j.at("font_size").get_to(c.font_size);
    j.at("font_name").get_to(c.font_name);
    j.at("colour").get_to(c.colour);
    j.at("opacity").get_to(c.opacity);
    j.at("justification").get_to(c.justification);

    if (j.contains("background_colour") && j.contains("background_opacity")) {
        j.at("background_colour").get_to(c.background_colour);
        j.at("background_opacity").get_to(c.background_opacity);
    }
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Caption &c) {

    j = nlohmann::json{
        {"text", c.text},
        {"position", c.position},
        {"wrap_width", c.wrap_width},
        {"font_size", c.font_size},
        {"font_name", c.font_name},
        {"colour", c.colour},
        {"opacity", c.opacity},
        {"justification", c.justification},
        {"background_colour", c.background_colour},
        {"background_opacity", c.background_opacity}};
}