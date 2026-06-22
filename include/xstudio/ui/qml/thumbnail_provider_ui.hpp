// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/utility/logging.hpp"

CAF_PUSH_WARNINGS
#include <QQuickImageProvider>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QDateTime>
#include <QMap>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

using namespace xstudio::media;
using namespace xstudio::thumbnail;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;
using namespace std::chrono_literals;

class ThumbnailReader : public ControllableJob<std::pair<QImage, QString>> {
  public:
    ThumbnailReader(const QString id, const QSize requestedSize)
        : ControllableJob(), id_(std::move(id)), requestedSize_(std::move(requestedSize)) {}

    std::pair<QImage, QString> run(JobControl &cjc) override;

  private:
    const QString id_;
    const QSize requestedSize_;
};


class ThumbnailResponse : public QQuickImageResponse {
  public:
    ThumbnailResponse(
        const QString &id,
        const QSize &requestedSize,
        QThreadPool *pool,
        QMap<QString, QDateTime> &bad_thumbs)
        : id_(id), bad_thumbs_(bad_thumbs) {
        // spdlog::warn("{}", StdFromQString(id));
        if (bad_thumbs_.contains(id_) and
            bad_thumbs_[id_].secsTo(QDateTime::currentDateTime()) < 60 * 20) {
            error_ = "Thumbnail does not exist 1.";
            emit finished();
        } else {

            // create a future..
            connect(
                &watcher_,
                &QFutureWatcher<std::pair<QImage, QString>>::finished,
                this,
                &ThumbnailResponse::handleFinished);
            connect(
                &watcher_,
                &QFutureWatcher<std::pair<QImage, QString>>::canceled,
                this,
                &ThumbnailResponse::handleCanceled);

            // Start the computation.
            QFuture<std::pair<QImage, QString>> future =
                JobExecutor::run(new ThumbnailReader(id, requestedSize), pool);

            watcher_.setFuture(future);
        }
    }
    [[nodiscard]] QString errorString() const override { return error_; }

    void handleFinished() {
        if (watcher_.future().resultCount()) {
            // spdlog::warn("finished {}", StdFromQString(id_));
            auto [i, e] = watcher_.result();

            if (not e.isEmpty()) {
                qDebug() << e;
                error_ = "Thumbnail does not exist 2.";
                bad_thumbs_.insert(id_, QDateTime::currentDateTime());
            } else {
                bad_thumbs_.remove(id_);
                m_image = i;
            }
            emit finished();
        } else {
            // spdlog::warn("finished no result CANCELED {}", StdFromQString(id_));
            emit finished();
        }
    }

    void handleCanceled() {
        // spdlog::warn("canceled {}", StdFromQString(id_));
        emit finished();
    }

    void cancel() override {
        // spdlog::warn("canceling {}", StdFromQString(id_));
        watcher_.cancel();
    }

    void handleDone(QImage image) {
        bad_thumbs_.remove(id_);
        m_image = image;
        emit finished();
    }

    void handleFailed(QString error) {
        error_ = "Thumbnail does not exist 3.";
        emit finished();
        bad_thumbs_.insert(id_, QDateTime::currentDateTime());
    }

    [[nodiscard]] QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QImage m_image;
    QString error_;

    QString id_;
    QMap<QString, QDateTime> &bad_thumbs_;
    QFutureWatcher<std::pair<QImage, QString>> watcher_;
};

class ThumbnailProvider : public QQuickAsyncImageProvider {
  public:
    QQuickImageResponse *
    requestImageResponse(const QString &id, const QSize &requestedSize) override {
        auto response = new ThumbnailResponse(id, requestedSize, &pool, bad_thumbs_);
        return response;
    }

  private:
    QThreadPool pool;
    QMap<QString, QDateTime> bad_thumbs_;
};
