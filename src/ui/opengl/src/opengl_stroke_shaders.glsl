// SPDX-License-Identifier: Apache-2.0

namespace {

auto const stroke_vertex_shader 
{R"glsl(

#version 410 core

uniform float z_adjust;
uniform float thickness;
uniform float brush_opacity;
uniform float size_sensitivity;
uniform float opacity_sensitivity;
uniform float soft_edge;

uniform mat4 to_coord_system;
uniform mat4 to_canvas;

layout (location = 0) in vec2 lstart;
layout (location = 1) in vec2 lend;
layout (location = 2) in vec2 vtx_size;

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

    line_start = lstart; // start point of the line
    line_end = lend; // end point of the line

    thickness_start = pow(vtx_size.x, size_sensitivity)*thickness;
    thickness_end = pow(vtx_size.y, size_sensitivity)*thickness;

    opacity_start = pow(vtx_size.x, opacity_sensitivity)*brush_opacity;
    opacity_end = pow(vtx_size.y, opacity_sensitivity)*brush_opacity;

    // vector between the two vertices
    vec2 v = (line_end == line_start ? vec2(1.0, 0.0) : normalize(line_end-line_start))*quad_thickness;
    // tangent
    vec2 tr = vec2(v.y,-v.x);

    if (i == 0) {
        vtx = line_start-v+tr;
    } else if (i == 1) {
        vtx = line_end+v+tr;
    } else if (i == 2) {
        vtx = line_end+v-tr;
    } else if (i == 3) {
        vtx = line_end+v-tr;
    } else if (i == 4) {
        vtx = line_start-v-tr;
    } else if (i == 5) {
        vtx = line_start-v+tr;
    }

    gl_Position = vec4(vtx,0.0,1.0)*to_coord_system*to_canvas;
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

in vec2 frag_pos;
out vec4 FragColor;
uniform vec3 brush_colour;

vec2 distToLine(vec2 pt)
{
    // returns a vec2. x component is distance to the line.
    // y component is scalar from 0.0-1.0 telling us how far
    // along the line from one end to the other the projection of
    // the point on the line is.

    float l2 = (line_end.x - line_start.x)*(line_end.x - line_start.x) +
               (line_end.y - line_start.y)*(line_end.y - line_start.y);

    if (l2 == 0.0) return vec2(length(pt-line_start), 1.0);

    vec2 a = pt-line_start;
    vec2 L = line_end-line_start;

    float dot = (a.x*L.x + a.y*L.y);

    float t = max(0.0, min(1.0, dot / l2));
    vec2 p = line_start + t*L;
    return vec2(length(pt-p), t);
}

void main(void)
{
 
    vec2 vv = distToLine(frag_pos);
    float r = vv.x;
    float thickness = (vv.y*thickness_end) + (1.0-vv.y)*thickness_start;
    float opacity   = (vv.y*opacity_end) + (1.0-vv.y)*opacity_start;

    r = smoothstep(
            thickness + soft_edge,
            thickness,
            r);

    if (r == 0.0f) discard;

    opacity = opacity*r;

    FragColor = vec4(
        brush_colour*opacity,
        opacity
        );
}
        
)glsl"};


auto const offscreen_vertex_shader
{R"glsl(

#version 410 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
uniform mat4 to_canvas;

out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos,0.0,1.0)*to_canvas;
    TexCoords = aTexCoords;
}

)glsl"};


auto const offscreen_frag_shader
{R"glsl(

#version 410 core

in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D offscreenTexture;

void main(void)
{
    FragColor = texture(offscreenTexture, TexCoords);
    if (FragColor.a == 0.0)
        discard;
}

)glsl"};

} // anonymous namespace
