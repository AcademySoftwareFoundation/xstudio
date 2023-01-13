// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/qml/actor_object.hpp"

CAF_PUSH_WARNINGS
#include <QOpenGLWidget>
CAF_POP_WARNINGS

namespace xstudio {
namespace ui {
    namespace qt {

        class ViewportGLWidget : public caf::mixin::actor_object<QOpenGLWidget> {

          public:
            using super = caf::mixin::actor_object<QOpenGLWidget>;

            ViewportGLWidget(QWidget *parent);

            virtual void init(caf::actor_system &system);

            void set_playhead(caf::actor playhead);

          protected:
            void initializeGL() override;

            void resizeGL(int w, int h) override;

            void paintGL() override;

            void receive_change_notification(viewport::Viewport::ChangeCallbackId id);

            std::shared_ptr<ui::viewport::Viewport> the_viewport_;
        };

    } // namespace qt
} // namespace ui
} // namespace xstudio