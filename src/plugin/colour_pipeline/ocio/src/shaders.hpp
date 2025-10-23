// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

struct ShaderTemplates {
    static constexpr auto OCIO_display = R"(
#version 410 core

uniform int show_chan;
uniform float saturation;

//OCIODisplay

float dot(vec3 a, vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

vec4 colour_transform_op(vec4 rgba, vec2 image_pos)
{
    if (saturation != 1.0) {
        vec3 luma_weights = vec3(0.2126f, 0.7152f, 0.0722f);
        float luma = dot(rgba.rgb, luma_weights);
        rgba.rgb = luma + saturation * (rgba.rgb - luma);
    }

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
        vec3 luma_weights = vec3(0.2126f, 0.7152f, 0.0722f);
        rgba = vec4(dot(rgba.rgb, luma_weights));
    }

    return rgba;
}
    )";

    static constexpr auto OCIO_linearise = R"(
#version 410 core

//OCIOLinearise

vec4 colour_transform_op(vec4 rgba, vec2 image_pos)
{
    return OCIOLinearise(rgba);
}
    )";
};