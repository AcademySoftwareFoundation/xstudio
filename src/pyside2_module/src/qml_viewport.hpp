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

    void resizeEvent(QResizeEvent *event) override;
};