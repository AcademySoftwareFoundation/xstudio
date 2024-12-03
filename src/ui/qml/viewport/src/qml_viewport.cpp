// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/qml_viewport_renderer.hpp"
// leave space or clang format will cause problems

#include "xstudio/ui/qml/qml_viewport.hpp"
#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/utility/logging.hpp"

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

QMLViewport::QMLViewport(QQuickItem *parent) : QQuickItem(parent), cursor_(Qt::ArrowCursor) {

    connect(this, &QQuickItem::windowChanged, this, &QMLViewport::handleWindowChanged);
    static int index = 0;

    renderer_actor = new QMLViewportRenderer(this);

    keypress_monitor_ = CafSystemObject::get_actor_system().registry().template get<caf::actor>(
        xstudio::keyboard_events);

    connect(
        renderer_actor,
        SIGNAL(translationChanged()),
        this,
        SIGNAL(imageBoundariesInViewportChanged()));

    connect(
        renderer_actor,
        SIGNAL(resolutionsChanged()),
        this,
        SIGNAL(imageResolutionsChanged()));

    connect(
        this,
        SIGNAL(quickViewSource(QStringList, QString, int, int)),
        renderer_actor,
        SLOT(quickViewSource(QStringList, QString, int, int)));

    connect(
        renderer_actor,
        SIGNAL(quickViewBackendRequest(QStringList, QString)),
        this,
        SIGNAL(quickViewBackendRequest(QStringList, QString)));

    connect(
        renderer_actor,
        SIGNAL(quickViewBackendRequestWithSize(QStringList, QString, QPoint, QSize)),
        this,
        SIGNAL(quickViewBackendRequestWithSize(QStringList, QString, QPoint, QSize)));

    connect(
        renderer_actor,
        SIGNAL(snapshotRequestResult(QString)),
        this,
        SIGNAL(snapshotRequestResult(QString)));

    connect(this, SIGNAL(visibleChanged()), this, SLOT(onVisibleChanged()));

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    renderer_actor->visibleChanged(isVisible());
}

QMLViewport::~QMLViewport() { delete renderer_actor; }

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
        renderer_actor->std_name());
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
        renderer_actor->std_name());
}

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
        window()->size(),
        window()->devicePixelRatio());

    renderer_actor->prepareRenderData();

}

void QMLViewport::cleanup() {

    spdlog::debug("QMLViewport::cleanup");
    if (renderer_actor) {
        // delete renderer_actor;
        delete renderer_actor;
        renderer_actor = nullptr;
    }
}

void QMLViewport::deleteRendererActor() {

    delete renderer_actor;
    renderer_actor = nullptr;
}

void QMLViewport::hoverEnterEvent(QHoverEvent *event) {

    emit pointerEntered();
    QQuickItem::hoverEnterEvent(event);
}

void QMLViewport::hoverLeaveEvent(QHoverEvent *event) {

    emit pointerExited();
    QQuickItem::hoverLeaveEvent(event);
}


void QMLViewport::mousePressEvent(QMouseEvent *event) {

    mouse_position = event->pos();
    emit(mousePositionChanged(event->pos()));
    emit mousePress(event->buttons());

    event->accept();

    if (renderer_actor) {
        renderer_actor->pointerEvent(makePointerEvent(Signature::EventType::ButtonDown, event));
    }
}

void QMLViewport::mouseReleaseEvent(QMouseEvent *event) {

    mouse_position = event->pos();
    emit(mousePositionChanged(event->pos()));
    emit mouseRelease(event->buttons());

    event->accept();

    if (renderer_actor) {
        renderer_actor->pointerEvent(
            makePointerEvent(Signature::EventType::ButtonRelease, event));
    }
}

void QMLViewport::hoverMoveEvent(QHoverEvent *event) {

    mouse_position = event->pos();
    emit(mousePositionChanged(event->pos()));

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

    emit(mousePositionChanged(event->pos()));

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
    emit(mousePositionChanged(event->pos()));
    emit mouseDoubleClick(event->buttons());

    if (renderer_actor) {
        renderer_actor->pointerEvent(
            makePointerEvent(Signature::EventType::DoubleClick, event));
    }
}

void QMLViewport::keyPressEvent(QKeyEvent *key_event) {

    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::text_entry_atom_v,
        StdFromQString(key_event->text()),
        renderer_actor->std_name(),
        StdFromQString(m_window->objectName()));
}

void QMLViewport::keyReleaseEvent(QKeyEvent *key_event) {

    if (!key_event->isAutoRepeat()) {
        anon_send(
            keypress_monitor_,
            ui::keypress_monitor::key_up_atom_v,
            key_event->key(),
            renderer_actor->std_name(),
            StdFromQString(m_window->objectName()));
    }
}

bool QMLViewport::event(QEvent *event) {

    // qDebug() << event;
    if (event->type() == QEvent::KeyPress) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event) {
            anon_send(
                keypress_monitor_,
                ui::keypress_monitor::key_down_atom_v,
                key_event->key(),
                renderer_actor->std_name(),
                StdFromQString(m_window->objectName()),
                key_event->isAutoRepeat());
        }
    } else if (event->type() == QEvent::KeyRelease) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event && !key_event->isAutoRepeat()) {
            anon_send(
                keypress_monitor_,
                ui::keypress_monitor::key_up_atom_v,
                key_event->key(),
                renderer_actor->std_name(),
                StdFromQString(m_window->objectName()));
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
        anon_send(
            keypress_monitor_,
            ui::keypress_monitor::all_keys_up_atom_v,
            renderer_actor->std_name());
    } else if (event->type() == QEvent::HoverEnter) {
        forceActiveFocus(Qt::MouseFocusReason);
    }
    return QQuickItem::event(event);
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
        renderer_actor->std_name(),
        std::make_pair(event->angleDelta().rx(), event->angleDelta().ry()),
        std::make_pair(event->pixelDelta().rx(), event->pixelDelta().ry()));

    if (!renderer_actor->pointerEvent(ev) && renderer_actor->playhead()) {
        // If viewport hasn't acted on the mouse wheel event (because user pref
        // to zoom with mouse wheel is false), assume that we can instead use it
        // to step the playhead
        anon_send(renderer_actor->playhead(), playhead::play_atom_v, false);
        anon_send(
            renderer_actor->playhead(),
            playhead::step_atom_v,
            event->angleDelta().ry() > 0 ? 1 : -1);
    }

    QQuickItem::wheelEvent(event);
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

QVariantList QMLViewport::imageResolutions() {
    return renderer_actor->imageResolutions();
}

QVariantList QMLViewport::imageBoundariesInViewport() {
    return renderer_actor->imageBoundariesInViewport();
}

class CleanupJob : public QRunnable {
  public:
    /* N.B. - we use a shared_ptr to manage the deletion of the viewport. The
    reason is that sometimes (on xstudio shotdown) the CleanupJob instance
    is created but run does NOT get executed. */
    CleanupJob(QMLViewportRenderer *vp) : renderer(vp) {}
    void run() override { renderer.reset(); }

  private:
    std::shared_ptr<QMLViewportRenderer> renderer;
};

void QMLViewport::releaseResources() {

    /* This is the recommended way to delete the object that manages OpenGL
    resources. Scheduling a render job means that it is run when the OpenGL
    context is valid and as such in the destructor of the ViewportRenderer
    we can do the appropriare release of OpenGL resources*/
    window()->scheduleRenderJob(
        new CleanupJob(renderer_actor), QQuickWindow::BeforeSynchronizingStage);
    renderer_actor = nullptr;
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

QString QMLViewport::name() const { return renderer_actor->name(); }

void QMLViewport::setPlayhead(const QString actorAddress) {

    if (actorAddress != "") {
        caf::actor playhead =
            actorFromQString(CafSystemObject::get_actor_system(), actorAddress);
        if (playhead) {
            renderer_actor->set_playhead(playhead);
        } else {
            spdlog::warn(
                "{} bad playhead actor address: {}",
                __PRETTY_FUNCTION__,
                StdFromQString(actorAddress));
        }
    }
}

void QMLViewport::reset() { renderer_actor->reset(); }

QString QMLViewport::playheadActorAddress() {

    return actorToQString(CafSystemObject::get_actor_system(), renderer_actor->playhead());
}

void QMLViewport::onVisibleChanged() {
    if (renderer_actor)
        renderer_actor->visibleChanged(isVisible());
}