// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/texture.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;

namespace {

class DebugTimer {
  public:
    DebugTimer(std::string m) : message_(std::move(m)) { t1_ = xstudio::utility::clock::now(); }

    ~DebugTimer() {
        std::cerr << message_ << " completed in "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         xstudio::utility::clock::now() - t1_)
                         .count()
                  << " microseconds\n";
    }

  private:
    xstudio::utility::clock::time_point t1_;
    const std::string message_;
};

} // namespace

void GLBlindTex::release() {
    mutex_.unlock();
    when_last_used_ = utility::clock::now();
}

GLBlindTex::~GLBlindTex() {}

GLDoubleBufferedTexture::GLDoubleBufferedTexture() {

    if (using_ssbo_) {
        textures_.emplace_back(new GLSsboTex());
    } else {
        textures_.emplace_back(new GLBlindRGBA8bitTex());
    }
    current_ = textures_.front();
}

void GLDoubleBufferedTexture::set_use_ssbo(const bool using_ssbo) {

    if (using_ssbo != using_ssbo_) {
        textures_.clear();
        using_ssbo_ = using_ssbo;
        if (using_ssbo_) {
            textures_.emplace_back(new GLSsboTex());
        } else {
            textures_.emplace_back(new GLBlindRGBA8bitTex());
        }
        current_ = textures_.front();
    }
}

void GLDoubleBufferedTexture::bind(int &tex_index, Imath::V2i &dims) {

    for (auto &t : textures_) {
        if (t->media_key() == active_media_key_) {
            // t->wait_on_upload();
            t->bind(tex_index, dims);
            current_ = t;
            break;
        }
    }
}

void GLDoubleBufferedTexture::upload_next(
    std::vector<media_reader::ImageBufPtr> images_due_onscreen_soon) {
    if (images_due_onscreen_soon.size()) {
        active_media_key_ = images_due_onscreen_soon.front()->media_key();
    }

    // ensure that we have enough textures - we create 2 more than we need to
    // give us some slack so we don't immediately need to re-use a texture that
    // was just used for drawing
    while (textures_.size() < (images_due_onscreen_soon.size() + 2)) {
        if (using_ssbo_) {
            textures_.emplace_back(new GLSsboTex());
        } else {
            textures_.emplace_back(new GLBlindRGBA8bitTex());
        }
    }

    auto available_textures = textures_;

    // knock images out of images_due_onscreen_soon that are already uploaded
    auto p = images_due_onscreen_soon.begin();
    while (p != images_due_onscreen_soon.end()) {

        const auto key = (*p)->media_key();
        auto cmp = [&key](const GLBlindTexturePtr &tex) { return tex->media_key() == key; };
        auto tx  = std::find_if(available_textures.begin(), available_textures.end(), cmp);

        if (tx != available_textures.end()) {
            available_textures.erase(tx);
            p = images_due_onscreen_soon.erase(p);
        } else {
            p++;
        }
    }

    // we have some textures that are 'free' because they do not have an image
    // uploaded that is part of the 'images_due_onscreen_soon' set. We want to make sure that we
    // use the texture that was actually used to draw the viewport as far back
    // in time as possible. This is because OpenGL/graphics drivers do some
    // clever async stuff meaning that although we think we have finished using
    // a texture in our own draw routines, the GPU might still be using it to
    // finalise draw instructions and thus our call to GLMapBuffer when we try
    // to re-use the texture with another image could be blocked.
    std::sort(
        available_textures.begin(),
        available_textures.end(),
        [](const GLBlindTexturePtr &a, const GLBlindTexturePtr &b) -> bool {
            return a->when_last_used() < b->when_last_used();
        });


    for (auto &tx : available_textures) {
        if (!images_due_onscreen_soon.size())
            break;
        tx->map_buffer_for_upload(images_due_onscreen_soon.front());
        images_due_onscreen_soon.erase(images_due_onscreen_soon.begin());
        tx->start_pixel_upload();
    }
}

void GLDoubleBufferedTexture::release() { current_->release(); }

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

void GLBlindRGBA8bitTex::start_pixel_upload() {

    if (new_source_frame_) {
        if (upload_thread_.joinable())
            upload_thread_.join();
        mutex_.lock();
        upload_thread_ = std::thread(&GLBlindRGBA8bitTex::pixel_upload, this);
    }
}


void GLBlindRGBA8bitTex::pixel_upload() {

    if (!new_source_frame_->size()) {
        mutex_.unlock();
        return;
    }

    const int n_threads = 8; // TODO: proper thread count here
    std::vector<std::thread> memcpy_threads;
    size_t sz   = std::min(tex_size_bytes(), new_source_frame_->size());
    size_t step = ((sz / n_threads) / 4096) * 4096;
    auto *dst   = (uint8_t *)new_source_frame_->buffer();

    uint8_t *ioPtrY = buffer_io_ptr_;

    for (int i = 0; i < n_threads; ++i) {
        memcpy_threads.emplace_back(memcpy, ioPtrY, dst, std::min(sz, step));
        ioPtrY += step;
        dst += step;
        sz -= step;
    }

    // ensure any threads still running to copy data to this texture are done
    for (auto &t : memcpy_threads) {
        if (t.joinable())
            t.join();
    }
    mutex_.unlock();
}

void GLBlindRGBA8bitTex::map_buffer_for_upload(media_reader::ImageBufPtr &frame) {

    if (!frame)
        return;
    // acquire a write lock,
    mutex_.lock();

    new_source_frame_ = frame;
    media_key_        = frame->media_key();

    glEnable(GL_TEXTURE_RECTANGLE);

    if (new_source_frame_->size()) {
        resize(new_source_frame_->size());

        glNamedBufferData(
            pixel_buf_object_id_,
            tex_width_ * tex_height_ * bytes_per_pixel_,
            nullptr,
            GL_DYNAMIC_DRAW);

        buffer_io_ptr_ = (uint8_t *)glMapNamedBuffer(pixel_buf_object_id_, GL_WRITE_ONLY);
    }

    mutex_.unlock();

    // N.B. threads are probably still running here!
}

void GLBlindRGBA8bitTex::bind(int tex_index, Imath::V2i &dims) {

    mutex_.lock();

    dims.x = tex_width_;
    dims.y = tex_height_;

    if (new_source_frame_) {

        if (new_source_frame_->size()) {
            if (upload_thread_.joinable()) {
                upload_thread_.join();
            }

            // now the texture data is transferred (on the GPU).
            // Assumption is that this is fast.

            glUnmapNamedBuffer(pixel_buf_object_id_);

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buf_object_id_);
            glBindTexture(GL_TEXTURE_RECTANGLE, tex_id_);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, tex_width_);
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, new_source_frame_->size() / (tex_width_ * 4));
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

            glTexSubImage2D(
                GL_TEXTURE_RECTANGLE,
                0,
                0,
                0,
                tex_width_,
                new_source_frame_->size() / (tex_width_ * 4),
                GL_RGBA_INTEGER,
                GL_UNSIGNED_BYTE,
                nullptr);

            glBindTexture(GL_TEXTURE_RECTANGLE, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        current_source_frame_ = new_source_frame_;
        new_source_frame_.reset();
    }

    if (current_source_frame_->size()) {
        glActiveTexture(tex_index + GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex_id_);
    }
}

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

GLSsboTex::GLSsboTex() { glGenBuffers(1, &ssbo_id_); }

GLSsboTex::~GLSsboTex() {
    if (upload_thread_.joinable())
        upload_thread_.join();
    glDeleteBuffers(1, &ssbo_id_);
}


void GLSsboTex::wait_on_upload() {

    mutex_.lock();
    if (new_source_frame_) {

        if (new_source_frame_->size()) {
            if (upload_thread_.joinable()) {
                upload_thread_.join();
            }

            glUnmapNamedBuffer(ssbo_id_);
        }

        current_source_frame_ = new_source_frame_;
        new_source_frame_.reset();
    }

    if (current_source_frame_->size()) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_id_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    mutex_.unlock();
}

void GLSsboTex::map_buffer_for_upload(media_reader::ImageBufPtr &frame) {

    auto t0 = utility::clock::now();

    if (!frame)
        return;

    mutex_.lock();

    new_source_frame_ = frame;
    media_key_        = frame->media_key();

    if (new_source_frame_->size()) {
        compute_size(new_source_frame_->size());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
        glNamedBufferData(ssbo_id_, tex_size_bytes(), nullptr, GL_DYNAMIC_DRAW);
        buffer_io_ptr_ = (uint8_t *)glMapNamedBuffer(ssbo_id_, GL_WRITE_ONLY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    mutex_.unlock();
}


void GLSsboTex::compute_size(const size_t required_size_bytes) {

    if (!required_size_bytes)
        return;

    if (ssbo_id_) {
        if (required_size_bytes <= tex_size_bytes()) {
            return;
        }
    }

    tex_data_size_ = pow(2, ceil(log((1 + (required_size_bytes / 4096)) * 4096) / log(2.0)));
}

void GLSsboTex::start_pixel_upload() {

    if (new_source_frame_) {
        if (upload_thread_.joinable())
            upload_thread_.join();
        mutex_.lock();
        upload_thread_ = std::thread(&GLSsboTex::pixel_upload, this);
    }
}

void GLSsboTex::pixel_upload() {

    if (!new_source_frame_->size()) {
        mutex_.unlock();
        return;
    }

    const int n_threads = 8; // TODO: proper thread count here
    std::vector<std::thread> memcpy_threads;
    size_t sz   = std::min(tex_size_bytes(), new_source_frame_->size());
    size_t step = ((sz / n_threads) / 4096) * 4096;
    auto *dst   = (uint8_t *)new_source_frame_->buffer();

    uint8_t *ioPtrY = buffer_io_ptr_;

    for (int i = 0; i < n_threads; ++i) {
        memcpy_threads.emplace_back(memcpy, ioPtrY, dst, std::min(sz, step));
        ioPtrY += step;
        dst += step;
        sz -= step;
    }

    // ensure any threads still running to copy data to this texture are done
    for (auto &t : memcpy_threads) {
        if (t.joinable())
            t.join();
    }

    mutex_.unlock();
}