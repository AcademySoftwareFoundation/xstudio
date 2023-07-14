// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/qml_viewport_renderer.hpp"
// leave space or clang format will cause problems

#include "xstudio/ui/qml/playhead_ui.hpp"
#include "xstudio/ui/qml/qml_viewport.hpp"
#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qt/offscreen_viewport.hpp"

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QOffscreenSurface>
CAF_POP_WARNINGS

using namespace xstudio::ui::qml;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui;
using namespace xstudio::ui::qt;


namespace {

int qtModifierToOurs(const Qt::KeyboardModifiers qt_modifiers) {
    int result = Signature::Modifier::NoModifier;

    if (qt_modifiers & Qt::ShiftModifier)
        result |= Signature::Modifier::ShiftModifier;
    if (qt_modifiers & Qt::ControlModifier)
        result |= Signature::Modifier::ControlModifier;
    if (qt_modifiers & Qt::AltModifier)
        result |= Signature::Modifier::AltModifier;
    if (qt_modifiers & Qt::MetaModifier)
        result |= Signature::Modifier::MetaModifier;
    if (qt_modifiers & Qt::KeypadModifier)
        result |= Signature::Modifier::KeypadModifier;
    if (qt_modifiers & Qt::GroupSwitchModifier)
        result |= Signature::Modifier::GroupSwitchModifier;

    return result;
}

/*PointerEvent makePointerEvent(Signature::EventType t, QWheelEvent *event) {

        return PointerEvent(
                        Signature::EventType::MouseWheel,
                        static_cast<Signature::Button>((int)event->buttons()),
                        event->x(),
                        event->y(),
                        qtModifierToOurs(event->modifiers()),
                        std::make_pair(event->angleDelta().rx(), event->angleDelta().ry()),
                        std::make_pair(event->pixelDelta().rx(), event->pixelDelta().ry())
                        );

}*/

} // namespace

qt::OffscreenViewport *QMLViewport::offscreen_viewport_ = nullptr;


QMLViewport::QMLViewport(QQuickItem *parent) : QQuickItem(parent), cursor_(Qt::ArrowCursor) {

    playhead_ = new PlayheadUI(this);
    playhead_->init(CafSystemObject::get_actor_system());

    connect(this, &QQuickItem::windowChanged, this, &QMLViewport::handleWindowChanged);
    static int index = 0;
    viewport_index_  = index++;
    renderer_actor   = new QMLViewportRenderer(static_cast<QObject *>(this), viewport_index_);
    connect(renderer_actor, SIGNAL(zoomChanged(float)), this, SIGNAL(zoomChanged(float)));
    connect(
        renderer_actor,
        SIGNAL(fpsChanged(QString)),
        this,
        SIGNAL(fpsExpressionChanged(QString)));
    connect(renderer_actor, SIGNAL(scaleChanged(float)), this, SIGNAL(scaleChanged(float)));
    connect(
        renderer_actor,
        SIGNAL(translateChanged(QVector2D)),
        this,
        SIGNAL(translateChanged(QVector2D)));
    connect(
        renderer_actor,
        SIGNAL(onScreenFrameChanged(int)),
        this,
        SLOT(setOnScreenImageLogicalFrame(int)));
    connect(renderer_actor, SIGNAL(outOfRange(bool)), this, SLOT(setFrameOutOfRange(bool)));

    connect(
        renderer_actor,
        SIGNAL(zoomChanged(float)),
        this,
        SIGNAL(imageBoundaryInViewportChanged()));
    connect(
        renderer_actor,
        SIGNAL(translateChanged(QVector2D)),
        this,
        SIGNAL(imageBoundaryInViewportChanged()));
    connect(
        renderer_actor,
        SIGNAL(scaleChanged(float)),
        this,
        SIGNAL(imageBoundaryInViewportChanged()));

    connect(
        renderer_actor,
        SIGNAL(noAlphaChannelChanged(bool)),
        this,
        SLOT(setNoAlphaChannel(bool)));

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    if (!offscreen_viewport_) {
        try {
            offscreen_viewport_ =
                new xstudio::ui::qt::OffscreenViewport(static_cast<QObject *>(this));
        } catch (std::exception &e) {
            spdlog::debug("Unable to create offscreen viewport renderer: {}", e.what());
        }
    }
}

void QMLViewport::handleWindowChanged(QQuickWindow *win) {
    spdlog::debug("QMLViewport::handleWindowChanged");
    if (win) {
        // Send screen info for the first time
        QScreen *screen = win->screen();
        this->handleScreenChanged(screen);

        connect(
            win,
            &QQuickWindow::beforeSynchronizing,
            this,
            &QMLViewport::sync,
            Qt::DirectConnection);
        connect(
            win,
            &QQuickWindow::sceneGraphInvalidated,
            this,
            &QMLViewport::cleanup,
            Qt::DirectConnection);
        connect(
            win,
            &QQuickWindow::frameSwapped,
            renderer_actor,
            &QMLViewportRenderer::frameSwapped,
            Qt::DirectConnection);

        connect(renderer_actor, &QMLViewportRenderer::doRedraw, win, &QQuickWindow::update);

        connect(
            win,
            &QQuickWindow::screenChanged,
            this,
            &QMLViewport::handleScreenChanged,
            Qt::DirectConnection);

        renderer_actor->setWindow(win);

        // This is crucial - we draw the viewport before QML renders to the screen.
        // If we don't turn off clear then the viewport gets wiped
        win->setClearBeforeRendering(false);
        m_window = win;
    }
}

void QMLViewport::handleScreenChanged(QScreen *screen) {

    spdlog::debug("QMLViewport::handleScreenChanged");
    renderer_actor->setScreenInfos(
        screen->name(),
        screen->model(),
        screen->manufacturer(),
        screen->serialNumber(),
        screen->refreshRate());
}

PointerEvent
QMLViewport::makePointerEvent(Signature::EventType t, QMouseEvent *event, int force_modifiers) {

    PointerEvent p(
        t,
        static_cast<Signature::Button>((int)event->buttons()),
        event->x(),
        event->y(),
        width(),  // FIXME should be width, but this function appears to never be called.
        height(), // FIXME should be height
        qtModifierToOurs(event->modifiers()) + force_modifiers,
        fmt::format("viewport{0}", viewport_index_));
    p.w_ = utility::clock::now();
    return p;
}

PointerEvent QMLViewport::makePointerEvent(
    Signature::EventType t, int buttons, int x, int y, int w, int h, int modifiers) {

    return PointerEvent(
        t,
        static_cast<Signature::Button>(buttons),
        x,
        y,
        w,
        h,
        modifiers,
        fmt::format("viewport{0}", viewport_index_));
}

static QOpenGLContext *__aa = nullptr;
static QOpenGLContext *__bb = nullptr;

void QMLViewport::sync() {

    // TODO: this is a little clunky. Can we do this somewhere else?
    if (!connected_) {
        connect(
            window(),
            &QQuickWindow::beforeRendering,
            renderer_actor,
            &QMLViewportRenderer::paint,
            Qt::DirectConnection);
        connected_ = true;
    }

    if (!window() || !renderer_actor)
        return;

    // Tell the renderer the viewport coordinates. These are the 4 corners of the viewport
    // within the overall GL viewport,
    renderer_actor->setSceneCoordinates(
        mapToScene(boundingRect().topLeft()),
        mapToScene(boundingRect().topRight()),
        mapToScene(boundingRect().bottomRight()),
        mapToScene(boundingRect().bottomLeft()),
        window()->size());

    /*static bool share = false;
    if (window() && !share) {
        // //qDebug() << this << " OGL CONTEXT " << window()->openglContext() << "\n";
        if (window()->openglContext()) {
            if (!__aa)
                __aa = window()->openglContext();
            if (__aa && __aa != window()->openglContext()) {
                __bb = window()->openglContext();
                window()->openglContext()->setShareContext(__aa);
                share = true;
            }
            //qDebug() << this << " SHARE " << window()->openglContext()->shareContext() <<
    "\n";
        }
    }*/
}

void QMLViewport::cleanup() {
    if (renderer_actor) {
        // delete renderer_actor;
        renderer_actor->deleteLater();
        renderer_actor = nullptr;
    }
    if (offscreen_viewport_) {
        offscreen_viewport_->deleteLater();
        renderer_actor = nullptr;
    }
}

void QMLViewport::mousePressEvent(QMouseEvent *event) {

    mouse_position = event->pos();
    emit(mouseChanged());
    mouse_buttons = event->buttons();
    emit(mouseButtonsChanged());

    event->accept();

    if (renderer_actor) {
        renderer_actor->pointerEvent(makePointerEvent(Signature::EventType::ButtonDown, event));
    }
}

void QMLViewport::mouseReleaseEvent(QMouseEvent *event) {

    mouse_position = event->pos();
    emit(mouseChanged());
    mouse_buttons = event->buttons();
    emit(mouseButtonsChanged());

    event->accept();

    if (renderer_actor) {
        renderer_actor->pointerEvent(
            makePointerEvent(Signature::EventType::ButtonRelease, event));
    }
}

void QMLViewport::hoverMoveEvent(QHoverEvent *event) {

    mouse_position = event->pos();

    emit(mouseChanged());

    if (renderer_actor) {

        renderer_actor->pointerEvent(makePointerEvent(
            Signature::EventType::Move,
            0,
            event->pos().x(),
            event->pos().y(),
            width(),
            height(),
            0));
    }
    QQuickItem::hoverMoveEvent(event);
}

void QMLViewport::mouseMoveEvent(QMouseEvent *event) {

    mouse_position = event->pos();

    emit(mouseChanged());

    if (renderer_actor) {
        renderer_actor->pointerEvent(makePointerEvent(
            event->buttons() ? Signature::EventType::Drag : Signature::EventType::Move,
            event,
            0));
    }
    update();
    // event->ignore();
    // QQuickItem::mouseMoveEvent(event);
}

void QMLViewport::mouseDoubleClickEvent(QMouseEvent *event) {

    mouse_position = event->pos();
    emit(mouseChanged());
    if (renderer_actor) {
        renderer_actor->pointerEvent(
            makePointerEvent(Signature::EventType::DoubleClick, event));
    }
}

void QMLViewport::keyPressEvent(QKeyEvent *key_event) {

    if (key_event) {
        // I don't think we need this, as key events are forwarded in
        // QMLViewport::event
        // renderer_actor->rawKeyDown(key_event->key(), key_event->isAutoRepeat());
    }
    renderer_actor->keyboardTextEntry(key_event->text());
}

void QMLViewport::keyReleaseEvent(QKeyEvent *key_event) {

    if (key_event && !key_event->isAutoRepeat()) {
        renderer_actor->rawKeyUp(key_event->key());
    }
}

bool QMLViewport::event(QEvent *event) {

    // qDebug() << event;

    /* This is where keyboard events are captured and sent to the backend!! */
    if (event->type() == QEvent::KeyPress) {
        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event) {
            renderer_actor->rawKeyDown(key_event->key(), key_event->isAutoRepeat());
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event && !key_event->isAutoRepeat()) {
            renderer_actor->rawKeyUp(key_event->key());
        }
    } else if (
        event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave ||
        event->type() == QEvent::DragLeave || event->type() == QEvent::GraphicsSceneDragLeave ||
        event->type() == QEvent::GraphicsSceneHoverLeave) {
        // It's possible to receive a KeyPress but not key release if the mouse
        // leaves the window/widget before the user releases the key. This is
        // a headache for us because we need to track key pressed state, not
        // only key down. We have to assume all keys are released (even if they
        // aren't) when we the mouse leaves the focus as we won't get the
        // mouse release event. It doesn't matter if we have false negatives
        // (in terms of whether a key is held down) but a false positive
        // (i.e. we think a key is down when it isn't) could really mess up the
        // UI behaviour
        renderer_actor->allKeysUp();
    }
    return QQuickItem::event(event);
}

float QMLViewport::zoom() {
    if (renderer_actor)
        return renderer_actor->zoom();
    return 1.0f;
}

void QMLViewport::setZoom(const float z) {
    if (renderer_actor)
        renderer_actor->setZoom(z);
}

void QMLViewport::revertFitZoomToPrevious(const bool ignoreOtherViewport) {
    if (renderer_actor)
        renderer_actor->revertFitZoomToPrevious();
    window()->update();
}

QString QMLViewport::fpsExpression() { return renderer_actor->fpsExpression(); }

float QMLViewport::scale() { return renderer_actor->scale(); }

QVector2D QMLViewport::translate() { return renderer_actor->translate(); }

QObject *QMLViewport::playhead() { return static_cast<QObject *>(playhead_); }

void QMLViewport::setOnScreenImageLogicalFrame(const int frame_num) {
    if (frame_num != on_screen_logical_frame_) {
        on_screen_logical_frame_ = frame_num;
        emit onScreenImageLogicalFrameChanged();
    }
}

void QMLViewport::setScale(const float s) { renderer_actor->setScale(s); }

void QMLViewport::setTranslate(const QVector2D &t) { renderer_actor->setTranslate(t); }

void QMLViewport::setColourUnderCursor(const QVector3D &c) {

    colour_under_cursor = QStringList(
        {QString("%1").arg(c.x(), 3, 'f', 3, '0'),
         QString("%1").arg(c.y(), 3, 'f', 3, '0'),
         QString("%1").arg(c.z(), 3, 'f', 3, '0')});
    emit(colourUnderCursorChanged());
}

void QMLViewport::wheelEvent(QWheelEvent *event) {

    // make a mouse wheel event and pass to viewport to process
    PointerEvent ev(
        Signature::EventType::MouseWheel,
        static_cast<Signature::Button>((int)event->buttons()),
        event->position().x(),
        event->position().y(),
        width(),  // FIXME should be width, but this function appears to never be called.
        height(), // FIXME should be height
        qtModifierToOurs(event->modifiers()),
        fmt::format("viewport{0}", viewport_index_),
        std::make_pair(event->angleDelta().rx(), event->angleDelta().ry()),
        std::make_pair(event->pixelDelta().rx(), event->pixelDelta().ry()));

    if (!renderer_actor->pointerEvent(ev) && playhead_) {
        // If viewport hasn't acted on the mouse wheel event (because user pref
        // to zoom with mouse wheel is false), assume that we can instead use it
        // to step the playhead
        playhead_->setPlaying(false);
        playhead_->step(event->angleDelta().ry() > 0 ? 1 : -1);
    }

    QQuickItem::wheelEvent(event);
}

void QMLViewport::setPlayhead(caf::actor playhead) {
    spdlog::debug("QMLViewport::setPlayhead");
    playhead_->set_backend(playhead);
}

void QMLViewport::hideCursor() {
    if (!cursor_hidden) {
        setCursor(QCursor(Qt::BlankCursor));
    }
}

void QMLViewport::showCursor() {
    if (cursor_hidden) {
        cursor_hidden = false;
        setCursor(cursor_);
    }
}

QSize QMLViewport::imageResolution() {
    Imath::V2i resolution = renderer_actor->imageResolutionCoords();
    return QSize(resolution.x, resolution.y);
}

QRectF QMLViewport::imageBoundaryInViewport() {
    QRectF r = renderer_actor->imageBoundsInViewportPixels();
    return QRectF(
        r.x() * width(), r.y() * height(), r.width() * width(), r.height() * height());
}

QVector2D QMLViewport::bboxCornerInViewport(const int min_x, const int min_y) {
    Imath::V2f corner_in_viewport = renderer_actor->imageCoordsToViewport(min_x, min_y);
    return QVector2D(corner_in_viewport.x * width(), corner_in_viewport.y * height());
}

void QMLViewport::setFrameOutOfRange(bool frame_out_of_range) {
    if (frame_out_of_range != frame_out_of_range_) {
        frame_out_of_range_ = frame_out_of_range;
        emit frameOutOfRangeChanged();
    }
}

void QMLViewport::setNoAlphaChannel(bool no_alpha_channel) {
    if (no_alpha_channel != no_alpha_channel_) {
        no_alpha_channel_ = no_alpha_channel;
        emit noAlphaChannelChanged();
    }
}

class CleanupJob : public QRunnable {
  public:
    CleanupJob(QMLViewportRenderer *renderer) : m_renderer(renderer) {}
    void run() override { delete m_renderer; }

  private:
    QMLViewportRenderer *m_renderer;
};

void QMLViewport::releaseResources() {
    spdlog::debug("QMLViewport::releaseResources");
    window()->scheduleRenderJob(
        new CleanupJob(renderer_actor), QQuickWindow::BeforeSynchronizingStage);
    renderer_actor = nullptr;
}

QString QMLViewport::renderImageToFile(
    const QUrl filePath,
    const int format,
    const int compression,
    const int width,
    const int height,
    const bool bakeColor) {

    if (!offscreen_viewport_) {
        return QString("Offscreen viewport renderer was not found.");
    }

    QString error_message;
    try {

        offscreen_viewport_->renderSnapshot(
            playhead_->backend(), width, height, compression, bakeColor, UriFromQUrl(filePath));

        spdlog::info(
            "Snapshot successfully generated: {}",
            xstudio::utility::uri_to_posix_path(UriFromQUrl(filePath)));

    } catch (std::exception &e) {

        error_message = QStringFromStd(e.what());
    }
    return error_message;
}


void QMLViewport::sendResetShortcut() {
    /* This emulates keyboard events for Ctrl+R (viewer reset), and is called from QML when
     * clicking the menu option */
    auto *eventOn1 = new QKeyEvent(QEvent::KeyPress, Qt::Key_Control, Qt::NoModifier, "ctrl");
    auto *eventOn2 = new QKeyEvent(QEvent::KeyPress, Qt::Key_R, Qt::NoModifier, "r");

    auto *eventOff1 =
        new QKeyEvent(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier, "ctrl");
    auto *eventOff2 = new QKeyEvent(QEvent::KeyRelease, Qt::Key_R, Qt::NoModifier, "r");

    // Send press events, Ctrl then R
    qApp->postEvent((QObject *)this, (QEvent *)eventOn1);
    qApp->postEvent((QObject *)this, (QEvent *)eventOn2);

    // Send release events, R then Ctrl
    qApp->postEvent((QObject *)this, (QEvent *)eventOff2);
    qApp->postEvent((QObject *)this, (QEvent *)eventOff1);
}

void QMLViewport::setOverrideCursor(const QString &name, const bool centerOffset) {

    if (name != "") {
        this->setCursor(QCursor(
            QPixmap(name).scaledToWidth(32, Qt::SmoothTransformation),
            centerOffset ? -1 : 0,
            centerOffset ? -1 : 0));
    } else {
        this->setCursor(cursor_);
    }
}

void QMLViewport::setOverrideCursor(const Qt::CursorShape cname) {

    this->setCursor(QCursor(cname));
}

void QMLViewport::setRegularCursor(const Qt::CursorShape cname) {

    cursor_ = QCursor(cname);
    this->setCursor(cursor_);
}

QString QMLViewport::name() const { return renderer_actor->name(); }