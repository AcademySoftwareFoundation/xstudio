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

    std::pair<QImage, QString> run(JobControl &cjc) override {


        int width  = 128;
        int height = 128;
        std::string error;


        try {
            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            auto actual_width  = requestedSize_.width() > 0 ? requestedSize_.width() : width;
            auto actual_height = requestedSize_.height() > 0 ? requestedSize_.height() : height;
            auto actor_addr    = StdFromQString(id_.section('/', 0, 0));
            auto frame         = std::atoi(StdFromQString(id_.section('/', 1, 1)).c_str());
            auto cache_to_disk = std::atoi(StdFromQString(id_.section('/', 2, 2)).c_str());
            char *end;
            auto hash = std::strtoull(StdFromQString(id_.section('/', 3, 3)).c_str(), &end, 10);
            caf::binary_serializer::container_type buf = hex_to_bytes(actor_addr);
            caf::actor_system &system_                 = CafSystemObject::get_actor_system();
            caf::actor_addr addr;
            caf::binary_deserializer bd{system_, buf};
            auto e = bd.apply(addr);

            if (not e) {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(bd.get_error()));
                throw std::runtime_error(to_string(bd.get_error()));
            }


            scoped_actor sys{system_};
            // get thumbnail generator.
            auto thumbgen = system_.registry().get<caf::actor>(thumbnail_manager_registry);

            AVFrameID mp;
            // super dirty...
            for (auto i = 1; i < 5; i++) {
                try {
                    mp = request_receive<AVFrameID>(
                        *sys,
                        caf::actor_cast<caf::actor>(addr),
                        get_media_pointer_atom_v,
                        media::MT_IMAGE,
                        frame);
                    break;
                } catch (const std::exception &err) {
                    // give up
                    if (i == 4 or std::string(err.what()).find("caf::") != std::string::npos)
                        throw;
                    std::this_thread::sleep_for(std::chrono::seconds(2) * i);
                }

                if (not cjc.shouldRun())
                    throw std::runtime_error("Cancelled");
            }

            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            // get thumbnail
            // don't disk cache frames other than 0 (it assumes the user is swipping, and we
            // don't cache those to disk.) this can also fail..
            ThumbnailBufferPtr tbp;
            for (auto i = 1; i < 5; i++) {
                try {
                    tbp = request_receive<ThumbnailBufferPtr>(
                        *sys,
                        thumbgen,
                        media_reader::get_thumbnail_atom_v,
                        mp,
                        static_cast<size_t>(std::max(requestedSize_.width(), 128)),
                        hash,
                        cache_to_disk ? true : false);
                    break;
                } catch (const std::exception &err) {
                    // give up
                    if (i == 4 or std::string(err.what()).find("caf::") != std::string::npos)
                        throw;
                    std::this_thread::sleep_for(std::chrono::seconds(1) * i);
                    mp = request_receive<AVFrameID>(
                        *sys,
                        caf::actor_cast<caf::actor>(addr),
                        get_media_pointer_atom_v,
                        media::MT_IMAGE,
                        frame);
                }
                if (not cjc.shouldRun())
                    throw std::runtime_error("Cancelled");
            }

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
            if (cjc.shouldRun())
                error = err.what();
        }

        return std::make_pair(QImage(), QStringFromStd(error));
    }

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
