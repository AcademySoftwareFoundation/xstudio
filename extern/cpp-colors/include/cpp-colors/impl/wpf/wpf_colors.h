#pragma once

#include "wpf_constants.h"
#include "../named_utils.h"


namespace colors {
  namespace wpf {

    template<typename ColorType>
    struct basic_colors {
      typedef ColorType color_type;

      basic_colors() = delete;
      ~basic_colors() = delete;
      basic_colors(const basic_colors&) = delete;
      basic_colors& operator =(const basic_colors&) = delete;

      static color_type black() {
        return color_type(value_of(known_color::black));
      }

      static color_type blue() {
        return color_type(value_of(known_color::blue));
      }

      static color_type brown() {
        return color_type(value_of(known_color::brown));
      }

      static color_type cyan() {
        return color_type(value_of(known_color::cyan));
      }

      static color_type dark_gray() {
        return color_type(value_of(known_color::dark_gray));
      }

      static color_type gray() {
        return color_type(value_of(known_color::gray));
      }

      static color_type green() {
        return color_type(value_of(known_color::green));
      }

      static color_type light_gray() {
        return color_type(value_of(known_color::light_gray));
      }

      static color_type magenta() {
        return color_type(value_of(known_color::magenta));
      }

      static color_type orange() {
        return color_type(value_of(known_color::orange));
      }

      static color_type purple() {
        return color_type(value_of(known_color::purple));
      }

      static color_type red() {
        return color_type(value_of(known_color::red));
      }

      static color_type transparent() {
        return color_type(value_of(known_color::transparent));
      }

      static color_type white() {
        return color_type(value_of(known_color::white));
      }

      static color_type yellow() {
        return color_type(value_of(known_color::yellow));
      }
    };

  } // namespace wpf
} // namespace colors
