// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_companion.hpp>

CAF_PUSH_WARNINGS

#include <QCursor>
#include <QObject>
#include <QPointF>
#include <QQuickItem>
#include <QVector3D>
#include <QtQml>
#include <QScreen>

CAF_POP_WARNINGS

#include <xstudio/ui/mouse.hpp>

namespace xstudio {
namespace ui {

    namespace qt {
        class OffscreenViewport;
    }

    namespace qml {

        class QMLViewportRenderer;
        class PlayheadUI;

        class QMLViewport : public QQuickItem {
            Q_OBJECT

            Q_PROPERTY(float zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
            Q_PROPERTY(bool frameOutOfRange READ frameOutOfRange NOTIFY frameOutOfRangeChanged)
            Q_PROPERTY(bool noAlphaChannel READ noAlphaChannel NOTIFY noAlphaChannelChanged)
            Q_PROPERTY(QString fpsExpression READ fpsExpression NOTIFY fpsExpressionChanged)
            Q_PROPERTY(float scale READ scale WRITE setScale NOTIFY scaleChanged)
            Q_PROPERTY(
                QVector2D translate READ translate WRITE setTranslate NOTIFY translateChanged)
            Q_PROPERTY(QObject *playhead READ playhead NOTIFY playheadChanged)
            Q_PROPERTY(int mouseButtons READ mouseButtons NOTIFY mouseButtonsChanged)
            Q_PROPERTY(QPoint mouse READ mouse NOTIFY mouseChanged)
            Q_PROPERTY(int onScreenImageLogicalFrame READ onScreenImageLogicalFrame NOTIFY
                           onScreenImageLogicalFrameChanged)
            Q_PROPERTY(QRectF imageBoundaryInViewport READ imageBoundaryInViewport NOTIFY
                           imageBoundaryInViewportChanged)
            Q_PROPERTY(QSize imageResolution READ imageResolution NOTIFY imageResolutionChanged)
            Q_PROPERTY(bool enableShortcuts READ enableShortcuts NOTIFY enableShortcutsChanged)
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(bool isQuickViewer READ isQuickViewer WRITE setIsQuickViewer NOTIFY
                           isQuickViewerChanged)

          public:
            QMLViewport(QQuickItem *parent = nullptr);
            virtual ~QMLViewport();
            float zoom();
            QString fpsExpression();
            float scale();
            QVector2D translate();
            QObject *playhead();
            [[nodiscard]] QString name() const;
            [[nodiscard]] int mouseButtons() const { return mouse_buttons; }
            [[nodiscard]] QPoint mouse() const { return mouse_position; }
            [[nodiscard]] int onScreenImageLogicalFrame() const {
                return on_screen_logical_frame_;
            }
            [[nodiscard]] bool frameOutOfRange() const { return frame_out_of_range_; }
            [[nodiscard]] bool noAlphaChannel() const { return no_alpha_channel_; }
            [[nodiscard]] bool enableShortcuts() const { return enable_shortcuts_; }
            void setPlayhead(caf::actor playhead);
            QMLViewportRenderer *viewportActor() { return renderer_actor; }
            void deleteRendererActor();
            bool isQuickViewer() const { return is_quick_viewer_; }

          protected:
            void hoverEnterEvent(QHoverEvent *event) override;
            void hoverLeaveEvent(QHoverEvent *event) override;
            void mousePressEvent(QMouseEvent *event) override;
            void mouseReleaseEvent(QMouseEvent *event) override;
            void hoverMoveEvent(QHoverEvent *event) override;
            void mouseMoveEvent(QMouseEvent *event) override;
            void wheelEvent(QWheelEvent *event) override;
            void mouseDoubleClickEvent(QMouseEvent *event) override;
            bool event(QEvent *event) override;
            void keyPressEvent(QKeyEvent *event) override;
            void keyReleaseEvent(QKeyEvent *event) override;

          public slots:

            void sync();
            void cleanup();
            void setZoom(const float z);
            void revertFitZoomToPrevious(const bool ignoreOtherViewport = false);
            void linkToViewport(QObject *other_viewport);
            void handleScreenChanged(QScreen *screen);

            void hideCursor();
            void showCursor();
            QSize imageResolution();
            QVector2D bboxCornerInViewport(const int min_x, const int min_y);
            void setScale(const float s);
            void setTranslate(const QVector2D &tr);
            void setOnScreenImageLogicalFrame(const int frame_num);
            QRectF imageBoundaryInViewport();
            void setFrameOutOfRange(bool frame_out_of_range);
            void setNoAlphaChannel(bool no_alpha_channel);
            void renderImageToFile(
                const QUrl filePath,
                const int format,
                const int compression,
                const int width,
                const int height,
                const bool bakeColor);
            void sendResetShortcut();
            void setOverrideCursor(const QString &name, const bool centerOffset);
            void setOverrideCursor(const Qt::CursorShape cname);
            void setRegularCursor(const Qt::CursorShape cname);
            void setIsQuickViewer(const bool is_quick_viewer);

          private slots:

            void handleWindowChanged(QQuickWindow *win);

          signals:

            void zoomChanged(float);
            void fpsExpressionChanged(QString);
            void scaleChanged(float);
            void playheadChanged(QObject *);
            void translateChanged(QVector2D);
            void mouseButtonsChanged();
            void mouseChanged();
            void onScreenImageLogicalFrameChanged();
            void imageBoundaryInViewportChanged();
            void imageResolutionChanged();
            void frameOutOfRangeChanged();
            void noAlphaChannelChanged();
            void enableShortcutsChanged();
            void doSnapshot(QString, QString, int, int, bool);
            void nameChanged();
            void quickViewSource(QStringList mediaActors, QString compareMode);
            void quickViewBackendRequest(QStringList mediaActors, QString compareMode);
            void quickViewBackendRequestWithSize(
                QStringList mediaActors, QString compareMode, QPoint position, QSize size);
            void snapshotRequestResult(QString resultMessage);
            void pointerEntered();
            void pointerExited();
            void isQuickViewerChanged();

          private:
            void releaseResources() override;
            PointerEvent makePointerEvent(
                Signature::EventType t, QMouseEvent *event, int force_modifiers = 0);
            PointerEvent makePointerEvent(
                Signature::EventType t, int buttons, int x, int y, int w, int h, int modifiers);

            QQuickWindow *m_window = {nullptr};
            QMLViewportRenderer *renderer_actor{nullptr};
            PlayheadUI *playhead_{nullptr};
            static qt::OffscreenViewport *offscreen_viewport_;

            bool connected_{false};
            QCursor cursor_;
            bool cursor_hidden{false};
            int mouse_buttons = {0};
            QPoint mouse_position;
            int on_screen_logical_frame_ = {0};
            bool frame_out_of_range_     = {false};
            bool no_alpha_channel_       = {false};
            bool enable_shortcuts_       = {true};
            int viewport_index_          = {0};
            bool is_quick_viewer_        = {false};
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio