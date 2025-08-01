// SPDX-License-Identifier: Apache-2.0
#include <sstream>

#include "xstudio/ui/opengl/no_image_shader_program.hpp"

using namespace xstudio::ui::opengl;

namespace {

static const std::string basic_vertex_shd = R"(
#version 330 core
vec2 calc_pixel_coordinate(vec2 viewport_coordinate)
{
    return viewport_coordinate*4.0;
}
)";

static const std::string linearise_colour_transform = R"(
vec4 linearise_colour_transform(vec4 rgba_in)
{
    return rgba_in;
}
)";

static const std::string display_colour_transform = R"(
vec4 display_colour_transform(vec4 rgba_in)
{
    return rgba_in;
}
)";

static const std::string basic_frag_shd = R"(
#version 330 core
vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    // black!
    return vec4(0.0,0.0,0.0,1.0);
}
)";

} // namespace

NoImageShaderProgram::NoImageShaderProgram()
    : GLShaderProgram(basic_vertex_shd, basic_frag_shd, std::vector<std::string>{}, false) {}
