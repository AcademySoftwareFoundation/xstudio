// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/logging.hpp"

CAF_PUSH_WARNINGS
#include <QQuickImageProvider>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QThreadPool>
CAF_POP_WARNINGS

using namespace xstudio::media;
using namespace xstudio::thumbnail;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;
using namespace std::chrono_literals;

class AsyncThumbnailResponseRunnable : public QObject, public QRunnable {
    Q_OBJECT

  signals:
    void done(QImage image);
    void failed(QImage image, QString error);

  public:
    AsyncThumbnailResponseRunnable(QString id, const QSize &requestedSize)
        : m_id(std::move(id)), m_requestedSize(requestedSize) {}

    void run() override {

        int width          = 128;
        int height         = 128;
        auto actor_addr    = StdFromQString(m_id.section('/', 0, 0));
        auto frame         = std::atoi(StdFromQString(m_id.section('/', 1, 1)).c_str());
        auto cache_to_disk = std::atoi(StdFromQString(m_id.section('/', 2, 2)).c_str());
        char *end;
        auto hash = std::strtoull(StdFromQString(m_id.section('/', 3, 3)).c_str(), &end, 10);
        caf::binary_serializer::container_type buf = hex_to_bytes(actor_addr);
        caf::actor_system &system_                 = CafSystemObject::get_actor_system();
        caf::actor_addr addr;
        caf::binary_deserializer bd{system_, buf};
        auto e             = bd.apply(addr);
        auto actual_width  = m_requestedSize.width() > 0 ? m_requestedSize.width() : width;
        auto actual_height = m_requestedSize.height() > 0 ? m_requestedSize.height() : height;
        std::string error;

        if (not e) {
            spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(bd.get_error()));
            error = to_string(bd.get_error());
        } else {
            try {
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
                            frame);
                        break;
                    } catch (const std::exception &err) {
                        // give up
                        if (i == 4 or
                            std::string(err.what()).find("caf::") != std::string::npos)
                            throw;
                        std::this_thread::sleep_for(std::chrono::seconds(2) * i);
                    }
                }

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
                            static_cast<size_t>(std::max(m_requestedSize.width(), 128)),
                            hash,
                            cache_to_disk ? true : false);
                        break;
                    } catch (const std::exception &err) {
                        // give up
                        if (i == 4 or
                            std::string(err.what()).find("caf::") != std::string::npos)
                            throw;
                        std::this_thread::sleep_for(std::chrono::seconds(1) * i);
                        mp = request_receive<AVFrameID>(
                            *sys,
                            caf::actor_cast<caf::actor>(addr),
                            get_media_pointer_atom_v,
                            frame);
                    }
                }

                // set size based on
                actual_width =
                    m_requestedSize.width() > 0 ? m_requestedSize.width() : tbp->width();
                actual_height =
                    m_requestedSize.height() > 0 ? m_requestedSize.height() : tbp->height();

                // rescale to requested size.
                emit done(QImage(
                              (uchar *)&(tbp->data()[0]),
                              tbp->width(),
                              tbp->height(),
                              3 * tbp->width(),
                              QImage::Format_RGB888)
                              .scaled(actual_width, actual_height, Qt::KeepAspectRatio));
                return;

            } catch (const std::exception &err) {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
                error = err.what();
            }
        }

        emit failed(
            QIcon(":/feather_icons/film.svg")
                .pixmap(QSize(actual_width, actual_height))
                .toImage(),
            QStringFromStd(error));
    }

  private:
    QString m_id;
    QSize m_requestedSize;
};

class AsyncThumbnailResponse : public QQuickImageResponse {
  public:
    AsyncThumbnailResponse(const QString &id, const QSize &requestedSize, QThreadPool *pool) {
        auto runnable = new AsyncThumbnailResponseRunnable(id, requestedSize);
        connect(
            runnable,
            &AsyncThumbnailResponseRunnable::done,
            this,
            &AsyncThumbnailResponse::handleDone);
        connect(
            runnable,
            &AsyncThumbnailResponseRunnable::failed,
            this,
            &AsyncThumbnailResponse::handleFailed);
        pool->start(runnable);
    }
    [[nodiscard]] QString errorString() const override { return error_; }

    void handleDone(QImage image) {
        m_image = image;
        emit finished();
    }

    void handleFailed(QImage image, QString error) {
        m_image = image;
        error_  = error;
        emit finished();
    }

    [[nodiscard]] QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QImage m_image;
    QString error_;
};

class AsyncThumbnailProvider : public QQuickAsyncImageProvider {
  public:
    QQuickImageResponse *
    requestImageResponse(const QString &id, const QSize &requestedSize) override {
        auto response = new AsyncThumbnailResponse(id, requestedSize, &pool);
        return response;
    }

  private:
    QThreadPool pool;
};
