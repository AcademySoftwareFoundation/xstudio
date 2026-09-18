// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio::ui::viewport {
enum FitMode { Free, Width, Height, Fill, One2One, Best };
enum MirrorMode { Off, Flip, Flop, Both };
enum GraphicsAPI { None, OpenGL, Metal, Vulkan, DirectX };
enum ImageFormat { RGBA_8, RGBA_10_10_10_2, RGBA_16, RGBA_16F, RGBA_32F };
} // namespace xstudio::ui::viewport
