// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include <caf/actor_registry.hpp>

CAF_PUSH_WARNINGS
#include <QMouseEvent>
#include <QWheelEvent>
CAF_POP_WARNINGS

using namespace xstudio::ui::qt;
using namespace xstudio::ui::qml;
using namespace xstudio::ui;

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

} // namespace

ViewportGLWidget::ViewportGLWidget(
    QWidget *parent,
    const bool live_viewport,
    const QString window_name,
    const QString viewport_name)
    : super(parent),
      live_viewport_(live_viewport),
      window_name_(StdFromQString(window_name)),
      viewport_name_(StdFromQString(viewport_name)) {

    if (live_viewport) {
        QObject::connect(
            this, &QOpenGLWidget::frameSwapped, this, &ViewportGLWidget::frameBufferSwapped);
        setFocusPolicy(Qt::StrongFocus);
    }
}

ViewportGLWidget::~ViewportGLWidget() { keypress_monitor_ = caf::actor(); }

void ViewportGLWidget::set_playhead(caf::actor playhead) {
    if (the_viewport_)
        the_viewport_->set_playhead(playhead);
}

void ViewportGLWidget::initializeGL() { the_viewport_->init(); }

void ViewportGLWidget::resizeGL(int w, int h) {

    anon_mail(
        ui::viewport::viewport_set_scene_coordinates_atom_v,
        0.0f,
        0.0f,
        float(w),
        float(h),
        float(w),
        float(h),
        1.0f)
        .send(self());
}

void ViewportGLWidget::paintGL() {

    std::cerr << the_viewport_->name() << " ";

    if (live_viewport_) {
        // if we're NOT a live viewport (e.g. offscreen viewport for rendering
        // thumbnails/snapshots) then prepare_render_data has already been
        // called with the appropriate image buffer etc. For a live viewport
        // e.g. in an embedded UI we must call prepare_render_data before each
        // redraw to fetch the image(s) from the playhead that will be drawn
        the_viewport_->prepare_render_data();
    }
    the_viewport_->render();

    // during playback we request redraw in a tight loop. The reason is that to
    // achieve frame accurate sync with correct pulldown to the system video
    // refresh beat xSTUDIO needs to perform a redraw on every video refresh
    if (the_viewport_->playing())
        update();
}

void ViewportGLWidget::frameBufferSwapped() {
    if (live_viewport_) {
        // this call is crucial for stable playback - we tell the playback engine
        // when the last image was put on the screen so it can infer the video
        // refresh beat and work out which image should be put on screen at the
        // next redraw
        the_viewport_->framebuffer_swapped(utility::clock::now());
    }
}


void ViewportGLWidget::receive_change_notification(viewport::Viewport::ChangeCallbackId) {
    update();
}

void ViewportGLWidget::init(caf::actor_system &system) {

    super::init(system);

    utility::JsonStore jsn;
    jsn["base"]      = utility::JsonStore();
    jsn["window_id"] = window_name_;

    the_viewport_.reset(new ui::viewport::Viewport(jsn, as_actor()));

    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };

    the_viewport_->set_change_callback(callback);

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return the_viewport_->message_handler();
    });

    keypress_monitor_ = system.registry().template get<caf::actor>(keyboard_events);
}

QString ViewportGLWidget::name() { return QString::fromUtf8(the_viewport_->name().c_str()); }

void ViewportGLWidget::mousePressEvent(QMouseEvent *event) {

    sendPointerEvent(EventType::ButtonDown, event);
}

void ViewportGLWidget::mouseReleaseEvent(QMouseEvent *event) {

    sendPointerEvent(EventType::ButtonRelease, event);
}

void ViewportGLWidget::mouseMoveEvent(QMouseEvent *event) {

    sendPointerEvent(event->buttons() ? EventType::Drag : EventType::Move, event, 0);
}

void ViewportGLWidget::mouseDoubleClickEvent(QMouseEvent *event) {

    sendPointerEvent(EventType::DoubleClick, event);
}

void ViewportGLWidget::keyPressEvent(QKeyEvent *key_event) {

    anon_mail(
        ui::keypress_monitor::text_entry_atom_v,
        StdFromQString(key_event->text()),
        the_viewport_->name(),
        window_name_)
        .send(keypress_monitor_);
}

void ViewportGLWidget::keyReleaseEvent(QKeyEvent *key_event) {

    if (!key_event->isAutoRepeat()) {
        anon_mail(
            ui::keypress_monitor::key_up_atom_v,
            key_event->key(),
            the_viewport_->name(),
            window_name_)
            .send(keypress_monitor_);
    }
}

bool ViewportGLWidget::event(QEvent *event) {

    if (event->type() == QEvent::KeyPress) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event) {
            anon_mail(
                ui::keypress_monitor::key_down_atom_v,
                key_event->key(),
                the_viewport_->name(),
                window_name_,
                key_event->isAutoRepeat())
                .send(keypress_monitor_);
        }
    } else if (event->type() == QEvent::KeyRelease) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event && !key_event->isAutoRepeat()) {
            anon_mail(
                ui::keypress_monitor::key_up_atom_v,
                key_event->key(),
                the_viewport_->name(),
                window_name_)
                .send(keypress_monitor_);
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
        anon_mail(ui::keypress_monitor::all_keys_up_atom_v, the_viewport_->name())
            .send(keypress_monitor_);
    }

    return super::event(event);
}

void ViewportGLWidget::wheelEvent(QWheelEvent *event) {

    // make a mouse wheel event and pass to viewport to process
    PointerEvent ev(
        EventType::MouseWheel,
        static_cast<Signature::Button>((int)event->buttons()),
        event->position().x(),
        event->position().y(),
        width(),  // FIXME should be width, but this function appears to never be called.
        height(), // FIXME should be height
        qtModifierToOurs(event->modifiers()),
        the_viewport_->name(),
        std::make_pair(event->angleDelta().rx(), event->angleDelta().ry()),
        std::make_pair(event->pixelDelta().rx(), event->pixelDelta().ry()));

    if (!the_viewport_->process_pointer_event(ev) && the_viewport_->playhead()) {
        // If viewport hasn't acted on the mouse wheel event (because user pref
        // to zoom with mouse wheel is false), assume that we can instead use it
        // to step the playhead
        anon_mail(playhead::play_atom_v, false).send(the_viewport_->playhead());
        anon_mail(playhead::step_atom_v, event->angleDelta().ry() > 0 ? 1 : -1)
            .send(the_viewport_->playhead());
    }

    QOpenGLWidget::wheelEvent(event);
}

void ViewportGLWidget::sendPointerEvent(EventType t, QMouseEvent *event, int force_modifiers) {

    PointerEvent p(
        t,
        static_cast<Signature::Button>((int)event->buttons()),
        event->position().x(),
        event->position().y(),
        width(),
        height(),
        qtModifierToOurs(event->modifiers()) + force_modifiers,
        the_viewport_->name());
    p.w_ = utility::clock::now();

    if (the_viewport_->process_pointer_event(p)) {
        update();
    } else {
        anon_mail(ui::keypress_monitor::mouse_event_atom_v, p).send(keypress_monitor_);
    }
}

void ViewportGLWidget::sendPointerEvent(QHoverEvent *event) {

    PointerEvent p(
        EventType::Move,
        static_cast<Signature::Button>(0),
        event->position().x(),
        event->position().y(),
        width(),
        height(),
        Signature::Modifier::NoModifier,
        the_viewport_->name());
    p.w_ = utility::clock::now();

    if (the_viewport_->process_pointer_event(p)) {
        update();
    } else {
        anon_mail(ui::keypress_monitor::mouse_event_atom_v, p).send(keypress_monitor_);
    }
}
