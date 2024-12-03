// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/utility/logging.hpp"
#include "definitions.hpp"

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

using namespace xstudio::thumbnail;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;

// QFututre<int> futureValue = TaskExecutor::run(new SomeControllableTask(inputForThatTask));
// Then she may cancel it by calling futureValue.cancel(), bearing in mind that cancellation is
// graceful and not immediate.

class ShotgridThumbnailReader : public ControllableJob<std::pair<QImage, QString>> {
  public:
    ShotgridThumbnailReader(const QString id, const QSize requestedSize)
        : ControllableJob(), id_(std::move(id)), requestedSize_(std::move(requestedSize)) {}

    std::pair<QImage, QString> run(JobControl &cjc) override {


        int width          = 100;
        int height         = 100;
        auto actual_width  = requestedSize_.width() > 0 ? requestedSize_.width() : width;
        auto actual_height = requestedSize_.height() > 0 ? requestedSize_.height() : height;
        std::string error;

        try {

            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            caf::actor_system &system_ = CafSystemObject::get_actor_system();
            auto shotgrid = system_.registry().get<caf::actor>(shotbrowser_datasource_registry);
            auto thumbnail_cache =
                system_.registry().get<caf::actor>(thumbnail_manager_registry);

            if (not shotgrid)
                throw std::runtime_error("ShotGrid not available");

            scoped_actor sys{system_};

            auto key = StdFromQString(id_);
            ThumbnailBufferPtr tbp;

            if (thumbnail_cache) {
                try {
                    tbp = request_receive<ThumbnailBufferPtr>(
                        *sys,
                        thumbnail_cache,
                        media_reader::get_thumbnail_atom_v,
                        key,
                        static_cast<size_t>(std::max(requestedSize_.width(), 128)));
                } catch (...) {
                }
            }

            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            if (not tbp or tbp->empty()) {
                auto mode   = StdFromQString(id_.section('/', 0, 0));
                auto entity = StdFromQString(id_.section('/', 1, 1));
                auto id     = std::atoi(StdFromQString(id_.section('/', 2, 2)).c_str());

                tbp = request_receive<ThumbnailBufferPtr>(
                    *sys,
                    shotgrid,
                    shotgun_client::shotgun_image_atom_v,
                    entity,
                    id,
                    (mode == "thumbnail" ? true : false),
                    true);
                if (thumbnail_cache)
                    sys->anon_send(
                        thumbnail_cache,
                        media_cache::store_atom_v,
                        key,
                        static_cast<size_t>(std::max(requestedSize_.width(), 128)),
                        tbp);
            }

            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            // set size based on
            actual_width = requestedSize_.width() > 0 ? requestedSize_.width() : tbp->width();
            actual_height =
                requestedSize_.height() > 0 ? requestedSize_.height() : tbp->height();

            return std::make_pair(
                QImage(
                    (uchar *)&(tbp->data()[0]),
                    tbp->width(),
                    tbp->height(),
                    3 * tbp->width(),
                    QImage::Format_RGB888)
                    .scaled(actual_width, actual_height, Qt::KeepAspectRatio),
                QString());
        } catch (const std::exception &err) {
            spdlog::debug("{} {} {}", __PRETTY_FUNCTION__, StdFromQString(id_), err.what());
            if (cjc.shouldRun())
                error = err.what();
        }

        return std::make_pair(QImage(), QStringFromStd(error));
    }

  private:
    const QString id_;
    const QSize requestedSize_;
};


class ShotgridResponse : public QQuickImageResponse {
  public:
    ShotgridResponse(
        const QString &id,
        const QSize &requestedSize,
        QThreadPool *pool,
        QMap<QString, QDateTime> &bad_thumbs)
        : id_(id), bad_thumbs_(bad_thumbs) {
        // spdlog::warn("{}", StdFromQString(id));
        if (bad_thumbs_.contains(id_) and
            bad_thumbs_[id_].secsTo(QDateTime::currentDateTime()) < 60 * 20) {
            error_ = "Thumbnail does not exist.";
            emit finished();
        } else {

            // create a future..
            connect(
                &watcher_,
                &QFutureWatcher<std::pair<QImage, QString>>::finished,
                this,
                &ShotgridResponse::handleFinished);
            connect(
                &watcher_,
                &QFutureWatcher<std::pair<QImage, QString>>::canceled,
                this,
                &ShotgridResponse::handleCanceled);

            // Start the computation.
            QFuture<std::pair<QImage, QString>> future =
                JobExecutor::run(new ShotgridThumbnailReader(id, requestedSize), pool);

            watcher_.setFuture(future);
        }
    }
    [[nodiscard]] QString errorString() const override { return error_; }

    void handleFinished() {
        if (watcher_.future().resultCount()) {
            // spdlog::warn("finished {}", StdFromQString(id_));
            auto [i, e] = watcher_.result();

            if (not e.isEmpty()) {
                error_ = "Thumbnail does not exist.";
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
        error_ = "Thumbnail does not exist.";
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

class ShotgridProvider : public QQuickAsyncImageProvider {
  public:
    QQuickImageResponse *
    requestImageResponse(const QString &id, const QSize &requestedSize) override {
        auto response = new ShotgridResponse(id, requestedSize, &pool, bad_thumbs_);
        return response;
    }

  private:
    QThreadPool pool;
    QMap<QString, QDateTime> bad_thumbs_;
};
