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

namespace fs = std::filesystem;


/* This actor allows other actors to interact with the OffscreenViewport in
the regular request().then() pattern, which is otherwise not possible with
QObject/caf::actor mixin class which normally can't have message handlers that
have a return value */
class OffscreenViewportMiddlemanActor : public caf::event_based_actor {
  public:
    OffscreenViewportMiddlemanActor(caf::actor_config &cfg, caf::actor offscreen_viewport)
        : caf::event_based_actor(cfg), offscreen_viewport_(std::move(offscreen_viewport)) {

        system().registry().put(offscreen_viewport_registry, this);

        behavior_.assign(
            // incoming request from somewhere in xstudio for a screen render
            [=](viewport::render_viewport_to_image_atom,
                caf::actor playhead,
                const int width,
                const int height,
                const int compression,
                const bool bakeColor,
                const caf::uri path) -> result<bool> {
                auto rp = make_response_promise<bool>();
                request(
                    offscreen_viewport_,
                    infinite,
                    viewport::render_viewport_to_image_atom_v,
                    playhead,
                    width,
                    height,
                    compression,
                    bakeColor,
                    path)
                    .then(
                        [=](bool r) mutable { rp.deliver(r); },
                        [=](caf::error &err) mutable { rp.deliver(err); });
                return rp;
            },
            [=](viewport::render_viewport_to_image_atom,
                caf::actor media_actor,
                const int media_frame,
                const int width,
                const int height,
                const caf::uri path) -> result<bool> {
                auto rp = make_response_promise<bool>();
                render_to_file(media_actor, media_frame, width, height, path, rp);
                return rp;
            },
            [=](viewport::render_viewport_to_image_atom,
                caf::actor media_actor,
                const int media_frame,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool auto_scale,
                const bool show_annotations) -> result<thumbnail::ThumbnailBufferPtr> {
                auto rp = make_response_promise<thumbnail::ThumbnailBufferPtr>();
                render_to_thumbail(
                    rp, media_actor, media_frame, format, width, auto_scale, show_annotations);
                return rp;
            });
    }

    ~OffscreenViewportMiddlemanActor() override { offscreen_viewport_ = caf::actor(); }

    void render_to_thumbail(
        caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp,
        caf::actor media_actor,
        const int media_frame,
        const thumbnail::THUMBNAIL_FORMAT format,
        const int width,
        const bool auto_scale,
        const bool show_annotations);

    void render_to_file(
        caf::actor media_actor,
        const int media_frame,
        const int width,
        const int height,
        const caf::uri path,
        caf::typed_response_promise<bool> rp);

    caf::behavior behavior_;
    caf::actor offscreen_viewport_;

    caf::behavior make_behavior() override { return behavior_; }
};

OffscreenViewport::OffscreenViewport(QObject *parent) : super(parent) {

    /*thread_ = new QThread(this);
    moveToThread(thread_);*/

    /*thread_ = new QThread(this);
    moveToThread(thread_);*/

    // This class is a QObject with a caf::actor 'companion' that allows it
    // to receive and send caf messages - here we run necessary initialisation
    // of the companion actor
    super::init(xstudio::ui::qml::CafSystemObject::get_actor_system());

    scoped_actor sys{xstudio::ui::qml::CafSystemObject::get_actor_system()};
    middleman_ = sys->spawn<OffscreenViewportMiddlemanActor>(as_actor());

    // Now we create our OpenGL xSTudio viewport - this has 'Viewport(Module)' as
    // its base class that provides various caf message handlers that are added
    // to our companion actor's 'behaviour' to create a fully functioning
    // viewport that can receive caf messages including framebuffers and also
    // to render the viewport into our GLContext
    static int offscreen_idx = -1;
    utility::JsonStore jsn;
    jsn["base"] = utility::JsonStore();
    viewport_renderer_.reset(new ui::viewport::Viewport(
        jsn,
        as_actor(),
        offscreen_idx--,
        ui::viewport::ViewportRendererPtr(new opengl::OpenGLViewportRenderer(true, false))));

    // Here we set-up the caf message handler for this class by combining the
    // message handler from OpenGLViewportRenderer with our own message handlers for offscreen
    // rendering
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return viewport_renderer_->message_handler().or_else(caf::message_handler{
            // insert additional message handlers here
            [=](viewport::render_viewport_to_image_atom,
                caf::actor playhead,
                const int width,
                const int height,
                const int compression,
                const bool bakeColor,
                const caf::uri path) -> result<bool> {
                try {
                    renderSnapshot(playhead, width, height, compression, bakeColor, path);
                    return true;
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
            },

            [=](viewport::render_viewport_to_image_atom,
                caf::actor playhead,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool render_annotations,
                const bool fit_to_annotations_outside_image)
                -> result<thumbnail::ThumbnailBufferPtr> {
                try {
                    return renderToThumbnail(
                        playhead,
                        format,
                        width,
                        render_annotations,
                        fit_to_annotations_outside_image);
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
            }});
    });
}

OffscreenViewport::~OffscreenViewport() {

    caf::scoped_actor sys(self()->home_system());
    sys->send_exit(middleman_, caf::exit_reason::user_shutdown);
    middleman_ = caf::actor();
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

        gl_context_ =
            new QOpenGLContext(static_cast<QObject *>(this)); // m_window->openglContext();
        gl_context_->setFormat(format);
        if (!gl_context_)
            throw std::runtime_error(
                "OffscreenViewport::initGL - could not create QOpenGLContext.");
        if (!gl_context_->create()) {
            throw std::runtime_error("OffscreenViewport::initGL - failed to creat GL Context "
                                     "for offscreen rendering.");
        }

        // we also require a QSurface to use the GL context
        surface_ = new QOffscreenSurface(nullptr, static_cast<QObject *>(this));
        surface_->setFormat(format);
        surface_->create();

        gl_context_->makeCurrent(surface_);
    }
}

thumbnail::ThumbnailBufferPtr OffscreenViewport::renderToThumbnail(
    caf::actor playhead,
    const thumbnail::THUMBNAIL_FORMAT format,
    const int width,
    const bool render_annotations,
    const bool fit_to_annotations_outside_image) {

    initGL();
    media_reader::ImageBufPtr image = viewport_renderer_->get_image_from_playhead(playhead);

    const Imath::V2i image_dims = image->image_size_in_pixels();
    if (image_dims.x <= 0 || image_dims.y <= 0) {
        throw std::runtime_error("On screen image is null.");
    }
    viewport_renderer_->update_fit_mode_matrix(
        image_dims.x, image_dims.y, image->pixel_aspect());
    viewport_renderer_->set_fit_mode(viewport::FitMode::One2One);

    const int x_size = image_dims.x;
    const int y_size = (int)round(float(image_dims.y) * image->pixel_aspect());

    thumbnail::ThumbnailBufferPtr r = renderOffscreen(x_size, y_size, image);

    if (width > 0)
        r->bilin_resize(width, (y_size * width) / x_size);

    r->convert_to(format);

    return r;
}

void OffscreenViewport::renderSnapshot(
    caf::actor playhead,
    const int width,
    const int height,
    const int compression,
    const bool bakeColor,
    const caf::uri path) {

    initGL();

    media_reader::ImageBufPtr image = viewport_renderer_->get_image_from_playhead(playhead);
    // temp hack - put in a 500ms delay so the playhead can update the
    // annotations plugin with the annotations data.
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (path.empty()) {
        throw std::runtime_error("Invalid (empty) file path.");
    }

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid image dimensions.");
    }

    thumbnail::ThumbnailBufferPtr r = renderOffscreen(width, height, image);

    auto p = fs::path(xstudio::utility::uri_to_posix_path(path));

    std::string ext = xstudio::utility::ltrim_char(
        xstudio::utility::to_upper(p.extension()),
        '.'); // yuk!

    if (ext == "EXR") {
        this->exportToEXR(r, path);
    } else {
        this->exportToCompressedFormat(r, path, compression, ext);
    }
}

void OffscreenViewport::exportToEXR(thumbnail::ThumbnailBufferPtr r, const caf::uri path) {
    std::unique_ptr<Imf::Rgba> buf(new Imf::Rgba[r->height() * r->width()]);
    Imf::Rgba *tbuf = buf.get();

    // m_image.convertTo(QImage::Format_RGBA64);
    auto *ff = (float *)r->data().data();
    int px   = r->height() * r->width();
    while (px--) {
        tbuf->r = *(ff++);
        tbuf->g = *(ff++);
        tbuf->b = *(ff++);
        tbuf->a = 1.0f;
        tbuf++;
    }

    Imf::Header header;
    header.dataWindow() = header.displayWindow() =
        Imath::Box2i(Imath::V2i(0, 0), Imath::V2i(r->width() - 1, r->height() - 1));
    header.compression() = Imf::PIZ_COMPRESSION;
    Imf::RgbaOutputFile outFile(utility::uri_to_posix_path(path).c_str(), header);
    outFile.setFrameBuffer(buf.get(), 1, r->width());
    outFile.writePixels(r->height());
}

void OffscreenViewport::exportToCompressedFormat(
    thumbnail::ThumbnailBufferPtr r,
    const caf::uri path,
    int compression,
    const std::string &ext) {

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

    int compLevel =
        ext == "TIF" || ext == "TIFF" ? std::max(compression, 1) : (10 - compression) * 10;
    // TODO : check m_filePath for extension, if not, add to it. Do it on QML side after merging
    // with new UI branch

    QImageWriter writer(xstudio::utility::uri_to_posix_path(path).c_str());
    writer.setCompression(compLevel);
    if (!writer.write(im)) {
        throw std::runtime_error(writer.errorString().toStdString().c_str());
    }
}

thumbnail::ThumbnailBufferPtr OffscreenViewport::renderOffscreen(
    const int w, const int h, const media_reader::ImageBufPtr &image) {
    // ensure our GLContext is current
    gl_context_->makeCurrent(surface_);
    if (!gl_context_->isValid()) {
        throw std::runtime_error(
            "OffscreenViewport::renderOffscreen - GL Context is not valid.");
    }

    // intialises shaders and textures where necessary
    viewport_renderer_->init();

    unsigned int texId, depth_texId;
    unsigned int fboId;

    // create texture
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    {

        glGenTextures(1, &depth_texId);
        glBindTexture(GL_TEXTURE_2D, depth_texId);
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
            w,
            h,
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
    glGenFramebuffers(1, &fboId);
    // bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texId, 0);

    // Clearup before render, probably useless for a new buffer
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    // This essential call tells the viewport renderer how to project the
    // viewport area into the glViewport window.
    viewport_renderer_->set_scene_coordinates(
        Imath::V2f(0.0f, 0.0),
        Imath::V2f(w, 0.0),
        Imath::V2f(w, h),
        Imath::V2f(0.0f, h),
        Imath::V2i(w, h));

    viewport_renderer_->render(image);

    // Not sure if this is necessary
    glFinish();

    // init RGBA float array
    thumbnail::ThumbnailBufferPtr r(new thumbnail::ThumbnailBuffer(w, h, thumbnail::TF_RGBF96));

    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, w);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // read GL pixels to array
    glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, r->data().data());
    glFinish();

    // unbind and delete
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &texId);
    glDeleteFramebuffers(1, &fboId);
    glDeleteTextures(1, &depth_texId);


    // Thumbanil coord system has y=0 at top of image, whereas GL viewport is
    // y=0 at bottom.
    r->flip();

    return r;
}

void OffscreenViewportMiddlemanActor::render_to_thumbail(
    caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp,
    caf::actor media_actor,
    const int media_frame,
    const thumbnail::THUMBNAIL_FORMAT format,
    const int width,
    const bool auto_scale,
    const bool show_annotations) {
    caf::actor playhead_actor;
    try {

        scoped_actor sys{system()};

        // make a temporary playhead
        playhead_actor = sys->spawn<playhead::PlayheadActor>("Offscreen Viewport Playhead");

        // set the incoming media actor as the source for the playhead
        utility::request_receive<bool>(
            *sys,
            playhead_actor,
            playhead::source_atom_v,
            std::vector<caf::actor>({media_actor}));

        // set the playhead frame
        utility::request_receive<bool>(
            *sys, playhead_actor, playhead::jump_atom_v, media_frame);

        // TODO: remove this and find
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // send a request to the offscreen viewport to
        // render an image
        request(
            offscreen_viewport_,
            infinite,
            viewport::render_viewport_to_image_atom_v,
            playhead_actor,
            format,
            width,
            auto_scale,
            show_annotations)
            .then(
                [=](thumbnail::ThumbnailBufferPtr buf) mutable {
                    rp.deliver(buf);
                    send_exit(playhead_actor, caf::exit_reason::user_shutdown);
                },
                [=](caf::error &err) mutable {
                    rp.deliver(err);
                    send_exit(playhead_actor, caf::exit_reason::user_shutdown);
                });

        // add

    } catch (std::exception &e) {
        if (playhead_actor)
            send_exit(playhead_actor, caf::exit_reason::user_shutdown);
        rp.deliver(caf::make_error(xstudio_error::error, e.what()));
    }
}

void OffscreenViewportMiddlemanActor::render_to_file(
    caf::actor media_actor,
    const int media_frame,
    const int width,
    const int height,
    const caf::uri path,
    caf::typed_response_promise<bool> rp) {

    caf::actor playhead_actor;
    try {

        scoped_actor sys{system()};

        // make a temporary playhead
        playhead_actor = sys->spawn<playhead::PlayheadActor>("Offscreen Viewport Playhead");

        // set the incoming media actor as the source for the playhead
        utility::request_receive<bool>(
            *sys,
            playhead_actor,
            playhead::source_atom_v,
            std::vector<caf::actor>({media_actor}));

        // set the playhead frame
        utility::request_receive<bool>(
            *sys, playhead_actor, playhead::jump_atom_v, media_frame);

        // TODO: remove this and find
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // send a request to the offscreen viewport to
        // render an image
        request(
            offscreen_viewport_,
            infinite,
            viewport::render_viewport_to_image_atom_v,
            playhead_actor,
            width,
            height,
            10,
            true,
            path)
            .then(
                [=](bool r) mutable {
                    rp.deliver(r);
                    send_exit(playhead_actor, caf::exit_reason::user_shutdown);
                },
                [=](caf::error &err) mutable {
                    rp.deliver(err);
                    send_exit(playhead_actor, caf::exit_reason::user_shutdown);
                });

        // add

    } catch (std::exception &e) {
        if (playhead_actor)
            send_exit(playhead_actor, caf::exit_reason::user_shutdown);
        rp.deliver(caf::make_error(xstudio_error::error, e.what()));
    }
}