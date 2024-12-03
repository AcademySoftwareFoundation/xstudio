// SPDX-License-Identifier: Apache-2.0
// #include <filesystem>
#ifdef __linux__
#include <dlfcn.h>
#endif
#include <filesystem>

#include <fstream>
#include <iostream>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media;
using namespace xstudio::media_reader;

namespace fs = std::filesystem;

/* ImageBufferRecyclerCache
 *
 *  During playback, once the cache is full, old image buffers are deleted by
 *  the cache while new image buffers are allocated by the image reader. This
 *  happens at high frequency (typically at least 24hz) and frame buffers can be
 *  large, many megabytes possibly even 100s. We are assuming this is a bad idea
 *  to expect the kernel's memory management to deal with this very efficiently
 *  where large amounts of memory are allocated and deleted at a high rate.
 *  Some testing is suggesting the application will consume an unexpectedly
 *  large amount of memory under these circumstances, though we're not sure
 *  exactly what's happenind. As such, we can hang on to the allocated memory in
 *  another smaller cache here.
 *
 *  Assuming that during playback the size of the allocation is the same (for
 *  some give image format) it's probably going to be much better to hang onto
 *  the recenly de -allocated memory and re-use it when the reader wants to
 *  allocate a new ImageBuffer. Due to threading and frame pools used by ffmpeg,
 *  for example, the allocation of image frames can be 'lumpy' with the reader
 *  asking for several new frames at once. Since the xStudio cache will ditch
 *  frames in  a more predictable one-by-one pattern, we need to hang on to
 *  several image buffers ready for re-use by the reader. Instead of burdening
 *  the image cache with the work of managing this, and possibly slowing the
 *  reader down while it waits for the cache to re-cycle image buffers, we are
 *  going to do it at a lower level within the ImageBuffer allocate method
 */
void ImageBufferRecyclerCache::store_unwanted_buffer(
    Buffer::BufferDataPtr &buf, const size_t size) {

    // uncomment this to turn the whole thing off
    // return;
    mutex_.lock();
    while ((total_size_ + size) > max_size_) {

        // simply delete oldest buffers - if user is playing through a timeline
        // of mixed formats it's likely that the cache is deleting frames
        // of a different size to the one that the readers are currently asking
        // for. However, it's going to be quite complex to address that
        // problem and we have to fallback on the OS memory managment coping
        // with rapid allocation/deallocation ok. The general case is that
        // the xStudio session is loaded with common format sources, though.
        auto p = size_by_time_.begin();
        if (p == size_by_time_.end())
            break;
        const size_t s = p->second;
        if (!recycle_buffer_bin_[s].empty()) {
            recycle_buffer_bin_[s].pop_back();
            if (recycle_buffer_bin_[s].empty()) {
                recycle_buffer_bin_.erase(recycle_buffer_bin_.find(s));
            }
            total_size_ -= s;
        }
        size_by_time_.erase(p);
    }
    auto tp = utility::clock::now();
    recycle_buffer_bin_[size].push_back(buf);
    total_size_ += size;
    size_by_time_[tp] = size;
    mutex_.unlock();
}

Buffer::BufferDataPtr
ImageBufferRecyclerCache::fetch_recycled_buffer(const size_t required_size) {

    Buffer::BufferDataPtr r;
    mutex_.lock();
    auto p = recycle_buffer_bin_.find(required_size);
    if (p != recycle_buffer_bin_.end() && !p->second.empty()) {

        auto q = size_by_time_.begin();
        while (q != size_by_time_.end()) {
            if (q->second == required_size) {
                size_by_time_.erase(q);
                break;
            }
            q++;
        }

        r = p->second.back();
        p->second.pop_back();
        if (p->second.empty())
            recycle_buffer_bin_.erase(p);
        total_size_ -= required_size;
    }
    mutex_.unlock();

    return r;
}

std::shared_ptr<ImageBufferRecyclerCache> Buffer::s_buf_cache =
    std::make_shared<ImageBufferRecyclerCache>();

Buffer::~Buffer() { s_buf_cache->store_unwanted_buffer(buffer_, size_); }

xstudio::media_reader::byte *Buffer::allocate(const size_t size) {
    if (size_ != size) {

        buffer_ = s_buf_cache->fetch_recycled_buffer(size);
        if (!buffer_) {
            buffer_.reset(new BufferData(size));
        }
        size_ = size;
    }
    return buffer();
}

void Buffer::resize(const size_t size) {
    auto old_buffer = buffer_;
    auto old_size   = size_;
    allocate(size);
    if (old_buffer) {
        memcpy(buffer(), old_buffer->data_.get(), std::min(old_size, size_));
        s_buf_cache->store_unwanted_buffer(old_buffer, old_size);
    }
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

void ImageBufDisplaySet::finalise() {
    
    utility::JsonStore image_info = nlohmann::json::parse("[]");
    images_hash_ = 0;
    for (int i = 0; i < num_onscreen_images(); ++i) {
        const auto & im = onscreen_image(i);
        nlohmann::json r;
        if (im) {
            r["image_size_in_pixels"] = im->image_size_in_pixels();
            //r["image_pixels_bounding_box"] = im->image_pixels_bounding_box();
            r["pixel_aspect"] = im->pixel_aspect();
            images_hash_ += im.frame_id().key().hash();
        }
        image_info.push_back(r);
    }

    as_json_.clear();
    as_json_["image_info"] = image_info;
    as_json_["hero_image_index"] = hero_sub_playhead_index_;
    as_json_["prev_hero_image_index"] = previous_hero_sub_playhead_index_;
    hash_ = int64_t(std::hash<std::string>{}(as_json_.dump()));

}

MediaReader::MediaReader(std::string name, const utility::JsonStore &)
    : name_(std::move(name)) {}

std::string MediaReader::name() const { return name_; }

ImageBufPtr MediaReader::image(const media::AVFrameID &) { return ImageBufPtr(); }

AudioBufPtr MediaReader::audio(const media::AVFrameID &) { return AudioBufPtr(); }

thumbnail::ThumbnailBufferPtr MediaReader::thumbnail(const media::AVFrameID &mp, const size_t) {
    throw std::runtime_error(
        "Thumbnail generation not supported for this format. " + mp.reader());
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

bool MediaReader::can_do_partial_frames() const { return false; }
