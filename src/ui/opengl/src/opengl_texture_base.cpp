// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/opengl_texture_base.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;


void GLBlindTex::release() { when_last_used_ = utility::clock::now(); }

GLBlindTex::GLBlindTex() {

    static int f = 0;
    media_key_ = media::MediaKey("EmptyTex{}{}{}",caf::uri(),f++,"_0");

}

GLBlindTex::~GLBlindTex() {}

void GLBlindTex::do_pixel_upload() {

    auto t0 = utility::clock::now();

    std::unique_lock lk(mutex_);
    in_progress_ = true;
    lk.unlock();
    if (pending_source_frame_) {

        uint8_t * xstudio_pixel_buffer = (uint8_t *)pending_source_frame_->buffer();
        size_t copy_size = std::min(tex_size_bytes(), pending_source_frame_->size());

        // just a memcpy!
        if (gpu_mapped_mem_ && xstudio_pixel_buffer && copy_size) {
            memcpy(gpu_mapped_mem_, xstudio_pixel_buffer, copy_size);
        }
    }
    lk.lock();
    pending_upload_ = false;
    in_progress_ = false;
    media_key_ = pending_media_key_;
    source_frame_ = pending_source_frame_;
    lk.unlock();
    cv_.notify_one();

    /*std::cerr << "Tex " << to_string(media_key_) << " uploaded in " <<
    std::chrono::duration_cast<std::chrono::microseconds>(utility::clock::now()-t0).count()
    <<
    "\n";*/
}

void GLBlindTex::wait_on_upload_pixels() {
    std::unique_lock lk(mutex_);
    while (pending_upload_) {
        cv_.wait(lk);
    }
}

void GLBlindTex::cancel_upload() {
    std::unique_lock lk(mutex_);
    while (in_progress_) {
        cv_.wait(lk);
    }
    pending_upload_ = false;
    pending_media_key_ = media::MediaKey();
    pending_source_frame_.reset();
}


void GLBlindTex::prepare_for_upload(const media_reader::ImageBufPtr &frame) {

    if (!frame)
        return;

    // make sure we're not still uploading pixels from a previous request
    wait_on_upload_pixels();

    pending_media_key_ = frame->media_key();
    pending_source_frame_ = frame;

    if (pending_source_frame_ && pending_source_frame_->size()) {

        gpu_mapped_mem_ = map_buffer_for_upload(pending_source_frame_->size());
        if (!gpu_mapped_mem_) {
            spdlog::warn("Failed to map buffer for frame {} ", to_string(media_key_));
            media_key_ = media::MediaKey();
            pending_source_frame_.reset();
            return;
        }

        {
            std::lock_guard lk(mutex_);
            pending_upload_ = true;
        }

    }
}