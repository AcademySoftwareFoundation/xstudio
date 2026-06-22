#pragma once

#include "dotnet/dotnet_named.h"
#include "wpf/wpf_named.h"
#include "x11/x11_named.h"

namespace colors {

  typedef dotnet::basic_named_color<char> dotnet_named_color;
  typedef dotnet::basic_color_mapper<char> dotnet_color_mapper;

  typedef wpf::basic_named_color<char> wpf_named_color;
  typedef wpf::basic_color_mapper<char> wpf_color_mapper;

  typedef x11::basic_named_color<char> x11_named_color;
  typedef x11::basic_color_mapper<char> x11_color_mapper;

} // namespace colors
