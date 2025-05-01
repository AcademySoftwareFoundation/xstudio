// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDebug>
#include <QQuickWidget>
#include <QQmlError>

class PySideQmlViewport : public QQuickWidget {
    Q_OBJECT

  public:
    PySideQmlViewport(QWidget *parent = nullptr);
    ~PySideQmlViewport() override = default;

  public slots:
    QString name();
};