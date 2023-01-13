// SPDX-License-Identifier: Apache-2.0
#define GL_GLEXT_PROTOTYPES

#include "xstudio/media_reader/media_reader.hpp"
#include "threaded_viewport.hpp"
#include <GL/gl.h>
#include <GL/glu.h>

#define WIDTH 2000
#define HEIGHT 1000
#define SIZE (3 * WIDTH * HEIGHT)

using namespace xstudio::ui::qt;

class Thread : public QThread {

  public:
    explicit Thread(ThreadedViewportWindow *parent = 0);

    void run() override;

    void start();
    void stop();

  private:
    bool m_running;
};


Thread::Thread(ThreadedViewportWindow *parent) : QThread((QObject *)parent), m_running(false) {}

void Thread::run() {
    auto window = dynamic_cast<ThreadedViewportWindow *>(parent());

    window->initialize();

    auto counter = 0;
    auto a       = std::chrono::system_clock::now();
    while (m_running) {
        window->render();
        ++counter;

        if (counter % 100 == 0) {
            auto b = std::chrono::system_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
            double fps = 1000.0 * counter / duration;
            std::cerr << "FPS: " << fps << std::endl;
        }
    }
}

void Thread::start() {
    m_running = true;
    QThread::start();
}

void Thread::stop() {
    m_running = false;
    QThread::wait();
}

ThreadedViewportWindow::ThreadedViewportWindow(QWindow *parent)
    : QWindow(parent), m_thread(new Thread(this)), m_context(nullptr), m_index(0) {
    setSurfaceType(QWindow::OpenGLSurface);
    installEventFilter(this);

    for (int i = 0; i < 3; ++i) {
        m_frames[i].resize(SIZE);
        for (int y = 0; y < HEIGHT; ++y) {
            auto value = int(255.0 * y / HEIGHT);
            for (int x = 0; x < WIDTH; ++x) {
                auto p = &m_frames[i][3 * (x + y * WIDTH)];
                p[i]   = value;
            }
        }
    }
}

void ThreadedViewportWindow::initialize() {
    if (m_context)
        return;

    m_context = new QOpenGLContext();
    m_context->setFormat(requestedFormat());
    m_context->create();
    m_context->makeCurrent(this);

    initializeOpenGLFunctions();

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &m_pbo);
}

void ThreadedViewportWindow::render() {
    if (!isExposed())
        return;

    m_context->makeCurrent(this);

    draw();

    m_context->swapBuffers(this);
}

void ThreadedViewportWindow::draw() {
    glViewport(0, 0, width(), height());

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, SIZE, 0, GL_STREAM_DRAW);

    void *mappedBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(mappedBuffer, &m_frames[m_index][0], SIZE);

    glBindTexture(GL_TEXTURE_2D, m_texture);

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glEnable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex2f(-1, -1);
    glTexCoord2f(1.0, 0.0);
    glVertex2f(+1, -1);
    glTexCoord2f(1.0, 1.0);
    glVertex2f(+1, +1);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(-1, +1);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_index = (m_index + 1) % 3;
}

bool ThreadedViewportWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Expose)
        m_thread->start();
    if (event->type() == QEvent::Close)
        m_thread->stop();
    return QObject::eventFilter(obj, event);
}
