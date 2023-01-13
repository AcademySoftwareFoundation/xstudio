// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDebug>
#include <QWindow>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QThread>

// Forward declaration
namespace xstudio {
namespace ui {
    namespace qt {
        class ViewportGLWidget;
    }
} // namespace ui
} // namespace xstudio

class Thread;

class ThreadedViewportWindow : public QWindow, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit ThreadedViewportWindow(QWindow *parent = nullptr);

    void initialize();
    void render();
    void draw();

  protected:
    bool eventFilter(QObject *obj, QEvent *event);

  private:
    Thread *m_thread;
    QOpenGLContext *m_context;

    GLuint m_pbo;
    GLuint m_texture;

    int m_index;

    xstudio::ui::qt::ViewportGLWidget *viewport_widget_;

    std::vector<unsigned char> m_frames[3];
};
