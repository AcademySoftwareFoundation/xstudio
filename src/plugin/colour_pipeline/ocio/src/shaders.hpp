// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

struct ShaderTemplates {
    static constexpr auto OCIO = R"(
#version 420 core

uniform int show_chan;

//OCIODisplay

vec4 colour_transforms(vec4 rgba)
{
    rgba = OCIODisplay(rgba);

    if (show_chan == 1) {
        rgba = vec4(rgba.r);
    } else if (show_chan == 2) {
        rgba = vec4(rgba.g);
    } else if (show_chan == 3) {
        rgba = vec4(rgba.b);
    } else if (show_chan == 4) {
        rgba = vec4(rgba.a);
    } else if (show_chan == 5) {
        rgba = vec4((rgba.r*0.29 + rgba.g*0.59 + rgba.b*0.12));
    }

    return rgba;
}
    )";
};
