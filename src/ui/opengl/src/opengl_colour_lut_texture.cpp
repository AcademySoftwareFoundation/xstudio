// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/opengl_colour_lut_texture.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;
 
GLint GLColourLutTexture::interpolation() {
    switch (descriptor_.interpolation_) {
    case colour_pipeline::LUTDescriptor::NEAREST:
        return GL_NEAREST;
    case colour_pipeline::LUTDescriptor::LINEAR:
        return GL_LINEAR;
    default:
        return GL_LINEAR;
    }
}

GLint GLColourLutTexture::internal_format() {
    if (descriptor_.channels_ == colour_pipeline::LUTDescriptor::RGB) {
        switch (descriptor_.data_type_) {
        case colour_pipeline::LUTDescriptor::UINT8:
            return GL_RGB8UI;
        case colour_pipeline::LUTDescriptor::UINT16:
            return GL_RGB16UI;
        case colour_pipeline::LUTDescriptor::FLOAT16:
            return GL_RGB16F;
        case colour_pipeline::LUTDescriptor::FLOAT32:
            return GL_RGB32F;
        default:
            return GL_RGB32F;
        }
    } else {
        switch (descriptor_.data_type_) {
        case colour_pipeline::LUTDescriptor::UINT8:
            return GL_R8UI;
        case colour_pipeline::LUTDescriptor::UINT16:
            return GL_R16UI;
        case colour_pipeline::LUTDescriptor::FLOAT16:
            return GL_R16F;
        case colour_pipeline::LUTDescriptor::FLOAT32:
            return GL_R32F;
        default:
            return GL_R32F;
        }
    }
}

GLint GLColourLutTexture::format() {
    if (descriptor_.channels_ == colour_pipeline::LUTDescriptor::RGB) {

        switch (descriptor_.data_type_) {
        case colour_pipeline::LUTDescriptor::UINT8:
            return GL_RGB_INTEGER;
        case colour_pipeline::LUTDescriptor::UINT16:
            return GL_RGB_INTEGER;
        case colour_pipeline::LUTDescriptor::FLOAT16:
            return GL_RGB;
        case colour_pipeline::LUTDescriptor::FLOAT32:
            return GL_RGB;
        default:
            return GL_RGB;
        }
    } else {
        switch (descriptor_.data_type_) {
        case colour_pipeline::LUTDescriptor::UINT8:
            return GL_RED_INTEGER;
        case colour_pipeline::LUTDescriptor::UINT16:
            return GL_RED_INTEGER;
        case colour_pipeline::LUTDescriptor::FLOAT16:
            return GL_RED;
        case colour_pipeline::LUTDescriptor::FLOAT32:
            return GL_RED;
        default:
            return GL_RED;
        }
    }
}

GLint GLColourLutTexture::data_type() {
    switch (descriptor_.data_type_) {
    case colour_pipeline::LUTDescriptor::UINT8:
        return GL_UNSIGNED_BYTE;
    case colour_pipeline::LUTDescriptor::UINT16:
        return GL_UNSIGNED_SHORT;
    case colour_pipeline::LUTDescriptor::FLOAT16:
        return GL_HALF_FLOAT;
    case colour_pipeline::LUTDescriptor::FLOAT32:
        return GL_FLOAT;
    default:
        return GL_FLOAT;
    }
}


GLColourLutTexture::GLColourLutTexture(
    const colour_pipeline::LUTDescriptor desc, const std::string texture_name)
    : descriptor_(std::move(desc)), texture_name_(std::move(texture_name)) {
    // Create the texture for RGB float display and the Y component of YUV display
    glGenTextures(1, &tex_id_);

    if (desc.dimension_ == colour_pipeline::LUTDescriptor::ONE_D) {
        glBindTexture(target(), tex_id_);
        glTexParameteri(target(), GL_TEXTURE_MIN_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_MAG_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage1D(
            target(), 0, internal_format(), desc.xsize_, 0, format(), data_type(), nullptr);

    } else if (desc.dimension_ & colour_pipeline::LUTDescriptor::TWO_D) {
        glBindTexture(target(), tex_id_);
        glTexParameteri(target(), GL_TEXTURE_MIN_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_MAG_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            target(),
            0,
            internal_format(),
            desc.xsize_,
            desc.ysize_,
            0,
            format(),
            data_type(),
            nullptr);

    } else if (desc.dimension_ == colour_pipeline::LUTDescriptor::THREE_D) {
        glBindTexture(target(), tex_id_);
        glTexParameteri(target(), GL_TEXTURE_MIN_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_MAG_FILTER, interpolation());
        glTexParameteri(target(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(target(), GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(
            target(),
            0,
            internal_format(),
            desc.xsize_,
            desc.ysize_,
            desc.zsize_,
            0,
            format(),
            data_type(),
            nullptr);
    }

    glGenBuffers(1, &pbo_);
}

GLColourLutTexture::~GLColourLutTexture() {
    glDeleteTextures(1, &tex_id_);
    glDeleteBuffers(1, &pbo_);
}

GLenum GLColourLutTexture::target() const {
    if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::ONE_D)
        return GL_TEXTURE_1D;
    else if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::TWO_D)
        return GL_TEXTURE_2D;
    else if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::THREE_D)
        return GL_TEXTURE_3D;
    else if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::RECT_TWO_D)
        return GL_TEXTURE_RECTANGLE;

    return GL_TEXTURE_RECTANGLE;
}


void GLColourLutTexture::upload_texture_data(const colour_pipeline::ColourLUTPtr &lut) {

    if (lut->cache_id() == lut_cache_id_)
        return;

    lut_cache_id_ = lut->cache_id();

    // Create the texture for RGGLColourLutTextureB float display and the Y component of YUV
    // display
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, lut->data_size(), nullptr, GL_STREAM_DRAW);

    void *mappedBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(mappedBuffer, lut->data(), lut->data_size());

    glBindTexture(target(), tex_id_);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::ONE_D) {


        glTexSubImage1D(target(), 0, 0, descriptor_.xsize_, format(), data_type(), nullptr);


    } else if (descriptor_.dimension_ & colour_pipeline::LUTDescriptor::TWO_D) {

        glTexSubImage2D(
            target(),
            0,
            0,
            0,
            descriptor_.xsize_,
            descriptor_.ysize_,
            format(),
            data_type(),
            nullptr);

    } else if (descriptor_.dimension_ == colour_pipeline::LUTDescriptor::THREE_D) {

        glTexSubImage3D(
            target(),
            0,
            0,
            0,
            0,
            descriptor_.xsize_,
            descriptor_.ysize_,
            descriptor_.zsize_,
            format(),
            data_type(),
            nullptr);
    }
    // glActiveTexture((GLenum)(GL_TEXTURE0));
    glBindTexture(target(), 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLColourLutTexture::bind(int tex_index) {
    glActiveTexture(tex_index + GL_TEXTURE0);
    glBindTexture(target(), tex_id_);
}