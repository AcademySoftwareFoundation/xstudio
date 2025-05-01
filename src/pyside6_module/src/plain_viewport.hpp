// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDebug>
#include <QWidget>
#include <QOpenGLWidget>

// Forward declaration
namespace xstudio {
namespace ui {
    namespace qt {
        class ViewportGLWidget;
    }
} // namespace ui
} // namespace xstudio

class PlainViewport : public QWidget {
    Q_OBJECT

  public:
    PlainViewport(QWidget *parent, const QString window_id);

    ~PlainViewport() override;

    void resizeEvent(QResizeEvent *event) override;

  public slots:

    QString name();

  private:
    xstudio::ui::qt::ViewportGLWidget *viewport_widget_;
};