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
    : position_(position),
      wrap_width_(wrap_width),
      font_size_(font_size),
      colour_(colour),
      opacity_(opacity),
      justification_(justification),
      font_name_(std::move(font_name)),
      background_colour_(background_colour),
      background_opacity_(background_opacity),
      id_(utility::Uuid::generate()) {

    update_vertices();
}

bool Caption::operator==(const Caption &o) const { return hash_ == o.hash_; }

void Caption::set_cursor_position(const Imath::V2f &screen_pos) {

    cursor_position_ =
        SDFBitmapFont::font_by_name(font_name_)
            ->viewport_position_to_cursor(
                screen_pos, text_, position_, wrap_width_, font_size_, justification_, 1.0f);
}

void Caption::move_cursor(int key) {


    if (key == 16777235) {
        // up arrow
        cursor_position_ = SDFBitmapFont::font_by_name(font_name_)
                               ->cursor_up_or_down(
                                   cursor_position_,
                                   true,
                                   text_,
                                   wrap_width_,
                                   font_size_,
                                   justification_,
                                   1.0f);

    } else if (key == 16777237) {
        // down arrow
        cursor_position_ = SDFBitmapFont::font_by_name(font_name_)
                               ->cursor_up_or_down(
                                   cursor_position_,
                                   false,
                                   text_,
                                   wrap_width_,
                                   font_size_,
                                   justification_,
                                   1.0f);

    } else if (key == 16777236) {
        // right arrow
        if (cursor_position_ != text_.cend())
            cursor_position_++;

    } else if (key == 16777234) {
        // left arrow
        if (cursor_position_ != text_.cbegin())
            cursor_position_--;

    } else if (key == 16777232) {
        // home
        cursor_position_ = text_.cbegin();

    } else if (key == 16777233) {
        // end
        cursor_position_ = text_.cend();
    }
}

void Caption::modify_text(const std::string &t) {

    if (t.size() != 1) {
        return;
    }

    if (cursor_position_ < text_.cbegin() || cursor_position_ > text_.cend()) {
        cursor_position_ = text_.cend();
    }

    const char ascii_code = t.c_str()[0];

    const int cpos =
        std::distance<std::string::const_iterator>(text_.cbegin(), cursor_position_);

    // N.B. - calling text_.begin() invalidates 'cursor_position_' as the string data gets
    // copied to writeable buffer (or something). Maybe the way I use a string iterator for the
    // caption cursor_position_ is bad.
    auto cr = text_.begin();

    std::advance(cr, cpos);

    if (ascii_code == 127) {
        // delete
        if (cr != text_.end()) {
            cr = text_.erase(cr);
        }
    } else if (ascii_code == 8) {
        // backspace
        if (text_.size() && cr != text_.begin()) {
            auto p = cr;
            p--;
            cr = text_.erase(p);
        }
    } else if (ascii_code >= 32 || ascii_code == '\r' || ascii_code == '\n') {
        // printable character
        cr = text_.insert(cr, ascii_code);
        cr++;
    }
    cursor_position_ = cr;

    update_vertices();
}

std::array<Imath::V2f, 2> Caption::cursor_position_on_image() const {

    std::array<Imath::V2f, 2> result = {Imath::V2f(0.0f, 0.0f), Imath::V2f(0.0f, 0.0f)};
    Imath::V2f v                     = SDFBitmapFont::font_by_name(font_name_)
                       ->get_cursor_screen_position(
                           text_,
                           position_,
                           wrap_width_,
                           font_size_,
                           justification_,
                           1.0f,
                           cursor_position_);

    result[0] = v;
    result[1] = v - Imath::V2f(0.0f, font_size_ * 2.0f / 1920.0f * 0.8f);
    return result;
}

void Caption::update_hash() {

    std::string hash;
    hash += text_;
    hash += std::to_string(position_.x);
    hash += std::to_string(position_.y);
    hash += std::to_string(wrap_width_);
    hash += std::to_string(font_size_);
    hash += std::to_string((int)justification_);
    hash += std::to_string(opacity_);
    hash += std::to_string(background_opacity_);

    hash_ = std::hash<std::string>{}(hash);
}

void Caption::update_vertices() {

    size_t old_hash = hash_;
    update_hash();

    if (old_hash != hash_) {
        bounding_box_ =
            SDFBitmapFont::font_by_name(font_name_)
                ->precompute_text_rendering_vertex_layout(
                    vertices_, text_, position_, wrap_width_, font_size_, justification_, 1.0f);
    }
}

void xstudio::ui::canvas::from_json(const nlohmann::json &j, Caption &c) {

    j.at("text").get_to(c.text_);
    j.at("position").get_to(c.position_);
    j.at("wrap_width").get_to(c.wrap_width_);
    j.at("font_size").get_to(c.font_size_);
    j.at("font_name").get_to(c.font_name_);
    j.at("colour").get_to(c.colour_);
    j.at("opacity").get_to(c.opacity_);
    j.at("justification").get_to(c.justification_);

    if (j.contains("background_colour") && j.contains("background_opacity")) {
        j.at("background_colour").get_to(c.background_colour_);
        j.at("background_opacity").get_to(c.background_opacity_);
    }
    c.update_vertices();
}

void xstudio::ui::canvas::to_json(nlohmann::json &j, const Caption &c) {

    j = nlohmann::json{
        {"text", c.text_},
        {"position", c.position_},
        {"wrap_width", c.wrap_width_},
        {"font_size", c.font_size_},
        {"font_name", c.font_name_},
        {"colour", c.colour_},
        {"opacity", c.opacity_},
        {"justification", c.justification_},
        {"background_colour", c.background_colour_},
        {"background_opacity", c.background_opacity_}};
}