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

// #define OPENGL_DEBUG_CB
#ifdef OPENGL_DEBUG_CB


void APIENTRY debug_message_callback(
    GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char *message,
    const void *userParam) {
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
        return; // ignore these non-significant error codes

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        std::cout << "Source: API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        std::cout << "Source: Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        std::cout << "Source: Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        std::cout << "Source: Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        std::cout << "Source: Application";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        std::cout << "Source: Other";
        break;
    }
    std::cout << std::endl;

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        std::cout << "Type: Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        std::cout << "Type: Deprecated Behaviour";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        std::cout << "Type: Undefined Behaviour";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        std::cout << "Type: Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        std::cout << "Type: Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        std::cout << "Type: Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        std::cout << "Type: Push Group";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        std::cout << "Type: Pop Group";
        break;
    case GL_DEBUG_TYPE_OTHER:
        std::cout << "Type: Other";
        break;
    }
    std::cout << std::endl;

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        std::cout << "Severity: high";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        std::cout << "Severity: medium";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        std::cout << "Severity: low";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        std::cout << "Severity: notification";
        break;
    }

    std::cout << std::endl;
    std::cout << std::endl;
}


// void debug_message_callback(
//     GLenum source,
//     GLenum type,
//     GLuint id,
//     GLenum severity,
//     GLsizei length,
//     const GLchar *msg,
//     const void *data) {

//     std::string _source;
//     std::string _type;
//     std::string _severity;

//     switch (source) {
//     case GL_DEBUG_SOURCE_API:
//         _source = "API";
//         break;

//     case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
//         _source = "WINDOW SYSTEM";
//         break;

//     case GL_DEBUG_SOURCE_SHADER_COMPILER:
//         _source = "SHADER COMPILER";
//         break;

//     case GL_DEBUG_SOURCE_THIRD_PARTY:
//         _source = "THIRD PARTY";
//         break;

//     case GL_DEBUG_SOURCE_APPLICATION:
//         _source = "APPLICATION";
//         break;

//     case GL_DEBUG_SOURCE_OTHER:
//         _source = "OTHER";
//         break;

//     default:
//         _source = "UNKNOWN";
//         break;
//     }

//     switch (type) {
//     case GL_DEBUG_TYPE_ERROR:
//         _type = "ERROR";
//         break;

//     case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
//         _type = "DEPRECATED BEHAVIOR";
//         break;

//     case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
//         _type = "UNDEFINED BEHAVIOR";
//         break;

//     case GL_DEBUG_TYPE_PORTABILITY:
//         _type = "PORTABILITY";
//         break;

//     case GL_DEBUG_TYPE_PERFORMANCE:
//         _type = "PERFORMANCE";
//         break;

//     case GL_DEBUG_TYPE_OTHER:
//         _type = "OTHER";
//         break;

//     case GL_DEBUG_TYPE_MARKER:
//         _type = "MARKER";
//         break;

//     default:
//         _type = "UNKNOWN";
//         break;
//     }

//     switch (severity) {
//     case GL_DEBUG_SEVERITY_HIGH:
//         _severity = "HIGH";
//         break;

//     case GL_DEBUG_SEVERITY_MEDIUM:
//         _severity = "MEDIUM";
//         break;

//     case GL_DEBUG_SEVERITY_LOW:
//         _severity = "LOW";
//         break;

//     case GL_DEBUG_SEVERITY_NOTIFICATION:
//         _severity = "NOTIFICATION";
//         break;

//     default:
//         _severity = "UNKNOWN";
//         break;
//     }

//     if (severity == GL_DEBUG_SEVERITY_HIGH) {
//         std::cerr << id << ": " << _type << " of " << _severity << " severity, raised from "
//                   << _source << ": " << msg << "\n";
//     }

//     // Uncomment to allow simple gdb usage, if GL_DEBUG_OUTPUT_SYNCHRONOUS
//     // is enabled, the stacktrace in gdb will directly point to the
//     // problematic opengl call.
//     // std::raise(SIGINT);
// }

#endif

} // namespace xstudio::ui::opengl