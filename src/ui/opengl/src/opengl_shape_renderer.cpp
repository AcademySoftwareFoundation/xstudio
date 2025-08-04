// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_shape_renderer.hpp"
#include "xstudio/ui/canvas/shapes.hpp"

#include <half.h>

using namespace xstudio::ui::canvas;
using namespace xstudio::ui::opengl;

#ifdef __OPENGL_4_1__

// To support GL 4.1 (for MacOS) we can't use SSBOs. Therefore the shape data
// is packed into a 1D texture. Keeping the SSBO implementation as it might
// have performance/clarity advantages

#define SHAPE_DATA_TEX_SIZE 4096

namespace {

const char *vertex_shader = R"(
    #version 410 core

    layout (location = 0) in vec2 vtx;
    out vec2 coords;

    uniform float scale_ar;

    void main() {
        gl_Position = vec4(vtx, 0.0, 1.0);
        coords = vec2(gl_Position.x, -gl_Position.y * scale_ar);
    }
)";

const char *frag_shader = R"(
    #version 410

    // We plot all the shapes in the fragment shader in one hit, using a 
    // sign-distance-function approach so for each fragment we iterate over 
    // all shapes and compute the distance to the shape boundary.
    // In order to use this approach we pack all the data about each shape (
    // including the polygon points for polygons) into texture data and unpack 
    // here in the shader routine
    uniform sampler1D shapesData;

    uniform int shape_count;

    in vec2 coords;
    out vec4 color;

    int getShapeDataIntVal(int idx) {    
        return int(texelFetch(shapesData, idx, 0).r);
    }

    float getShapeDataFloatVal(int idx) {    
        return texelFetch(shapesData, idx, 0).r;
    }

    vec4 getShapeDataVec4(int idx) {
        return vec4(
            texelFetch(shapesData, idx, 0).r,
            texelFetch(shapesData, idx+1, 0).r,
            texelFetch(shapesData, idx+2, 0).r,
            texelFetch(shapesData, idx+3, 0).r
            );
    }

    vec2 getShapeDataVec2(int idx) {
        return vec2(
            texelFetch(shapesData, idx, 0).r,
            texelFetch(shapesData, idx+1, 0).r
            );
    }


    vec2 opRotate(vec2 coord, float degrees)
    {
        float rad = radians(degrees);

        mat2 rotation_mat = mat2(
            cos(rad),-sin(rad),
            sin(rad), cos(rad)
        );

        return coord * rotation_mat;
    }

    /////////////////////////////////////////
    // License for sdQuad() sdPolygon()
    /////////////////////////////////////////

    // The MIT License
    // Copyright © 2021 Inigo Quilez
    // Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    // https://www.shadertoy.com/view/7dSGWK
    float sdQuad(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3)
    {
        vec2 e0 = p1 - p0; vec2 v0 = p - p0;
        vec2 e1 = p2 - p1; vec2 v1 = p - p1;
        vec2 e2 = p3 - p2; vec2 v2 = p - p2;
        vec2 e3 = p0 - p3; vec2 v3 = p - p3;

        vec2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
        vec2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
        vec2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
        vec2 pq3 = v3 - e3*clamp( dot(v3,e3)/dot(e3,e3), 0.0, 1.0 );
        
        vec2 ds = min( min( vec2( dot( pq0, pq0 ), v0.x*e0.y-v0.y*e0.x ),
                            vec2( dot( pq1, pq1 ), v1.x*e1.y-v1.y*e1.x )),
                    min( vec2( dot( pq2, pq2 ), v2.x*e2.y-v2.y*e2.x ),
                            vec2( dot( pq3, pq3 ), v3.x*e3.y-v3.y*e3.x ) ));

        float d = sqrt(ds.x);

        return (ds.y>0.0) ? -d : d;
    }

    // https://iquilezles.org/articles/distfunctions2d/
    float sdPolygon(vec2 p, int idx, int point_count)
    {
        vec2 v0 = getShapeDataVec2(idx);
        float d = dot(p-v0,p-v0);
        float s = 1.0;
        for( int i=0, j=point_count-1; i<point_count; j=i, i++ )
        {
            vec2 vi = getShapeDataVec2(idx+i*2);
            vec2 vj = getShapeDataVec2(idx+j*2);

            vec2 e = vj - vi;
            vec2 w =  p - vi;
            vec2 b = w - e*clamp( dot(w,e)/dot(e,e), 0.0, 1.0 );
            d = min( d, dot(b,b) );
            bvec3 c = bvec3(p.y>=vi.y,p.y<vj.y,e.x*w.y>e.y*w.x);
            if( all(c) || all(not(c)) ) s*=-1.0;
        }
        return s*sqrt(d);
    }

    // https://blog.chatfield.io/simple-method-for-distance-to-ellipse/ trig-less version
    // https://github.com/0xfaded/ellipse_demo/issues/1
    // https://www.shadertoy.com/view/tt3yz7
    float sdEllipse4(vec2 p, vec2 e)
    {
        vec2 pAbs = abs(p);
        vec2 ei = 1.0 / e;
        vec2 e2 = e*e;
        vec2 ve = ei * vec2(e2.x - e2.y, e2.y - e2.x);

        // cos/sin(math.pi / 4)
        vec2 t = vec2(0.70710678118654752, 0.70710678118654752);

        for (int i = 0; i < 3; i++) {
            vec2 v = ve*t*t*t;
            vec2 u = normalize(pAbs - v) * length(t * e - v);
            vec2 w = ei * (v + u);
            t = normalize(clamp(w, 0.0, 1.0));
        }

        vec2 nearestAbs = t * e;
        float dist = length(pAbs - nearestAbs);
        return dot(pAbs, pAbs) < dot(nearestAbs, nearestAbs) ? -dist : dist;
    }

    vec4 evaluate_polygon(int idx, vec4 in_colour) {

        vec4 color = getShapeDataVec4(idx);
        idx += 4;
        float softness = getShapeDataFloatVal(idx);
        idx += 1;
        float opacity = getShapeDataFloatVal(idx);
        idx += 1;
        float invert = getShapeDataFloatVal(idx);
        idx += 1;
        int pcount = getShapeDataIntVal(idx);
        idx += 1;
        float d = sdPolygon(coords, idx, pcount) * invert;
        d = d + softness * (invert - 1) * -0.5;
        return max(in_colour, mix(color*opacity, vec4(0.0f), smoothstep(0.0f, softness, d)));
    }

    vec4 evaluate_ellipse(int idx, vec4 in_colour) {

        vec4 color = getShapeDataVec4(idx);
        idx += 4;
        float softness = getShapeDataFloatVal(idx);
        idx += 1;
        float opacity = getShapeDataFloatVal(idx);
        idx += 1;
        float invert = getShapeDataFloatVal(idx);
        idx += 1;
        float angle = getShapeDataFloatVal(idx);
        idx += 1;
        vec2 center = getShapeDataVec2(idx);
        idx += 2;
        vec2 radius = getShapeDataVec2(idx);

        float d = sdEllipse4(opRotate(coords - center, angle), radius) * invert;
        d = d + softness * (invert - 1) * -0.5;
        return max(in_colour, mix(color*opacity, vec4(0.0f), smoothstep(0.0f, softness, d)));

    }

    vec4 evaluate_quad(int idx, vec4 in_colour) {
            
        vec4 color = getShapeDataVec4(idx);
        idx += 4;
        float softness = getShapeDataFloatVal(idx);
        idx += 1;
        float opacity = getShapeDataFloatVal(idx);
        idx += 1;
        float invert = getShapeDataFloatVal(idx);
        idx += 1;
        vec2 bl = getShapeDataVec2(idx);
        idx += 2;
        vec2 tl = getShapeDataVec2(idx);
        idx += 2;
        vec2 tr = getShapeDataVec2(idx);
        idx += 2;
        vec2 br = getShapeDataVec2(idx);
        idx += 2;
        
        float d = sdQuad(coords, bl, tl, tr, br) * invert;

        // When shape is inverted, add softness to the current distance
        // so that softness expands the original shape outward.
        // (q.invert - 1) * -0.5 is 1 if inverted, 0 otherwise
        d = d + softness * (invert - 1) * -0.5;
        return max(in_colour, mix(color*opacity, vec4(0.0f), smoothstep(0.0f, softness, d)));

    }

    void main()
    {
        vec4 accum_color = vec4(0.0f);

        int idx = 0;
        for (int i = 0; i < shape_count; ++i) {
            int shape_type = getShapeDataIntVal(idx);
            idx++;
            int shape_size = getShapeDataIntVal(idx);
            idx++;
            if (shape_type == 0) {
                //accum_color = evaluate_quad(idx, accum_color);
            } else if (shape_type == 1) {
                accum_color = evaluate_ellipse(idx, accum_color);
            } else if (shape_type == 2) {
                accum_color = evaluate_polygon(idx, accum_color);
            }
            idx += shape_size;

        }
        color = accum_color;

    }
)";

} // namespace

OpenGLShapeRenderer::~OpenGLShapeRenderer() { cleanup_gl(); }

void OpenGLShapeRenderer::init_gl() {

    if (!shader_) {

        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);

        std::array<Imath::V2f, 6> bg_verts = {
            Imath::V2f(-1.0f, -1.0f),
            Imath::V2f(1.0f, -1.0f),
            Imath::V2f(1.0f, 1.0f),
            Imath::V2f(1.0f, 1.0f),
            Imath::V2f(-1.0f, 1.0f),
            Imath::V2f(-1.0f, -1.0f)};

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(bg_verts), bg_verts.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Create a 1D 32 bit float texture to stuff lists of shape descriptions
        // into for use by our fragment shader
        glGenTextures(1, &tex_id_);
        glBindTexture(GL_TEXTURE_1D, tex_id_);
        glEnable(GL_TEXTURE_1D);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

        glTexImage1D(
            GL_TEXTURE_1D, 0, GL_R32F, SHAPE_DATA_TEX_SIZE, 0, GL_RED, GL_FLOAT, nullptr);

        glBindTexture(GL_TEXTURE_1D, 0);
    }
}

void OpenGLShapeRenderer::cleanup_gl() {
    if (shader_) {
        glDeleteTextures(1, &tex_id_);
        shader_.reset();
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
    }
}

void OpenGLShapeRenderer::upload_shape_data_to_tex(const std::vector<float> &data_buf) {

    int row_length;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_1D, tex_id_);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, SHAPE_DATA_TEX_SIZE);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

    glTexSubImage1D(
        GL_TEXTURE_1D, 0, 0, SHAPE_DATA_TEX_SIZE, GL_RED, GL_FLOAT, data_buf.data());

    // need to restore row length - stuff deep in QML expects this to be set
    // to a particular value
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_1D, tex_id_);
}

void OpenGLShapeRenderer::render_shapes(
    const std::vector<Quad> &quads,
    const std::vector<canvas::Polygon> &polygons,
    const std::vector<canvas::Ellipse> &ellipses,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float image_aspectratio) {

    if (!shader_)
        init_gl();

    utility::JsonStore shader_params;

    shader_params["scale_ar"]    = 1.0f / image_aspectratio;
    shader_params["shape_count"] = quads.size() + ellipses.size() + polygons.size();
    shader_params["shapesData"]  = 10;

    // Transform every shape coordinates back into a square aspect ratio to ease
    // shader computations and produce soft blur that behaves isotropically.
    const float inv_canvas_ar = 1.0f / transform_window_to_viewport_space[1][1];

    // here we pack the data about the shapes into a buffer - it gets unpacked
    // in the gl fragment shader. We use 32 bit words, generally putting two
    // 16 bit floats into each, but when we need to store an int (e.g. the
    // number of polygon points) we can use all 32 bits
    std::vector<float> shapes_data;
    shapes_data.resize(SHAPE_DATA_TEX_SIZE);
    float *data_buf = shapes_data.data();
    int ct          = 0;

    // note - we can pack up to SHAPE_DATA_TEX_SIZE floats for the shape data. It is
    // extremely unlikely we will ever exceed this. If we do the shapes that
    // would overrun the limit will just not get drawn.

    for (const auto &quad : quads) {

        ct += 17;
        if (ct > SHAPE_DATA_TEX_SIZE)
            break;

        (*data_buf++) = 0;  // shape type
        (*data_buf++) = 15; // shape data size

        (*data_buf++) = quad.colour.r;
        (*data_buf++) = quad.colour.g;
        (*data_buf++) = quad.colour.b;
        (*data_buf++) = 1.0f;
        (*data_buf++) = quad.softness / 500.0f;
        (*data_buf++) = quad.opacity / 100.0f;
        (*data_buf++) = quad.invert ? -1.f : 1.f;
        (*data_buf++) = quad.tl.x;
        (*data_buf++) = quad.tl.y;
        (*data_buf++) = quad.tr.x;
        (*data_buf++) = quad.tr.y;
        (*data_buf++) = quad.br.x;
        (*data_buf++) = quad.br.y;
        (*data_buf++) = quad.bl.x;
        (*data_buf++) = quad.bl.y;
    }

    for (const auto &ellipse : ellipses) {

        ct += 14;
        if (ct > SHAPE_DATA_TEX_SIZE)
            break;

        (*data_buf++) = 1.0;  // shape type
        (*data_buf++) = 12.0; // shape data size
        (*data_buf++) = ellipse.colour.r;
        (*data_buf++) = ellipse.colour.g;
        (*data_buf++) = ellipse.colour.b;
        (*data_buf++) = 1.0f;
        (*data_buf++) = ellipse.softness / 500.0f;
        (*data_buf++) = ellipse.opacity / 100.0f;
        (*data_buf++) = ellipse.invert ? -1.f : 1.f;
        (*data_buf++) = ellipse.angle;
        (*data_buf++) = ellipse.center.x;
        (*data_buf++) = ellipse.center.y;
        (*data_buf++) = ellipse.radius.x;
        (*data_buf++) = ellipse.radius.y;
    }

    for (const auto &polygon : polygons) {

        ct += 10 + polygon.points.size() * 2;
        if (ct > SHAPE_DATA_TEX_SIZE)
            break;

        (*data_buf++) = 2.0;                             // shape type
        (*data_buf++) = 8.0 + polygon.points.size() * 2; // shape data size

        (*data_buf++) = polygon.colour.r;
        (*data_buf++) = polygon.colour.g;
        (*data_buf++) = polygon.colour.b;
        (*data_buf++) = 1.0f;
        (*data_buf++) = polygon.softness / 500.0f;
        (*data_buf++) = polygon.opacity / 100.0f;
        (*data_buf++) = polygon.invert ? -1.f : 1.f;
        (*data_buf++) = (float)polygon.points.size();

        memcpy(data_buf, polygon.points.data(), sizeof(float) * 2 * polygon.points.size());

        data_buf += 2 * polygon.points.size();
    }

    upload_shape_data_to_tex(shapes_data);

    shader_->use();
    shader_->set_shader_parameters(shader_params);


    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glBindVertexArray(vao_);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glDisableVertexAttribArray(vao_);
    glBindTexture(GL_TEXTURE_1D, 0);

    shader_->stop_using();
}

#else // #ifdef __OPENGL_4_1__


namespace {

const char *vertex_shader = R"(
    #version 430 core

    in vec4 vertices;
    out vec2 coords;

    uniform float scale_ar;

    const vec2 positions[4] = {
        vec2(-1, -1),
        vec2( 1, -1),
        vec2(-1,  1),
        vec2( 1,  1),
    };

    void main() {
        gl_Position = vec4(positions[gl_VertexID].xy, 0.0, 1.0);
        coords = vec2(gl_Position.x, -gl_Position.y * scale_ar);
    }
)";

const char *frag_shader = R"(
    #version 430
    #extension GL_ARB_shader_storage_buffer_object : require

    // We use SSBO to avoid thinking about size limits,
    // in theory UBO would be much faster and might be used if
    // we determine we won't need more than the allowed size.
    // For our currrent usage here, we do not compute masked
    // shape in real time, only when the canvas gets updated
    // by the user, so no performance review was done in that
    // sense.

    // Note that std430 is used here for easier match to C++
    // memory layout, note the following points:
    //   1. std430 doesn't pack vec3 so we use vec4 instead
    //      to avoid dealing with padding in the C++ side
    //      (eg. for color fields).
    //   2. vec4 are aligned on 16 bytes, so we place them
    //      at the begining of the structs to avoid having
    //      to deal with alignment on the C++ side.
    // See the OpenGL specs for more details on std430

    struct Quad {
        vec4 color;
        vec2 tl;
        vec2 tr;
        vec2 br;
        vec2 bl;
        float opacity;
        float softness;
        float invert;
    };
    layout (std430, binding = 1) buffer QuadsBuffer {
        Quad items[];
    } quads;

    struct Ellipse {
        vec4 color;
        vec2 center;
        vec2 radius;
        float angle;
        float opacity;
        float softness;
        float invert;
    };
    layout (std430, binding = 2) buffer EllipsesBuffer {
        Ellipse items[];
    } ellipses;

    struct Polygon {
        vec4 color;
        uint offset;
        uint count;
        float opacity;
        float softness;
        float invert;
    };
    layout (std430, binding = 3) buffer PolygonsBuffer {
        Polygon items[];
    } polygons;

    layout (std430, binding = 4) buffer PointsBuffer {
        vec2 items[];
    } points;

    // Note that we could in theory use the .length() function
    // on the variable sized arrays, however it seem to have
    // issues working reliably when multiple SSBO are present.
    // So we use explicit size forr the time being.
    uniform int quads_count;
    uniform int ellipses_count;
    uniform int polygons_count;

    in vec2 coords;
    out vec4 color;

    /////////////////////////////////////////
    // License for sdQuad() sdPolygon()
    /////////////////////////////////////////

    // The MIT License
    // Copyright © 2021 Inigo Quilez
    // Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    // https://www.shadertoy.com/view/7dSGWK
    float sdQuad(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3)
    {
        vec2 e0 = p1 - p0; vec2 v0 = p - p0;
        vec2 e1 = p2 - p1; vec2 v1 = p - p1;
        vec2 e2 = p3 - p2; vec2 v2 = p - p2;
        vec2 e3 = p0 - p3; vec2 v3 = p - p3;

        vec2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
        vec2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
        vec2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
        vec2 pq3 = v3 - e3*clamp( dot(v3,e3)/dot(e3,e3), 0.0, 1.0 );
        
        vec2 ds = min( min( vec2( dot( pq0, pq0 ), v0.x*e0.y-v0.y*e0.x ),
                            vec2( dot( pq1, pq1 ), v1.x*e1.y-v1.y*e1.x )),
                    min( vec2( dot( pq2, pq2 ), v2.x*e2.y-v2.y*e2.x ),
                            vec2( dot( pq3, pq3 ), v3.x*e3.y-v3.y*e3.x ) ));

        float d = sqrt(ds.x);

        return (ds.y>0.0) ? -d : d;
    }

    // https://iquilezles.org/articles/distfunctions2d/
    float sdPolygon(vec2 p, uint offset, uint count)
    {
        vec2 v0 = points.items[offset];
        float d = dot(p-v0,p-v0);
        float s = 1.0;
        for( uint i=0, j=count-1; i<count; j=i, i++ )
        {
            vec2 vi = points.items[offset+i];
            vec2 vj = points.items[offset+j];

            vec2 e = vj - vi;
            vec2 w =  p - vi;
            vec2 b = w - e*clamp( dot(w,e)/dot(e,e), 0.0, 1.0 );
            d = min( d, dot(b,b) );
            bvec3 c = bvec3(p.y>=vi.y,p.y<vj.y,e.x*w.y>e.y*w.x);
            if( all(c) || all(not(c)) ) s*=-1.0;
        }
        return s*sqrt(d);
    }

    // https://blog.chatfield.io/simple-method-for-distance-to-ellipse/ trig-less version
    // https://github.com/0xfaded/ellipse_demo/issues/1
    // https://www.shadertoy.com/view/tt3yz7
    float sdEllipse4(vec2 p, vec2 e)
    {
        vec2 pAbs = abs(p);
        vec2 ei = 1.0 / e;
        vec2 e2 = e*e;
        vec2 ve = ei * vec2(e2.x - e2.y, e2.y - e2.x);

        // cos/sin(math.pi / 4)
        vec2 t = vec2(0.70710678118654752, 0.70710678118654752);

        for (int i = 0; i < 3; i++) {
            vec2 v = ve*t*t*t;
            vec2 u = normalize(pAbs - v) * length(t * e - v);
            vec2 w = ei * (v + u);
            t = normalize(clamp(w, 0.0, 1.0));
        }

        vec2 nearestAbs = t * e;
        float dist = length(pAbs - nearestAbs);
        return dot(pAbs, pAbs) < dot(nearestAbs, nearestAbs) ? -dist : dist;
    }

    float opRound(float shape, float r)
    {
        return shape - r;
    }

    vec2 opRotate(vec2 coord, float degrees)
    {
        float rad = radians(degrees);

        mat2 rotation_mat = mat2(
            cos(rad),-sin(rad),
            sin(rad), cos(rad)
        );

        return coord * rotation_mat;
    }

    void main()
    {
        vec4 accum_color = vec4(0.0f);

        for (int i = 0; i < quads_count; ++i) {
            Quad q = quads.items[i];
            // Shape boundary is at distance 0, inside is < 0, outside is > 0
            // To invert the shape, we can simply multiply by -1.
            float d = sdQuad(coords, q.bl, q.tl, q.tr, q.br) * q.invert;
            // When shape is inverted, add softness to the current distance
            // so that softness expands the original shape outward.
            // (q.invert - 1) * -0.5 is 1 if inverted, 0 otherwise
            d = d + q.softness * (q.invert - 1) * -0.5;
            vec4 color = mix(vec4(0.0f), q.color, q.opacity);
            accum_color = max(accum_color, mix(color, vec4(0.0f), smoothstep(0.0f, q.softness, d)));
        }

        for (int i = 0; i < ellipses_count; ++i) {
            Ellipse e = ellipses.items[i];
            float d = sdEllipse4(opRotate(coords - e.center, e.angle), e.radius) * e.invert;
            d = d + e.softness * (e.invert - 1) * -0.5;
            vec4 color = mix(vec4(0.0f), e.color, e.opacity);
            accum_color = max(accum_color, mix(color, vec4(0.0f), smoothstep(0.0f, e.softness, d)));
        }

        for (int i = 0; i < polygons_count; ++i) {
            Polygon p = polygons.items[i];
            float d = sdPolygon(coords, p.offset, p.count) * p.invert;
            d = d + p.softness * (p.invert - 1) * -0.5;
            vec4 color = mix(vec4(0.0f), p.color, p.opacity);
            accum_color = max(accum_color, mix(color, vec4(0.0f), smoothstep(0.0f, p.softness, d)));
        }

        color = accum_color;
    }
)";

// These struct must match the GLSL buffer layout, see shader for more details.
// We avoid the need to manually specify each member alignment (with alignas())
// by using the adjustments mentioned in the shader.
// However, we need to align the whole structures to vec4 (16 bytes), quote from
// the spec (https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf#page=160):
//
//   If the member is a structure, the base alignment of the structure is N, where
//   N is the largest base alignment value of any of its members, and rounded
//   up to the base alignment of a vec4.

struct alignas(16) GLQuad {
    float colour[4];
    float tl[2];
    float tr[2];
    float br[2];
    float bl[2];
    float opacity;
    float softness;
    float invert;
};

struct alignas(16) GLEllipse {
    float colour[4];
    float center[2];
    float radius[2];
    float angle;
    float opacity;
    float softness;
    float invert;
};

struct alignas(16) GLPolygon {
    float colour[4];
    uint32_t offset;
    uint32_t count;
    float opacity;
    float softness;
    float invert;
};

struct GLPoint {
    float pt[2];
};


// Use to debug memory layout issues
void dump_mem_layout(GLuint prog_obj) {
    // C++

    std::cout << "sizeof GLQuad = " << sizeof(GLQuad) << "\n";
    std::cout << "offset of GLQuad.colour = " << offsetof(GLQuad, colour) << '\n'
              << "offset of GLQuad.tl = " << offsetof(GLQuad, tl) << '\n'
              << "offset of GLQuad.tr = " << offsetof(GLQuad, tr) << '\n'
              << "offset of GLQuad.br = " << offsetof(GLQuad, br) << '\n'
              << "offset of GLQuad.bl = " << offsetof(GLQuad, bl) << '\n'
              << "offset of GLQuad.opacity = " << offsetof(GLQuad, opacity) << '\n'
              << "offset of GLQuad.softness = " << offsetof(GLQuad, softness) << '\n'
              << "offset of GLQuad.invert = " << offsetof(GLQuad, invert) << '\n';

    std::cout << "sizeof GLEllipse = " << sizeof(GLEllipse) << "\n";
    std::cout << "offset of GLEllipse.colour = " << offsetof(GLEllipse, colour) << '\n'
              << "offset of GLEllipse.center = " << offsetof(GLEllipse, center) << '\n'
              << "offset of GLEllipse.radius = " << offsetof(GLEllipse, radius) << '\n'
              << "offset of GLEllipse.angle = " << offsetof(GLEllipse, angle) << '\n'
              << "offset of GLEllipse.opacity = " << offsetof(GLEllipse, opacity) << '\n'
              << "offset of GLEllipse.softness = " << offsetof(GLEllipse, softness) << '\n'
              << "offset of GLEllipse.invert = " << offsetof(GLEllipse, invert) << '\n';

    std::cout << "sizeof GLPolygon = " << sizeof(GLPolygon) << "\n";
    std::cout << "offset of GLPolygon.colour = " << offsetof(GLPolygon, colour) << '\n'
              << "offset of GLPolygon.offset = " << offsetof(GLPolygon, offset) << '\n'
              << "offset of GLPolygon.count = " << offsetof(GLPolygon, count) << '\n'
              << "offset of GLPolygon.opacity = " << offsetof(GLPolygon, opacity) << '\n'
              << "offset of GLPolygon.softness = " << offsetof(GLPolygon, softness) << '\n'
              << "offset of GLPolygon.invert = " << offsetof(GLPolygon, invert) << '\n';

    // GLSL
    // https://stackoverflow.com/a/56513136

    GLint no_of, ssbo_max_len, var_max_len;
    glGetProgramInterfaceiv(prog_obj, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &no_of);
    glGetProgramInterfaceiv(
        prog_obj, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &ssbo_max_len);
    glGetProgramInterfaceiv(prog_obj, GL_BUFFER_VARIABLE, GL_MAX_NAME_LENGTH, &var_max_len);

    std::vector<GLchar> name(var_max_len);
    for (int i_resource = 0; i_resource < no_of; i_resource++) {

        // get name of the shader storage block
        GLsizei strLength;
        glGetProgramResourceName(
            prog_obj,
            GL_SHADER_STORAGE_BLOCK,
            i_resource,
            ssbo_max_len,
            &strLength,
            name.data());

        // get resource index of the shader storage block
        GLint resInx =
            glGetProgramResourceIndex(prog_obj, GL_SHADER_STORAGE_BLOCK, name.data());

        // get number of the buffer variables in the shader storage block
        GLenum prop = GL_NUM_ACTIVE_VARIABLES;
        GLint num_var;
        glGetProgramResourceiv(
            prog_obj, GL_SHADER_STORAGE_BLOCK, resInx, 1, &prop, 1, nullptr, &num_var);

        // get resource indices of the buffer variables
        std::vector<GLint> vars(num_var);
        prop = GL_ACTIVE_VARIABLES;
        glGetProgramResourceiv(
            prog_obj,
            GL_SHADER_STORAGE_BLOCK,
            resInx,
            1,
            &prop,
            (GLsizei)vars.size(),
            nullptr,
            vars.data());

        std::vector<GLint> offsets(num_var);
        std::vector<std::string> var_names(num_var);
        for (GLint i = 0; i < num_var; i++) {

            // get offset of buffer variable relative to SSBO
            GLenum prop = GL_OFFSET;
            glGetProgramResourceiv(
                prog_obj,
                GL_BUFFER_VARIABLE,
                vars[i],
                1,
                &prop,
                (GLsizei)offsets.size(),
                nullptr,
                &offsets[i]);

            // get name of buffer variable
            std::vector<GLchar> var_name(var_max_len);
            GLsizei strLength;
            glGetProgramResourceName(
                prog_obj,
                GL_BUFFER_VARIABLE,
                vars[i],
                var_max_len,
                &strLength,
                var_name.data());
            var_names[i] = var_name.data();
        }

        for (int i = 0; i < offsets.size(); ++i) {
            std::cout << var_names[i] << " offset is " << offsets[i] << "\n";
        }
    }
}

} // anonymous namespace


OpenGLShapeRenderer::~OpenGLShapeRenderer() { cleanup_gl(); }

void OpenGLShapeRenderer::init_gl() {

    if (!shader_) {
        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
    }

    if (ssbo_id_ == std::array<GLuint, 4>{0, 0, 0, 0}) {
        glCreateBuffers(4, ssbo_id_.data());
    }
}

void OpenGLShapeRenderer::cleanup_gl() {

    if (ssbo_id_ != std::array<GLuint, 4>{0, 0, 0, 0}) {
        glDeleteBuffers(4, ssbo_id_.data());
        ssbo_id_ = {0, 0, 0, 0};
    }
}

void OpenGLShapeRenderer::upload_ssbo(
    const std::vector<GLQuad> &quads,
    const std::vector<GLEllipse> &ellipses,
    const std::vector<GLPolygon> &polygons,
    const std::vector<GLPoint> &points) {

    std::array<GLuint, 4> size = {
        static_cast<GLuint>(quads.size() * sizeof(GLQuad)),
        static_cast<GLuint>(ellipses.size() * sizeof(GLEllipse)),
        static_cast<GLuint>(polygons.size() * sizeof(GLPolygon)),
        static_cast<GLuint>(points.size() * sizeof(GLPoint))};

    std::array<const void *, 4> data = {
        quads.data(), ellipses.data(), polygons.data(), points.data()};

    for (int i = 0; i < 4; ++i) {
        glNamedBufferData(ssbo_id_[i], size[i], nullptr, GL_DYNAMIC_DRAW);
        if (size[i] > 0) {
            void *buf = glMapNamedBuffer(ssbo_id_[i], GL_WRITE_ONLY);
            memcpy(buf, data[i], size[i]);
            glUnmapNamedBuffer(ssbo_id_[i]);
        }
    }
}

void OpenGLShapeRenderer::render_shapes(
    const std::vector<Quad> &quads,
    const std::vector<canvas::Polygon> &polygons,
    const std::vector<canvas::Ellipse> &ellipses,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    float viewport_du_dx,
    float image_aspectratio) {

    if (!shader_)
        init_gl();

    utility::JsonStore shader_params;

    shader_params["scale_ar"]       = 1.0f / image_aspectratio;
    shader_params["quads_count"]    = quads.size();
    shader_params["ellipses_count"] = ellipses.size();
    shader_params["polygons_count"] = polygons.size();

    std::vector<GLQuad> gl_quads;
    for (int i = 0; i < quads.size(); ++i) {
        gl_quads.push_back({
            {quads[i].colour.r, quads[i].colour.g, quads[i].colour.b, 1.0f},
            {quads[i].tl.x, quads[i].tl.y},
            {quads[i].tr.x, quads[i].tr.y},
            {quads[i].br.x, quads[i].br.y},
            {quads[i].bl.x, quads[i].bl.y},
            quads[i].opacity / 100.0f,
            quads[i].softness / 500.0f,
            quads[i].invert ? -1.f : 1.f,
        });
    }

    std::vector<GLEllipse> gl_ellipses;
    for (int i = 0; i < ellipses.size(); ++i) {
        gl_ellipses.push_back({
            {ellipses[i].colour.r, ellipses[i].colour.g, ellipses[i].colour.b, 1.0f},
            {ellipses[i].center.x, ellipses[i].center.y},
            {ellipses[i].radius.x, ellipses[i].radius.y},
            ellipses[i].angle,
            ellipses[i].opacity / 100.0f,
            ellipses[i].softness / 500.0f,
            ellipses[i].invert ? -1.f : 1.f,
        });
    }

    std::vector<GLPolygon> gl_polygons;
    std::vector<GLPoint> gl_points;
    for (int i = 0; i < polygons.size(); ++i) {
        gl_polygons.push_back({
            {polygons[i].colour.r, polygons[i].colour.g, polygons[i].colour.b, 1.0f},
            (unsigned int)gl_points.size(),
            (unsigned int)polygons[i].points.size(),
            polygons[i].opacity / 100.0f,
            polygons[i].softness / 500.0f,
            polygons[i].invert ? -1.f : 1.f,
        });
        for (int j = 0; j < polygons[i].points.size(); ++j) {
            gl_points.push_back({{polygons[i].points[j].x, polygons[i].points[j].y}});
        }
    }

    upload_ssbo(gl_quads, gl_ellipses, gl_polygons, gl_points);

    for (int i = 0; i < 4; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_[i]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i + 1, ssbo_id_[i]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }


    shader_->use();
    shader_->set_shader_parameters(shader_params);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    shader_->stop_using();
}


#endif