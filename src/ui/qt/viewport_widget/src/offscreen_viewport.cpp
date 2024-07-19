// SPDX-License-Identifier: Apache-2.0
#include <GL/glew.h>
#include <GL/gl.h>

#include <filesystem>


#include "xstudio/ui/qt/offscreen_viewport.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/thumbnail/enums.hpp"

#include <ImfRgbaFile.h>
#include <vector>
#include <QStringList>
#include <QWidget>
#include <QByteArray>
#include <QImageWriter>
#include <QThread>

using namespace caf;
using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::ui::qt;
using namespace xstudio::ui::viewport;

namespace fs = std::filesystem;

namespace {
    static void threaded_memcpy(void * _dst, void * _src, size_t n, int n_threads) {

        std::vector<std::thread> memcpy_threads;
        size_t step = ((n / n_threads) / 4096) * 4096;

        uint8_t *dst = (uint8_t *)_dst; 
        uint8_t *src = (uint8_t *)_src;

        for (int i = 0; i < n_threads; ++i) {
            memcpy_threads.emplace_back(memcpy, dst, src, std::min(n, step));
            dst += step;
            src += step;
            n -= step;
        }

        // ensure any threads still running to copy data to this texture are done
        for (auto &t : memcpy_threads) {
            if (t.joinable())
                t.join();
        }

    }

    static std::map<viewport::ImageFormat, GLint> format_to_gl_tex_format = {
        {viewport::ImageFormat::RGBA_8, GL_RGBA8},
        {viewport::ImageFormat::RGBA_10_10_10_2, GL_RGBA8},
        {viewport::ImageFormat::RGBA_16, GL_RGBA16},
        {viewport::ImageFormat::RGBA_16F, GL_RGBA16F},
        {viewport::ImageFormat::RGBA_32F, GL_RGBA32F}
    };

    static std::map<viewport::ImageFormat, GLint> format_to_gl_pixe_type = {
        {viewport::ImageFormat::RGBA_8, GL_UNSIGNED_BYTE},
        {viewport::ImageFormat::RGBA_10_10_10_2,  GL_UNSIGNED_BYTE},
        {viewport::ImageFormat::RGBA_16, GL_UNSIGNED_SHORT},
        {viewport::ImageFormat::RGBA_16F, GL_HALF_FLOAT},
        {viewport::ImageFormat::RGBA_32F, GL_FLOAT}
    };

    static std::map<viewport::ImageFormat, GLint> format_to_bytes_per_pixel = {
        {viewport::ImageFormat::RGBA_8, 4},
        {viewport::ImageFormat::RGBA_10_10_10_2,  4},
        {viewport::ImageFormat::RGBA_16, 8},
        {viewport::ImageFormat::RGBA_16F, 8},
        {viewport::ImageFormat::RGBA_32F, 16}
    };

}

OffscreenViewport::OffscreenViewport(const std::string name) : super() {

    // This class is a QObject with a caf::actor 'companion' that allows it
    // to receive and send caf messages - here we run necessary initialisation
    // of the companion actor
    super::init(qml::CafSystemObject::get_actor_system());

    scoped_actor sys{qml::CafSystemObject::get_actor_system()};

    // Now we create our OpenGL xSTudio viewport - this has 'Viewport(Module)' as
    // its base class that provides various caf message handlers that are added
    // to our companion actor's 'behaviour' to create a fully functioning
    // viewport that can receive caf messages including framebuffers and also
    // to render the viewport into our GLContext
    static int offscreen_idx = -1;
    utility::JsonStore jsn;
    jsn["base"]        = utility::JsonStore();
    viewport_renderer_ = new Viewport(
        jsn,
        as_actor(),
        offscreen_idx--,
        ViewportRendererPtr(new opengl::OpenGLViewportRenderer(true, false)),
        name);

    /* Provide a callback so the Viewport can tell this class when some property of the viewport
    has changed and such events can be propagated to other QT components, for example */
    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };
    //viewport_renderer_->set_change_callback(callback);

    self()->set_down_handler([=](down_msg &msg) {
        if (msg.source == video_output_actor_) {
            video_output_actor_ = caf::actor();
        }
    });

    // Here we set-up the caf message handler for this class by combining the
    // message handler from OpenGLViewportRenderer with our own message handlers for offscreen
    // rendering
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return viewport_renderer_->message_handler().or_else(caf::message_handler{

            // insert additional message handlers here
            [=](viewport::render_viewport_to_image_atom, const int width, const int height)
                -> result<bool> {
                try {
                    // copies a QImage to the Clipboard
                    renderSnapshot(width, height);
                    return true;
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
            },

            [=](viewport::render_viewport_to_image_atom,
                const caf::uri path,
                const int width,
                const int height) -> result<bool> {
                try {
                    renderSnapshot(width, height, path);
                    return true;
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
            },

            [=](viewport::render_viewport_to_image_atom,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const int height) -> result<thumbnail::ThumbnailBufferPtr> {
                try {
                    return renderToThumbnail(format, width, height);
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
            },

            [=](viewport::render_viewport_to_image_atom,
                caf::actor media_actor,
                const int media_frame,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool auto_scale,
                const bool show_annotations) -> result<thumbnail::ThumbnailBufferPtr> {
                thumbnail::ThumbnailBufferPtr r;
                try {
                    r = renderMediaFrameToThumbnail(
                        media_actor, media_frame, format, width, auto_scale, show_annotations);
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
                return r;
            },
            
            [=](video_output_actor_atom,
                caf::actor video_output_actor,
                int outputWidth,
                int outputHeight,
                viewport::ImageFormat format) {
                
                video_output_actor_ = video_output_actor;
                vid_out_width_ = outputWidth;
                vid_out_height_ = outputHeight;
                vid_out_format_ = format;

            },

            [=](video_output_actor_atom,
                caf::actor video_output_actor) {                
                video_output_actor_ = video_output_actor;
            },

            [=](render_viewport_to_image_atom) {                
                // force a redraw
                receive_change_notification(Viewport::ChangeCallbackId::Redraw);
            }
                        
            });
    });

    initGL();

}

OffscreenViewport::~OffscreenViewport() {

    gl_context_->makeCurrent(surface_);
    delete viewport_renderer_;
    glDeleteTextures(1, &texId_);
    glDeleteFramebuffers(1, &fboId_);
    glDeleteTextures(1, &depth_texId_);

    delete gl_context_;
    delete surface_;

    video_output_actor_ = caf::actor();

}


void OffscreenViewport::autoDelete() {

    delete this;

}


void OffscreenViewport::initGL() {

    if (!gl_context_) {
        // create our own GL context
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setDepthBufferSize(24);
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setAlphaBufferSize(8);
        format.setRenderableType(QSurfaceFormat::OpenGL);

        gl_context_ = new QOpenGLContext(nullptr); // m_window->openglContext();
        gl_context_->setFormat(format);
        if (!gl_context_)
            throw std::runtime_error(
                "OffscreenViewport::initGL - could not create QOpenGLContext.");
        if (!gl_context_->create()) {
            throw std::runtime_error("OffscreenViewport::initGL - failed to creat GL Context "
                                     "for offscreen rendering.");
        }

        // we also require a QSurface to use the GL context
        surface_ = new QOffscreenSurface(nullptr, nullptr);
        surface_->setFormat(format);
        surface_->create();

        // gl_context_->makeCurrent(surface_);

        // we also require a QSurface to use the GL context
        surface_ = new QOffscreenSurface(nullptr, nullptr);
        surface_->setFormat(format);
        surface_->create();

        thread_ = new QThread();
        gl_context_->moveToThread(thread_);
        moveToThread(thread_);
        thread_->start();

        // Note - the only way I seem to be able to 'cleanly' exit is 
        // delete ourselves when the thread quits. Not 100% sure if this 
        // is correct approach. I'm still cratching my head as to how
        // to destroy thread_ ... calling deleteLater() directly or
        // using finished signal has no effect.

        connect(thread_, SIGNAL(finished()), this, SLOT(autoDelete()));

        // this has no effect!
        // connect(thread_, SIGNAL(finished()), this, SLOT(deleteLater()));
    }
}

void OffscreenViewport::stop() {
    thread_->quit();
    thread_->wait();
}

void OffscreenViewport::renderSnapshot(const int width, const int height, const caf::uri path) {

    // initGL();

    // temp hack - put in a 500ms delay so the playhead can update the
    // annotations plugin with the annotations data.
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (path.empty()) {
        throw std::runtime_error("Invalid (empty) file path.");
    }

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid image dimensions.");
    }

    media_reader::ImageBufPtr image(new media_reader::ImageBuffer());
    renderToImageBuffer(width, height, image, viewport::ImageFormat::RGBA_16F);

    auto p = fs::path(xstudio::utility::uri_to_posix_path(path));

    std::string ext = xstudio::utility::ltrim_char(
#ifdef _WIN32
        xstudio::utility::to_upper_path(p.extension()),
#else
        xstudio::utility::to_upper(p.extension()),
#endif
        '.'); // yuk!

    if (ext == "EXR") {
        this->exportToEXR(image, path);
    } else {
        this->exportToCompressedFormat(image, path, ext);
    }
}

void OffscreenViewport::setPlayhead(const QString &playheadAddress) {

    try {

        scoped_actor sys{as_actor()->home_system()};
        auto playhead_actor = qml::actorFromQString(as_actor()->home_system(), playheadAddress);

        if (playhead_actor) {
            viewport_renderer_->set_playhead(playhead_actor);

            if (viewport_renderer_->colour_pipeline()) {
                // get the current on screen media source
                auto media_source = utility::request_receive<utility::UuidActor>(
                    *sys, playhead_actor, playhead::media_source_atom_v, true);

                // update the colour pipeline with the media source so it can
                // run its logic to update the view/display attributes etc.
                utility::request_receive<bool>(
                    *sys,
                    viewport_renderer_->colour_pipeline(),
                    playhead::media_source_atom_v,
                    media_source);
            }
        }


    } catch (std::exception &e) {
        spdlog::warn("{} {} ", __PRETTY_FUNCTION__, e.what());
    }
}

void OffscreenViewport::exportToEXR(const media_reader::ImageBufPtr &buf, const caf::uri path) {
    Imf::Header header;
    const Imath::V2i dim = buf->image_size_in_pixels();
    Imath::Box2i box;
    box.min.x           = 0;
    box.min.y           = 0;
    box.max.x           = dim.x - 1;
    box.max.y           = dim.y - 1;
    header.dataWindow() = header.displayWindow() = box;
    header.compression()                         = Imf::PIZ_COMPRESSION;
    Imf::RgbaOutputFile outFile(utility::uri_to_posix_path(path).c_str(), header);
    outFile.setFrameBuffer((Imf::Rgba *)buf->buffer(), 1, dim.x);
    outFile.writePixels(dim.y);
}

void OffscreenViewport::exportToCompressedFormat(
    const media_reader::ImageBufPtr &buf, const caf::uri path, const std::string &ext) {

    thumbnail::ThumbnailBufferPtr r = rgb96thumbFromHalfFloatImage(buf);
    r->convert_to(thumbnail::TF_RGB24);

    // N.B. We can't pass our thumnail buffer directly to QImage constructor as
    // it requires 32 bit alignment on scanlines and our Thumbnail buffer is
    // not designed as such.

    const int width  = r->width();
    const int height = r->height();

    const auto *in_px = (const uint8_t *)r->data().data();
    QImage im(width, height, QImage::Format_RGB888);

    // In fact QImage is a bit funky and won't let us write whole scanlines so
    // have to do it pixel by pixel
    for (int line = 0; line < height; line++) {
        for (int x = 0; x < width; x++) {
            im.setPixelColor(x, line, QColor((int)in_px[0], (int)in_px[1], (int)in_px[2]));
            in_px += 3;
        }
    }

    QApplication::clipboard()->setImage(im, QClipboard::Clipboard);

    /*int compLevel =
        ext == "TIF" || ext == "TIFF" ? std::max(compression, 1) : (10 - compression) * 10;*/
    // TODO : check m_filePath for extension, if not, add to it. Do it on QML side after merging
    // with new UI branch

    if (path.empty())
        return;

    QImageWriter writer(xstudio::utility::uri_to_posix_path(path).c_str());
    // writer.setCompression(compLevel);
    if (!writer.write(im)) {
        throw std::runtime_error(writer.errorString().toStdString().c_str());
    }
}

void OffscreenViewport::setupTextureAndFrameBuffer(const int width, const int height, const viewport::ImageFormat format) {

    if (tex_width_ == width && tex_height_ == height && format == vid_out_format_  ) {
        // bind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId_, 0);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texId_, 0);
        return;
    }

    if (texId_) {
        glDeleteTextures(1, &texId_);
        glDeleteFramebuffers(1, &fboId_);
        glDeleteTextures(1, &depth_texId_);
    }

    tex_width_  = width;
    tex_height_ = height;
    vid_out_format_ = format;

    utility::JsonStore j;
    j["pack_rgb_10_bit"] = format == viewport::RGBA_10_10_10_2;
    viewport_renderer_->set_aux_shader_uniforms(j);

    // create texture
    glGenTextures(1, &texId_);
    glBindTexture(GL_TEXTURE_2D, texId_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format_to_gl_tex_format[vid_out_format_],
        tex_width_,
        tex_height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_SHORT,
        nullptr);

    GLint iTexFormat;
    glGetTexLevelParameteriv ( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &iTexFormat);
    if (iTexFormat != format_to_gl_tex_format[vid_out_format_]) {
        spdlog::warn("{} offscreen viewport texture internal format is {:#x}, which does not match desired format {:#x}",
            __PRETTY_FUNCTION__,
            iTexFormat,
            format_to_gl_tex_format[vid_out_format_]);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    {

        glGenTextures(1, &depth_texId_);
        glBindTexture(GL_TEXTURE_2D, depth_texId_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        // NULL means reserve texture memory, but texels are undefined
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            tex_width_,
            tex_height_,
            0,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_BYTE,
            nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    // init framebuffer
    glGenFramebuffers(1, &fboId_);
    // bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texId_, 0);
}

void OffscreenViewport::renderToImageBuffer(
    const int w, const int h, media_reader::ImageBufPtr &image, const viewport::ImageFormat format)
{
    auto t0 = utility::clock::now();

    // ensure our GLContext is current
    gl_context_->makeCurrent(surface_);
    if (!gl_context_->isValid()) {
        throw std::runtime_error(
            "OffscreenViewport::renderToImageBuffer - GL Context is not valid.");
    }

    setupTextureAndFrameBuffer(w, h, format);

    // intialises shaders and textures where necessary
    viewport_renderer_->init();

    auto t1 = utility::clock::now();

    // Clearup before render, probably useless for a new buffer
    //glClearColor(0.0, 1.0, 0.0, 0.0);
    //glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    // This essential call tells the viewport renderer how to project the
    // viewport area into the glViewport window.
    viewport_renderer_->set_scene_coordinates(
        Imath::V2f(0.0f, 0.0),
        Imath::V2f(w, 0.0),
        Imath::V2f(w, h),
        Imath::V2f(0.0f, h),
        Imath::V2i(w, h),
        1.0f);

    viewport_renderer_->render();

    // Not sure if this is necessary
    //glFinish();

    auto t2 = utility::clock::now();

    // unbind
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    size_t pix_buf_size = w*h*format_to_bytes_per_pixel[vid_out_format_];

    // init RGBA float array
    image->allocate(pix_buf_size);
    image->set_image_dimensions(Imath::V2i(w, h));
    image.when_to_display_ = utility::clock::now();
    image->params()["pixel_format"] = (int)format;
    
    if (!pixel_buffer_object_) {
        glGenBuffers(1, &pixel_buffer_object_);
    }

    if (pix_buf_size != pix_buf_size_) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_buffer_object_);
        glBufferData(GL_PIXEL_PACK_BUFFER, pix_buf_size, NULL, GL_STREAM_COPY);
        pix_buf_size_ = pix_buf_size;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId_);

    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, w);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    auto t3 = utility::clock::now();
        
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGetTexImage(GL_TEXTURE_2D,
            0,
            GL_RGBA,
            format_to_gl_pixe_type[vid_out_format_],
            nullptr);
    

    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_buffer_object_);
    void* mappedBuffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    auto t4 = utility::clock::now();
    
    threaded_memcpy(
        image->buffer(),
        mappedBuffer,
        pix_buf_size,
        8
        );


    //now mapped buffer contains the pixel data
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    auto t5 = utility::clock::now();

    auto tt = utility::clock::now();
    /*std::cerr << "glBindBuffer " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count() << " " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << " " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count() << " "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count() << " "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t5-t4).count() << " : "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t5-t0).count() << "\n";*/

}


void OffscreenViewport::receive_change_notification(
    Viewport::ChangeCallbackId id) {

    if (id == Viewport::ChangeCallbackId::Redraw) {

        if (video_output_actor_) {

            std::vector<media_reader::ImageBufPtr> output_buffers_;
            media_reader::ImageBufPtr ready_buf;
            for (auto &buf : output_buffers_) {
                if (buf.use_count() == 1) {
                    ready_buf = buf;
                    break;
                }
            }
            if (!ready_buf) {
                ready_buf.reset(new media_reader::ImageBuffer());
                output_buffers_.push_back(ready_buf);
            }

            renderToImageBuffer(vid_out_width_, vid_out_height_, ready_buf, vid_out_format_);
            anon_send(video_output_actor_, ready_buf);
        }
    }
}

void OffscreenViewport::make_conversion_lut() {

    if (half_to_int_32_lut_.empty()) {
        const double int_max = double(std::numeric_limits<uint32_t>::max());
        half_to_int_32_lut_.resize(1 << 16);
        for (size_t i = 0; i < (1 << 16); ++i) {
            half h;
            h.setBits(i);
            half_to_int_32_lut_[i] =
                uint32_t(round(std::max(0.0, std::min(1.0, double(h))) * int_max));
        }
    }
}

thumbnail::ThumbnailBufferPtr
OffscreenViewport::rgb96thumbFromHalfFloatImage(const media_reader::ImageBufPtr &image) {

    const Imath::V2i image_size = image->image_size_in_pixels();

    // since we only run this routine ourselves and set-up the image properly
    // this mismatch can't happen but check anyway just in case. Due to padding
    // image buffers are usually a bit larger than the tight pixel size.
    size_t expected_size = image_size.x * image_size.y * sizeof(half) * 4;
    if (expected_size > image->size()) {

        std::string err(fmt::format(
            "{} Image buffer size of {} does not agree with image pixels size of {} ({}x{}).",
            __PRETTY_FUNCTION__,
            image->size(),
            expected_size,
            image_size.x,
            image_size.y));
        throw std::runtime_error(err.c_str());
    }


    // init RGBA float array
    thumbnail::ThumbnailBufferPtr r(
        new thumbnail::ThumbnailBuffer(image_size.x, image_size.y, thumbnail::TF_RGBF96));

    // note 'image' is (probably) already in a display space. The offscreen
    // viewport has its own instance of ColourPipeline plugin doing the colour
    // management. So our colours are normalised to 0-1 range.

    make_conversion_lut();

    const half *in = (half *)image->buffer();
    float *out     = (float *)r->data().data();
    size_t sz      = image_size.x * image_size.y;
    while (sz--) {
        *(out++) = *(in++);
        *(out++) = *(in++);
        *(out++) = *(in++);
        in++; // skip alpha
    }

    r->flip();

    return r;
}

thumbnail::ThumbnailBufferPtr OffscreenViewport::renderToThumbnail(
    const thumbnail::THUMBNAIL_FORMAT format,
    const int width,
    const bool auto_scale,
    const bool show_annotations) {

    media_reader::ImageBufPtr image = viewport_renderer_->get_onscreen_image();

    if (!image) {
        std::string err(fmt::format(
            "{} Failed to pull images to offscreen renderer.", __PRETTY_FUNCTION__));
        throw std::runtime_error(err.c_str());
    }

    const Imath::V2i image_dims = image->image_size_in_pixels();
    if (image_dims.x <= 0 || image_dims.y <= 0) {
        std::string err(fmt::format("{} Null image in viewport.", __PRETTY_FUNCTION__));
        throw std::runtime_error(err.c_str());
    }

    float effective_image_height = float(image_dims.y) / image->pixel_aspect();

    if (width <= 0 || auto_scale) {
        viewport_renderer_->set_fit_mode(viewport::FitMode::One2One);
        return renderToThumbnail(format, image_dims.x, int(round(effective_image_height)));
    } else {
        viewport_renderer_->set_fit_mode(viewport::FitMode::Best);
        return renderToThumbnail(
            format, width, int(round(width * effective_image_height / image_dims.x)));
    }
}

thumbnail::ThumbnailBufferPtr OffscreenViewport::renderToThumbnail(
    const thumbnail::THUMBNAIL_FORMAT format, const int width, const int height) {
    media_reader::ImageBufPtr image(new media_reader::ImageBuffer());
    renderToImageBuffer(width, height, image, viewport::ImageFormat::RGBA_16F);
    thumbnail::ThumbnailBufferPtr r = rgb96thumbFromHalfFloatImage(image);
    r->convert_to(format);
    return r;
}


thumbnail::ThumbnailBufferPtr OffscreenViewport::renderMediaFrameToThumbnail(
    caf::actor media_actor,
    const int media_frame,
    const thumbnail::THUMBNAIL_FORMAT format,
    const int width,
    const bool auto_scale,
    const bool show_annotations) {
    if (!local_playhead_) {
        auto a = caf::actor_cast<caf::event_based_actor *>(as_actor());
        local_playhead_ =
            a->spawn<playhead::PlayheadActor>("Offscreen Viewport Local Playhead");
        a->link_to(local_playhead_);
    }
    // first, set the local playhead to be our image source
    viewport_renderer_->set_playhead(local_playhead_);

    scoped_actor sys{as_actor()->home_system()};

    // now set the media source on the local playhead
    utility::request_receive<bool>(
        *sys, local_playhead_, playhead::source_atom_v, std::vector<caf::actor>({media_actor}));

    // now move the playhead to requested frame
    utility::request_receive<bool>(*sys, local_playhead_, playhead::jump_atom_v, media_frame);

    return renderToThumbnail(format, auto_scale, show_annotations);
}
