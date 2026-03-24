// SPDX-License-Identifier: Apache-2.0

namespace {

auto const vertex_shader 
{R"glsl(

    #version 410 core

    layout (location = 1) in vec4 pos;
    layout (location = 0) in vec4 color;

    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform vec2 v_pos;
    uniform float viewport_du_dpixel;
    out vec4 t_color;
    out vec2 nrd;

    void main()
    {
        nrd = pos.xy;
        float l = length(nrd);
        float F = 5;
        float lens_scale = 2*l - l*l/F;
        nrd = normalize(nrd)*lens_scale;
        vec4 rpos = vec4((nrd.x+4.0)*50.0*viewport_du_dpixel, -(nrd.y+4.0)*50.0*viewport_du_dpixel, 0.0, 1.0);
        rpos.x += v_pos.x;
        rpos.y += v_pos.y;
        gl_Position = rpos*to_coord_system*to_canvas;
        t_color = color;
    }
)glsl"};

auto const frag_shader
{R"glsl(

    #version 410 core

    out vec4 FragColor;
    uniform vec3 line_colour;
    in vec4 t_color;
    in vec2 nrd;

    void main(void)
    {
        float c = length(nrd);
        FragColor = t_color*smoothstep(4.1,4.0,c);
    }

)glsl"};

} // anonymous namespace
