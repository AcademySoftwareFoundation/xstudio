// SPDX-License-Identifier: Apache-2.0
#ifdef __linux__
#include <GL/gl.h>
#else
#include <GL/glew.h>
#endif
#include <ImfRgbaFile.h>
#include <vector>
#include <array>

namespace xstudio::ui::opengl {

// reads the entire GL viewport into a buffer and writes out
// to EXR in /user_data/.tmp/xstudio_viewport.%04d.exr
// incrementing frame number on every draw from 1
void grab_framebuffer_to_disk() {

    std::array<GLint, 4> viewport;
    glGetIntegerv(GL_VIEWPORT, viewport.data());

    if (viewport[2] == 1920 && viewport[3] == 1080) {
        std::vector<half> hbuf(viewport[2] * viewport[3] * 4);
        glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_HALF_FLOAT, hbuf.data());

        std::array<char, 2048> nm;
        static int fnum = 1;
        sprintf(nm.data(), "/user_data/.tmp/xstudio_viewport.%04d.exr", fnum++);
        Imf::RgbaOutputFile out(
            nm.data(),
            Imath::Box2i(Imath::V2i(0, 0), Imath::V2i(viewport[2] - 1, viewport[3] - 1)));
        out.setFrameBuffer((Imf::Rgba *)hbuf.data(), 1, viewport[2]);
        out.writePixels(viewport[3]);
    }
}

} // namespace xstudio::ui::opengl