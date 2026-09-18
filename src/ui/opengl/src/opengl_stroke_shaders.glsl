// SPDX-License-Identifier: Apache-2.0

namespace {

auto const stroke_vertex_shader
{R"glsl(

#version 410 core

uniform float z_adjust;
uniform float thickness;
uniform float brush_opacity;
uniform float soft_edge;
uniform float just_black;

uniform mat4 to_coord_system;
uniform mat4 to_canvas;

layout (location = 0) in vec2 lstart;
layout (location = 1) in vec2 lend;
layout (location = 2) in vec2 vtx_size_mapped;
layout (location = 3) in vec2 vtx_opacity_mapped;

flat out vec2 line_start;
flat out vec2 line_end;
flat out float thickness_start;
flat out float thickness_end;
flat out float opacity_start;
flat out float opacity_end;

out vec2 frag_pos;

void main()
{
    // 6 verts are needed for each line segment - we are drawing a rectangle
    // that bounds the line and is 'thickness' wide and thickness longer
    // than the line segment

    int i = gl_VertexID%6;
    vec2 vtx;
    float quad_thickness = thickness + soft_edge;

    // When rendering black (clearing pass), expand the quad by an extra
    // soft_edge so that the black fill fully covers any anti-aliased edge
    // pixels that will be rendered in the subsequent alpha pass.
    if (just_black == 1.0) {
        quad_thickness += soft_edge;
    }

    line_start = lstart; // start point of the line
    line_end = lend; // end point of the line

    thickness_start = vtx_size_mapped.x * thickness;
    thickness_end = vtx_size_mapped.y * thickness;

    opacity_start = vtx_opacity_mapped.x * brush_opacity;
    opacity_end = vtx_opacity_mapped.y * brush_opacity;

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

    gl_Position = to_canvas * to_coord_system * vec4(vtx, 0.0, 1.0);
    gl_Position.z = z_adjust;
    frag_pos = vtx;
}

)glsl"};


auto const stroke_frag_shader
{R"glsl(

#version 410 core

flat in vec2 line_start;
flat in vec2 line_end;
flat in float thickness_start;
flat in float thickness_end;
flat in float opacity_start;
flat in float opacity_end;
uniform float soft_edge;
uniform float just_black;

in vec2 frag_pos;
out vec4 FragColor;

vec2 distToLine(vec2 pt)
{
    float l2 = dot(line_end - line_start, line_end - line_start);
    if (l2 == 0.0) return vec2(length(pt - line_start), 1.0);

    vec2 a = pt - line_start;
    vec2 L = line_end - line_start;
    float t = max(0.0, min(1.0, dot(a, L) / l2));
    vec2 p = line_start + t * L;

    return vec2(length(pt - p), t);
}

void main(void)
{
    // Pass 1 "clearing" sub-pass: output black to reset the offscreen
    // texture in the stroke's footprint. No distance calculation needed.
    if (just_black == 1.0) {
        FragColor.r = 0.0;
        return;
    }

    // Pass 1 "alpha" sub-pass (just_black == 0.0) and
    // erase stroke pass (just_black == 2.0):
    // Compute distance-based opacity and write to the red channel only.
    // The brush colour is applied later in pass 2 by the offscreen shader.
    vec2 vv = distToLine(frag_pos);
    float r = vv.x;
    float thickness = (vv.y*thickness_end) + (1.0-vv.y)*thickness_start;
    float opacity   = (vv.y*opacity_end) + (1.0-vv.y)*opacity_start;

    r = smoothstep(
            thickness + soft_edge,
            thickness,
            r);

    opacity = opacity * r;

    // For erase strokes (just_black == 2.0), discard fully transparent
    // fragments so they don't write to the depth buffer
    if (opacity == 0.0 && just_black == 2.0) discard;

    FragColor.r = opacity;
}

)glsl"};

} // anonymous namespace
