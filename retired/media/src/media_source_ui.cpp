// SPDX-License-Identifier: Apache-2.0

#include <filesystem>

#include <iostream>
#include <regex>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

namespace fs = std::filesystem;


using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

MediaSourceUI::MediaSourceUI(QObject *parent)
    : QMLActor(parent), backend_(), backend_events_() {}

void MediaSourceUI::set_backend(caf::actor backend) {
    spdlog::debug("MediaSourceUI set_backend");

    const auto tt = utility::clock::now();

    backend_ = backend;
    scoped_actor sys{system()};

    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        backend_events_ = caf::actor();
    }

    // we can't do this.. we rely on the value..
    // we're getting in a big mess with how this stuff currently works.
    try {
        const auto tmp = request_receive_wait<media::MediaStatus>(
            *sys, backend_, std::chrono::seconds(1), media::media_status_atom_v);
        if (media_status_ != tmp) {
            media_status_ = tmp;
            emit mediaStatusChanged();
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    try {
        auto detail = request_receive_wait<ContainerDetail>(
            *sys, backend_, std::chrono::seconds(1), utility::detail_atom_v);
        update_from_detail(detail);
    } catch (std::exception &e) {
        spdlog::warn("MediaSourceUI set_backend {}", e.what());
    }

    if (backend_) {
        anon_send(backend_, media::get_media_details_atom_v, as_actor());
    }

    spdlog::debug(
        "MediaSourceUI set_backend took {} milliseconds to complete",
        std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now() - tt)
            .count());
}

QFuture<QString> MediaSourceUI::getThumbnailURLFuture(int frame) {
    return QtConcurrent::run([=]() {
        QString thumburl("qrc:///feather_icons/film.svg");
        try {
            auto the_frame = frame;
            if (backend_ && ((frame == -1 && duration_frames_ != "") || frame != -1)) {
                auto middle_frame = (duration_frames_int_ - 1) / 2;
                if (frame == -1)
                    the_frame = middle_frame;

                thumburl = qml::getThumbnailURL(
                    system(), backend_, the_frame, the_frame == middle_frame);
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return thumburl;
    });
}

void MediaSourceUI::requestThumbnail(float position_in_source_duration) {

    if (backend_) {
        latest_thumb_request_job_id_ = utility::Uuid::generate();
        anon_send(
            backend_,
            media_reader::get_thumbnail_atom_v,
            position_in_source_duration,
            latest_thumb_request_job_id_,
            as_actor());
    }
}

void MediaSourceUI::cancelThumbnailRequest() {
    if (backend_ && !latest_thumb_request_job_id_.is_null()) {
        anon_send(
            backend_,
            media_reader::cancel_thumbnail_request_atom_v,
            latest_thumb_request_job_id_);
        latest_thumb_request_job_id_ = utility::Uuid();
    }
}

void MediaSourceUI::setFpsString(const QString &fps) {
    try {
        double value = std::stod(fps.toStdString());
        if (backend_ and value >= 1.0) {
            // push change top backend..
            scoped_actor sys{system()};

            auto mr =
                request_receive<MediaReference>(*sys, backend_, media::media_reference_atom_v);
            mr.set_rate(FrameRate(1.0 / value));
            anon_send(backend_, media::media_reference_atom_v, mr);
            fps_string_ = fps;
            emit fpsChanged();
        }
    } catch (...) {
    }
}


// ??? NEVER CALLED ?

void MediaSourceUI::update_from_backend(
    const utility::JsonStore &meta_data,
    const utility::JsonStore &colour_params,
    const utility::ContainerDetail &detail) {
    scoped_actor sys{system()};

    try {
        inspect_metadata(meta_data);
    } catch (const std::exception &e) {
        format_     = "TBD";
        resolution_ = "TBD";
        bit_depth_  = "TBD";
        // suppress warning as metadata may not yet be read, watch for event instead
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (colour_params.contains("colourspace"))
        colourManaged_ = colour_params["colourspace"].get<std::string>() != "";

    name_ = QStringFromStd(detail.name_);
    uuid_ = detail.uuid_;

    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        backend_events_ = caf::actor();
    }

    backend_events_ = detail.group_;
    try {
        request_receive<bool>(
            *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }


    update_on_change();
    emit backendChanged();
    emit nameChanged();
    emit uuidChanged();
    emit metadataChanged();
    emit bitDepthChanged();
    emit formatChanged();
    emit resolutionChanged();

    spdlog::debug("MediaSourceUI set_backend {}", to_string(uuid_));
}

void MediaSourceUI::set_sources_count(const size_t count) {
    while (streams_.size() > count) {
        delete streams_.back();
        streams_.pop_back();
    }

    while (streams_.size() < count) {
        streams_.push_back(new MediaStreamUI(this));
        streams_.back()->initSystem(this);
    }
}

QString MediaSourceUI::metadata() {
    QString result;
    try {
        if (backend_) {
            scoped_actor sys{system()};
            result = QStringFromStd(to_string(request_receive<utility::JsonStore>(
                *sys, backend_, json_store::get_json_atom_v, "")));
        }
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}


void MediaSourceUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void MediaSourceUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("MediaSourceUI init");

    // self()->set_down_handler([=](down_msg& msg) {
    // 	if(msg.source == store)
    // 		unsubscribe();
    // });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](backend_atom, caf::actor actor) { set_backend(actor); },

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](const group_down_msg & /*msg*/) {
                // if(msg.source == store_events)
                //   unsubscribe();
            },

            [=](const std::tuple<
                utility::Uuid,
                std::string,
                std::string,
                double,
                media::StreamDetail,
                std::vector<UuidActor>,
                Uuid> &stream_details) {
                // it's possible we get all these details from a media source
                // that has been switched out for another one - so we verify
                // that these are the details we want using the uuid.
                utility::Uuid media_source_uuid = std::get<0>(stream_details);
                if (media_source_uuid == uuid_) {
                    std::string filename              = std::get<1>(stream_details);
                    std::string fps_string            = std::get<2>(stream_details);
                    double fps                        = std::get<3>(stream_details);
                    media::StreamDetail stream_detail = std::get<4>(stream_details);
                    std::vector<UuidActor> streams    = std::get<5>(stream_details);
                    Uuid current_stream               = std::get<6>(stream_details);

                    update_streams_from_backend(
                        filename, fps_string, fps, stream_detail, streams, current_stream);
                }
            },

            [=](utility::detail_atom, const ContainerDetail &detail) {
                update_from_detail(detail);
            },

            [=](utility::event_atom, media::add_media_stream_atom, const UuidActor &) {
                anon_send(backend_, media::get_media_details_atom_v, as_actor());
            },

            [=](utility::event_atom,
                media_metadata::get_metadata_atom,
                const utility::JsonStore &meta) {
                try {
                    inspect_metadata(meta);
                } catch (std::exception &e) {
                    format_     = "TBD";
                    resolution_ = "TBD";
                    bit_depth_  = "TBD";
                }
                emit currentChanged();
                emit metadataChanged();
                emit bitDepthChanged();
                emit pixelAspectChanged();
                emit formatChanged();
                emit resolutionChanged();
            },

            [=](utility::event_atom, utility::change_atom) { update_on_change(); },

            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
                update_on_change();
            },

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                QString tmp = QStringFromStd(name);
                if (tmp != name_) {
                    name_ = tmp;
                    emit nameChanged();
                }
            },
            [=](const thumbnail::ThumbnailBufferPtr &tnail,
                const float position_in_source_duration,
                const utility::Uuid job_uuid,
                const std::string &err_msg) {
                if (tnail && job_uuid == latest_thumb_request_job_id_) {
                    thumbnail_ = QImage(
                        (uchar *)&(tnail->data()[0]),
                        tnail->width(),
                        tnail->height(),
                        3 * tnail->width(),
                        QImage::Format_RGB888);
                    thumbnail_position_in_clip_duration_ = position_in_source_duration;
                    emit thumbnailChanged(thumbnail_, thumbnail_position_in_clip_duration_);
                } else if (!tnail) {
                    spdlog::warn("Thumbanil load failuer: {}", err_msg);
                }
            }};
    });
}

void MediaSourceUI::update_from_detail(const ContainerDetail &detail) {

    name_           = QStringFromStd(detail.name_);
    uuid_           = detail.uuid_;
    backend_events_ = detail.group_;
    scoped_actor sys{system()};
    request_receive<bool>(*sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
    update_on_change();
    emit backendChanged();
    emit nameChanged();
    emit uuidChanged();
}


void MediaSourceUI::update_streams_from_backend(
    const std::string filename,
    const std::string fps_string,
    const double fps,
    const media::StreamDetail stream_detail,
    std::vector<UuidActor> streams,
    Uuid current_stream) {

    file_name_ = QStringFromStd(fs::path(filename).filename());
    file_path_ = QStringFromStd(filename);
    set_sources_count(streams.size());
    size_t ind = 0;
    for (auto i : streams) {
        anon_send(streams_[ind]->as_actor(), backend_atom_v, i.actor());
        if (i.uuid() == current_stream) {
            current_ = streams_[ind];
        }
        ind++;
    }

    fps_string_ = QStringFromStd(fps_string);
    fps_        = fps;

    duration_frames_     = (stream_detail.duration_.rate().to_seconds() == 0.0)
                               ? QString("error")
                               : QString("%1").arg(stream_detail.duration_.frames());
    duration_frames_int_ = (stream_detail.duration_.rate().to_seconds() == 0.0)
                               ? 0
                               : stream_detail.duration_.frames();

    if (stream_detail.duration_.rate().to_seconds() == 0.0) {

        duration_seconds_ = QString("error");

    } else {
        const double dur  = stream_detail.duration_.seconds();
        const int hours   = (int)dur / (60 * 60);
        const int minutes = ((int)dur / 60) % 60;
        const int seconds = int(dur - minutes * 60 - hours * 60 * 60);
        const int fsec    = int(floor((dur - floor(dur)) * 10.0));

        if (hours) {
            duration_seconds_ = QString("%1:%2:%3.%4")
                                    .arg(hours, 2, 10, QChar('0'))
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(seconds, 2, 10, QChar('0'))
                                    .arg(fsec);
        } else {
            duration_seconds_ = QString("%1:%2.%3")
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(seconds, 2, 10, QChar('0'))
                                    .arg(fsec);
        }
    }

    if (stream_detail.duration_.rate().to_seconds() == 0.0) {
        duration_timecode_ = QString("error");
    } else {

        const Timecode tc(
            stream_detail.duration_.frames(),
            stream_detail.duration_.rate().to_fps(),
            false // TODO: find out if the TC is drop frame
        );

        duration_timecode_ = QStringFromStd(tc.to_string());
    }

    emit durationChanged();
    emit streamsChanged();
    emit currentChanged();
    emit fpsChanged();
    emit pathChanged();
    emit fileNameChanged();
    emit thumbnailURLChanged();
}

void MediaSourceUI::update_on_change() {
    scoped_actor sys{system()};
    try {
        const auto tmp = request_receive_wait<media::MediaStatus>(
            *sys, backend_, std::chrono::seconds(1), media::media_status_atom_v);
        if (media_status_ != tmp) {
            media_status_ = tmp;
            emit mediaStatusChanged();
        }
    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    // force rebuild of thumbnail on media change
    // captures colourspace change, this is really needs it's own event to trigger.
    emit thumbnailURLChanged();
}

void MediaSourceUI::inspect_metadata(const nlohmann::json &data) {
    if (data.size() and data.begin().value().contains("standard_fields")) {
        const nlohmann::json &standard_fields = data.begin().value()["standard_fields"];
        if (standard_fields.contains("bit_depth"))
            bit_depth_ = QStringFromStd(standard_fields["bit_depth"].get<std::string>());
        if (standard_fields.contains("format"))
            format_ = QStringFromStd(standard_fields["format"].get<std::string>());
        if (standard_fields.contains("resolution"))
            resolution_ = QStringFromStd(standard_fields["resolution"].get<std::string>());
        if (standard_fields.contains("pixel_aspect"))
            pixel_aspect_ = standard_fields["pixel_aspect"].get<float>();
    } else if (data.contains("standard_fields")) {
        const nlohmann::json &standard_fields = data["standard_fields"];
        if (standard_fields.contains("bit_depth"))
            bit_depth_ = QStringFromStd(standard_fields["bit_depth"].get<std::string>());
        if (standard_fields.contains("format"))
            format_ = QStringFromStd(standard_fields["format"].get<std::string>());
        if (standard_fields.contains("resolution"))
            resolution_ = QStringFromStd(standard_fields["resolution"].get<std::string>());
        if (standard_fields.contains("pixel_aspect"))
            pixel_aspect_ = standard_fields["pixel_aspect"].get<float>();
    } else {
        throw std::runtime_error("No standard metadata fields.");
    }
}

void MediaSourceUI::switchToStream(const QUuid stream_uuid) {

    if (backend_) {
        scoped_actor sys{system()};
        try {
            const bool changed = request_receive<bool>(
                *sys,
                backend_,
                media::current_media_stream_atom_v,
                media::MT_IMAGE,
                UuidFromQUuid(stream_uuid));
            if (changed) {
                anon_send(backend_, media::get_media_details_atom_v, as_actor());
            }
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void MediaSourceUI::switchToStream(const QString stream_id) {

    if (backend_) {
        scoped_actor sys{system()};
        try {
            const bool changed = request_receive<bool>(
                *sys,
                backend_,
                media::current_media_stream_atom_v,
                media::MT_IMAGE,
                StdFromQString(stream_id));
            if (changed) {
                anon_send(backend_, media::get_media_details_atom_v, as_actor());
            }
        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}