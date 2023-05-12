// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/tag/tag.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

class CafRequest : public ControllableJob<QMap<int, QString>> {
  public:
    CafRequest(const nlohmann::json &json, const int role)
        : ControllableJob(), json_(json), role_(role) {}

    QMap<int, QString> run(JobControl &cjc) override {

        QMap<int, QString> result;

        try {
            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            caf::actor_system &system_ = CafSystemObject::get_actor_system();
            scoped_actor sys{system_};

            switch (role_) {

            case SessionModel::Roles::thumbnailURLRole:
                if (json_.at("type") == "MediaSource") {
                    // get current source/stream..

                    QString thumburl("qrc:///feather_icons/film.svg");
                    try {
                        // get mediasource stream detail.
                        auto detail = request_receive<media::StreamDetail>(
                            *sys,
                            actorFromString(system_, json_.at("actor")),
                            media::get_stream_detail_atom_v,
                            media::MT_IMAGE);
                        if (detail.duration_.rate().to_seconds() != 0.0) {
                            auto middle_frame = (detail.duration_.frames() - 1) / 2;
                            thumburl          = getThumbnailURL(
                                system_,
                                actorFromString(system_, json_.at("actor")),
                                middle_frame,
                                true);
                        }

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }

                    result[SessionModel::Roles::thumbnailURLRole] =
                        QStringFromStd(json(StdFromQString(thumburl)).dump());
                }
                break;

            case SessionModel::Roles::rateFPSRole:
                if (json_.at("type") == "MediaSource") {
                    auto rr = request_receive<MediaReference>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::media_reference_atom_v);

                    result[SessionModel::Roles::pathRole] =
                        QStringFromStd(json(to_string(rr.uri())).dump());
                    result[SessionModel::Roles::rateFPSRole] =
                        QStringFromStd(json(rr.rate()).dump());
                }
                break;

            case SessionModel::Roles::pathRole:
                if (json_.at("type") == "Session") {
                    auto pt = request_receive<std::pair<caf::uri, fs::file_time_type>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        session::path_atom_v);

                    result[SessionModel::Roles::pathRole] =
                        QStringFromStd(json(to_string(pt.first)).dump());
                    result[SessionModel::Roles::mtimeRole] =
                        QStringFromStd(json(pt.second).dump());
                } else if (json_.at("type") == "MediaSource") {
                    auto rr = request_receive<MediaReference>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::media_reference_atom_v);

                    result[SessionModel::Roles::pathRole] =
                        QStringFromStd(json(to_string(rr.uri())).dump());
                    result[SessionModel::Roles::rateFPSRole] =
                        QStringFromStd(json(rr.rate()).dump());
                }
                break;
            case SessionModel::Roles::nameRole:
            case SessionModel::Roles::actorUuidRole:
            case SessionModel::Roles::groupActorRole:
            case SessionModel::Roles::typeRole:
                if (json_.at("type") == "Session" or json_.at("type") == "Playlist" or
                    json_.at("type") == "Subset" or json_.at("type") == "Media") {
                    spdlog::warn("request detail");
                    // caf::actor actor_;
                    auto detail = request_receive<ContainerDetail>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        utility::detail_atom_v);

                    result[SessionModel::Roles::groupActorRole] =
                        QStringFromStd(json(actorToString(system_, detail.group_)).dump());
                    result[SessionModel::Roles::nameRole] =
                        QStringFromStd(json(detail.name_).dump());
                    result[SessionModel::Roles::actorUuidRole] =
                        QStringFromStd(json(detail.uuid_).dump());
                    result[SessionModel::Roles::typeRole] =
                        QStringFromStd(json(detail.type_).dump());
                    spdlog::warn("got detail");
                }
                break;
            case SessionModel::Roles::mediaStatusRole:
                if (json_.at("type") == "Media" or json_.at("type") == "MediaSource") {
                    auto answer = request_receive<media::MediaStatus>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::media_status_atom_v);

                    result[SessionModel::Roles::mediaStatusRole] =
                        QStringFromStd(json(answer).dump());
                }

            case SessionModel::Roles::imageActorUuidRole:
                if (json_.at("type") == "Media") {
                    auto answer = request_receive<UuidActor>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::current_media_source_atom_v,
                        media::MT_IMAGE);

                    result[SessionModel::Roles::imageActorUuidRole] =
                        QStringFromStd(json(answer.uuid()).dump());
                } else if (json_.at("type") == "MediaSource") {
                    auto answer = request_receive<UuidActor>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::current_media_stream_atom_v,
                        media::MT_IMAGE);

                    result[SessionModel::Roles::imageActorUuidRole] =
                        QStringFromStd(json(answer.uuid()).dump());
                }
                break;

            case SessionModel::Roles::flagRole:
                if (json_.at("type") == "Media") {
                    auto [flag, text] = request_receive<std::tuple<std::string, std::string>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        playlist::reflag_container_atom_v);

                    result[SessionModel::Roles::flagRole] = QStringFromStd(json(flag).dump());
                }
                break;
            case SessionModel::Roles::audioActorUuidRole:
                if (json_.at("type") == "Media") {
                    auto answer = request_receive<UuidActor>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::current_media_source_atom_v,
                        media::MT_AUDIO);

                    result[SessionModel::Roles::audioActorUuidRole] =
                        QStringFromStd(json(answer.uuid()).dump());
                } else if (json_.at("type") == "MediaSources") {
                    auto answer = request_receive<UuidActor>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        media::current_media_stream_atom_v,
                        media::MT_AUDIO);

                    result[SessionModel::Roles::audioActorUuidRole] =
                        QStringFromStd(json(answer.uuid()).dump());
                }
                break;

            case SessionModel::Roles::childrenRole:
                if (json_.at("type") == "Session") {
                    auto session    = actorFromString(system_, json_.at("actor"));
                    auto containers = request_receive<utility::PlaylistTree>(
                        *sys, session, playlist::get_container_atom_v);
                    auto actors = request_receive<std::vector<UuidActor>>(
                        *sys, session, session::get_playlists_atom_v);

                    result[SessionModel::Roles::childrenRole] = QStringFromStd(
                        "[" +
                        SessionModel::playlistTreeToJson(
                            containers, system_, uuidactor_vect_to_map(actors))
                            .dump() +
                        "]");
                } else if (json_.at("type") == "Playlist Media") {
                    spdlog::error(
                        "get Playlist Media children {}",
                        json_.at("actor_owner").get<std::string>());
                    auto detail = request_receive<std::vector<ContainerDetail>>(
                        *sys,
                        actorFromString(system_, json_.at("actor_owner")),
                        playlist::get_media_atom_v,
                        true);
                    auto jsn = R"([])"_json;
                    for (const auto &i : detail)
                        jsn.emplace_back(SessionModel::containerDetailToJson(i, system_));

                    result[SessionModel::Roles::childrenRole] = QStringFromStd(jsn.dump());
                } else if (json_.at("type") == "Playlist Container") {
                    spdlog::error(
                        "get Playlist Container children {}",
                        json_.at("actor_owner").get<std::string>());
                    auto containers = request_receive<utility::PlaylistTree>(
                        *sys,
                        actorFromString(system_, json_.at("actor_owner")),
                        playlist::get_container_atom_v);
                    auto actors = request_receive<std::vector<UuidActor>>(
                        *sys,
                        actorFromString(system_, json_.at("actor_owner")),
                        playlist::get_container_atom_v,
                        true);
                    result[SessionModel::Roles::childrenRole] = QStringFromStd(
                        "[" +
                        SessionModel::playlistTreeToJson(
                            containers, system_, uuidactor_vect_to_map(actors))
                            .dump() +
                        "]");
                } else if (json_.at("type") == "Media") {
                    auto detail = request_receive<std::vector<ContainerDetail>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        utility::detail_atom_v,
                        true);
                    auto jsn = R"([])"_json;
                    for (const auto &i : detail)
                        jsn.emplace_back(SessionModel::containerDetailToJson(i, system_));

                    result[SessionModel::Roles::childrenRole] = QStringFromStd(jsn.dump());
                } else if (json_.at("type") == "Subset") {
                    spdlog::error(
                        "get subset children {}", json_.at("actor").get<std::string>());
                    auto media = request_receive<std::vector<UuidActor>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        playlist::get_media_atom_v);
                    auto jsn = R"([])"_json;

                    for (const auto &i : media) {
                        auto item = SessionModel::createEntry(
                            R"({
                                "actor": null, "actor_uuid": null, "type": "Media",
                                "name": null, "group_actor": null,
                                "audio_actor_uuid": null, "image_actor_uuid": null,
                                "media_status": null, "flag":null
                            })"_json);
                        item["actor"]      = actorToString(system_, i.actor());
                        item["actor_uuid"] = i.uuid();
                        jsn.emplace_back(item);
                    }
                    // spdlog::error("{}", jsn.dump(2));

                    result[SessionModel::Roles::childrenRole] = QStringFromStd(jsn.dump());
                } else if (json_.at("type") == "MediaSource") {

                    auto idetail = request_receive<std::vector<ContainerDetail>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        utility::detail_atom_v,
                        media::MT_IMAGE);
                    auto adetail = request_receive<std::vector<ContainerDetail>>(
                        *sys,
                        actorFromString(system_, json_.at("actor")),
                        utility::detail_atom_v,
                        media::MT_AUDIO);

                    auto jsn = R"([{},{}])"_json;

                    jsn[0] = SessionModel::createEntry(
                        R"({"type": "Image Stream", "children": [], "actor_owner": null})"_json);
                    jsn[0]["actor_owner"] = json_.at("actor");
                    jsn[1]                = SessionModel::createEntry(
                        R"({"type": "Audio Stream", "children": [], "actor_owner": null})"_json);
                    jsn[1]["actor_owner"] = json_.at("actor");

                    for (const auto &i : idetail)
                        jsn[0]["children"].emplace_back(
                            SessionModel::containerDetailToJson(i, system_));
                    for (const auto &i : adetail)
                        jsn[1]["children"].emplace_back(
                            SessionModel::containerDetailToJson(i, system_));

                    result[SessionModel::Roles::childrenRole] = QStringFromStd(jsn.dump());
                } else {
                    spdlog::warn(
                        "CafRequest unhandled type {}", json_.at("type").get<std::string>());
                }
                break;
            }

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    }

  private:
    const nlohmann::json json_;
    const int role_;
};

CafResponse::CafResponse(
    const QVariant &search_value,
    const int search_role,
    const nlohmann::json &data,
    const int role,
    QThreadPool *pool)
    : search_value_(search_value), search_role_(search_role), role_(role) {

    // create a future..
    connect(
        &watcher_,
        &QFutureWatcher<QMap<int, QString>>::finished,
        this,
        &CafResponse::handleFinished);

    try {
        QFuture<QMap<int, QString>> future = JobExecutor::run(new CafRequest(data, role), pool);

        watcher_.setFuture(future);
    } catch (...) {
        deleteLater();
    }
}

CafResponse::CafResponse(
    const QVariant &search_value,
    const int search_role,
    const QModelIndex &index,
    const int role,
    QThreadPool *pool)
    : search_value_(search_value), search_role_(search_role), role_(role) {

    // create a future..
    connect(
        &watcher_,
        &QFutureWatcher<QMap<int, QString>>::finished,
        this,
        &CafResponse::handleFinished);

    try {
        if (index.isValid() and index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

            QFuture<QMap<int, QString>> future =
                JobExecutor::run(new CafRequest(j, role), pool);

            watcher_.setFuture(future);
        } else {
            deleteLater();
        }

    } catch (...) {
        deleteLater();
    }
}

void CafResponse::handleFinished() {
    if (watcher_.future().resultCount()) {
        auto result = watcher_.result();

        QMap<int, QString>::const_iterator i = result.constBegin();
        while (i != result.constEnd()) {
            emit received(search_value_, search_role_, i.key(), i.value());
            ++i;
        }

        deleteLater();
    }
}


SessionModel::SessionModel(QObject *parent) : super(parent) {
    tag_manager_ = new TagManagerUI(this);
    init(CafSystemObject::get_actor_system());

    setRoleNames({
        {"actorRole"},
        {"actorUuidRole"},
        {"audioActorUuidRole"},
        {"busyRole"},
        {"childrenRole"},
        {"containerUuidRole"},
        {"flagRole"},
        {"groupActorRole"},
        {"idRole"},
        {"imageActorUuidRole"},
        {"mediaStatusRole"},
        {"mtimeRole"},
        {"nameRole"},
        {"pathRole"},
        {"rateFPSRole"},
        {"thumbnailURLRole"},
        {"typeRole"},
    });
}

void SessionModel::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(caf::drop);
    setModified(false);

    try {
        utility::print_on_create(as_actor(), "SessionModel");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {

            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {},

            [=](json_store::update_atom, const utility::JsonStore &js) {},

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                // find this actor... in our data..
                // we need to map this to a string..
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info("event_atom name_atom {} {} {}", name, to_string(src), src_str);
                // search from index..
                receivedData(
                    QVariant::fromValue(QStringFromStd(src_str)),
                    actorRole,
                    nameRole,
                    QStringFromStd(json(name).dump()));
            },

            [=](utility::event_atom, playlist::add_media_atom, const UuidActor &ua) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info(
                    "utility::event_atom, playlist::add_media_atom {} {}",
                    to_string(ua.uuid()),
                    src_str);

                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                // trigger update of model..
                if (index.isValid() and index.internalPointer()) {

                    emit mediaAdded(index);

                    const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    // request update of containers.
                    try {
                        if (j.at("type") == "Playlist") {
                            // spdlog::warn("add_media_atom Playlist {}", j.dump(2));

                            index = SessionModel::index(0, 0, index);
                            if (index.isValid() and index.internalPointer()) {
                                const nlohmann::json &jj =
                                    *((nlohmann::json *)index.internalPointer());
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    childrenRole);
                            }
                        } else {
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                childrenRole);
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, playlist::create_divider_atom, const Uuid &uuid) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info(
                    "create_divider_atom {} {} {}", to_string(src), src_str, to_string(uuid));
                // request update of children..
                // find container owner..
                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                if (index.isValid() and index.internalPointer()) {
                    const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    // request update of containers.
                    try {
                        if (j.at("type") == "Playlist") {
                            spdlog::warn("create_divider_atom Playlist {}", j.dump(2));

                            index = SessionModel::index(1, 0, index);
                            if (index.isValid() and index.internalPointer()) {
                                const nlohmann::json &jj =
                                    *((nlohmann::json *)index.internalPointer());
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    childrenRole);
                            }
                        } else {
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                childrenRole);
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, playlist::create_group_atom, const Uuid & /*uuid*/) {},

            [=](utility::event_atom,
                session::path_atom,
                const std::pair<caf::uri, fs::file_time_type> &pt) {},

            [=](utility::event_atom, playlist::loading_media_atom, const bool value) {
                spdlog::info("utility::event_atom, playlist::loading_media_atom {}", value);

                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // request update of children..
                // find container owner..
                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                if (index.isValid() and index.internalPointer()) {
                    nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    // request update of containers.
                    try {
                        if (j.at("type") == "Playlist") {
                            if (j.count("busy") and j.at("busy") != value) {
                                j["busy"] = value;
                                emit dataChanged(index, index, QVector<int>({busyRole}));
                            }
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, session::current_playlist_atom, caf::actor playlist) {
                spdlog::info(
                    "utility::event_atom, session::current_playlist_atom {}",
                    to_string(playlist));
            },

            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid &src,
                const Uuid &before,
                const bool) {
                spdlog::info("utility::event_atom, playlist::move_container_atom");
                auto src_index = search_recursive(
                    QVariant::fromValue(QUuidFromUuid(src)), containerUuidRole);

                if (src_index.isValid()) {
                    if (before.is_null()) {
                        JSONTreeModel::moveRows(
                            src_index.parent(),
                            src_index.row(),
                            1,
                            src_index.parent(),
                            rowCount(src_index.parent()));
                    } else {
                        auto before_index = search_recursive(
                            QVariant::fromValue(QUuidFromUuid(before)), containerUuidRole);
                        spdlog::warn("move before {} {}", src_index.row(), before_index.row());
                        if (before_index.isValid()) {
                            JSONTreeModel::moveRows(
                                src_index.parent(),
                                src_index.row(),
                                1,
                                before_index.parent(),
                                before_index.row());
                        } else {
                            spdlog::warn("unfound before");
                        }
                    }
                } else {
                    spdlog::warn("unfound to end");
                }
                // spdlog::warn("{}", data_.dump(2));
            },


            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid &src,
                const Uuid &before) {
                spdlog::info("utility::event_atom, playlist::move_container_atom");
                auto src_index = search_recursive(
                    QVariant::fromValue(QUuidFromUuid(src)), containerUuidRole);

                if (src_index.isValid()) {
                    if (before.is_null()) {
                        JSONTreeModel::moveRows(
                            src_index.parent(),
                            src_index.row(),
                            1,
                            src_index.parent(),
                            rowCount(src_index.parent()));
                    } else {
                        auto before_index = search_recursive(
                            QVariant::fromValue(QUuidFromUuid(before)), containerUuidRole);
                        spdlog::warn("move before {} {}", src_index.row(), before_index.row());
                        if (before_index.isValid()) {
                            JSONTreeModel::moveRows(
                                src_index.parent(),
                                src_index.row(),
                                1,
                                before_index.parent(),
                                before_index.row());
                        } else {
                            spdlog::warn("unfound before");
                        }
                    }
                } else {
                    spdlog::warn("unfound to end");
                }
                // spdlog::warn("{}", data_.dump(2));
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid &uuid,
                const std::string &value) {
                spdlog::info("reflag_container_atom {} {}", to_string(uuid), value);
                receivedData(
                    QVariant::fromValue(QUuidFromUuid(uuid)),
                    containerUuidRole,
                    flagRole,
                    QStringFromStd(json(value).dump()));
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid &uuid,
                const std::tuple<std::string, std::string> &value) {
                spdlog::info("reflag_container_atom {}", to_string(uuid));

                const auto [flag, text] = value;
                receivedData(
                    QVariant::fromValue(QUuidFromUuid(uuid)),
                    actorUuidRole,
                    flagRole,
                    QStringFromStd(json(flag).dump()));
            },


            [=](utility::event_atom,
                media::current_media_source_atom,
                const utility::UuidActor &) {},
            [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::Uuid &) {},

            [=](utility::event_atom,
                playlist::remove_container_atom,
                const std::vector<Uuid> &uuids) {
                for (const auto &uuid : uuids) {
                    spdlog::info("remove_containers_atom {} {}", to_string(uuid));

                    auto index = search_recursive(
                        QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
                    if (index.isValid()) {
                        // use base class so we don't reissue the removal.
                        JSONTreeModel::removeRows(index.row(), 1, index.parent());
                    }
                }
            },

            [=](utility::event_atom, playlist::remove_container_atom, const Uuid &uuid) {
                // find container entry and remove it..
                spdlog::info("remove_container_atom {} {}", to_string(uuid));
                auto index = search_recursive(
                    QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
                if (index.isValid()) {
                    // use base class so we don't reissue the removal.
                    JSONTreeModel::removeRows(index.row(), 1, index.parent());
                }
            },

            // [=](utility::event_atom, playlist::remove_container_atom, const Uuid &uuid, const
            // bool) {
            //     // find container entry and remove it..
            //     spdlog::info("remove_container_atom {} {}", to_string(uuid));
            //     auto index = search_recursive(
            //         QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
            //     if (index.isValid()) {
            //         // use base class so we don't reissue the removal.
            //         JSONTreeModel::removeRows(index.row(), 1, index.parent());
            //     }
            // },

            [=](utility::event_atom,
                playlist::rename_container_atom,
                const Uuid &uuid,
                const std::string &name) {
                // update model..
                spdlog::info("rename_container_atom {} {}", to_string(uuid), name);
                receivedData(
                    QVariant::fromValue(QUuidFromUuid(uuid)),
                    containerUuidRole,
                    nameRole,
                    QStringFromStd(json(name).dump()));
            },

            [=](utility::event_atom, session::add_playlist_atom, const UuidActor &ua) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info(
                    "add_playlist_atom {} {} {}",
                    to_string(src),
                    src_str,
                    to_string(ua.uuid()));
                // request update of children..
                // find container owner..
                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                if (index.isValid() and index.internalPointer()) {
                    try {
                        const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                        // request update of containers.
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            childrenRole);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, session::media_rate_atom, const FrameRate &rate) {},

            [=](utility::event_atom,
                playlist::media_content_changed_atom,
                const utility::UuidActorVector &) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info(
                    "utility::event_atom, playlist::media_content_changed_atom {}", src_str);

                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                // trigger update of model..
                if (index.isValid() and index.internalPointer()) {

                    emit mediaAdded(index);

                    const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    // request update of containers.
                    try {
                        if (j.at("type") == "Playlist") {
                            // spdlog::warn("add_media_atom Playlist {}", j.dump(2));

                            index = SessionModel::index(0, 0, index);
                            if (index.isValid() and index.internalPointer()) {
                                const nlohmann::json &jj =
                                    *((nlohmann::json *)index.internalPointer());
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    childrenRole);
                            }
                        } else {
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                childrenRole);
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, utility::change_atom) {
                // arbitary change ..
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);

                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                spdlog::info("utility::event_atom, utility::change_atom {}", src_str);

                // trigger update of model..
                if (index.isValid() and index.internalPointer()) {
                    const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    if (j.at("type") == "Subset") {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            childrenRole);
                    }
                }
            },

            [=](utility::event_atom,
                utility::last_changed_atom,
                const time_point &last_changed) {
                auto src = caf::actor_cast<caf::actor>(self()->current_sender());
                if (src == session_actor_) {
                    last_changed_ = last_changed;
                    emit modifiedChanged();
                }
            },

            [=](utility::event_atom,
                playlist::create_subset_atom,
                const utility::UuidActor &ua) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                spdlog::info(
                    "create_subset_atom {} {} {}",
                    to_string(src),
                    src_str,
                    to_string(ua.uuid()));
                // request update of children..
                // find container owner..
                auto index =
                    search_recursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                if (index.isValid() and index.internalPointer()) {
                    const nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                    // request update of containers.
                    try {
                        if (j.at("type") == "Playlist") {
                            spdlog::warn("create_subset_atom Playlist {}", j.dump(2));

                            index = SessionModel::index(1, 0, index);
                            if (index.isValid() and index.internalPointer()) {
                                const nlohmann::json &jj =
                                    *((nlohmann::json *)index.internalPointer());
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    childrenRole);
                            }
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },
            [=](utility::event_atom,
                playlist::create_contact_sheet_atom,
                const utility::UuidActor &) {},
            [=](utility::event_atom,
                playlist::create_timeline_atom,
                const utility::UuidActor &) {},

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &g) {
                caf::aout(self()) << "down: " << to_string(g.source) << std::endl;
            }

        };
    });
}

void SessionModel::setSessionActorAddr(const QString &addr) {
    try {
        if (addr != session_actor_addr_) {
            scoped_actor sys{system()};

            setModified(false);

            auto data = R"([])"_json;

            data.push_back(createEntry(R"({
                "type": "Session",
                "path": null,
                "mtime": null,
                "name": null,
                "actor_uuid": null,
                "actor": null,
                "group_actor": null,
                "container_uuid": null,
                "flag": null})"_json));

            session_actor_addr_ = addr;
            emit sessionActorAddrChanged();

            session_actor_ = actorFromQString(system(), addr);
            spdlog::warn("setSessionActorAddr {}", to_string(session_actor_));
            data[0]["actor"] = StdFromQString(addr);

            try {
                auto actor =
                    request_receive<caf::actor>(*sys, session_actor_, tag::get_tag_atom_v);
                tag_manager_->set_backend(actor);
                emit tagsChanged();

            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }

            setModelData(data);


            // if (backend_events_) {
            //     try {
            //         request_receive<bool>(
            //             *sys, backend_events_, broadcast::leave_broadcast_atom_v,
            //             as_actor());
            //     } catch (const std::exception &e) {
            //     }
            //     backend_events_ = caf::actor();
            // }

            // join bookmark events
            if (session_actor_) {
                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_[0].at("id"))),
                    idRole,
                    data_[0],
                    pathRole);

                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_[0].at("id"))),
                    idRole,
                    data_[0],
                    groupActorRole);

                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_[0].at("id"))),
                    idRole,
                    data_[0],
                    childrenRole);

                try {
                    bookmark_actor_addr_ = actorToQString(
                        system(),
                        request_receive<caf::actor>(
                            *sys, session_actor_, bookmark::get_bookmark_atom_v));
                    emit bookmarkActorAddrChanged();
                } catch (const std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.isValid() and index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

            switch (role) {
            case Roles::typeRole:
                if (j.count("type"))
                    result = QString::fromStdString(j.at("type"));
                break;

            case Roles::idRole:
                if (j.count("id"))
                    result = QVariant::fromValue(QUuidFromUuid(j.at("id")));
                break;

            case Roles::busyRole:
                if (j.count("busy"))
                    result = j.at("busy").get<bool>();
                break;

            case Roles::pathRole:
                if (j.count("path")) {
                    if (j.at("path").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    } else {
                        auto uri = caf::make_uri(j.at("path"));
                        if (uri)
                            result = QVariant::fromValue(QUrlFromUri(*uri));
                    }
                }
                break;

            case Roles::rateFPSRole:
                if (j.count("rate")) {
                    if (j.at("rate").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    } else {
                        result = QVariant::fromValue(j.at("rate").get<FrameRate>().to_fps());
                    }
                }
                break;

            case Roles::mtimeRole:
                if (j.count("mtime") and not j.at("mtime").is_null())
                    result = QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            j.at("mtime").get<fs::file_time_type>().time_since_epoch())
                            .count()));
                break;


            case Roles::mediaStatusRole:
                if (j.count("media_status")) {
                    if (j.at("media_status").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QVariant::fromValue(QStringFromStd(to_string(
                            static_cast<media::MediaStatus>(j.at("media_status").get<int>()))));
                }
                break;

            case Roles::audioActorUuidRole:
                if (j.count("audio_actor_uuid")) {
                    if (j.at("audio_actor_uuid").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QVariant::fromValue(QUuidFromUuid(j.at("audio_actor_uuid")));
                }
                break;

            case Roles::imageActorUuidRole:
                if (j.count("image_actor_uuid")) {
                    if (j.at("image_actor_uuid").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QVariant::fromValue(QUuidFromUuid(j.at("image_actor_uuid")));
                }
                break;

            case Roles::actorRole:
                if (j.count("actor")) {
                    if (j.at("actor").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("actor"));
                }
                break;

            case Roles::groupActorRole:
                if (j.count("group_actor")) {
                    if (j.at("group_actor").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("group_actor"));
                }
                break;

            case Roles::actorUuidRole:
                if (j.count("actor_uuid")) {
                    if (j.at("actor_uuid").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else {
                        result = QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid")));
                    }
                }
                break;

            case Roles::thumbnailURLRole:
                if (j.count("thumbnail_url")) {
                    if (j.at("thumbnail_url").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else {
                        auto uri = caf::make_uri(j.at("thumbnail_url"));
                        if (uri)
                            result = QVariant::fromValue(QUrlFromUri(*uri));
                    }
                }
                break;

            case Roles::containerUuidRole:
                if (j.count("container_uuid"))
                    result = QVariant::fromValue(QUuidFromUuid(j.at("container_uuid")));
                break;

            case Roles::flagRole:
                if (j.count("flag")) {
                    if (j.at("flag").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    } else
                        result = QString::fromStdString(j.at("flag"));
                }

                break;

            case Qt::DisplayRole:
            case Roles::nameRole:
                if (j.count("name")) {
                    if (j.at("name").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("name"));
                }
                break;
            case Roles::childrenRole:
                if (j.count("children")) {
                    if (j.at("children").is_null()) {
                        // stop more requests being sent..
                        j["children"] = R"([])"_json;
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            role);
                    } else
                        result = QVariantMapFromJson(j.at("children"));
                }
                break;
            default:
                result = JSONTreeModel::data(index, role);
                break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalPointer());
        if (index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
            spdlog::warn("{}", j.dump(2));
        }
    }

    return result;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &qvalue, int role) {
    bool result = false;
    QVector<int> roles({role});

    try {
        if (index.isValid() and index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
            auto value        = mapFromValue(qvalue);
            auto type  = j.count("type") ? j.at("type").get<std::string>() : std::string();
            auto actor = j.count("actor") and not j.at("actor").is_null()
                             ? actorFromString(system(), j.at("actor"))
                             : caf::actor();

            switch (role) {

            case busyRole:
                if (j.count("busy") and j["busy"] != value) {
                    j["busy"] = value;
                    result    = true;
                }
                break;

            case rateFPSRole:
                if (type == "MediaSource" and j.count("rate") and actor) {
                    auto fps = std::stod(value.get<std::string>());
                    auto fr  = FrameRate(1.0 / (fps > 0.1 ? fps : 0.1));
                    auto r   = json(fr);

                    if (j["rate"] != r) {
                        spdlog::warn("rateFPSRole {} {}", j["rate"].dump(), r.dump());
                        scoped_actor sys{system()};

                        auto mr = request_receive<MediaReference>(
                            *sys, actor, media::media_reference_atom_v);
                        mr.set_rate(fr);
                        anon_send(actor, media::media_reference_atom_v, mr);
                        j["rate"] = r;
                        result    = true;
                    }
                }
                break;

            case nameRole:
                if (j.count("name") and j["name"] != value) {
                    if ((type == "Session" or type == "Subset" or type == "Playlist") and
                        actor) {
                        spdlog::warn("Send update {} {}", j["name"], value);
                        anon_send(actor, utility::name_atom_v, value.get<std::string>());
                        j["name"] = value;
                        result    = true;
                    } else if (type == "ContainerDivider") {
                        auto p = index.parent();
                        if (p.isValid() and p.internalPointer()) {
                            nlohmann::json &pj = *((nlohmann::json *)p.internalPointer());
                            auto pactor = pj.count("actor") and not pj.at("actor").is_null()
                                              ? actorFromString(system(), pj.at("actor"))
                                              : caf::actor();
                            if (not pactor) {
                                pactor = pj.count("actor_owner") and
                                                 not pj.at("actor_owner").is_null()
                                             ? actorFromString(system(), pj.at("actor_owner"))
                                             : caf::actor();
                            }

                            if (pactor) {
                                spdlog::warn("Send update {} {}", j["name"], value);
                                anon_send(
                                    pactor,
                                    playlist::rename_container_atom_v,
                                    value.get<std::string>(),
                                    j.at("container_uuid").get<Uuid>());
                                j["name"] = value;
                                result    = true;
                            }
                        }
                    }
                }
                break;
            case flagRole:
                if (j.count("flag") and j["flag"] != value) {
                    if (type == "Media") {
                        if (index.isValid() and index.internalPointer()) {
                            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                            auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                    ? actorFromString(system(), j.at("actor"))
                                                    : caf::actor();
                            if (actor) {
                                spdlog::warn("Send update {} {}", j["flag"], value);
                                anon_send(
                                    actor,
                                    playlist::reflag_container_atom_v,
                                    std::make_tuple(
                                        std::optional<std::string>(value.get<std::string>()),
                                        std::optional<std::string>()));
                                j["flag"] = value;
                                result    = true;
                            }
                        }
                    } else if (
                        type == "ContainerDivider" or type == "Subset" or type == "Playlist") {
                        auto p = index.parent();
                        if (p.isValid() and p.internalPointer()) {
                            nlohmann::json &pj = *((nlohmann::json *)p.internalPointer());
                            auto pactor = pj.count("actor") and not pj.at("actor").is_null()
                                              ? actorFromString(system(), pj.at("actor"))
                                              : caf::actor();
                            if (not pactor) {
                                pactor = pj.count("actor_owner") and
                                                 not pj.at("actor_owner").is_null()
                                             ? actorFromString(system(), pj.at("actor_owner"))
                                             : caf::actor();
                            }

                            if (pactor) {
                                spdlog::warn("Send update {} {}", j["flag"], value);
                                anon_send(
                                    pactor,
                                    playlist::reflag_container_atom_v,
                                    value.get<std::string>(),
                                    j.at("container_uuid").get<Uuid>());
                                j["flag"] = value;
                                result    = true;
                            }
                        }
                    }
                }
                break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result) {
        emit dataChanged(index, index, roles);
    }

    return result;
}

caf::actor SessionModel::actorFromIndex(const QModelIndex &index, const bool try_parent) {
    auto result = caf::actor();

    if (index.isValid() and index.internalPointer()) {
        nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
        result            = j.count("actor") and not j.at("actor").is_null()
                                ? actorFromString(system(), j.at("actor"))
                                : caf::actor();

        if (not result and try_parent) {
            QModelIndex pindex = index.parent();
            if (pindex.isValid() and pindex.internalPointer()) {
                nlohmann::json &pj = *((nlohmann::json *)pindex.internalPointer());
                result             = pj.count("actor") and not pj.at("actor").is_null()
                                         ? actorFromString(system(), pj.at("actor"))
                                         : caf::actor();
            }
        }
    }
    return result;
}

utility::Uuid
SessionModel::actorUuidFromIndex(const QModelIndex &index, const bool try_parent) {
    auto result = utility::Uuid();

    if (index.isValid() and index.internalPointer()) {
        nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
        result            = j.count("actor_uuid") and not j.at("actor_uuid").is_null()
                                ? j.at("actor_uuid").get<Uuid>()
                                : utility::Uuid();

        if (result.is_null() and try_parent) {
            QModelIndex pindex = index.parent();
            if (pindex.isValid() and pindex.internalPointer()) {
                nlohmann::json &pj = *((nlohmann::json *)pindex.internalPointer());
                result = pj.count("actor_uuid") and not pj.at("actor_uuid").is_null()
                             ? pj.at("actor_uuid").get<Uuid>()
                             : utility::Uuid();
            }
        }
    }
    return result;
}

Q_INVOKABLE bool
SessionModel::removeRows(int row, int count, const bool deep, const QModelIndex &parent) {
    // check if we can delete it..
    auto can_delete = false;
    auto result     = false;
    spdlog::warn("removeRows {} {}", row, count);

    try {
        for (auto i = row; i < row + count; i++) {
            auto index = SessionModel::index(i, 0, parent);
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                if (j.at("type") == "ContainerDivider" or j.at("type") == "Subset") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    if (pactor) {
                        spdlog::warn("Send remove {}", j["container_uuid"]);
                        anon_send(
                            pactor,
                            playlist::remove_container_atom_v,
                            j.at("container_uuid").get<Uuid>());
                        can_delete = true;
                    }
                } else if (j.at("type") == "Playlist") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    if (pactor) {
                        spdlog::warn("Send remove {}", j["container_uuid"]);
                        anon_send(
                            pactor,
                            playlist::remove_container_atom_v,
                            j.at("container_uuid").get<Uuid>());
                        can_delete = true;
                    }

                } else if (j.at("type") == "Media") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    if (pactor) {
                        spdlog::warn("Send remove {}", j["container_uuid"]);
                        anon_send(
                            pactor,
                            playlist::remove_media_atom_v,
                            j.at("actor_uuid").get<Uuid>());
                        can_delete = true;
                    }
                } else {
                    spdlog::warn("unhandled {}", j.dump(2));
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (can_delete)
        result = JSONTreeModel::removeRows(row, count, parent);

    return result;
}

bool SessionModel::removeRows(int row, int count, const QModelIndex &parent) {
    return removeRows(row, count, false, parent);
}

bool SessionModel::duplicateRows(int row, int count, const QModelIndex &parent) {
    auto can_duplicate = false;
    auto result        = false;
    spdlog::warn("duplicateRows {} {}", row, count);

    try {
        auto before = Uuid();
        try {
            auto before_ind = SessionModel::index(row + 1, 0, parent);
            if (before_ind.isValid() and before_ind.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)before_ind.internalPointer());
                if (j.count("container_uuid"))
                    before = j.at("container_uuid").get<Uuid>();
                else if (j.count("actor_uuid"))
                    before = j.at("actor_uuid").get<Uuid>();
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        for (auto i = row; i < row + count; i++) {
            auto index = SessionModel::index(i, 0, parent);
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

                if (j.at("type") == "ContainerDivider" or j.at("type") == "Subset" or
                    j.at("type") == "Playlist") {
                    auto pactor = actorFromIndex(index.parent(), true);

                    if (pactor) {
                        spdlog::warn("Send Duplicate {}", j["container_uuid"]);
                        anon_send(
                            pactor,
                            playlist::duplicate_container_atom_v,
                            j.at("container_uuid").get<Uuid>(),
                            before,
                            false);
                        can_duplicate = true;
                    }
                } else if (j.at("type") == "Media") {
                    // find parent
                    auto uuid = actorUuidFromIndex(parent, true);

                    if (not uuid.is_null()) {
                        anon_send(
                            session_actor_,
                            playlist::copy_media_atom_v,
                            uuid,
                            UuidVector({j.at("actor_uuid").get<Uuid>()}),
                            true,
                            before,
                            false);
                    }

                } else {
                    spdlog::warn(
                        "duplicateRows unhandled type {}", j.at("type").get<std::string>());
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // if(can_duplicate)
    //     result = JSONTreeModel::insertRows(row, count, parent);

    return result;
}


QModelIndexList SessionModel::copyRows(
    const QModelIndexList &indexes, const int row, const QModelIndex &parent) {
    // only media to subset and playlists.
    // make sure indexes are valid.. count..
    // if parent is playlist the media needs to be inserted in the media child.
    QModelIndexList result;
    try {
        UuidVector media_uuids;
        Uuid target;

        auto media_parent = parent;
        if (parent.isValid() and parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            spdlog::warn("copyRows {}", j.dump(2));


            if (j.at("type") == "Playlist") {
                media_parent = index(0, 0, parent);
                target       = j.at("actor_uuid");
            } else {
                target = j.at("actor_uuid");
            }
        } else {
            throw std::runtime_error("invalid parent index");
        }

        for (auto &i : indexes) {
            if (i.isValid() and i.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)i.internalPointer());
                if (j.at("type") == "Media")
                    media_uuids.emplace_back(j.at("actor_uuid"));
            }
        }

        // insert dummy entries.
        auto before    = Uuid();
        auto insertrow = index(row, 0, media_parent);
        if (insertrow.isValid() and insertrow.internalPointer()) {
            nlohmann::json &irj = *((nlohmann::json *)insertrow.internalPointer());
            before              = irj.at("actor_uuid");
        }

        JSONTreeModel::insertRows(
            row, media_uuids.size(), media_parent, R"({"type": "Media"})"_json);
        for (int i = 0; i < media_uuids.size(); i++) {
            result.push_back(index(row + i, 0, media_parent));
        }

        // send action to backend.
        anon_send(
            session_actor_,
            playlist::copy_media_atom_v,
            target,
            media_uuids,
            false,
            before,
            false);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

QModelIndexList SessionModel::moveRows(
    const QModelIndexList &indexes, const int row, const QModelIndex &parent) {
    QModelIndexList result;
    try {
        UuidVector media_uuids;
        Uuid target;
        Uuid source;

        auto media_parent = parent;
        if (parent.isValid() and parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            spdlog::warn("moveRows {}", j.dump(2));


            if (j.at("type") == "Playlist") {
                media_parent = index(0, 0, parent);
                target       = j.at("actor_uuid");
            } else {
                target = j.at("container_uuid");
            }
        } else {
            throw std::runtime_error("invalid parent index");
        }

        for (auto &i : indexes) {
            if (i.isValid() and i.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)i.internalPointer());
                if (j.at("type") == "Media") {
                    media_uuids.emplace_back(j.at("actor_uuid"));
                    if (source.is_null()) {
                        // get media owner
                        source = actorUuidFromIndex(i.parent(), true);
                    }
                }
            }
        }

        // insert dummy entries.
        auto before    = Uuid();
        auto insertrow = index(row, 0, media_parent);
        if (insertrow.isValid() and insertrow.internalPointer()) {
            nlohmann::json &irj = *((nlohmann::json *)insertrow.internalPointer());
            before              = irj.at("actor_uuid");
        }

        JSONTreeModel::insertRows(
            row, media_uuids.size(), media_parent, R"({"type": "Media"})"_json);
        for (int i = 0; i < media_uuids.size(); i++) {
            result.push_back(index(row + i, 0, media_parent));
        }

        // send action to backend.
        anon_send(
            session_actor_,
            playlist::move_media_atom_v,
            target,
            source,
            media_uuids,
            before,
            false);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QModelIndexList SessionModel::insertRows(
    int row,
    int count,
    const QString &qtype,
    const QString &qname,
    const QModelIndex &parent,
    const bool sync) {

    auto result = QModelIndexList();

    try {
        if (parent.isValid() and parent.internalPointer()) {
            const auto type = StdFromQString(qtype);
            const auto name = StdFromQString(qname);
            scoped_actor sys{system()};
            spdlog::warn("SessionModel::insertRows {} {} {} {}", row, count, type, name);

            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            // spdlog::warn("{}", j.dump(2));

            auto before    = Uuid();
            auto insertrow = index(row, 0, parent);
            auto actor     = caf::actor();

            if (insertrow.isValid() and insertrow.internalPointer()) {
                nlohmann::json &irj = *((nlohmann::json *)insertrow.internalPointer());
                before              = irj.at("container_uuid");
            }

            if (type == "ContainerDivider") {
                if (j.at("type") == "Session") {
                    actor = j.count("actor") and not j.at("actor").is_null()
                                ? actorFromString(system(), j.at("actor"))
                                : caf::actor();
                } else {
                    actor = j.count("actor_owner") and not j.at("actor_owner").is_null()
                                ? actorFromString(system(), j.at("actor_owner"))
                                : caf::actor();
                    // parent is nested..
                }

                if (actor) {
                    nlohmann::json &pj = *((nlohmann::json *)parent.internalPointer());
                    spdlog::warn("divider parent {}", pj.dump(2));

                    if (before.is_null())
                        row = rowCount(parent);

                    JSONTreeModel::insertRows(
                        row, count, parent, R"({"type": "ContainerDivider"})"_json);

                    for (auto i = 0; i < count; i++) {
                        if (not sync) {
                            anon_send(
                                actor, playlist::create_divider_atom_v, name, before, false);
                        } else {
                            auto new_item = request_receive<Uuid>(
                                *sys,
                                actor,
                                playlist::create_divider_atom_v,
                                name,
                                before,
                                false);
                        }
                        result.push_back(index(row + i, 0, parent));
                    }
                }
            } else if (type == "Subset") {
                actor = j.count("actor_owner") and not j.at("actor_owner").is_null()
                            ? actorFromString(system(), j.at("actor_owner"))
                            : caf::actor();

                if (actor) {
                    if (before.is_null())
                        row = rowCount(parent);

                    JSONTreeModel::insertRows(row, count, parent, R"({"type": "Subset"})"_json);

                    spdlog::warn("JSONTreeModel::insertRows({}, {}, parent);", row, count);

                    for (auto i = 0; i < count; i++) {
                        if (not sync) {
                            anon_send(
                                actor, playlist::create_subset_atom_v, name, before, false);
                        } else {
                            auto new_item = request_receive<UuidUuidActor>(
                                *sys,
                                actor,
                                playlist::create_subset_atom_v,
                                name,
                                before,
                                false);
                        }
                        result.push_back(index(row + i, 0, parent));
                    }
                }
            } else if (type == "Playlist") {
                if (j.at("type") == "Session") {
                    actor = j.count("actor") and not j.at("actor").is_null()
                                ? actorFromString(system(), j.at("actor"))
                                : caf::actor();

                    if (before.is_null())
                        row = rowCount(parent);

                    JSONTreeModel::insertRows(
                        row, count, parent, R"({"type": "Playlist"})"_json);

                    spdlog::warn("JSONTreeModel::insertRows({}, {}, parent);", row, count);

                    for (auto i = 0; i < count; i++) {
                        if (not sync) {
                            anon_send(actor, session::add_playlist_atom_v, name, before, false);
                        } else {
                            auto tmp = request_receive<UuidUuidActor>(
                                *sys, actor, session::add_playlist_atom_v, name, before, false);
                        }
                        spdlog::warn("ROW {}, {}", row + i, data_.dump(2));
                        result.push_back(index(row + i, 0, parent));
                    }
                }
            } else {
                spdlog::warn("insertRows: unsupported type {}", type);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

void SessionModel::processChildren(
    const nlohmann::json &rj, nlohmann::json &ij, const QModelIndex &index) {
    QVector<int> roles({Roles::childrenRole});
    auto changed = false;

    // spdlog::info("{}", rj.dump(2));
    // spdlog::info("{}", ij.dump(2));

    try {
        if (ij.at("type") == "Session" or ij.at("type") == "Playlist Container" or
            ij.at("type") == "Subset" or ij.at("type") == "Media" or
            ij.at("type") == "Playlist Media") {

            // point to result  children..
            auto rjc = nlohmann::json();
            if (rj.is_array())
                if (ij.at("type") == "Session" or ij.at("type") == "Playlist Container")
                    rjc = rj.at(0).at("children");
                else
                    rjc = rj;
            else
                rjc = rj.at("children");


            // init local children if null
            if (ij.at("children").is_null()) {
                ij["children"] = R"([])"_json;

                // special handling for nested..
                if (ij.at("type") == "Session" or ij.at("type") == "Playlist Container") {
                    ij["container_uuid"] = rj.at(0).at("container_uuid");
                    roles.append(containerUuidRole);
                    ij["flag"] = rj.at(0).at("flag");
                    roles.append(flagRole);
                }
                changed = true;
            }

            auto &ijc = ij["children"];

            // if (ij.at("type") == "Session" or ij.at("type") == "Playlist Container")
            //     rjc = rj.at(0).at("children");

            // compare children, append / insert / remove..
            spdlog::info("processChildren {} {}", ij.at("type").get<std::string>(), rjc.size());

            // set up comparison key
            auto compare_key = "";
            if (ij.at("type") == "Session" or ij.at("type") == "Playlist Container")
                compare_key = "container_uuid";
            else if (
                ij.at("type") == "Subset" or ij.at("type") == "Playlist Media" or
                ij.at("type") == "Media")
                compare_key = "actor_uuid";

            // remove delete entries
            // build lookup.
            std::set<Uuid> rju;
            for (size_t i = 0; i < rjc.size(); i++) {
                rju.insert(rjc.at(i).at(compare_key).get<Uuid>());
            }

            bool removal_done = false;
            while (not removal_done) {
                removal_done = true;
                for (size_t i = 0; i < ijc.size(); i++) {
                    if (ijc.at(i).count(compare_key) and
                        not rju.count(ijc.at(i).at(compare_key))) {
                        spdlog::info("REMOVING {} {}", i, to_string(ijc.at(i).at(compare_key)));
                        JSONTreeModel::removeRows(i, 1, index);
                        removal_done = false;
                        build_parent_map();
                        changed = true;
                    }
                }
            }

            // append / insert / re-order
            std::map<Uuid, size_t> iju;
            for (size_t i = 0; i < ijc.size(); i++) {
                if (ijc.at(i).count(compare_key))
                    iju[ijc.at(i).at(compare_key).get<Uuid>()] = i;
            }

            // process new / changed.

            for (size_t i = 0; i < rjc.size(); i++) {
                // check for append..
                if (i >= ijc.size()) {
                    spdlog::warn("append {}", i);
                    beginInsertRows(index, i, i);
                    ijc.push_back(rjc.at(i));
                    endInsertRows();
                    build_parent_map();

                    if (ijc.back().count("group_actor") and
                        not ijc.back().at("group_actor").is_null()) {
                        auto grp = actorFromString(
                            system(), ijc.back().at("group_actor").get<std::string>());
                        if (grp) {
                            anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
                            spdlog::error(
                                "join grp {} {} {}",
                                ijc.back().at("type").is_string()
                                    ? ijc.back().at("type").get<std::string>()
                                    : "",
                                ijc.back().at("name").is_string()
                                    ? ijc.back().at("name").get<std::string>()
                                    : "",
                                to_string(grp));
                        }
                    } else if (
                        ijc.back().count("actor") and not ijc.back().at("actor").is_null() and
                        ijc.back().at("group_actor").is_null()) {
                        spdlog::info("request group {}", i);
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(ijc.back().at("id"))),
                            Roles::idRole,
                            ijc.back(),
                            Roles::groupActorRole);
                    }
                    changed = true;
                } else {
                    if (not ijc.at(i).count(compare_key)) {
                        // place holder overwrite.
                        spdlog::warn("replace {}", i);
                        ijc.at(i) = rjc.at(i);
                        build_parent_map();
                        if (ijc.back().count("group_actor") and
                            not ijc.back().at("group_actor").is_null()) {
                            auto grp = actorFromString(
                                system(), ijc.back().at("group_actor").get<std::string>());
                            if (grp) {
                                anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
                                spdlog::error(
                                    "join grp {} {} {}",
                                    ijc.back().at("type").is_string()
                                        ? ijc.back().at("type").get<std::string>()
                                        : "",
                                    ijc.back().at("name").is_string()
                                        ? ijc.back().at("name").get<std::string>()
                                        : "",
                                    to_string(grp));
                            }

                        } else if (
                            ijc.back().count("actor") and
                            not ijc.back().at("actor").is_null() and
                            ijc.back().at("group_actor").is_null()) {
                            spdlog::info("request group {}", i);
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(ijc.back().at("id"))),
                                Roles::idRole,
                                ijc.back(),
                                Roles::groupActorRole);
                        }

                        roles.clear();
                        // force updated of replace child.
                        emit dataChanged(
                            SessionModel::index(i, 0, index),
                            SessionModel::index(i, 0, index),
                            roles);

                        changed = true;
                    } else if (ijc.at(i).at(compare_key) == rjc.at(i).at(compare_key)) {
                        // skip..
                        spdlog::warn("skip {}", i);
                    } else if (not iju.count(rjc.at(i).at(compare_key))) {
                        // insert
                        spdlog::warn("insert {}", i);
                        beginInsertRows(index, i, i);
                        ijc.insert(ijc.begin() + i, rjc.at(i));
                        endInsertRows();
                        build_parent_map();

                        if (ijc.back().count("group_actor") and
                            not ijc.back().at("group_actor").is_null()) {
                            auto grp = actorFromString(
                                system(), ijc.back().at("group_actor").get<std::string>());
                            if (grp) {
                                anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
                                spdlog::error(
                                    "join grp {} {} {}",
                                    ijc.back().at("type").is_string()
                                        ? ijc.back().at("type").get<std::string>()
                                        : "",
                                    ijc.back().at("name").is_string()
                                        ? ijc.back().at("name").get<std::string>()
                                        : "",
                                    to_string(grp));
                            }
                        } else if (
                            ijc.back().count("actor") and
                            not ijc.at(i).at("actor").is_null() and
                            ijc.at(i).at("group_actor").is_null()) {
                            spdlog::info("request group {}", i);
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(ijc.at(i).at("id"))),
                                Roles::idRole,
                                ijc.at(i),
                                Roles::groupActorRole);
                        }
                        changed = true;
                    } else {
                        // reorder
                        // result row doesn't match input row.
                        // find result in input and move to this position
                        spdlog::warn("MOVE {} to {}", i, iju[rjc.at(i).at(compare_key)]);
                    }
                }
            }
            spdlog::error("{}", ijc.dump(2));
        } else {
            spdlog::warn(
                "SessionModel::processChildren unhandled {}", ij.at("type").get<std::string>());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (changed)
        emit dataChanged(index, index, roles);
}

void SessionModel::receivedData(
    const QVariant &search_value,
    const int search_role,
    const int role,
    const QString &result) {
    try {
        // spdlog::warn("receivedData {}", data_.dump(2));
        auto jsn = json::parse(StdFromQString(result));
        // find index...

        auto indexes = search_recursive(
            search_value, search_role, QModelIndex(), 0, search_role == idRole ? 1 : -1);

        // spdlog::warn("receivedData found {} {} {}", mapFromValue(search_value).dump(),
        // search_role, search_role == idRole); spdlog::warn("receivedData found {}",
        // indexes.size());

        for (auto &index : indexes) {

            // spdlog::warn("receivedData {}", jsn.dump(2));

            if (index.isValid() and index.internalPointer()) {
                QVector<int> roles({role});
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

                switch (role) {
                case Roles::pathRole:
                    if (j.count("path") and j["path"] != jsn) {
                        spdlog::warn("Roles::pathRole changed {}", jsn.dump(2));
                        j["path"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;
                case Roles::rateFPSRole:
                    if (j.count("rate") and j["rate"] != jsn) {
                        spdlog::warn("Roles::rateFPSRole changed {}", jsn.dump(2));
                        j["rate"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;
                case Roles::mtimeRole:
                    if (j.count("mtime") and j["mtime"] != jsn) {
                        // spdlog::warn("nameRole changed {}", jsn.dump(2));
                        j["mtime"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;
                case Roles::nameRole:
                    if (j.count("name") and j["name"] != jsn) {
                        // spdlog::warn("nameRole changed {}", jsn.dump(2));
                        j["name"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::actorUuidRole:
                    if (j.count("actor_uuid") and j["actor_uuid"] != jsn) {
                        // spdlog::warn("actorUuidRole changed {}", jsn.dump(2));
                        j["actor_uuid"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::audioActorUuidRole:
                    if (j.count("audio_actor_uuid") and j["audio_actor_uuid"] != jsn) {
                        // spdlog::warn("actorUuidRole changed {}", jsn.dump(2));
                        j["audio_actor_uuid"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::imageActorUuidRole:
                    if (j.count("image_actor_uuid") and j["image_actor_uuid"] != jsn) {
                        // spdlog::warn("actorUuidRole changed {}", jsn.dump(2));
                        j["image_actor_uuid"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::mediaStatusRole:
                    if (j.count("media_status") and j["media_status"] != jsn) {
                        spdlog::warn("mediaStatusRole changed {}", jsn.dump(2));
                        j["media_status"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::groupActorRole:
                    if (j.count("group_actor") and j["group_actor"] != jsn) {
                        j["group_actor"] = jsn;
                        auto grp         = actorFromString(system(), jsn.get<std::string>());
                        if (grp) {
                            anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
                            spdlog::error(
                                "join grp {} {} {}",
                                j.at("type").is_string() ? j.at("type").get<std::string>() : "",
                                j.at("name").is_string() ? j.at("name").get<std::string>() : "",
                                to_string(grp));
                        }
                        //  should we join ??
                        emit dataChanged(index, index, roles);
                        // spdlog::warn("{}", data_.dump(2));
                    }
                    break;

                case Roles::flagRole:
                    spdlog::warn("flag update {}", jsn.dump(2));
                    if (j.count("flag") and j["flag"] != jsn) {
                        j["flag"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::thumbnailURLRole:
                    if (j.count("thumbnail_url") and j["thumbnail_url"] != jsn) {
                        j["thumbnail_url"] = jsn;
                        emit dataChanged(index, index, roles);
                    }
                    break;

                case Roles::childrenRole:
                    processChildren(jsn, j, index);
                    break;
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    // spdlog::warn("{}", data_.dump(2));
}


void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const QModelIndex &index,
    const int role) const {
    // dispatch call to backend to retrieve missing data.
    auto tmp =
        new CafResponse(search_value, search_role, index, role, QThreadPool::globalInstance());

    connect(tmp, &CafResponse::received, this, &SessionModel::receivedData);
}

void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const nlohmann::json &data,
    const int role) const {
    // dispatch call to backend to retrieve missing data.
    auto tmp =
        new CafResponse(search_value, search_role, data, role, QThreadPool::globalInstance());

    connect(tmp, &CafResponse::received, this, &SessionModel::receivedData);
}

nlohmann::json SessionModel::playlistTreeToJson(
    const utility::PlaylistTree &tree,
    actor_system &sys,
    const utility::UuidActorMap &uuid_actors) {
    auto result = createEntry(R"({
        "container_uuid": null,
        "actor": null,
        "group_actor": null,
        "actor_uuid": null,
        "flag": null,
        "name": null
    })"_json);

    try {
        result["container_uuid"] = tree.uuid();
        result["actor_uuid"]     = tree.value_uuid();
        if (uuid_actors.count(tree.value_uuid()))
            result["actor"] = actorToString(sys, uuid_actors.at(tree.value_uuid()));
        result["type"] = tree.type();
        result["name"] = tree.name();
        result["flag"] = tree.flag();

        for (const auto &i : tree.children_ref()) {
            if (result["children"].is_null())
                result["children"] = R"([])"_json;
            result["children"].push_back(playlistTreeToJson(i, sys, uuid_actors));
        }

        if (result["type"] == "Playlist") {
            result["busy"] = false;
            if (result["children"].is_null()) {
                result["children"] = R"([{},{}])"_json;
                result["children"][0] =
                    createEntry(R"({"type":"Playlist Media", "actor_owner": null})"_json);
                result["children"][1] =
                    createEntry(R"({"type":"Playlist Container", "actor_owner": null})"_json);

                result["children"][0]["actor_owner"] = result["actor"];
                result["children"][1]["actor_owner"] = result["actor"];
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

nlohmann::json SessionModel::containerDetailToJson(
    const utility::ContainerDetail &detail, caf::actor_system &sys) {

    auto result = createEntry(R"({
        "actor": null,
        "group_actor": null,
        "actor_uuid": null,
        "name": null
    })"_json);

    try {
        result["name"]        = detail.name_;
        result["group_actor"] = actorToString(sys, detail.group_);
        result["type"]        = detail.type_;
        result["actor"]       = actorToString(sys, detail.actor_);
        result["actor_uuid"]  = detail.uuid_;
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (detail.type_ == "Media" or detail.type_ == "MediaSource") {
        result["audio_actor_uuid"] = nullptr;
        result["image_actor_uuid"] = nullptr;
        result["media_status"]     = nullptr;
        result["flag"]             = nullptr;
        if (detail.type_ == "Media") {
        } else if (detail.type_ == "MediaSource") {
            result["thumbnail_url"] = nullptr;
            result["rate"]          = nullptr;
            result["path"]          = nullptr;
        }
    }

    return result;
}

nlohmann::json SessionModel::createEntry(const nlohmann::json &update) {
    auto result = R"({"id": null, "children": null, "type": null})"_json;

    result["id"] = Uuid::generate();

    result.update(update);

    return result;
}

void SessionModel::mergeRows(const QModelIndexList &indexes, const QString &name) {
    try {
        UuidVector playlists;

        for (const auto &i : indexes) {
            if (i.data(typeRole) == "Playlist")
                playlists.emplace_back(UuidFromQUuid(i.data(actorUuidRole).toUuid()));
        }

        if (not playlists.empty())
            anon_send(
                session_actor_,
                session::merge_playlist_atom_v,
                StdFromQString(name),
                Uuid(),
                playlists);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

QFuture<QString> SessionModel::saveFuture(const QUrl &path, const QModelIndexList &selection) {
    return QtConcurrent::run([=]() {
        QString result;
        try {
            scoped_actor sys{system()};
            UuidVector playlists;

            for (const auto &i : selection) {
                if (i.data(typeRole) == "Playlist")
                    playlists.emplace_back(UuidFromQUuid(i.data(containerUuidRole).toUuid()));
            }

            if (playlists.empty()) {
                try {
                    if (request_receive_wait<size_t>(
                            *sys,
                            session_actor_,
                            std::chrono::seconds(60),
                            global_store::save_atom_v,
                            UriFromQUrl(path))) {
                        setModified(false);
                        // setPath(path);
                    }
                } catch (std::exception &err) {
                    std::string error =
                        "Session save to " + StdFromQString(path.path()) + " failed!\n";

                    if (std::string(err.what()) == "caf::sec::request_timeout") {
                        error += "Backend did not return session data within timeout.";
                    } else {
                        auto errstr = std::string(err.what());
                        errstr      = std::regex_replace(
                            errstr,
                            std::regex(R"foo(.*caf::sec::runtime_error\("([^"]+)"\).*)foo"),
                            "$1");
                        error += errstr;
                    }

                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, error);
                    result = QStringFromStd(error);
                }
            } else {
                try {
                    request_receive_wait<size_t>(
                        *sys,
                        session_actor_,
                        std::chrono::seconds(60),
                        global_store::save_atom_v,
                        UriFromQUrl(path),
                        playlists);
                } catch (std::exception &err) {
                    std::string error = "Selected session save to " +
                                        StdFromQString(path.path()) + " failed!\n";

                    if (std::string(err.what()) == "caf::sec::request_timeout") {
                        error += "Backend did not return session data within timeout.";
                    } else {
                        auto errstr = std::string(err.what());
                        errstr      = std::regex_replace(
                            errstr,
                            std::regex(R"foo(.*caf::sec::runtime_error\("([^"]+)"\).*)foo"),
                            "$1");
                        error += errstr;
                    }

                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, error);
                    result = QStringFromStd(error);
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}

QFuture<QList<QUuid>>
SessionModel::handleDropFuture(const QVariantMap &drop, const QModelIndex &index) {
    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;

        try {
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &ij = *((nlohmann::json *)index.internalPointer());
                UuidActorVector new_media;
                auto playlist = caf::actor();
                auto jsn      = dropToJsonStore(drop);

                if (ij.at("type") == "Playlist") {
                    playlist = actorFromString(system(), ij.at("actor"));
                } else if (ij.at("type") == "Subset") {
                    playlist = actorFromIndex(index.parent(), true);
                }

                if (playlist) {
                    // conver to json..
                    if (jsn.count("text/uri-list")) {
                        for (const auto &path : jsn["text/uri-list"]) {
                            auto uri = caf::make_uri(path);
                            if (uri) {
                                auto new_media_tmp = request_receive<UuidActorVector>(
                                    *sys, playlist, playlist::add_media_atom_v, *uri, true);

                                new_media.insert(
                                    new_media.end(),
                                    new_media_tmp.begin(),
                                    new_media_tmp.end());
                            }
                        }
                    } else {
                        // forward to datasources for non file paths
                        auto pm = system().registry().template get<caf::actor>(
                            plugin_manager_registry);
                        try {
                            auto plugin_media_tmp = request_receive<UuidActorVector>(
                                *sys, pm, data_source::use_data_atom_v, JsonStore(jsn), true);
                            if (not plugin_media_tmp.empty()) {
                                // we've got a collection of actors..
                                // lets assume they are media... (WARNING this may not be the
                                // case...) create new playlist and add them...
                                for (const auto &i : plugin_media_tmp)
                                    anon_send(
                                        playlist,
                                        playlist::add_media_atom_v,
                                        i,
                                        utility::Uuid());

                                new_media.insert(
                                    new_media.end(),
                                    plugin_media_tmp.begin(),
                                    plugin_media_tmp.end());
                            } else {
                                // try file load..
                                for (const auto &i : jsn["text/plain"]) {
                                    auto uri = caf::make_uri("file:" + i.get<std::string>());
                                    if (uri) {
                                        auto new_media_tmp = request_receive<UuidActorVector>(
                                            *sys,
                                            playlist,
                                            playlist::add_media_atom_v,
                                            *uri,
                                            true);

                                        new_media.insert(
                                            new_media.end(),
                                            new_media_tmp.begin(),
                                            new_media_tmp.end());
                                    }
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                }
                for (const auto &i : new_media)
                    results.push_back(QUuidFromUuid(i.uuid()));

                if (ij.at("type") == "Subset") {
                    auto sub_actor = actorFromString(system(), ij.at("actor"));
                    if (sub_actor) {
                        for (const auto &i : new_media)
                            anon_send(sub_actor, playlist::add_media_atom_v, i.uuid(), Uuid());
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}

QUuid SessionModel::addTag(
    const QUuid &quuid,
    const QString &type,
    const QString &data,
    const QString &unique,
    const bool persistent) {
    QUuid result;

    try {
        tag::Tag tag;
        tag.set_link(UuidFromQUuid(quuid));
        tag.set_type(StdFromQString(type));
        tag.set_data(StdFromQString(data));
        if (not unique.isEmpty())
            tag.set_unique(StdFromQString(unique));
        tag.set_persistent(persistent);

        scoped_actor sys{system()};
        auto tag_actor = request_receive<caf::actor>(*sys, session_actor_, tag::get_tag_atom_v);
        result         = QUuidFromUuid(
            request_receive<utility::Uuid>(*sys, tag_actor, tag::add_tag_atom_v, tag));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QFuture<bool> SessionModel::importFuture(const QUrl &path, const QVariant &json) {
    return QtConcurrent::run([=]() {
        bool result = false;
        JsonStore js;
        scoped_actor sys{system()};

        if (json.isNull()) {
            try {
                std::ifstream i(StdFromQString(path.path()));
                i >> js;
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return false;
            }
        } else {
            try {
                js = JsonStore(qvariant_to_json(json));
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return false;
            }
        }

        try {
            auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));
            sys->anon_send(session_actor_, session::merge_session_atom_v, session);

            result = true;
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}

QFuture<QUrl> SessionModel::getThumbnailURLFuture(const QModelIndex &index, const float frame) {
    return QtConcurrent::run([=]() {
        QString thumburl("qrc:///feather_icons/film.svg");
        try {
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

                if (j.at("type") == "MediaSource") {
                    scoped_actor sys{system()};

                    // get mediasource stream detail.
                    auto actor  = actorFromString(system(), j.at("actor"));
                    auto detail = request_receive<media::StreamDetail>(
                        *sys, actor, media::get_stream_detail_atom_v, media::MT_IMAGE);

                    if (detail.duration_.rate().to_seconds() != 0.0) {
                        auto middle_frame    = (detail.duration_.frames() - 1) / 2;
                        auto requested_frame = static_cast<int>(
                            static_cast<float>(detail.duration_.frames() - 1) * frame);
                        thumburl = getThumbnailURL(
                            system(),
                            actor,
                            frame == -1.0 ? middle_frame : requested_frame,
                            frame == -1.0);
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return QUrl(thumburl);
    });
}

QFuture<QUrl> SessionModel::getThumbnailURLFuture(const QModelIndex &index, const int frame) {
    return QtConcurrent::run([=]() {
        QString thumburl("qrc:///feather_icons/film.svg");
        try {
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());

                if (j.at("type") == "MediaSource") {
                    scoped_actor sys{system()};

                    // get mediasource stream detail.
                    auto actor = actorFromString(system(), j.at("actor"));

                    auto detail = request_receive<media::StreamDetail>(
                        *sys, actor, media::get_stream_detail_atom_v, media::MT_IMAGE);

                    if (detail.duration_.rate().to_seconds() != 0.0) {
                        auto middle_frame = (detail.duration_.frames() - 1) / 2;

                        thumburl = getThumbnailURL(
                            system(), actor, frame == -1 ? middle_frame : frame, frame == -1);
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return QUrl(thumburl);
    });
}

QFuture<bool> SessionModel::clearCacheFuture(const QModelIndexList &indexes) {
    return QtConcurrent::run([=]() {
        auto result = false;

        for (const auto &index : indexes) {
            if (index.isValid() and index.internalPointer()) {
                nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
                if (j.at("type") == "Media") {
                    auto actor = actorFromString(system(), j.at("actor"));
                    if (actor) {
                        anon_send(actor, media::invalidate_cache_atom_v);
                        result = true;
                    }
                }
            }
        }

        return result;
    });
}


// QFuture<QList<QUuid>> SessionUI::handleDropFuture(const QVariantMap &drop) {
//     return QtConcurrent::run([=]() {
//         QList<QUuid> results;
//         try {
//             auto jsn = dropToJsonStore(drop);

//             // spdlog::warn("{}", jsn.dump(2));

//             if (jsn.count("text/uri-list")) {
//                 QList<QUrl> urls;
//                 for (const auto &i : jsn["text/uri-list"])
//                     urls.append(QUrl(QStringFromStd(i)));
//                 loadUrls(urls);
//             } else {
//                 // something else...
//                 // dispatch to datasource, if that doesn't work then assume file paths..
//                 scoped_actor sys{system()};
//                 auto pm = system().registry().template
//                 get<caf::actor>(plugin_manager_registry); try {
//                     auto result = request_receive<UuidActorVector>(
//                         *sys, pm, data_source::use_data_atom_v, JsonStore(jsn), true);
//                     if (result.empty()) {
//                         for (const auto &i : jsn["text/plain"])
//                             loadUrl(QUrl(QStringFromStd("file:" + i.get<std::string>())));
//                     } else {
//                         // we've got a collection of actors..
//                         // lets assume they are media... (WARNING this may not be the
//                         case...)
//                         // create new playlist and add them...

//                         auto new_playlist = request_receive<UuidUuidActor>(
//                             *sys,
//                             backend_,
//                             session::add_playlist_atom_v,
//                             "DragDrop Playlist",
//                             utility::Uuid(),
//                             false);
//                         for (const auto &i : result) {
//                             anon_send(
//                                 new_playlist.second.actor(),
//                                 playlist::add_media_atom_v,
//                                 i,
//                                 utility::Uuid());
//                             results.push_back(QUuidFromUuid(i.uuid()));
//                         }
//                     }
//                 } catch (const std::exception &err) {
//                     spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
//                 }
//             }
//         } catch (const std::exception &err) {
//             spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
//         }

//         return results;
//     });
// }
