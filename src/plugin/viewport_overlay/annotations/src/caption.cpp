// SPDX-License-Identifier: Apache-2.0
#include "caption.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;

Caption::Caption(
    const Imath::V2f position,
    const float wrap_width,
    const float font_size,
    const utility::ColourTriplet colour,
    const float opacity,
    const Justification justification,
    const std::string font_name)
    : position_(position),
      wrap_width_(wrap_width),
      font_size_(font_size),
      colour_(colour),
      opacity_(opacity),
      justification_(justification),
      font_name_(std::move(font_name)) {}

void Caption::modify_text(const std::string &t, std::string::const_iterator &cursor) {
    if (t.size() != 1) {
        return;
    }

    if (cursor < text_.cbegin() || cursor > text_.cend()) {
        cursor = text_.cend();
    }


    const char ascii_code = t.c_str()[0];

    const int cpos = std::distance<std::string::const_iterator>(text_.cbegin(), cursor);

    // N.B. - calling text_.begin() invalidates 'cursor' as the string data gets copied
    // to writeable buffer (or something). Maybe the way I use a string iterator for
    // the caption cursor is bad.
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
    cursor = cr;
}