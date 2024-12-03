// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/opengl/opengl_offscreen_renderer.hpp"

using namespace xstudio;
using namespace xstudio::ui::opengl;


OpenGLOffscreenRenderer::OpenGLOffscreenRenderer(GLint color_format)
    : color_format_(color_format) {}

OpenGLOffscreenRenderer::~OpenGLOffscreenRenderer() { cleanup(); }

void OpenGLOffscreenRenderer::resize(const Imath::V2f &dims) {
    if (dims == fbo_dims_) {
        return;
    }

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restore_fbo_id_);

    cleanup();

    fbo_dims_      = dims;
    unsigned int w = dims.x;
    unsigned int h = dims.y;

    glGenTextures(1, &tex_id_);

    glBindTexture(tex_target_, tex_id_);
    glTexImage2D(tex_target_, 0, color_format_, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(tex_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(tex_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(tex_target_, 0);

    glGenRenderbuffers(1, &rbo_id_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_id_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &fbo_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex_target_, tex_id_, 0);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_id_);

    glBindFramebuffer(GL_FRAMEBUFFER, restore_fbo_id_);
}

void OpenGLOffscreenRenderer::begin() {
    // Save viewport state
    unsigned int w = fbo_dims_.x;
    unsigned int h = fbo_dims_.y;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restore_fbo_id_);
    glGetIntegerv(GL_VIEWPORT, vp_state_.data());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);
    glViewport(0, 0, w, h);
}

void OpenGLOffscreenRenderer::end() {
    // restore bound FB
    glBindFramebuffer(GL_FRAMEBUFFER, restore_fbo_id_);
    // Restore viewport state
    glViewport(vp_state_[0], vp_state_[1], vp_state_[2], vp_state_[3]);
}

void OpenGLOffscreenRenderer::cleanup() {
    if (fbo_id_) {
        glDeleteFramebuffers(1, &fbo_id_);
        fbo_id_ = 0;
    }
    if (rbo_id_) {
        glDeleteRenderbuffers(1, &rbo_id_);
        rbo_id_ = 0;
    }
    if (tex_id_) {
        glDeleteTextures(1, &tex_id_);
        tex_id_ = 0;
    }

    fbo_dims_ = Imath::V2f(0.0f, 0.0f);
    vp_state_ = std::array<int, 4>{0, 0, 0, 0};
}
