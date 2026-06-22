#pragma once

#include "color.h"
#include "color_named.h"
#include "color_io.h"
#include "color_utils.h"
#include "pixel_format.h"
#include "pixel_utils.h"

#include "impl/colors_impl.h"
#include "impl/constants_impl.h"
#include "impl/named_impl.h"

namespace colors {
  typedef basic_named_color_converter<dotnet_color_mapper> dotnet_named_color_converter;
  typedef basic_named_color_converter<wpf_color_mapper> wpf_named_color_converter;
  typedef basic_named_color_converter<x11_color_mapper> x11_named_color_converter;
}
