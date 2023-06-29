// SPDX-License-Identifier: Apache-2.0
#include <iostream>

#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/ui/qml/thumbnail_ui.hpp"

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

static QImage film_image;

static QTimer *s_anim_timer = nullptr;

ThumbNail::ThumbNail(QQuickItem *parent) : QQuickPaintedItem(parent), thumb_image_(film_image) {

    if (!s_anim_timer) {
        s_anim_timer = new QTimer(nullptr);
        s_anim_timer->setInterval(100);
        s_anim_timer->start();
    }

    /*if (film_image.isNull()) {
        QPixmap p(256, 256);
        QPainter ptr(&p);
        film_image = QImage(QString(":/feather_icons/film.svg"));
        ptr.drawImage(QRectF(0.0f,0.0f,256.f,256.f), film_image);
        ptr.setCompositionMode(QPainter::CompositionMode_SourceIn);
        ptr.fillRect( p.rect(), Qt::white );
        ptr.end();
        film_image = p.toImage();
        thumb_image_ = film_image;
    }*/
}

void ThumbNail::paint(QPainter *painter) {

    if (!thumb_loaded_) {
        // when no thumbnail, fill with mid grey
        painter->fillRect(0, 0, width(), height(), color_);

        QTransform t;
        t.translate(width() / 2, height() / 2);
        t.rotate(busy_anim_);
        t.translate(-width() / 2, -height() / 2);
        painter->setTransform(t);

        painter->fillRect(0, height() / 2 - 0.5f, width(), 10.0f, QColor(192, 192, 192));

        return;
    }

    const float image_aspect  = float(thumb_image_.width()) / float(thumb_image_.height());
    const float canvas_aspect = float(width()) / float(height());

    if (image_aspect > canvas_aspect) {
        float bottom = float(height()) * 0.5f * (image_aspect - canvas_aspect) / image_aspect;
        painter->drawImage(
            QRectF(0.0f, bottom, width(), height() - (bottom * 2.0f)), thumb_image_);
    } else {
        float left = float(width()) * 0.5f * (canvas_aspect - image_aspect) / canvas_aspect;
        painter->drawImage(QRectF(left, 0.0f, width() - (left * 2.0f), height()), thumb_image_);
    }
}

QObject *ThumbNail::mediaSource() const { return static_cast<QObject *>(media_source_); }

void ThumbNail::setMediaSource(QObject *media_item) {

    auto src = dynamic_cast<MediaSourceUI *>(media_item);
    if (media_source_ != src) {

        // TODO: don't hold pointer to media_itme ... what if it is destroyed?
        // use signal slot exclusively here instead
        if (src) {
            if (media_source_) {
                QObject::disconnect(
                    media_source_,
                    &MediaSourceUI::thumbnailChanged,
                    this,
                    &ThumbNail::newThumbnail);
            }
            media_source_ = src;
            QObject::connect(
                media_source_,
                &MediaSourceUI::thumbnailChanged,
                this,
                &ThumbNail::newThumbnail);
            if (width()) {
                // ThumbNail is hidden if width is zero
                media_source_->requestThumbnail(position_in_clip_duration_);
            }
        } else {
            media_source_ = src;
        }
        connect(s_anim_timer, &QTimer::timeout, this, &ThumbNail::busyAnimAdvance);
        thumb_loaded_              = false;
        position_in_clip_duration_ = -1.0f;
        emit thumbLoadedChanged();
        emit mediaSourceChanged();
        update();
    }
}

void ThumbNail::setBusyAnim(float anim) {
    busy_anim_ = anim;
    emit busyAnimChanged();
    update();
}

void ThumbNail::busyAnimAdvance() {
    busy_anim_ += 30;
    emit busyAnimChanged();
    update();
}

void ThumbNail::setColor(QColor c) {
    if (c != color_) {
        color_ = c;
        emit colorChanged();
        /*if (position_in_clip_duration_ == -1.0f) {
            QPixmap p(256, 256);
            QPainter ptr(&p);
            QImage film_image(QString(":/feather_icons/film.svg"));
            ptr.drawImage(QRectF(0.0f,0.0f,256.f,256.f), film_image);
            ptr.setCompositionMode(QPainter::CompositionMode_SourceIn);
            ptr.fillRect( p.rect(), color_ );
            ptr.end();
            film_image = p.toImage();
            thumb_image_ = film_image;
            update();
        }*/
    }
}


void ThumbNail::loadThumbnail(float position_in_clip_duration) {

    // position_in_clip_duration should be in range 0.0 (first frame) and 1.0 (last frame of
    // source). We round it to first decimal place so we can only load up to 10 thumbnail frames
    // through the range of the source - we're doing this to limit IO load
    position_in_clip_duration =
        std::min(std::max(0.0f, round(position_in_clip_duration * 10.0f) / 10.0f), 1.0f);

    if (position_in_clip_duration != position_in_clip_duration_ && media_source_) {
        media_source_->requestThumbnail(position_in_clip_duration);
    }
}

void ThumbNail::cancelLoadThumbnail() {
    if (media_source_)
        media_source_->cancelThumbnailRequest();
}

void ThumbNail::newThumbnail(const QImage &tnail, const float position_in_clip_duration) {
    if (!thumb_loaded_) {
        thumb_loaded_ = true;
        emit thumbLoadedChanged();
        disconnect(s_anim_timer, &QTimer::timeout, this, &ThumbNail::busyAnimAdvance);
    }
    thumb_image_               = tnail;
    position_in_clip_duration_ = position_in_clip_duration;
    update();
}
