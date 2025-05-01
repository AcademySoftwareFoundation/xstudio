// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDebug>
#include <QQuickWidget>
#include <QQmlError>

class XstudioPyApp : public QObject {
    Q_OBJECT

  public:
    XstudioPyApp(QWidget *parent = nullptr);
    ~XstudioPyApp();
};