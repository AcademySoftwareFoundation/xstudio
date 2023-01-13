// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <sstream>

#include "xstudio/ui/opengl/texture.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::ui::opengl;

namespace {

// convenience method to unpack a json item into a typed std vector.
template <typename T>
std::vector<T> unpack_json_to_vect(
    const xstudio::utility::JsonStore &v, const int size, const std::string &nm) {
    auto vv = v.begin();
    if (v.size() != static_cast<size_t>(size + 2)) {
        std::stringstream a;
        a << "Shader parameter " << nm << " of type " << vv.value().get<std::string>()
          << " is being passed from video frame buffer with " << v.size()
          << " values, expecting " << (size + 2)
          << " values (including type and count values at start).";
        throw std::runtime_error(a.str());
    }
    vv++; // skip param type
    vv++; // skip count
    std::vector<T> rt(size);
    for (int i = 0; i < size; ++i) {
        rt[i] = (vv++).value().get<T>();
    }
    return rt;
}

// convenience method to call a glUniformXTv, passing in a Json item holding the values to
// be set
template <typename T>
void set_gl_uniform(
    std::function<void(int, GLsizei, T *)> f,
    int location,
    const xstudio::utility::JsonStore &v,
    const int count,
    const int typesize,
    const std::string &nm) {
    std::vector<T> vals = unpack_json_to_vect<T>(v, count * typesize, nm);
    std::apply(f, std::make_tuple(location, count, vals.data()));
}

// convenience method to call a glUniformMatrixXfv, passing in a Json item holding the
// values to be set on the matrix
void set_gl_uniform_matrix(
    std::function<void(int, GLsizei, bool, float *)> f,
    int location,
    const xstudio::utility::JsonStore &v,
    const int count,
    const int typesize,
    const std::string &nm) {
    std::vector<float> vals = unpack_json_to_vect<float>(v, count * typesize, nm);
    std::apply(f, std::make_tuple(location, count, false, vals.data()));
}


const char *vertex_shader_base = R"(
#version 330 core
layout (location = 0) in vec4 aPos;
out vec2 texPosition;
uniform ivec2 image_dims;
uniform mat4 to_coord_system;
uniform mat4 to_canvas;
uniform float pixel_aspect;

vec2 calc_pixel_coordinate(vec2 viewport_coordinate);

void main()
{
    vec4 rpos = aPos*to_coord_system;
    gl_Position = aPos*to_canvas;
    texPosition = vec2((rpos.x + 1.0f)*float(image_dims.x), (rpos.y*pixel_aspect*float(image_dims.x))+float(image_dims.y))*0.5f;
}
)";

const char *frag_shader_base = R"(
#version 430 core
#extension GL_ARB_shader_storage_buffer_object : require
in vec2 texPosition;
out vec4 FragColor;
//uniform usampler2DRect the_tex;
uniform ivec2 image_dims;
uniform ivec2 image_bounds_min;
uniform ivec2 image_bounds_max;

uniform bool use_bilinear_filtering;
uniform bool use_ssbo;

uniform usampler2DRect the_tex;
uniform ivec2 tex_dims;

ivec2 step_sample(ivec2 tex_coord)
{
    if (tex_coord.x < (tex_dims.x-1))
    {
        return ivec2(tex_coord.x+1,tex_coord.y);
    } else {
        return ivec2(0,tex_coord.y+1);
    }
}

// This function returns 1 byte of image data packed
// in an int at the given byte address into the raw image
// buffer as created by the image reader
int get_image_data_1byte_tex(int byte_address) {

    int elem = byte_address&3;
    int newAddress = byte_address >> 2;
    ivec2 tex_coord = ivec2(newAddress%tex_dims.x, newAddress/tex_dims.x);
    uvec4 c1 = texture(the_tex, tex_coord);

    int r;
    if (elem == 0) {
        r = int(c1.x);
    } else if (elem == 1) {
        r = int(c1.y);
    } else if (elem == 2) {
        r = int(c1.z);
    } else if (elem == 3) {
        r = int(c1.w);
    }
    return r;
}

// This function returns 4 bytes of image data packed
// in a uvec4 at the given byte address into the raw image
// buffer as created by the image reader
uvec4 get_image_data_4bytes_tex(int byte_address) {

    uvec4 result;

    int elem = byte_address&3;
    int newAddress = byte_address >> 2;
    ivec2 tex_coord = ivec2(newAddress%tex_dims.x, newAddress/tex_dims.x);
    uvec4 c1 = texture(the_tex, tex_coord);

    if (elem == 0) {
        result = c1;
    } else {
        uvec4 c2 = texture(the_tex, step_sample(tex_coord));
        if (elem == 1) {
            result = uvec4(c1.yzw,c2.x);
        } else if (elem == 2) {
            result = uvec4(c1.zw,c2.xy);
        } else if (elem == 3) {
            result = uvec4(c1.w,c2.xyz);
        }
    }
    return result;
}

// This function returns 4 bytes of image data packed
// in a uint
uint get_image_data_4bytes_packed_tex(int byte_address) {

    uvec4 bytes_4 = get_image_data_4bytes_tex(byte_address);
    uint c = bytes_4.x + (bytes_4.y << 8) + (bytes_4.z << 16) + (bytes_4.w << 24);
    return c;
}

// This function returns 2 floats of image data packed
// in a vec2 at the given byte address into the raw image
// buffer as created by the image reader
vec2 get_image_data_2floats_tex(int byte_address) {
    return unpackHalf2x16(get_image_data_4bytes_packed_tex(byte_address));
}

// This function returns 2 bytes of image data packed
// in an int at the given byte address into the raw image
// buffer as created by the image reader
int get_image_data_2bytes_tex(int byte_address) {

    int result;

    int elem = byte_address&3;
    int newAddress = byte_address >> 2;
    ivec2 tex_coord = ivec2(newAddress%tex_dims.x, newAddress/tex_dims.x);
    uvec4 c1 = texture(the_tex, tex_coord);

    if (elem == 0) {
        result = int(c1.x + (c1.y << 8));
    } else {
        if (elem == 1) {
            result = int(c1.y + (c1.z << 8));
        } else if (elem == 2) {
            result = int(c1.z + (c1.w << 8));
        } else if (elem == 3) {
            uvec4 c2 = texture(the_tex, step_sample(tex_coord));
            result = int(c1.w + (c2.x << 8));
        }
    }
    return result;
}

layout (std430, binding = 0) buffer ssboObject {
    uint data[];
} ssboData;

// This function returns 1 byte of image data packed
// in an int at the given byte address into the raw image
// buffer as created by the image reader
int get_image_data_1byte_ssbo(int byte_address) {

    int bitshift = (byte_address&3)*8;
    int newAddress = byte_address / 4;
    uint a = ssboData.data[newAddress];

    return int((a >> bitshift)&255);
}

// This function returns 2 floats of image data packed
// in a vec2 at the given byte address into the raw image
// buffer as created by the image reader
uint get_image_data_4bytes_packed_ssbo(int byte_address) {

    int bitshift = (byte_address&3)*8;
    int address = byte_address / 4;

    uint a = ssboData.data[address];
    uint c = (a >> bitshift);

    if (bitshift != 0) {
        c += (ssboData.data[address+1] << (32-bitshift));
    }

    return c;
}

// This function returns 2 floats of image data packed
// in a vec2 at the given byte address into the raw image
// buffer as created by the image reader
vec2 get_image_data_2floats_ssbo(int byte_address) {

    uint c = get_image_data_4bytes_packed_ssbo(byte_address);

    return unpackHalf2x16(c);
}

// This function returns 4 bytes of image data packed
// in a uvec4 at the given byte address into the raw image
// buffer as created by the image reader
uvec4 get_image_data_4bytes_ssbo(int byte_address) {

    uvec4 result;

    uint c = get_image_data_4bytes_packed_ssbo(byte_address);

    result.x = c & 255;
    result.y = (c >> 8) & 255;
    result.z = (c >> 16) & 255;
    result.w = (c >> 24) & 255;

    return result;
}

// This function returns 2 bytes of image data packed
// in an int at the given byte address into the raw image
// buffer as created by the image reader
int get_image_data_2bytes_ssbo(int byte_address) {

    int result;

    int bitshift = (byte_address&3)*8;
    int address = byte_address / 4;

    uint a = ssboData.data[address];
    result = int(a >> bitshift) & 65535;

    if (bitshift == 24) {
        result = result&255;
        result += int((ssboData.data[address+1]&255) << 8);
    }

    return result;
}

uvec4 get_image_data_4bytes(int byte_address) {

    return use_ssbo ? get_image_data_4bytes_ssbo(byte_address) : get_image_data_4bytes_tex(byte_address);

}

float get_image_data_float32(int byte_address) {

    return uintBitsToFloat(use_ssbo ? get_image_data_4bytes_packed_ssbo(byte_address) : get_image_data_4bytes_packed_tex(byte_address));

}

int get_image_data_1byte(int byte_address) {

    return use_ssbo ? get_image_data_1byte_ssbo(byte_address) : get_image_data_1byte_tex(byte_address);

}

vec2 get_image_data_2floats(int byte_address) {

    return unpackHalf2x16(use_ssbo ? get_image_data_4bytes_packed_ssbo(byte_address) : get_image_data_4bytes_packed_tex(byte_address));
}

int get_image_data_2bytes(int byte_address) {

    return use_ssbo ? get_image_data_2bytes_ssbo(byte_address) : get_image_data_2bytes_tex(byte_address);

}

// forward declared fetch function - this is provided
// by image reader plugin in a glsl snippet
vec4 fetch_rgba_pixel(ivec2 image_coord);

// forward declared colour transform function - this is provided
// by colour pipeline classes
vec4 colour_transforms(vec4 rgba);

#define getTexel(p) fetch_rgba_pixel(ivec2(p))

// Returns a rgba pixel at "pos" index, computed with a bilinear filter
vec4 get_bilinear_filtered_pixel(vec2 pos)
{
    vec4 texL = getTexel(pos);
    vec4 texR = getTexel(pos + vec2(1.0, 0.0));
    vec4 biL = getTexel(pos + vec2(0.0, 1.0));
    vec4 biR = getTexel(pos + vec2(1.0, 1.0));
    vec2 f = fract(pos);
    vec4 texA = mix(texL, texR, f.x);
    vec4 texB = mix(biL, biR, f.x);
    return mix(texA, texB, f.y);
}

vec4 cubic(float v)
{
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w);
}

vec4 get_bicubic_filter(vec2 pos)
{
    float fx = fract(pos.x);
    float fy = fract(pos.y);
    pos.x -= fx;
    pos.y -= fy;

    vec4 xcubic = cubic(fx);
    vec4 ycubic = cubic(fy);

    vec4 c = vec4(pos.x - 0.5, pos.x + 1.5, pos.y -
        0.5, pos.y + 1.5);
    vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x +
        ycubic.y, ycubic.z + ycubic.w);
    vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

    vec4 sample0 = getTexel(vec2(offset.x, offset.z));
    vec4 sample1 = getTexel(vec2(offset.y, offset.z));
    vec4 sample2 = getTexel(vec2(offset.x, offset.w));
    vec4 sample3 = getTexel(vec2(offset.y, offset.w));

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(
        mix(sample3, sample2, sx),
        mix(sample1, sample0, sx), sy);
}

void main(void)
{
    if (texPosition.x < image_bounds_min.x || texPosition.x > image_bounds_max.x) FragColor = vec4(0.0,0.0,0.0,1.0);
    else if (texPosition.y < image_bounds_min.y || texPosition.y > image_bounds_max.y) FragColor = vec4(0.0,0.0,0.0,1.0);
    else {

        // For now, disabling bilinear filtering as it is too expensive and slowing refresh badly
        // for high res formats when the colour transforms are also expensive
        if(use_bilinear_filtering) {
            FragColor.rgb = colour_transforms(get_bilinear_filtered_pixel(texPosition - 0.5)).rgb;
        }
        else{
            FragColor.rgb = colour_transforms(
                fetch_rgba_pixel(
                    ivec2(texPosition.x, texPosition.y))
                ).rgb;
        }
        FragColor.a = 1.0;
    }
}
)";

GLuint compile_frag_shader(const std::string fragmentSource) {

    // Create an empty fragment shader handle
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    // Send the fragment shader source code to GL
    // Note that std::string's .c_str is NULL character terminated.
    auto source = (const GLchar *)fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &source, nullptr);

    // Compile the fragment shader
    glCompileShader(fragmentShader);

    GLint isCompiled = 0;
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);

        // We don't need the shader anymore.
        glDeleteShader(fragmentShader);

        // Use the infoLog as you see fit.
        std::stringstream e;
        e << "Fragment shader error:\n\n"
          << infoLog.data() << "\n\nin program: \n\n"
          << fragmentSource;
        throw std::runtime_error(e.str().c_str());
    }
    return fragmentShader;
}

GLuint compile_vertex_shader(const std::string vertexSource) {

    // Create an empty vertex shader handle
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

    // Send the vertex shader source code to GL
    // Note that std::string's .c_str is NULL character terminated.
    auto source = (const GLchar *)vertexSource.c_str();
    glShaderSource(vertexShader, 1, &source, nullptr);

    // Compile the vertex shader
    glCompileShader(vertexShader);

    GLint isCompiled = 0;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);

        // We don't need the shader anymore.
        glDeleteShader(vertexShader);

        // Use the infoLog as you see fit.
        std::stringstream e;
        e << "Vertex shader error:\n\n"
          << infoLog.data() << "\n\nin program: \n\n"
          << vertexSource;
        throw std::runtime_error(e.str().c_str());

        // In this simple program, we'll just leave
        return false;
    }

    return vertexShader;
}

} // namespace

GLShaderProgram::GLShaderProgram(
    const std::string &vtx_shader,
    const std::string &colour_transform_shader,
    const std::string &frg_shader,
    bool do_compile) {
    vertex_shaders_.push_back(vtx_shader);
    vertex_shaders_.emplace_back(vertex_shader_base);
    fragment_shaders_.push_back(frg_shader);
    fragment_shaders_.push_back(colour_transform_shader);
    fragment_shaders_.emplace_back(frag_shader_base);

    try {
        if (do_compile)
            compile();

    } catch (std::exception &e) {
        spdlog::error("shader error: {}", e.what());
    }
}

GLShaderProgram::GLShaderProgram(
    const std::string &vertex_shader,
    const std::string &fragment_shader,
    const bool do_compile) {
    vertex_shaders_.push_back(vertex_shader);
    fragment_shaders_.push_back(fragment_shader);
    try {
        if (do_compile)
            compile();

    } catch (std::exception &e) {
        spdlog::error("shader error: {}", e.what());
    }
}

void GLShaderProgram::compile() {

    // Get a program object.
    program_ = glCreateProgram();

    std::vector<GLuint> shaders;

    try {

        // compile the vertex shader objects
        std::for_each(
            vertex_shaders_.begin(),
            vertex_shaders_.end(),
            [&shaders](const std::string &shader_code) {
                shaders.push_back(compile_vertex_shader(shader_code));
            });

        // compile the fragment shader objects
        std::for_each(
            fragment_shaders_.begin(),
            fragment_shaders_.end(),
            [&shaders](const std::string &shader_code) {
                shaders.push_back(compile_frag_shader(shader_code));
            });

    } catch (...) {

        // a shader hasn't compiled ... delete anthing that did compile
        std::for_each(
            shaders.begin(), shaders.end(), [](GLuint shdr) { glDeleteShader(shdr); });
        throw;
    }

    // attach the shaders to the program
    std::for_each(shaders.begin(), shaders.end(), [&](GLuint shader_id) {
        glAttachShader(program_, shader_id);
    });

    // Link our program
    glLinkProgram(program_);

    // Note the different functions here: glGetProgram* instead of glGetShader*.
    GLint isLinked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program_, maxLength, &maxLength, &infoLog[0]);

        // We don't need the program anymore.
        glDeleteProgram(program_);

        // Always detach shaders after a successful link.
        std::for_each(
            shaders.begin(), shaders.end(), [](GLuint shdr) { glDeleteShader(shdr); });

        // Use the infoLog as you see fit.
        std::stringstream e;
        e << "Shader link error:\n\n" << infoLog.data();
        throw std::runtime_error(e.str().c_str());
    }

    // Always detach shaders after a successful link.
    std::for_each(
        shaders.begin(), shaders.end(), [&](GLuint shdr) { glDetachShader(program_, shdr); });
}

void GLShaderProgram::use() const { glUseProgram(program_); }

void GLShaderProgram::stop_using() const { glUseProgram(0); }

int GLShaderProgram::get_param_location(const std::string &param_name) {
    const auto p = locations_.find(param_name);
    if (p != locations_.end())
        return p->second;
    const int loc          = glGetUniformLocation(program_, param_name.c_str());
    locations_[param_name] = loc;
    return loc;
}

void GLShaderProgram::set_shader_parameters(
    const media_reader::ImageBufPtr &image, int is_main_viewer) {

    if (image) {
        use();
        int location = get_param_location("image_dims");
        glUniform2i(location, image->image_size_in_pixels().x, image->image_size_in_pixels().y);
        location = get_param_location("image_bounds_max");
        glUniform2i(
            location,
            image->image_pixels_bounding_box().max.x,
            image->image_pixels_bounding_box().max.y);
        location = get_param_location("image_bounds_min");
        glUniform2i(
            location,
            image->image_pixels_bounding_box().min.x,
            image->image_pixels_bounding_box().min.y);
        set_shader_parameters(image->shader_params(), is_main_viewer);
    }
}

void GLShaderProgram::set_shader_parameters(
    const utility::JsonStore &shader_params, const int is_main_viewer) {

    use();
    auto params = shader_params;

    /*if (is_main_viewer != -1) {
        int location = get_param_location("main_viewer");
        if (location != -1) glUniform1i(location, (bool)is_main_viewer);
    }*/

    for (nlohmann::json::const_iterator it = params.begin(); it != params.end(); ++it) {

        const std::string param_name = it.key();
        int location                 = get_param_location(param_name);

        if (location == -1) {

            spdlog::debug(
                "GLShaderProgram::set_shader_parameter: Request for shader uniform attr \"{}\" "
                "failed, no such parameter.",
                it.key());

        } else if (it->is_boolean()) {

            glUniform1i(location, it.value().get<bool>());

        } else if (it->is_number_integer()) {

            glUniform1i(location, it.value().get<int>());

        } else if (it->is_number_unsigned()) {

            glUniform1ui(location, it.value().get<unsigned int>());

        } else if (it->is_number_float()) {

            glUniform1f(location, it.value().get<float>());

        } else if (it->is_array()) {

            auto vs                      = it.value();
            auto cs                      = vs.begin();
            const auto shader_param_type = cs.value().get<std::string>();
            cs++;
            const int count = cs.value().get<int>();

            if (shader_param_type == "float") {
                set_gl_uniform<float>(glUniform1fv, location, vs, count, 1, param_name);
            } else if (shader_param_type == "vec2") {
                set_gl_uniform<float>(glUniform2fv, location, vs, count, 2, param_name);
            } else if (shader_param_type == "colour") {
                set_gl_uniform<float>(glUniform3fv, location, vs, count, 3, param_name);
            } else if (shader_param_type == "vec3") {
                set_gl_uniform<float>(glUniform3fv, location, vs, count, 3, param_name);
            } else if (shader_param_type == "vec4") {
                set_gl_uniform<float>(glUniform4fv, location, vs, count, 4, param_name);
            } else if (shader_param_type == "int" || shader_param_type == "bool") {
                set_gl_uniform<int>(glUniform1iv, location, vs, count, 1, param_name);
            } else if (shader_param_type == "ivec2") {
                set_gl_uniform<int>(glUniform2iv, location, vs, count, 2, param_name);
            } else if (shader_param_type == "ivec3") {
                set_gl_uniform<int>(glUniform3iv, location, vs, count, 3, param_name);
            } else if (shader_param_type == "ivec4") {
                set_gl_uniform<int>(glUniform4iv, location, vs, count, 4, param_name);
            } else if (shader_param_type == "uint") {
                set_gl_uniform<unsigned int>(glUniform1uiv, location, vs, count, 1, param_name);
            } else if (shader_param_type == "uvec2") {
                set_gl_uniform<unsigned int>(glUniform2uiv, location, vs, count, 2, param_name);
            } else if (shader_param_type == "uvec3") {
                set_gl_uniform<unsigned int>(glUniform3uiv, location, vs, count, 3, param_name);
            } else if (shader_param_type == "uvec4") {
                set_gl_uniform<unsigned int>(glUniform4uiv, location, vs, count, 4, param_name);
            } else if (shader_param_type == "mat2") {
                set_gl_uniform_matrix(glUniformMatrix2fv, location, vs, count, 4, param_name);
            } else if (shader_param_type == "mat3") {
                set_gl_uniform_matrix(glUniformMatrix3fv, location, vs, count, 9, param_name);
            } else if (shader_param_type == "mat4") {
                set_gl_uniform_matrix(glUniformMatrix4fv, location, vs, count, 16, param_name);
            }
        }
    }
}