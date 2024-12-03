// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/qml_viewport_renderer.hpp"
#include "xstudio/ui/qml/qml_viewport.hpp"
#include "xstudio/media_reader/media_reader.hpp"

#include <QOpenGLContext>

using namespace xstudio::ui;
using namespace xstudio::ui::qml;
using namespace xstudio::ui::viewport;
using namespace xstudio;

namespace {
static int ctt = 0;
} // namespace


// N.B. we don't pass in 'parent' as the parent of the base class. The owner
// of this class must schedule its destruction directly rather than rely on
// Qt object child destruction.
QMLViewportRenderer::QMLViewportRenderer(QObject *parent)
    : QMLActor(nullptr), m_window(nullptr) {

    viewport_qml_item_ = dynamic_cast<QMLViewport *>(parent);
    init_system();
}

QMLViewportRenderer::~QMLViewportRenderer() { 

    delete viewport_renderer_; 

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

    if (viewport_qml_item_ && viewport_qml_item_->isVisible() && viewport_renderer_) {

        // TODO: again, this init call probably shouldn't happen in the main
        // draw call. see above.

        if (!init_done) {
            init_done = true;
            init_renderer();
        }
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        viewport_renderer_->render();
        glPopClientAttrib();

        m_window->resetOpenGLState();
        if (viewport_renderer_->playing()) {
            emit(doRedraw());
        }
    }
}

void QMLViewportRenderer::frameSwapped() {
    if (viewport_renderer_)
        viewport_renderer_->framebuffer_swapped(utility::clock::now());
}

void QMLViewportRenderer::setWindow(QQuickWindow *window) {

    if (window && m_window != window) {
        m_window = window;

        // window has changed - we need a new xstudio viewport
        make_xstudio_viewport();
    }
}

void QMLViewportRenderer::setSceneCoordinates(
    const QPointF topleft,
    const QPointF topright,
    const QPointF bottomright,
    const QPointF bottomleft,
    const QSize sceneSize,
    const float devicePixelRatio) {

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
            Imath::V2i(sceneSize.width(), sceneSize.height()),
            devicePixelRatio);
    }
}

void QMLViewportRenderer::prepareRenderData() {
    if (viewport_renderer_) viewport_renderer_->prepare_render_data();
}

void QMLViewportRenderer::init_system() {
    QMLActor::init(CafSystemObject::get_actor_system());

    spdlog::debug("QMLViewportRenderer init");

    self()->set_default_handler(caf::drop);
}

void QMLViewportRenderer::make_xstudio_viewport() {

    if (viewport_renderer_)
        delete viewport_renderer_;

    utility::JsonStore jsn;
    jsn["base"]      = utility::JsonStore();
    jsn["window_id"] = StdFromQString(m_window->objectName());

    /* Here we create the all important Viewport class that actually draws images to the screen
     */
    viewport_renderer_ = new ui::viewport::Viewport(jsn, as_actor());

    viewport_renderer_->set_visibility(viewport_qml_item_->isVisible());

    /* Provide a callback so the Viewport can tell this class when some property of the viewport
    has changed and such events can be propagated to other QT components, for example */
    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };
    viewport_renderer_->set_change_callback(callback);

    viewport_qml_item_->setPlayheadUuid(
                QUuidFromUuid(viewport_renderer_->playhead_uuid()));
                
    /* The Viewport object provides a message handler that will process update events like new
    frame buffers coming from the playhead and so-on. Instead of being an actor itself, the
    Viewport is a regular class but it will process messages received by a parent actor (like
    this QMLViewportRenderer class) as long as the messages are received in the same thread
    which Viewport::render() is also called. In this case the thread is the QT main event loop
    thread because this class is a caf::mixing/QObject combo that ensures messages are received
    through QTs event loop thread rather than a regular caf thread.*/
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return caf::message_handler(
                   [=](quickview_media_atom,
                       const std::vector<utility::UuidActor> &media_items,
                       const std::string &compare_mode) {
                       // the viewport has been sent a quickview request message - we
                       // use qsignal to pass this up to the QMLViewport object as
                       // a repeated signal that can be used in the QML engine to
                       // execute the necessary QML code to launch a new light viewer
                       // to show the media
                       QStringList media;
                       for (auto &media_item : media_items) {
                           media.append(
                               QStringFromStd(actorToString(system(), media_item.actor())));
                       }
                       emit quickViewBackendRequest(media, QStringFromStd(compare_mode));
                   },
                   [=](quickview_media_atom,
                       const std::vector<utility::UuidActor> &media_items,
                       const std::string &compare_mode,
                       const int x_position,
                       const int y_position,
                       const int x_size,
                       const int y_size) {
                       // the viewport has been sent a quickview request message - we
                       // use qsignal to pass this up to the QMLViewport object as
                       // a repeated signal that can be used in the QML engine to
                       // execute the necessary QML code to launch a new light viewer
                       // to show the media
                       QStringList media;
                       for (auto &media_item : media_items) {
                           media.append(
                               QStringFromStd(actorToString(system(), media_item.actor())));
                       }
                       emit quickViewBackendRequestWithSize(
                           media,
                           QStringFromStd(compare_mode),
                           QPoint(x_position, y_position),
                           QSize(x_size, y_size));
                   },
                   [=](viewport::render_viewport_to_image_atom,
                       std::string snapshotRenderResult) {
                       emit snapshotRequestResult(QStringFromStd(snapshotRenderResult));
                   }

                   )
            .or_else(viewport_renderer_->message_handler());
    });

    /* The global KeypressMonitor filters out system auto repeat when the holder presses and
    holds a key down, and sends messages back to the viewport only once when a key is pressed
    and released */
    keypress_monitor_ = system().registry().template get<caf::actor>(keyboard_events);

}
void QMLViewportRenderer::set_playhead(caf::actor playhead) {
    if (viewport_renderer_)
        viewport_renderer_->set_playhead(playhead);
}

void QMLViewportRenderer::reset() {
    if (viewport_renderer_)
        viewport_renderer_->reset();
}

bool QMLViewportRenderer::pointerEvent(const PointerEvent &e) {

    // make a mutable copy, so we can add more coordinate info in
    // 'process_pointer_event'
    PointerEvent _e = e;
    if (viewport_renderer_ && viewport_renderer_->process_pointer_event(_e)) {
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

QVariantList QMLViewportRenderer::imageResolutions() const {
    QVariantList v;
    const auto & image_resolutions = viewport_renderer_->image_resolutions();
    for (const auto &r: image_resolutions) {        
        v.append(QSize(r.x ,r.y));
    }
    return v;
}

QVariantList QMLViewportRenderer::imageBoundariesInViewport() const {

    QVariantList v;
    const std::vector<Imath::Box2f> image_boxes = viewport_renderer_->image_bounds_in_viewport_pixels();
    for (const auto &box: image_boxes) {
        QRectF imageBoundsInViewportPixels(
            box.min.x,
            box.min.y,
            box.max.x - box.min.x,
            box.max.y - box.min.y);
        v.append(imageBoundsInViewportPixels);
    }
    return v;
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

void QMLViewportRenderer::quickViewSource(
    QStringList mediaActors, QString compareMode, int in_pt, int out_pt) {

    std::vector<caf::actor> media;
    for (const auto &media_actor_as_string : mediaActors) {
        caf::actor media_actor =
            actorFromString(system(), StdFromQString(media_actor_as_string));
        if (media_actor) {
            media.push_back(media_actor);
        }
    }
    if (!media.empty()) {
        anon_send(
            self(), quickview_media_atom_v, media, StdFromQString(compareMode), in_pt, out_pt);
    }
}

void QMLViewportRenderer::receive_change_notification(Viewport::ChangeCallbackId id) {

    if (id == Viewport::ChangeCallbackId::Redraw) {
        m_window->update();
    } else if (id == Viewport::ChangeCallbackId::TranslationChanged) {
        emit translationChanged();
    } else if (id == Viewport::ChangeCallbackId::PlayheadChanged) {
        if (viewport_qml_item_ && viewport_renderer_) {
            viewport_qml_item_->setPlayheadUuid(
                QUuidFromUuid(viewport_renderer_->playhead_uuid()));
        }
    } else if (id == Viewport::ChangeCallbackId::ImageBoundsChanged) {
        emit translationChanged();
    } else if (id == Viewport::ChangeCallbackId::ImageResolutionsChanged) {
        emit resolutionsChanged();
    }
}

void QMLViewportRenderer::setScreenInfos(
    QString name,
    QString model,
    QString manufacturer,
    QString serialNumber,
    double refresh_rate) {
    if (viewport_renderer_)
        viewport_renderer_->set_screen_infos(
            name.toStdString(),
            model.toStdString(),
            manufacturer.toStdString(),
            serialNumber.toStdString(),
            refresh_rate);
}

void QMLViewportRenderer::setIsQuickViewer(const bool is_quick_viewer) {}

void QMLViewportRenderer::visibleChanged(const bool is_visible) {
    if (viewport_renderer_)
        viewport_renderer_->set_visibility(is_visible);
}
