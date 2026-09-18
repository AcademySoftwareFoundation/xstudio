// SPDX-License-Identifier: Apache-2.0

// Pass 2 offscreen shader: renders the stroke geometry again but this time
// samples the offscreen texture (which contains the alpha mask from pass 1)
// and applies the brush colour. This replaces the old fullscreen-quad
// composite and only touches pixels where the stroke actually exists.

namespace {

auto const offscreen_vertex_shader
{R"glsl(

#version 410 core

uniform float z_adjust;
uniform float thickness;
uniform float brush_opacity;
uniform float soft_edge;

uniform mat4 to_coord_system;
uniform mat4 to_canvas;

layout (location = 0) in vec2 lstart;
layout (location = 1) in vec2 lend;
layout (location = 2) in vec2 vtx_size_mapped;
layout (location = 3) in vec2 vtx_opacity_mapped;

out vec2 frag_pos;

void main()
{
    // Same geometry expansion as the stroke shader
    int i = gl_VertexID%6;
    vec2 vtx;
    float quad_thickness = thickness + soft_edge;

    vec2 line_start = lstart;
    vec2 line_end = lend;

    // Calculate direction
    vec2 dir = (line_end == line_start ? vec2(1.0, 0.0) : normalize(line_end - line_start));

    // Extend line by thickness amount to create round caps
    vec2 extension = dir * quad_thickness;
    vec2 extended_start = line_start - extension;
    vec2 extended_end = line_end + extension;

    // Calculate perpendicular offset
    vec2 v = extension;
    vec2 tr = vec2(v.y, -v.x);

    if (i == 0) {
        vtx = extended_start + tr;
    } else if (i == 1) {
        vtx = extended_end + tr;
    } else if (i == 2) {
        vtx = extended_end - tr;
    } else if (i == 3) {
        vtx = extended_end - tr;
    } else if (i == 4) {
        vtx = extended_start - tr;
    } else if (i == 5) {
        vtx = extended_start + tr;
    }

    // gl_Position uses the full transform chain for correct screen placement
    vec4 p = to_canvas * to_coord_system * vec4(vtx, 0.0, 1.0);
    gl_Position = p;
    gl_Position.z = z_adjust;

    // frag_pos is computed WITHOUT to_canvas, matching pass 1 which
    // renders to the offscreen texture using identity for to_canvas.
    // This ensures the NDC->UV mapping samples the correct texel
    // regardless of where the viewport sits within the window.
    vec4 p_offscreen = to_coord_system * vec4(vtx, 0.0, 1.0);
    frag_pos = vec2(p_offscreen.x/p_offscreen.w, p_offscreen.y/p_offscreen.w);
}

)glsl"};


auto const offscreen_frag_shader
{R"glsl(

#version 410 core

uniform sampler2D offscreenTexture;
uniform vec3 brush_colour;
uniform int blend_mode;

in vec2 frag_pos;
out vec4 FragColor;

void main(void)
{
    // frag_pos is in NDC space (-1 to 1). Convert to UV space (0 to 1)
    // to sample the offscreen texture.
    vec2 coord = vec2((frag_pos.x+1.0)*0.5, (frag_pos.y+1.0)*0.5);
    float mask = texture(offscreenTexture, coord).r;

    if (blend_mode >= 1 && blend_mode <= 4) {
        // Effect strokes (burn/dodge): output intensity strength in RGB, alpha=0.
        // brush_colour.r carries the intensity value.
        float strength = brush_colour.r * mask;
        FragColor = vec4(strength, strength, strength, 0.0);
    } else {
        // Normal stroke: premultiplied alpha composite
        float opacity = mask;
        FragColor = vec4(brush_colour * opacity, opacity);
    }
}

)glsl"};

} // anonymous namespace
