#pragma once

#include <cstdint>

namespace colors {
  namespace wpf {

    enum class known_color : uint32_t {
      black = 0xFF000000,   //                  Black: argb(255, 0, 0, 0)
      blue = 0xFF0000FF,   //                   Blue: argb(255, 0, 0, 255)
      brown = 0xFFA52A2A,   //                  Brown: argb(255, 165, 42, 42)
      cyan = 0xFF00FFFF,   //                   Cyan: argb(255, 0, 255, 255)
      dark_gray = 0xFFA9A9A9,   //              Dark Gray: argb(255, 169, 169, 169)
      gray = 0xFF808080,   //                   Gray: argb(255, 128, 128, 128)
      green = 0xFF008000,   //                  Green: argb(255, 0, 128, 0)
      light_gray = 0xFFD3D3D3,   //             Light Gray: argb(255, 211, 211, 211)
      magenta = 0xFFFF00FF,   //                Magenta: argb(255, 255, 0, 255)
      orange = 0xFFFFA500,   //                 Orange: argb(255, 255, 165, 0)
      purple = 0xFF800080,   //                 Purple: argb(255, 128, 0, 128)
      red = 0xFFFF0000,   //                    Red: argb(255, 255, 0, 0)
      transparent = 0x00FFFFFF,   //            Transparent: argb(0, 255, 255, 255)
      white = 0xFFFFFFFF,   //                  White: argb(255, 255, 255, 255)
      yellow = 0xFFFFFF00,   //                 Yellow: argb(255, 255, 255, 0)
    };

  } // namespace wpf
} // namespace colors
