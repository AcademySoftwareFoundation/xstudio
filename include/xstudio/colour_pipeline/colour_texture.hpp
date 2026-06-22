// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace xstudio {

namespace colour_pipeline {

    // Quick and dirty placeholder for OpenGL texture descriptor
    // representing an already existing and created texture.
    // Probably should use some structure directly from core xStudio
    // if already available or create otherwise.

    enum ColourTextureTarget {
        TEXTURE_2D,
    };

    struct ColourTexture {
        std::string name;
        ColourTextureTarget target;
        unsigned int id;
    };

} // namespace colour_pipeline
} // namespace xstudio
