// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/thumbnail_provider_ui.hpp"

using namespace xstudio;
using namespace xstudio::media;

std::pair<QImage, QString> ThumbnailReader::run(JobControl &cjc) {

    int width  = 128;
    int height = 128;
    std::string error;

    try {

        caf::actor_system &system_ = CafSystemObject::get_actor_system();

        if (not cjc.shouldRun())
            throw std::runtime_error("Cancelled");

        if (id_.indexOf("http") == 0) {
            auto thumbgen = system_.registry().get<caf::actor>(thumbnail_manager_registry);
            int frame     = 1;
            auto hash     = std::hash<std::string>{}(id_.toStdString());

            auto _uri = make_uri(id_.toStdString());
            if (id_.indexOf("@") != -1) {
                frame = id_.section("@", 1, 1).toInt();
                _uri  = make_uri(id_.section("@", 0, 0).toStdString());
            }

            if (_uri) {

                AVFrameID frame_id(*_uri, frame);
                scoped_actor sys{system_};
                const auto tbp = request_receive<ThumbnailBufferPtr>(
                    *sys,
                    thumbgen,
                    media_reader::get_thumbnail_atom_v,
                    frame_id,
                    static_cast<size_t>(std::max(requestedSize_.width(), 128)),
                    static_cast<size_t>(hash),
                    true);

                // set size based on
                auto actual_width =
                    requestedSize_.width() > 0 ? requestedSize_.width() : tbp->width();
                auto actual_height =
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
            }
        }

        auto actual_width  = requestedSize_.width() > 0 ? requestedSize_.width() : width;
        auto actual_height = requestedSize_.height() > 0 ? requestedSize_.height() : height;
        auto actor_addr    = StdFromQString(id_.section('/', 0, 0));
        auto frame         = std::atoi(StdFromQString(id_.section('/', 1, 1)).c_str());
        auto cache_to_disk = std::atoi(StdFromQString(id_.section('/', 2, 2)).c_str());
        char *end;
        auto hash = std::strtoull(StdFromQString(id_.section('/', 3, 3)).c_str(), &end, 10);
        caf::binary_serializer::container_type buf = hex_to_bytes(actor_addr);
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
                    cache_to_disk ? true : false,
                    utility::clock::now());
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
        actual_width  = requestedSize_.width() > 0 ? requestedSize_.width() : tbp->width();
        actual_height = requestedSize_.height() > 0 ? requestedSize_.height() : tbp->height();


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
