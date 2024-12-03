// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/opengl_ssbo_image_texture.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;
 
GLSsboTex::~GLSsboTex() {

    // ensure no copying is in flight
    if (upload_thread_.joinable())
        upload_thread_.join();

    wait_on_upload_pixels();
    if (source_frame_ && source_frame_->size()) {
        glUnmapNamedBuffer(ssbo_id_);
    }
    glDeleteBuffers(1, &ssbo_id_);
}


void GLSsboTex::__bind(int /*tex_index*/, Imath::V2i & /*dims*/) {

    if (source_frame_ && source_frame_->size()) {
        glUnmapNamedBuffer(ssbo_id_);
        source_frame_.reset();
    }

    if (ssbo_id_) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_id_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

uint8_t *GLSsboTex::map_buffer_for_upload(const size_t buffer_size) {

    if (!ssbo_id_) {
        glGenBuffers(1, &ssbo_id_);
    }
    compute_size(buffer_size);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id_);
    glNamedBufferData(ssbo_id_, tex_size_bytes(), nullptr, GL_DYNAMIC_DRAW);
    auto rt = (uint8_t *)glMapNamedBuffer(ssbo_id_, GL_WRITE_ONLY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return rt;
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