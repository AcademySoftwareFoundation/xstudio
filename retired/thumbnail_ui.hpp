// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "xstudio/thumbnail/thumbnail.hpp"

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QQuickPaintedItem>
#include <QImage>
CAF_POP_WARNINGS

namespace xstudio {
namespace ui {
    namespace qml {

        class MediaSourceUI;

        class ThumbNail : public QQuickPaintedItem {

            Q_OBJECT

            Q_PROPERTY(QObject *mediaSource READ mediaSource WRITE setMediaSource NOTIFY
                           mediaSourceChanged)
            Q_PROPERTY(bool thumbLoaded READ thumbLoaded NOTIFY thumbLoadedChanged)
            Q_PROPERTY(bool hasError READ hasError NOTIFY hasErrorChanged)
            Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
            Q_PROPERTY(float busyAnim READ busyAnim WRITE setBusyAnim NOTIFY busyAnimChanged)

          public:
            ThumbNail(QQuickItem *parent = nullptr);
            ~ThumbNail() override = default;

            [[nodiscard]] QObject *mediaSource() const;
            [[nodiscard]] bool thumbLoaded() const { return thumb_loaded_; }
            [[nodiscard]] bool hasError() const { return has_error_; }
            [[nodiscard]] QColor color() const { return color_; }
            [[nodiscard]] float busyAnim() const { return busy_anim_; }

            void setMediaSource(QObject *media_item);
            void setColor(QColor);
            void setBusyAnim(float);

            void paint(QPainter *painter) override;

          public slots:

            void newThumbnail(const QImage &tnail, const float position_in_clip_duration);
            void loadThumbnail(const float position_in_clip_duration);
            void cancelLoadThumbnail();
            void busyAnimAdvance();

          signals:

            void mediaSourceChanged();
            void thumbLoadedChanged();
            void colorChanged();
            void hasErrorChanged();
            void busyAnimChanged();

          private:
            QImage thumb_image_;
            MediaSourceUI *media_source_     = {nullptr};
            float position_in_clip_duration_ = {-1.0f};
            bool thumb_loaded_               = {false};
            bool has_error_                  = {false};
            QColor color_;
            float busy_anim_ = {0.0f};
        };


    } // namespace qml
} // namespace ui
} // namespace xstudio