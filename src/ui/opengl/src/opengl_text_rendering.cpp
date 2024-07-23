// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <memory>
#include <sstream>

#include <GL/glew.h>
#include <GL/gl.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftglyph.h>

#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/chrono.hpp"

#define NUM_AA_SAMPS_1D 1

using namespace xstudio::ui::opengl;

namespace {

const char *bitmap_vtx_shader = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;

void main()
{
    gl_Position = vec4(vertex.xy, 0.0, 1.0)*to_coord_system*to_canvas;
    TexCoords = vertex.zw;
}
)";

const char *bitmap_frag_shader = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2DRect text;
uniform vec3 textColor;
uniform float opacity;

void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r*opacity);
    color = vec4(textColor, 1.0)*sampled;
}
)";
} // namespace


OpenGLTextRendererBitmap::OpenGLTextRendererBitmap(
    const std::string ttf_font_path, const int glyph_pixel_size)
    : AlphaBitmapFont(ttf_font_path, glyph_pixel_size) {

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ROW_LENGTH, atlas_width());
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, atlas_height());
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glGenTextures(1, &texture_);

    // set texture options
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_RECTANGLE, texture_);

    glTexImage2D(
        GL_TEXTURE_RECTANGLE,
        0,
        GL_RED,
        atlas_width(),
        atlas_height(),
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        atlas().data());

    // set texture options
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    shader_ =
        std::make_unique<ui::opengl::GLShaderProgram>(bitmap_vtx_shader, bitmap_frag_shader);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8192 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OpenGLTextRendererBitmap::render_text(
    const std::vector<float> &precomputed_vertex_buffer,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const utility::ColourTriplet &colour,
    const float viewport_du_dx,
    const float text_size,
    const float opacity) const {
    // activate corresponding render state
    shader_->use();
    utility::JsonStore tparams;
    tparams["textColor"]       = colour;
    tparams["to_coord_system"] = transform_viewport_to_image_space.inverse();
    tparams["to_canvas"]       = transform_window_to_viewport_space;
    tparams["opacity"]         = opacity;

    shader_->set_shader_parameters(tparams);

    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_RECTANGLE, texture_);
    // update content of vbo_ memory

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(float) * precomputed_vertex_buffer.size(),
        precomputed_vertex_buffer.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // render quad
    glDrawArrays(GL_QUADS, 0, precomputed_vertex_buffer.size() / 4);
    // now advance cursors for next glyph (note that advance is number of 1/64 pixels)

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
}


namespace {

const char *sdf_vtx_shader = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;

void main()
{
    gl_Position = vec4(vertex.xy, 0.0, 1.0)*to_coord_system*to_canvas;
    TexCoords = vertex.zw;
}
)";

const char *sdf_frag_shader = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2DRect text;
uniform vec3 textColor;
uniform float aa_edge_transition;
uniform float opacity;

void main()
{
    float opac = smoothstep(
            -aa_edge_transition,
            aa_edge_transition,
            texture(text, TexCoords).r
            )*opacity;

    color = vec4(textColor*opac, opac);
}
)";
} // namespace


OpenGLTextRendererSDF::OpenGLTextRendererSDF(
    const std::string ttf_font_path, const int glyph_image_size_in_pixels)
    : SDFBitmapFont(ttf_font_path, glyph_image_size_in_pixels) {

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ROW_LENGTH, atlas_width());
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, atlas_height());
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glGenTextures(1, &texture_);

    // set texture options
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_RECTANGLE, texture_);

    glTexImage2D(
        GL_TEXTURE_RECTANGLE,
        0,
        GL_R32F,
        atlas_width(),
        atlas_height(),
        0,
        GL_RED,
        GL_FLOAT,
        atlas().data());

    // set texture options
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(sdf_vtx_shader, sdf_frag_shader);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8192 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void OpenGLTextRendererSDF::render_text(
    const std::vector<float> &precomputed_vertex_buffer,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const utility::ColourTriplet &colour,
    const float viewport_du_dx,
    const float text_size,
    const float opacity) const {

    const float scale = text_size * 2.0f / (1920.0f * glyph_pixel_size());

    // activate corresponding render state
    shader_->use();
    utility::JsonStore tparams;
    tparams["textColor"]       = colour;
    tparams["to_coord_system"] = transform_viewport_to_image_space.inverse();
    tparams["to_canvas"]       = transform_window_to_viewport_space;
    tparams["opacity"]         = opacity;

    // This parameter scales the 'soft' edge of the font to achieve anti-aliasing via
    // the glyph signed-distance-function maps. The 1.5 factor was found through
    // experimentation to give the best anti-aliased appearance - bigger value
    // means softer edges on font rendering
    tparams["aa_edge_transition"] = 1.5f * viewport_du_dx / scale;

    shader_->set_shader_parameters(tparams);

    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_RECTANGLE, texture_);

    // update content of vbo_ memory

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(float) * precomputed_vertex_buffer.size(),
        precomputed_vertex_buffer.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // render quad
    glDrawArrays(GL_QUADS, 0, precomputed_vertex_buffer.size() / 4);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    shader_->stop_using();
}


namespace {
const char *vertex_shader = R"(
#version 330 core
layout (location = 0) in vec2 vertex; // <vec2 pos, vec2 tex>
uniform mat4 to_coord_system;
uniform mat4 to_canvas;
uniform float x_pos;
uniform float z_pos;
uniform float y_pos;
uniform float scale;

void main()
{
    gl_Position = vec4(vertex.x*scale + x_pos,-vertex.y*scale+y_pos,z_pos,1.0)*to_coord_system*to_canvas;
    gl_Position.z = z_pos;
    //TexCoords = vertex.zw;
}
)";

const char *frag_shader = R"(
#version 330 core
out vec4 color;
uniform float alpha;

//uniform sampler2D text;
uniform vec3 textColor;

void main()
{
    color = vec4(textColor, alpha);
}
)";
} // namespace

void OpenGLTextRendererVector::compute_wrap_indeces(
    const std::string &text, const float box_width, const float scale) {

    wrap_points_.clear();
    std::string::const_iterator c               = text.begin();
    std::string::const_iterator last_word_begin = text.begin();
    float x                                     = 0.0f;
    int last_space                              = -1;
    bool doneline                               = false;
    while (c != text.end()) {

        const CharacterMetrics &character = get_character(*c);
        x += character.hori_advance_ * scale; // bitshift by 6 to get value in pixels (2^6 = 64)

        if (x > box_width && (*c == ' ' || *c == '\n') && doneline) {
            // need to wrap ... go back to last space
            c = last_word_begin;
            wrap_points_.push_back(c);
            c++;
            if (c == text.end())
                break;
            x        = 0;
            doneline = false;
        }

        if (*c == ' ' || *c == '\n') {
            auto p = c;
            p++;
            if (p == text.end())
                break;
            last_word_begin = p;
            doneline        = true;
        }

        c++;
    }
}

OpenGLTextRendererVector::OpenGLTextRendererVector(const std::string ttf_font_path)
    : VectorFont(ttf_font_path, 48.0f) {

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);

    // make a vertex buffer and upload ALL the vertices for all the glyphs for
    // the font
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        outline_vertices().size() * sizeof(Imath::V2f),
        outline_vertices().data(),
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // make an element array buffer and upload ALL the triangle indeces for all
    // the glyphs for the font
    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(unsigned int) * triangle_indeces().size(),
        triangle_indeces().data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLTextRendererVector::render_text(
    const std::string &text,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const Imath::V2f wrap_margin_topleft,
    const Imath::V2f wrap_margin_topright,
    const float text_size,
    const utility::ColourTriplet &colour,
    const Justification &just,
    const float line_size) {

    // N.B. the 'text_size' defines the font size in pixels if the viewport is width
    // fitted to a 1080p display. The 2.0 comes from the fact that xstudio's coordinate
    // system spans from -1.0 to +1.0 where the left and right edges of the image land.
    const float scale = text_size * 2.0f / (1920.0f);

    // activate corresponding render state
    shader_->use();
    utility::JsonStore tparams;
    tparams["textColor"]       = colour;
    tparams["to_coord_system"] = transform_viewport_to_image_space.inverse();
    tparams["to_canvas"]       = transform_window_to_viewport_space;
    tparams["scale"]           = scale;

    shader_->set_shader_parameters(tparams);

    glEnable(GL_DEPTH_TEST);
    glClearDepth(0.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_GREATER);

    glBindVertexArray(vao_);

    float x = wrap_margin_topleft.x;
    float y = wrap_margin_topleft.y;

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_ONE);

    compute_wrap_indeces(text, wrap_margin_topright.x - wrap_margin_topleft.x, scale);

    x = wrap_margin_topleft.x;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    std::string::const_iterator c;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    int location  = glGetUniformLocation(shader_->program_, "x_pos");
    int location2 = glGetUniformLocation(shader_->program_, "y_pos");

    tparams["z_pos"]     = 0.1;
    tparams["y_pos"]     = y;
    tparams["x_pos"]     = x;
    tparams["alpha"]     = 0.0;
    tparams["textColor"] = colour;
    shader_->set_shader_parameters(tparams);

    auto wrap_point = wrap_points_.begin();
    for (c = text.begin(); c != text.end(); c++) {

        const CharacterMetrics &character = get_character(*c);

        if (wrap_point != wrap_points_.end() && *wrap_point == c) {
            y += line_size * scale;
            x = wrap_margin_topleft.x;
            wrap_point++;
        }

        glUniform1f(location, x);
        glUniform1f(location2, y);

        {
            for (const auto &shape_details : character.negative_shapes_) {

                uint8_t *v = nullptr;
                v += shape_details.first_triangle_index_ * sizeof(unsigned int);
                glDrawElements(
                    GL_TRIANGLES, shape_details.num_triangles_, GL_UNSIGNED_INT, (void *)v);
            }
        }
        x += character.hori_advance_ * scale;
    }

    tparams["alpha"] = 1.0;
    shader_->set_shader_parameters(tparams);
    x = wrap_margin_topleft.x;
    y = wrap_margin_topleft.y;

    wrap_point = wrap_points_.begin();
    for (c = text.begin(); c != text.end(); c++) {

        const CharacterMetrics &character = get_character(*c);
        if (wrap_point != wrap_points_.end() && *wrap_point == c) {
            y += line_size * scale;
            x = wrap_margin_topleft.x;
            wrap_point++;
        }

        glUniform1f(location, x);
        glUniform1f(location2, y);

        {
            for (const auto &shape_details : character.positive_shapes_) {

                uint8_t *v = nullptr;
                v += shape_details.first_triangle_index_ * sizeof(unsigned int);
                glDrawElements(
                    GL_TRIANGLES, shape_details.num_triangles_, GL_UNSIGNED_INT, (void *)v);
            }
        }
        x += character.hori_advance_ * scale;
    };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
    // glBindTexture(GL_TEXTURE_2D, 0);
}