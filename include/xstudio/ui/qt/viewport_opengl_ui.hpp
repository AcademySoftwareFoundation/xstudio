// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include <caf/mixin/actor_widget.hpp>

CAF_PUSH_WARNINGS
#include <QGLWidget>
CAF_POP_WARNINGS

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/mouse.hpp"

namespace xstudio {
namespace ui {
    namespace qt {
        class ViewportOpenGLUI : public caf::mixin::actor_widget<QGLWidget> {

            Q_OBJECT
            using super = caf::mixin::actor_widget<QGLWidget>;

          public:
            ViewportOpenGLUI(QWidget *parent);
            virtual ~ViewportOpenGLUI();

            virtual void initializeGL() override;
            virtual void paintGL() override;
            virtual void mousePressEvent(QMouseEvent *event) override;
            virtual void mouseReleaseEvent(QMouseEvent *event) override;
            virtual void mouseMoveEvent(QMouseEvent *event) override;

            void init(caf::actor_system &system, caf::actor player);

            void join_playhead(caf::actor group) { join_broadcast(self(), group); }
            void leave_playhead(caf::actor group) { leave_broadcast(self(), group); }

          private:
            void emitPointerEvent(viewport::Signature::EventType t, QMouseEvent *event);

            opengl:: viewport_;
            caf::actor player_;
        };
    } // namespace qt
} // namespace ui
} // namespace xstudio
