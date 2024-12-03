#pragma once

#include "dotnet/dotnet_colors.h"
#include "wpf/wpf_colors.h"
#include "x11/x11_colors.h"

namespace colors {

  typedef dotnet::basic_colors<color> dotnet_colors;
  typedef wpf::basic_colors<color> wpf_colors;
  typedef x11::basic_colors<color> x11_colors;

} // namespace colors
