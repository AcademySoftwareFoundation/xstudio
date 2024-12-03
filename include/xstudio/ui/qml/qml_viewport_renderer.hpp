// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/ui/viewport/viewport.hpp"

CAF_PUSH_WARNINGS
#include <QObject>
#include <QQuickWindow>
#include <QRunnable>
#include <QVector2D>
CAF_POP_WARNINGS

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/viewport_qml_export.h"

namespace xstudio {
namespace ui {
    namespace qml {

        class QMLViewport;

        class VIEWPORT_QML_EXPORT QMLViewportRenderer : public QMLActor {
            Q_OBJECT

          public:
            QMLViewportRenderer(QObject *owner);
            virtual ~QMLViewportRenderer();

            void setWindow(QQuickWindow *window);

            void setSceneCoordinates(
                const QPointF topleft,
                const QPointF topright,
                const QPointF bottomright,
                const QPointF bottomleft,
                const QSize sceneSize,
                const float devicePixelRatio);

            void prepareRenderData();

            void init_system();
            void join_playhead(caf::actor group) {
                scoped_actor sys{system()};
                try {
                    utility::request_receive<bool>(
                        *sys, group, broadcast::join_broadcast_atom_v, as_actor());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }
            void leave_playhead(caf::actor group) {
                scoped_actor sys{system()};
                try {
                    utility::request_receive<bool>(
                        *sys, group, broadcast::leave_broadcast_atom_v, as_actor());
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }

            void set_playhead(caf::actor playhead);
            
            [[nodiscard]] QVariantList imageResolutions() const;
            [[nodiscard]] QVariantList imageBoundariesInViewport() const;
            [[nodiscard]] caf::actor playhead() {
                return viewport_renderer_ ? viewport_renderer_->playhead() : caf::actor();
            }
            bool pointerEvent(const PointerEvent &e);
            void setScreenInfos(
                QString name,
                QString model,
                QString manufacturer,
                QString serialNumber,
                double refresh_rate);
            [[nodiscard]] QString name() const {
                return viewport_renderer_ ? QStringFromStd(viewport_renderer_->name())
                                          : QString("Not Yet");
            }

            [[nodiscard]] std::string std_name() const {
                return viewport_renderer_ ? viewport_renderer_->name() : "not yet";
            }

            void setIsQuickViewer(const bool is_quick_viewer);
            void visibleChanged(const bool is_visible);

          public slots:

            void init_renderer();
            void paint();
            void frameSwapped();
            void quickViewSource(
                QStringList mediaActors, QString compareMode, int in_pt, int out_pt);
            void reset();

          signals:

            void fpsChanged(QString);
            void exposureChanged(float);
            void translationChanged();
            void resolutionsChanged();
            void doRedraw();
            void doSnapshot(QString, QString, int, int, bool);
            void quickViewBackendRequest(QStringList mediaActors, QString compareMode);
            void quickViewBackendRequestWithSize(
                QStringList mediaActors, QString compareMode, QPoint position, QSize size);
            void snapshotRequestResult(QString resultMessage);

          private:
            void receive_change_notification(viewport::Viewport::ChangeCallbackId id);
            void make_xstudio_viewport();

            QQuickWindow *m_window;
            ui::viewport::Viewport *viewport_renderer_ = nullptr;
            bool init_done{false};
            QString fps_expression_;
            class QMLViewport *viewport_qml_item_;

            caf::actor viewport_update_group;
            caf::actor playhead_group_;
            caf::actor playhead_events_;
            caf::actor fps_monitor_;
            caf::actor keypress_monitor_;

            struct ViewportCoords {
                QPointF corners[4];
                QSize size;
                bool
                set(const QPointF &,
                    const QPointF &,
                    const QPointF &,
                    const QPointF &,
                    const QSize &);
            } viewport_coords_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio