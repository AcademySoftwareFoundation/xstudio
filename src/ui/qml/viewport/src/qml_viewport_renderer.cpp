// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/qml_viewport_renderer.hpp"
#include "xstudio/ui/qml/qml_viewport.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/ui/qml/playhead_ui.hpp"

#include <QOpenGLContext>

using namespace xstudio::ui;
using namespace xstudio::ui::qml;
using namespace xstudio::ui::viewport;
using namespace xstudio;

namespace {} // namespace

QMLViewportRenderer::QMLViewportRenderer(QObject *parent, const int viewport_index)
    : QMLActor(parent), m_window(nullptr), viewport_index_(viewport_index) {
    init_system();
}

static QQuickWindow *win = nullptr;

void QMLViewportRenderer::init_renderer() {
    // note: as it stands this is not called when I expect (on first draw)
    // hence our own gl init happens on first call to paint.

    QSGRendererInterface *rif = m_window->rendererInterface();
    Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

    auto qq = dynamic_cast<QQuickWindow *>(m_window);
    if (qq) {
        if (!win) {
            win = qq;
        } else if (win && qq) {
            // std::cerr << "Are sharing " << QOpenGLContext::areSharing(win->openglContext(),
            // qq->openglContext()) << "\n";
        }
    }
}

void QMLViewportRenderer::paint() {

    // TODO: again, this init call probably shouldn't happen in the main
    // draw call. see above.

    if (!init_done) {
        init_done = true;
        init_renderer();
    }

    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    viewport_renderer_->render();
    glPopClientAttrib();

    // TODO: this is in the wrong place, we should check this *before* the
    // redraw
    const QRectF bounds = imageBoundsInViewportPixels();
    if (bounds != imageBounds_) {
        imageBounds_ = bounds;
        emit(zoomChanged(viewport_renderer_->pixel_zoom()));
    }

    m_window->resetOpenGLState();
    if (viewport_renderer_->playing()) {
        emit(doRedraw());
    }
}

void QMLViewportRenderer::frameSwapped() { viewport_renderer_->framebuffer_swapped(); }

void QMLViewportRenderer::setWindow(QQuickWindow *window) { m_window = window; }

void QMLViewportRenderer::setSceneCoordinates(
    const QPointF topleft,
    const QPointF topright,
    const QPointF bottomright,
    const QPointF bottomleft,
    const QSize sceneSize) {

    // this is called on every draw, as Qt does not provide a suitable
    // signal to detect when the viewport coordinates in the top level
    // Qml item changes. viewport_coords_.set returns true only when
    // these values have changed since the last call

    if (viewport_coords_.set(topleft, topright, bottomright, bottomleft, sceneSize)) {

        anon_send(
            self(),
            viewport_set_scene_coordinates_atom_v,
            Imath::V2f(topleft.x(), topleft.y()),
            Imath::V2f(topright.x(), topright.y()),
            Imath::V2f(bottomright.x(), bottomright.y()),
            Imath::V2f(bottomleft.x(), bottomleft.y()),
            Imath::V2i(sceneSize.width(), sceneSize.height()));
    }
}

void QMLViewportRenderer::init_system() {
    QMLActor::init(CafSystemObject::get_actor_system());

    spdlog::debug("QMLViewportRenderer init");

    self()->set_default_handler(caf::drop);

    utility::JsonStore jsn;
    jsn["base"] = utility::JsonStore();

    /* Here we create the all important Viewport class that actually draws images to the screen
     */
    viewport_renderer_.reset(new ui::viewport::Viewport(
        jsn,
        as_actor(),
        viewport_index_,
        ui::viewport::ViewportRendererPtr(
            new opengl::OpenGLViewportRenderer(viewport_index_, false))));

    /* Provide a callback so the Viewport can tell this class when some property of the viewport
    has changed and such events can be propagated to other QT components, for example */
    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };
    viewport_renderer_->set_change_callback(callback);

    // update PlayheadUI object owned by the 'QMLViewport'
    auto *vp = dynamic_cast<QMLViewport *>(parent());
    if (vp) {
        vp->setPlayhead(viewport_renderer_->playhead());
    }

    /* The Viewport object provides a message handler that will process update events like new
    frame buffers coming from the playhead and so-on. Instead of being an actor itself, the
    Viewport is a regular class but it will process messages received by a parent actor (like
    this QMLViewportRenderer class) as long as the messages are received in the same thread
    which Viewport::render() is also called. In this case the thread is the QT main event loop
    thread because this class is a caf::mixing/QObject combo that ensures messages are received
    through QTs event loop thread rather than a regular caf thread.*/
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return caf::message_handler(/*messagehandlers here*/)
            .or_else(viewport_renderer_->message_handler());
    });

    /* The global KeypressMonitor filters out system auto repeat when the holder presses and
    holds a key down, and sends messages back to the viewport only once when a key is pressed
    and released */
    keypress_monitor_ = system().registry().template get<caf::actor>(keyboard_events);
}

void QMLViewportRenderer::set_playhead(PlayheadUI *playhead) {

    spdlog::debug("QMLViewportRenderer::set_playhead");
    viewport_renderer_->set_playhead(playhead ? playhead->backend() : caf::actor());
}

bool QMLViewportRenderer::pointerEvent(const PointerEvent &e) {

    // make a mutable copy, so we can add more coordinate info in
    // 'process_pointer_event'
    PointerEvent _e = e;
    if (viewport_renderer_->process_pointer_event(_e)) {
        // pointer event will be consumed if the user is doing interactive
        // pan/zoom, for example. Force a redraw.
        if (m_window)
            m_window->update();
        return true; // pointer event was used
    } else {
        // viewport did not consume pointer event, forward it on to the
        // mouse/leyboard event manager
        anon_send(keypress_monitor_, ui::keypress_monitor::mouse_event_atom_v, _e);
    }
    return false; // pointer event not used
}

float QMLViewportRenderer::zoom() { return viewport_renderer_->pixel_zoom(); }

void QMLViewportRenderer::setZoom(const float f) {
    anon_send(self(), viewport_pixel_zoom_atom_v, f);
}

Imath::V2i QMLViewportRenderer::imageResolutionCoords() {
    return viewport_renderer_->image_resolution();
}

QRectF QMLViewportRenderer::imageBoundsInViewportPixels() const {
    Imath::Box2f box = viewport_renderer_->image_bounds_in_viewport_pixels();
    return QRectF(box.min.x, box.min.y, box.max.x - box.min.x, box.max.y - box.min.y);
}

void QMLViewportRenderer::revertFitZoomToPrevious() {
    viewport_renderer_->revert_fit_zoom_to_previous();
}

Imath::V2f QMLViewportRenderer::imageCoordsToViewport(const int x, const int y) {
    return viewport_renderer_->image_coordinate_to_viewport_coordinate(x, y);
}

bool QMLViewportRenderer::ViewportCoords::set(
    const QPointF &a, const QPointF &b, const QPointF &c, const QPointF &d, const QSize &s) {
    if (corners[0] == a && corners[1] == b && corners[2] == c && corners[3] == d && size == s) {
        // coordinates are unchanged
        return false;
    }

    corners[0] = a;
    corners[1] = b;
    corners[2] = c;
    corners[3] = d;
    size       = s;
    return true;
}

void QMLViewportRenderer::rawKeyDown(const int key, const bool auto_repeat) {
    // key events from QT are passed from the QMLViewport to this class, which sends a caf
    // message to itself qhich is handled in the message handlers provided by Viewport object.
    // The viewport passes the message to a KeyPressMonitor actor, which uses some simple timing
    // to filter out key events that are auto-repeats generated by the system when the user
    // holds a key down. The goal is that we get one message coming back to this class when the
    // key is pressed and one when it is released and all auto-repeat keypress events are eaten.
    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::key_down_atom_v,
        key,
        viewport_renderer_->name(),
        auto_repeat);
}

void QMLViewportRenderer::keyboardTextEntry(const QString text) {
    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::text_entry_atom_v,
        text.toStdString(),
        viewport_renderer_->name());
}

void QMLViewportRenderer::rawKeyUp(const int key) {
    // see above
    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::key_up_atom_v,
        key,
        viewport_renderer_->name());
}

void QMLViewportRenderer::allKeysUp() {
    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::all_keys_up_atom_v,
        viewport_renderer_->name());
}

void QMLViewportRenderer::setScale(const float s) {
    anon_send(self(), viewport_scale_atom_v, s);
}

void QMLViewportRenderer::setTranslate(const QVector2D &t) {
    anon_send(self(), viewport_pan_atom_v, t.x(), t.y());
}

float QMLViewportRenderer::scale() { return viewport_renderer_->scale(); }

QVector2D QMLViewportRenderer::translate() {
    return QVector2D(viewport_renderer_->pan().x, viewport_renderer_->pan().y);
}

void QMLViewportRenderer::receive_change_notification(Viewport::ChangeCallbackId id) {

    if (id == Viewport::ChangeCallbackId::Redraw) {
        m_window->update();
    } else if (id == Viewport::ChangeCallbackId::ZoomChanged) {
        emit zoomChanged(zoom());
    } else if (id == Viewport::ChangeCallbackId::ScaleChanged) {
        emit scaleChanged(scale());
    } else if (id == Viewport::ChangeCallbackId::FrameRateChanged) {
        fps_expression_ = QStringFromStd(viewport_renderer_->frame_rate_expression());
        emit fpsChanged(fps_expression_);
    } else if (id == Viewport::ChangeCallbackId::OutOfRangeChanged) {
        emit outOfRange(viewport_renderer_->frame_out_of_range());
    } else if (id == Viewport::ChangeCallbackId::OnScreenFrameChanged) {
        emit onScreenFrameChanged(viewport_renderer_->on_screen_frame());
    } else if (id == Viewport::ChangeCallbackId::TranslationChanged) {
        emit translateChanged(
            QVector2D(viewport_renderer_->pan().x, viewport_renderer_->pan().y));
    } else if (id == Viewport::ChangeCallbackId::PlayheadChanged) {
        auto *vp = dynamic_cast<QMLViewport *>(parent());
        if (vp) {
            vp->setPlayhead(viewport_renderer_->playhead());
        }
    } else if (id == Viewport::ChangeCallbackId::NoAlphaChannelChanged) {
        emit noAlphaChannelChanged(viewport_renderer_->no_alpha_channel());
    }
}

void QMLViewportRenderer::setScreenInfos(
    QString name,
    QString model,
    QString manufacturer,
    QString serialNumber,
    double refresh_rate) {
    viewport_renderer_->set_screen_infos(
        name.toStdString(),
        model.toStdString(),
        manufacturer.toStdString(),
        serialNumber.toStdString(),
        refresh_rate);
}