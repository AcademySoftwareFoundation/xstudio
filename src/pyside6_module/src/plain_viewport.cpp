// SPDX-License-Identifier: Apache-2.0
#include "xstudio/global/xstudio_actor_system.hpp" //NOLINT
#include "xstudio/ui/qml/helper_ui.hpp"            //NOLINT
#include "xstudio/ui/qt/viewport_widget.hpp"       //NOLINT

CAF_PUSH_WARNINGS
#include "plain_viewport.hpp"
#include <QVBoxLayout>
CAF_POP_WARNINGS

using namespace xstudio;

PlainViewport::PlainViewport(QWidget *parent, const QString window_id) : QWidget(parent) {

    // Create the xstudio viewport widget/actor
    viewport_widget_ = new ui::qt::ViewportGLWidget(this, true, window_id);
    viewport_widget_->init(CafActorSystem::system());

    QVBoxLayout *layout = new QVBoxLayout((QWidget *)this);
    layout->addWidget(viewport_widget_, 1);
}

PlainViewport::~PlainViewport() {}

void PlainViewport::resizeEvent(QResizeEvent *event) {

    viewport_widget_->resizeGL(event->size().width(), event->size().height());
}

QString PlainViewport::name() { return viewport_widget_->name(); }
