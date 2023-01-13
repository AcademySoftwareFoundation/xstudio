// SPDX-License-Identifier: Apache-2.0
// #include <filesystem>
#include <dlfcn.h>
#include <filesystem>

#include <fstream>
#include <iostream>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media;
using namespace xstudio::media_reader;

namespace fs = std::filesystem;


media_reader::byte *Buffer::allocate(const size_t size) {
    if (size_ != size) {
        // This seems like a way over the top aligment amount but (I think) it's
        // required for some vide ocodec buffers in ffmpeg like VP9
        buffer_.reset(new (std::align_val_t(1024)) byte[size]);
        // buffer_.reset(new byte[size*2]);
        size_ = size;
    }
    return buffer_.get();
}

void Buffer::resize(const size_t size) {
    auto new_buf = new (std::align_val_t(1024)) byte[size];
    memcpy(new_buf, buffer_.get(), std::min(size, size_));
    buffer_.reset(new_buf);
    size_ = size;
}

xstudio::media_reader::byte *ImageBuffer::allocate(const size_t _size) {

    // OpenGL HACK - we need the total size of the image buffer to be an exact multiple
    // of the number of bytes in one line of the OpenGL texture that it is finally copied to.
    // The reason is that the memory mapped texture upload that we use doesn't allow you
    // to call glTexSubImage2D with a texture area that is bigger than the area of memory
    // that was written to with the memcpy call. We don't know how wide the texture area is
    // but it is a power of two up to 8192, and it's format is 4 bytes per pixel (RGBA 8 bit).
    // Hence ensuring the image buffer size is padded up to a multiple of 8192*4 means we
    // can be sure that the buffer size fits exactly into a whole number of horizontal lines
    // in the texture.
    const size_t gl_line_size = 8192 * 4;
    const size_t padded_size =
        (_size & (gl_line_size - 1)) ? ((_size / gl_line_size) + 1) * gl_line_size : _size;

    return Buffer::allocate(padded_size);
}

MediaReader::MediaReader(std::string name, const utility::JsonStore &)
    : name_(std::move(name)) {}

std::string MediaReader::name() const { return name_; }

ImageBufPtr MediaReader::image(const media::AVFrameID &) { return ImageBufPtr(); }

AudioBufPtr MediaReader::audio(const media::AVFrameID &) { return AudioBufPtr(); }

thumbnail::ThumbnailBufferPtr MediaReader::thumbnail(const media::AVFrameID &mp, const size_t) {
    throw std::runtime_error(
        "Thumbnail generation not supported for this format. " + mp.reader_);
    return thumbnail::ThumbnailBufferPtr();
}

MRCertainty MediaReader::supported(const caf::uri &, const std::array<uint8_t, 16> &) {
    return MRC_NO;
}

void MediaReader::update_preferences(const utility::JsonStore &) {}

MediaDetail MediaReader::detail(const caf::uri &) const {
    return MediaDetail(name(), {StreamDetail()});
}

uint8_t MediaReader::maximum_readers(const caf::uri &) const { return 1; }

bool MediaReader::prefer_sequential_access(const caf::uri &) const { return true; }

bool MediaReader::can_decode_audio() const { return false; }
