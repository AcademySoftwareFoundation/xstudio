// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/ui/qt/viewport_widget.hpp"

using namespace xstudio::ui::qt;

ViewportGLWidget::ViewportGLWidget(QWidget *parent) : super(parent) {}

void ViewportGLWidget::set_playhead(caf::actor playhead) {
    if (the_viewport_)
        the_viewport_->set_playhead(playhead);
}

void ViewportGLWidget::initializeGL() { the_viewport_->init(); }

void ViewportGLWidget::resizeGL(int w, int h) {
    anon_send(
        self(),
        ui::viewport::viewport_set_scene_coordinates_atom_v,
        Imath::V2f(0, 0),
        Imath::V2f(w, 0),
        Imath::V2f(w, h),
        Imath::V2f(0, h),
        Imath::V2i(w, h),
        1.0f);
}

void ViewportGLWidget::paintGL() { the_viewport_->render(); }

void ViewportGLWidget::receive_change_notification(viewport::Viewport::ChangeCallbackId) {
    update();
}

void ViewportGLWidget::init(caf::actor_system &system) {

    super::init(system);

    self()->set_default_handler(caf::drop);

    utility::JsonStore jsn;
    jsn["base"] = utility::JsonStore();

    the_viewport_.reset(new ui::viewport::Viewport(
        jsn,
        as_actor(),
        true,
        viewport::ViewportRendererPtr(new opengl::OpenGLViewportRenderer(true, false))));

    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };

    the_viewport_->set_change_callback(callback);

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return the_viewport_->message_handler();
    });
}
