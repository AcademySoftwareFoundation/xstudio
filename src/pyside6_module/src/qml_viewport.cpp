// SPDX-License-Identifier: Apache-2.0
#include "xstudio/global/xstudio_actor_system.hpp" //NOLINT
#include "xstudio/ui/qml/studio_ui.hpp"            //NOLINT

CAF_PUSH_WARNINGS
#include "qml_viewport.hpp"
#include <QVBoxLayout>
CAF_POP_WARNINGS

using namespace xstudio;

PySideQmlViewport::PySideQmlViewport(QWidget *parent) : QQuickWidget(parent) {

    ui::qml::setup_xstudio_qml_emgine(engine(), CafActorSystem::system());

    setResizeMode(QQuickWidget::SizeRootObjectToView);

    const QUrl url(QStringLiteral("qrc:/views/viewport/XsViewportWidget.qml"));
    setSource(url);
}

QString PySideQmlViewport::name() { return rootObject()->property("name").toString(); }
