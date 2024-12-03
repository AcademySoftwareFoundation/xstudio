// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/opengl_rgba8bit_image_texture.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;
 
GLBlindRGBA8bitTex::~GLBlindRGBA8bitTex() {
    // ensure no copying is in flight
    if (upload_thread_.joinable())
        upload_thread_.join();

    glDeleteTextures(1, &tex_id_);
    glDeleteBuffers(1, &pixel_buf_object_id_);
}

void GLBlindRGBA8bitTex::resize(const size_t required_size_bytes) {

    if (!required_size_bytes)
        return;

    // N.B. Seeing redraw issues if the textures owned by GLDoubleBufferedTexture
    // are not all the same size. This can happen if, when not playing back, the
    // user looks at one frame of a large image (so one texture gets resized to
    // suit that large image). Then they start playing back another image of a
    // smaller size, the other textures might get initialised to the smaller
    // size of the playback images. When this happens, we see scrambled pixels.
    // I haven't got to the bottom of the problem so for now we force all
    // textures to have the same size, the maximum of any texture that has been
    // requested. Note that this method is called on every texture upload.

    static size_t max_tex_size = 0;
    max_tex_size               = std::max(max_tex_size, required_size_bytes);

    if (tex_id_) {

        if (max_tex_size <= tex_size_bytes()) {

            return;
        }

        glDeleteTextures(1, &tex_id_);
        glDeleteBuffers(1, &pixel_buf_object_id_);
    }

    // Create the texture for RGB float display and the Y component of YUV display
    glGenTextures(1, &tex_id_);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex_id_);
    glEnable(GL_TEXTURE_RECTANGLE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

    // ensure the texture width and height are both a power of 2.
    bytes_per_pixel_ = 4;
    tex_width_       = (int)std::pow(
        2.0f,
        std::ceil(
            std::log(std::sqrt(float(max_tex_size / bytes_per_pixel_))) / std::log(2.0f)));
    tex_height_ = (int)std::pow(
        2.0f,
        std::ceil(
            std::log(float(max_tex_size / (tex_width_ * bytes_per_pixel_))) / std::log(2.0f)));

    // tex_width_ = 8192;
    // tex_height_ = 4096;

    glTexImage2D(
        GL_TEXTURE_RECTANGLE,
        0,
        GL_RGBA8UI,
        tex_width_,
        tex_height_,
        0,
        GL_RGBA_INTEGER,
        GL_UNSIGNED_BYTE,
        nullptr);

    glGenBuffers(1, &pixel_buf_object_id_);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buf_object_id_);
    glNamedBufferData(
        pixel_buf_object_id_,
        tex_width_ * tex_height_ * bytes_per_pixel_,
        nullptr,
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

uint8_t *GLBlindRGBA8bitTex::map_buffer_for_upload(const size_t buffer_size) {

    glEnable(GL_TEXTURE_RECTANGLE);

    resize(buffer_size);

    glNamedBufferData(
        pixel_buf_object_id_,
        tex_width_ * tex_height_ * bytes_per_pixel_,
        nullptr,
        GL_DYNAMIC_DRAW);

    return (uint8_t *)glMapNamedBuffer(pixel_buf_object_id_, GL_WRITE_ONLY);
}

void GLBlindRGBA8bitTex::__bind(int tex_index, Imath::V2i &dims) {

    dims.x = tex_width_;
    dims.y = tex_height_;

    if (source_frame_ && source_frame_->size()) {

        // now the texture data is transferred (on the GPU).
        // Assumption is that this is fast.

        glUnmapNamedBuffer(pixel_buf_object_id_);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buf_object_id_);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex_id_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, tex_width_);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, source_frame_->size() / (tex_width_ * 4));
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

        glTexSubImage2D(
            GL_TEXTURE_RECTANGLE,
            0,
            0,
            0,
            tex_width_,
            source_frame_->size() / (tex_width_ * 4),
            GL_RGBA_INTEGER,
            GL_UNSIGNED_BYTE,
            nullptr);

        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        // we can reset our source frame as we have transferred it to
        // texture memory
        source_frame_.reset();
    }

    if (tex_id_) {
        glActiveTexture(tex_index + GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex_id_);
    }
}