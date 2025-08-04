#pragma once

#include "color.h"


namespace colors {

  enum class brightness_value {
    light,
    dark
  };

  /**
  * Check whether the color is light or dark
  * The function converts the RGB color space into YIQ
  *
  * http://www.w3.org/TR/AERT#color-contrast
  */
  template <typename T, typename PixelTraits>
  brightness_value brightness(const basic_color<T, PixelTraits>& c) {
    color temp(c);
    auto value = ((temp.r * 299.0) + (temp.g * 587.0) + (temp.b * 114.0)) / 1000.0;
    return (value >= 128.0 ? brightness_value::light : brightness_value::dark);
  }


}
