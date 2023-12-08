// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace ui {
    namespace viewport {
        enum FitMode { Free, Width, Height, Fill, One2One, Best };
        enum MirrorMode { Off, Flip, Flop, Both };
        enum GraphicsAPI { None, OpenGL, Metal, Vulkan, DirectX };
    } // namespace viewport
} // namespace ui
} // namespace xstudio