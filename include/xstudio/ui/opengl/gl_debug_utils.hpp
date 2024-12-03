// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace xstudio {
namespace ui {
    namespace opengl {

        // reads the entire GL viewport into a buffer and writes out
        // to EXR in /user_data/.tmp/xstudio_viewport.%04d.exr
        // incrementing frame number on every draw from 1
        void grab_framebuffer_to_disk();

        void debug_message_callback(
            GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *msg,
            const void *data);

    } // namespace opengl
} // namespace ui
} // namespace xstudio