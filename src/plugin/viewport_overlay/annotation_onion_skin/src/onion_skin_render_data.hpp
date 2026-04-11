// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>

#include "xstudio/ui/canvas/canvas.hpp"
#include "xstudio/utility/blind_data.hpp"


namespace xstudio {
namespace ui {
    namespace viewport {

        // Each neighbor canvas has opacity and tint baked into its items,
        // so it can be rendered directly with no FBO compositing.
        class OnionSkinRenderData : public utility::BlindDataObject {
          public:
            OnionSkinRenderData() = default;
            explicit OnionSkinRenderData(std::vector<canvas::Canvas> c)
                : canvases(std::move(c)) {}
            ~OnionSkinRenderData() override = default;

            std::vector<canvas::Canvas> canvases; // farthest-to-nearest order
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
