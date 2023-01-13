// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDebug>
#include <QQuickWidget>
#include <QQmlError>

class QMLViewport : public QQuickWidget {
    Q_OBJECT

  public:
    QMLViewport(QWidget *parent = nullptr);
    ~QMLViewport() override = default;

    void resizeEvent(QResizeEvent *event) override;
};