// SPDX-License-Identifier: Apache-2.0
#include <memory>

#ifdef __apple__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "video_render_plugin.hpp"
#include <xstudio/media_reader/image_buffer_set.hpp>

using namespace xstudio::video_render_plugin_1_0;
using namespace xstudio::global_store;

namespace {
const char *vertex_shader = R"(
    #version 410 core
    layout (location = 0) in vec4 pos;
    out vec2 image_st;
    void main()
    {
        image_st = vec2(0.5*(pos.x+1.0), 0.5*(pos.y+1.0));
        gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
    }
    )";

const char *frag_shader = R"(
    #version 410 core
    out vec4 FragColor;
    uniform sampler2D vp_tex;
    in vec2 image_st;
    void main(void)
    {
        vec2 tx_coord = vec2(image_st.x, 1.0-image_st.y);
        vec3 rgb = texture(vp_tex, tx_coord).rgb;

        // scale to 10 bits
        uint offset = 0; //64; (uncomment for vid range)
        float scale = 1023.0; // 876.0f; (uncomment for vid range)
        uint r = offset + uint(max(0.0,min(rgb.r*scale,scale)));
        uint g = offset + uint(max(0.0,min(rgb.g*scale,scale)));
        uint b = offset + uint(max(0.0,min(rgb.b*scale,scale)));

        // pack
        uint RR = (r << 20) + (g << 10) + b;

        // unpack!
        FragColor = vec4(float((RR >> 24)&255)/255.0,
            float((RR >> 16)&255)/255.0,
            float((RR >> 8)&255)/255.0,
            float(RR&255)/255.0);

    }

    )";
} // namespace

RGB10BitFrameGrabber::~RGB10BitFrameGrabber() {
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
        glDeleteFramebuffers(1, &fboId_);
        glDeleteTextures(1, &texId_);
    }
}

void RGB10BitFrameGrabber::viewport_capture_gl_framebuffer(
    uint32_t tex_id,
    uint32_t fbo_id,
    const int fb_width,
    const int fb_height,
    const xstudio::ui::viewport::ImageFormat format,
    media_reader::ImageBufPtr &destination_image) {
    // tex_id is the GL texture ID containing the image of the rendered
    // xstudio viewport

    if (!shader_) {

        shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);

        // Set up the geometry used at draw time ... it couldn't be more simple,
        // it's just two triangles to make a rectangle
        glGenBuffers(1, &vbo_);
        glGenVertexArrays(1, &vao_);

        static std::array<float, 24> vertices = {
            // 1st triangle
            -1.0f,
            1.0f,
            0.0f,
            1.0f,
            // top left
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            // top right
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            // bottom right
            // 2nd triangle
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            // bottom right
            -1.0f,
            1.0f,
            0.0f,
            1.0f,
            // top left
            -1.0f,
            -1.0f,
            0.0f,
            1.0f
            // bottom left
        };

        glBindVertexArray(vao_);
        // 2. copy our vertices array in a buffer for OpenGL to use
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Here - we have the viewport rendered to an RGBA texture with ID = tex_id.
    // We now need to convert that image to YUV420, so we execute our shader
    // and render to a new texture/framebuffer

    setupImageTextureAndFrameBuffer(fb_width, fb_height);

    // std::cerr << "tex_width_ " << tex_width_ << " " << tex_height_ << "\n";

    glViewport(0, 0, tex_width_, tex_height_);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    // set the active tex IDs for our texture targets
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    shader_->use();

    // set-up core shader parameters (e.g. image transform matrix etc)
    utility::JsonStore shader_params;
    shader_params["vp_tex"] = 10;
    shader_->set_shader_parameters(shader_params);

    glDisable(GL_BLEND);
    // the actual draw .. a quad that spans -1.0, 1.0 in x & y.
    glBindVertexArray(vao_);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glUseProgram(0);
    glFinish();
    /*glBlitFramebuffer(0, 0, 1920, 1080,
                      0, 0, 1920*2, 1080,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);*/
    grabRGB10bitFrameBuffer(fb_width, fb_height, destination_image);
}

void RGB10BitFrameGrabber::setupImageTextureAndFrameBuffer(const int width, const int height) {

    if (tex_width_ == width && tex_height_ == height) {
        // bind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId_, 0);
        return;
    }

    if (texId_) {
        glDeleteTextures(1, &texId_);
        glDeleteFramebuffers(1, &fboId_);
    }

    tex_width_  = width;
    tex_height_ = height;

    // create texture
    glGenTextures(1, &texId_);
    glBindTexture(GL_TEXTURE_2D, texId_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        tex_width_,
        tex_height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // init framebuffer
    glGenFramebuffers(1, &fboId_);
    // bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId_, 0);
}

void RGB10BitFrameGrabber::grabRGB10bitFrameBuffer(
    const int width, const int height, media_reader::ImageBufPtr &destination_image) {

    destination_image = get_video_output_frame();

    if (!pixel_buffer_object_) {
        glGenBuffers(1, &pixel_buffer_object_);
    }

    size_t pix_buf_size = width * height * sizeof(uint8_t) * 4;

    if (pix_buf_size != pix_buf_size_) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_buffer_object_);
        glBufferData(GL_PIXEL_PACK_BUFFER, pix_buf_size, NULL, GL_STREAM_COPY);
        pix_buf_size_ = pix_buf_size;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId_);

    int skip_rows, skip_pixels, row_length, alignment;
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);

    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, width);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_buffer_object_);
    void *mappedBuffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    auto t4 = utility::clock::now();

    destination_image->allocate(pix_buf_size);
    destination_image->set_image_dimensions(Imath::V2i(width, height));
    memcpy(destination_image->buffer(), mappedBuffer, pix_buf_size);

    // now mapped buffer contains the pixel data
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    auto t5 = utility::clock::now();

    auto tt = utility::clock::now();

    // TODO: Gather stats on draw times etc and send to video_output_actor_
    // so it can monitor performance

    /*std::cerr << "Draw time "  <<
    std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << "\n"; std::cerr <<
    "Overlays time "  << std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count() <<
    "\n"; std::cerr << "Map buffer time "  <<
    std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count() << "\n"; std::cerr <<
    "Copy buffer time "  << std::chrono::duration_cast<std::chrono::milliseconds>(t5-t4).count()
    << "\n";*/


    glBindTexture(GL_TEXTURE_2D, 0);

    glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
    glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
}
