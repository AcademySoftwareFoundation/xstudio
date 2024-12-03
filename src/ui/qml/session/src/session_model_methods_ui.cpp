// SPDX-License-Identifier: Apache-2.0

#include "xstudio/conform/conformer.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/timeline.hpp"
#include "xstudio/ui/qml/caf_response_ui.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"
#include "xstudio/utility/notification_handler.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSignalSpy>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

void SessionModel::removeNotification(const QModelIndex &index, const QUuid &uuid) {
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor)
            anon_send(actor, utility::notification_atom_v, UuidFromQUuid(uuid));
    }
}

QUuid SessionModel::infoNotification(
    const QModelIndex &index,
    const QString &text,
    const int seconds,
    const QUuid &replaceUuid) {
    auto result       = QUuid();
    auto replace_uuid = UuidFromQUuid(replaceUuid);
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            auto n = Notification::InfoNotification(
                StdFromQString(text), std::chrono::seconds(seconds));
            if (not replace_uuid.is_null())
                n.uuid(replace_uuid);
            result = QUuidFromUuid(n.uuid());
            anon_send(actor, utility::notification_atom_v, n);
        }
    }

    return result;
}

QUuid SessionModel::warnNotification(
    const QModelIndex &index,
    const QString &text,
    const int seconds,
    const QUuid &replaceUuid) {
    auto result       = QUuid();
    auto replace_uuid = UuidFromQUuid(replaceUuid);

    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            auto n = Notification::WarnNotification(
                StdFromQString(text), std::chrono::seconds(seconds));
            if (not replace_uuid.is_null())
                n.uuid(replace_uuid);
            result = QUuidFromUuid(n.uuid());
            anon_send(actor, utility::notification_atom_v, n);
        }
    }

    return result;
}

QUuid SessionModel::processingNotification(const QModelIndex &index, const QString &text) {
    auto result = QUuid();
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            auto n = Notification::ProcessingNotification(StdFromQString(text));
            result = QUuidFromUuid(n.uuid());
            anon_send(actor, utility::notification_atom_v, n);
        }
    }

    return result;
}

QUuid SessionModel::progressPercentageNotification(
    const QModelIndex &index, const QString &text) {
    auto result = QUuid();
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            auto n = Notification::ProgressPercentageNotification(StdFromQString(text));
            result = QUuidFromUuid(n.uuid());
            anon_send(actor, utility::notification_atom_v, n);
        }
    }

    return result;
}

QUuid SessionModel::progressRangeNotification(
    const QModelIndex &index, const QString &text, const float min, const float max) {
    auto result = QUuid();
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            auto n =
                Notification::ProgressRangeNotification(StdFromQString(text), min, min, max);
            result = QUuidFromUuid(n.uuid());
            anon_send(actor, utility::notification_atom_v, n);
        }
    }

    return result;
}


void SessionModel::updateProgressNotification(
    const QModelIndex &index, const QUuid &uuid, const float value) {
    if (index.isValid()) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor) {
            anon_send(actor, utility::notification_atom_v, UuidFromQUuid(uuid), value);
        }
    }
}


QVariant SessionModel::playlists() const {
    // scan model for playlists..
    auto data = R"([])"_json;
    try {
        auto value = R"({"text": null, "uuid":null})"_json;
        for (const auto &i : *(data_.child(0))) {
            value["text"] = i.data().at("name");
            value["uuid"] = i.data().at("actor_uuid");
            data.push_back(value);
        }
    } catch (...) {
    }

    return mapFromValue(data);
}

void SessionModel::setSelectedMedia(const QModelIndexList &indexes) {
    auto media = UuidActorVector();

    for (const auto &i : indexes) {
        auto muuid  = UuidFromQUuid(i.data(actorUuidRole).toUuid());
        auto mactor = actorFromQString(system(), i.data(actorRole).toString());
        media.emplace_back(UuidActor(muuid, mactor));
    }

    anon_send(session_actor_, media::current_media_atom_v, media);
}


QModelIndex SessionModel::getPlaylistIndex(const QModelIndex &index) const {
    QModelIndex result = index;
    auto matched       = QVariant::fromValue(QString("Playlist"));

    while (result.isValid()) {
        if (data(result, typeRole) == matched)
            break;
        else
            result = result.parent();
    }

    return result;
}

QModelIndex SessionModel::getContainerIndex(const QModelIndex &index) const {
    const static std::set<std::string> container_names(
        {"Timeline", "Subset", "ContactSheet", "Playlist"});
    try {
        if (index.isValid()) {
            auto type = StdFromQString(index.data(typeRole).toString());
            if (container_names.count(type))
                return index;
            else
                return getContainerIndex(index.parent());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QModelIndex();
}

Q_INVOKABLE void SessionModel::purgePlaylist(const QModelIndex &index) {
    if (index.isValid() and index.data(typeRole).toString() == QString("Playlist")) {
        auto actor = actorFromQString(system(), index.data(actorRole).toString());
        if (actor)
            anon_send(actor, playlist::remove_orphans_atom_v, UuidVector());
    }
}

void SessionModel::updateCurrentMediaContainerIndexFromBackend() {

    scoped_actor sys{system()};
    try {
        auto playlist = request_receive<UuidActor>(
            *sys, session_actor_, session::active_media_container_atom_v);
        auto actor_string = QStringFromStd(actorToString(system(), playlist.actor()));

        // playlists are in 2nd row of root of the session model. We only need to go 2 levels
        // deep to see all playlists and subsets/timelines which are children of playlists
        auto r =
            QPersistentModelIndex(searchRecursive(actor_string, "actorRole", index(1, 0), 2));

        if (!r.isValid() && playlist) {
            // we didn't find the actor in the model ... this could be that the model is
            // still building so re-try in 250ms
            QTimer::singleShot(250, this, SLOT(updateCurrentMediaContainerIndexFromBackend()));
        }

        if (r != current_playlist_index_) {
            current_playlist_index_ = r;
            emit currentMediaContainerChanged();
        }

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void SessionModel::updateViewportCurrentMediaContainerIndexFromBackend() {
    scoped_actor sys{system()};
    try {

        auto playlist = request_receive<UuidActor>(
            *sys, session_actor_, session::viewport_active_media_container_atom_v);
        auto actor_string = QStringFromStd(actorToString(system(), playlist.actor()));

        // playlists are in 2nd row of root of the session model. We only need to go 2 levels
        // deep to see all playlists and subsets/timelines which are children of playlists
        auto r =
            QPersistentModelIndex(searchRecursive(actor_string, "actorRole", index(1, 0), 2));

        if (!r.isValid() && playlist) {
            // we didn't find the actor in the model ... this could be that the model is
            // still building so re-try in 250ms
            QTimer::singleShot(
                250, this, SLOT(updateViewportCurrentMediaContainerIndexFromBackend()));
        }

        if (r != current_playhead_owner_index_) {
            current_playhead_owner_index_ = r;
            emit viewportCurrentMediaContainerIndexChanged();
        }

    } catch (const std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}


Q_INVOKABLE void SessionModel::decomposeMedia(const QModelIndexList &indexes) {
    for (const auto &i : indexes) {
        if (i.isValid() and i.data(typeRole).toString() == QString("Media")) {
            auto plindex = getPlaylistIndex(i);
            if (plindex.isValid()) {
                auto actor = actorFromQString(system(), plindex.data(actorRole).toString());
                if (actor)
                    anon_send(
                        actor,
                        media::decompose_atom_v,
                        UuidFromQUuid(i.data(actorUuidRole).toUuid()));
            }
        }
    }
}

Q_INVOKABLE void SessionModel::rescanMedia(const QModelIndexList &indexes) {
    for (const auto &i : indexes) {
        if (i.isValid() and i.data(typeRole).toString() == QString("Media")) {
            auto plindex = getPlaylistIndex(i);
            if (plindex.isValid()) {
                auto actor = actorFromQString(system(), plindex.data(actorRole).toString());
                if (actor)
                    anon_send(
                        actor,
                        media::rescan_atom_v,
                        UuidFromQUuid(i.data(actorUuidRole).toUuid()));
            }
        }
    }
}

Q_INVOKABLE void SessionModel::relinkMedia(const QModelIndexList &indexes, const QUrl &path) {

    auto scanner = system().registry().template get<caf::actor>(scanner_registry);

    if (scanner) {
        auto uri = UriFromQUrl(path);

        for (const auto &i : indexes) {
            if (i.isValid()) {
                if (i.data(typeRole).toString() == QString("Media")) {
                    auto iind = searchRecursive(i.data(imageActorUuidRole), actorUuidRole, i);
                    auto aind = searchRecursive(i.data(audioActorUuidRole), actorUuidRole, i);

                    if (iind.isValid()) {
                        auto actor =
                            actorFromQString(system(), iind.data(actorRole).toString());
                        if (actor)
                            anon_send(scanner, media::relink_atom_v, actor, uri);
                    }

                    if (aind.isValid() and aind != iind) {
                        auto actor =
                            actorFromQString(system(), aind.data(actorRole).toString());
                        if (actor)
                            anon_send(scanner, media::relink_atom_v, actor, uri);
                    }
                }
            }
        }
    }
}

QStringList
SessionModel::getMediaSourceNames(const QModelIndex &media_index, const bool image_source) {

    QStringList rt;
    try {

        scoped_actor sys{system()};

        if (media_index.isValid()) {
            nlohmann::json &j = indexToData(media_index);
            if (j.at("actor").is_string()) {
                auto actor = actorFromString(system(), j.at("actor"));
                if (actor) {
                    auto media_source_names =
                        request_receive<std::vector<std::pair<utility::Uuid, std::string>>>(
                            *sys,
                            actor,
                            media::get_media_source_names_atom_v,
                            image_source ? media::MT_IMAGE : media::MT_AUDIO);
                    for (const auto &n : media_source_names) {
                        rt.append(QStringFromStd(n.second));
                    }
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        /*if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            spdlog::warn("{}", j.dump(2));
        }*/
    }
    return rt;
}

QStringList SessionModel::setMediaSource(
    const QModelIndex &media_index, const QString &mediaSourceName, const bool image_source) {

    QStringList rt;
    try {

        scoped_actor sys{system()};

        if (media_index.isValid()) {
            nlohmann::json &j = indexToData(media_index);
            if (j.at("actor").is_string()) {
                auto actor = actorFromString(system(), j.at("actor"));
                if (actor) {
                    anon_send(
                        actor,
                        playhead::media_source_atom_v,
                        StdFromQString(mediaSourceName),
                        image_source ? media::MT_IMAGE : media::MT_AUDIO);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        /*if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            spdlog::warn("{}", j.dump(2));
        }*/
    }
    return rt;
}

void SessionModel::setSessionActorAddr(const QString &addr) {
    try {
        if (addr != session_actor_addr_) {
            scoped_actor sys{system()};

            setModified(false, true);

            auto data = R"({"children":[]})"_json;

            data["children"].push_back(createEntry(R"({
                "type": "Session",
                "path": null,
                "mtime": null,
                "name": null,
                "actor_uuid": null,
                "actor": null,
                "group_actor": null,
                "container_uuid": null,
                "rate": null,
                "flag": null})"_json));

            session_actor_addr_ = addr;
            emit sessionActorAddrChanged();

            session_actor_               = actorFromQString(system(), addr);
            data["children"][0]["actor"] = StdFromQString(addr);

            // clear lookup..
            id_uuid_lookup_.clear();
            uuid_lookup_.clear();
            string_lookup_.clear();

            setModelData(data);
            add_lookup(*indexToTree(index(0, 0)), index(0, 0));
            emit playlistsChanged();

            // get the 'current' playlists (inspected and on-screen)
            // from the session backend actor
            updateCurrentMediaContainerIndexFromBackend();
            updateViewportCurrentMediaContainerIndexFromBackend();

            try {
                auto playhead_events_actor =
                    system().registry().template get<caf::actor>(global_playhead_events_actor);
                auto playhead = request_receive<caf::actor>(
                    *sys, playhead_events_actor, ui::viewport::viewport_playhead_atom_v);
                if (playhead) {
                    on_screen_playhead_uuid_ = QUuidFromUuid(
                        request_receive<utility::Uuid>(*sys, playhead, utility::uuid_atom_v));
                    emit onScreenPlayheadUuidChanged();
                }
            } catch (...) {
            }

            // join bookmark events
            if (session_actor_) {
                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_.front().data().at("id"))),
                    idRole,
                    QModelIndex(),
                    data_.front().data(),
                    pathRole);

                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_.front().data().at("id"))),
                    idRole,
                    QModelIndex(),
                    data_.front().data(),
                    groupActorRole);

                requestData(
                    QVariant::fromValue(QUuidFromUuid(data_.front().data().at("id"))),
                    idRole,
                    QModelIndex(),
                    data_.front().data(),
                    JSONTreeModel::Roles::childrenRole);

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

QFuture<QString> SessionModel::saveFuture(const QUrl &path, const QModelIndexList &selection) {
    return QtConcurrent::run([=]() {
        QString result;
        try {
            scoped_actor sys{system()};
            UuidVector playlists;

            for (const auto &i : selection) {
                if (i.data(typeRole) == "Playlist")
                    playlists.emplace_back(UuidFromQUuid(i.data(containerUuidRole).toUuid()));
                else {
                    auto tmp = getPlaylistIndex(i);
                    if (tmp.isValid())
                        playlists.emplace_back(
                            UuidFromQUuid(tmp.data(containerUuidRole).toUuid()));
                }
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

QFuture<QList<QUuid>> SessionModel::handleDropFuture(
    const int proposedAction_, const QVariantMap &drop, const QModelIndex &index) {

    try {
        auto jdrop = dropToJsonStore(drop);
        if (jdrop.count("xstudio/media-ids"))
            return handleMediaIdDropFuture(proposedAction_, jdrop, index);
        else if (jdrop.count("xstudio/timeline-ids"))
            return handleTimelineIdDropFuture(proposedAction_, jdrop, index);
        else if (jdrop.count("xstudio/container-ids"))
            return handleContainerIdDropFuture(proposedAction_, jdrop, index);
        else if (jdrop.count("text/uri-list"))
            return handleUriListDropFuture(proposedAction_, jdrop, index);
        else
            return handleOtherDropFuture(proposedAction_, jdrop, index);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QtConcurrent::run([=]() {
        QList<QUuid> results;
        return results;
    });
}

QFuture<QList<QUuid>> SessionModel::handleMediaIdDropFuture(
    const int proposedAction_, const utility::JsonStore &jdrop, const QModelIndex &index) {

    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;
        UuidActorVector new_media;
        auto proposedAction = proposedAction_;
        auto dropIndex      = index;

        try {
            // spdlog::warn(
            //     "handleDropFuture {} {} {}", proposedAction, jdrop.dump(2), index.isValid());
            auto valid_index = index.isValid();
            Uuid before;
            // build list of media actor uuids
            UuidActorVector media;
            Uuid media_owner_uuid;
            std::string media_owner_name;

            for (const auto &i : jdrop.at("xstudio/media-ids")) {
                // find media index
                auto mind = searchRecursive(QUuid::fromString(QStringFromStd(i)), idRole);
                if (mind.isValid()) {
                    if (media_owner_uuid.is_null()) {
                        auto p = mind.parent().parent();
                        if (p.isValid()) {
                            media_owner_name = StdFromQString(p.data(nameRole).toString());
                            media_owner_uuid = UuidFromQUuid(p.data(actorUuidRole).toUuid());
                        }
                    }
                    media.emplace_back(UuidActor(
                        UuidFromQUuid(mind.data(actorUuidRole).toUuid()),
                        actorFromQString(system(), mind.data(actorRole).toString())));
                }
            }

            if (not media.empty()) {
                if (valid_index) {
                    // Moving or copying Media to existing playlist, possibly itself.
                    const auto &ij = indexToData(index);
                    // spdlog::warn("{}", ij.at("type").get<std::string>());
                    auto type        = ij.at("type").get<std::string>();
                    auto target      = caf::actor();
                    auto target_uuid = Uuid();

                    if (type == "Playlist") {
                        target      = actorFromString(system(), ij.at("actor"));
                        target_uuid = ij.at("actor_uuid").get<Uuid>();
                    } else if (type == "ContactSheet") {
                        target      = actorFromIndex(index.parent(), true);
                        target_uuid = ij.at("actor_uuid").get<Uuid>();
                    } else if (type == "Subset") {
                        target      = actorFromIndex(index.parent(), true);
                        target_uuid = ij.at("actor_uuid").get<Uuid>();
                    } else if (type == "Timeline") {
                        target      = actorFromIndex(index.parent(), true);
                        target_uuid = ij.at("actor_uuid").get<Uuid>();
                    } else if (type == "Media") {
                        before      = ij.at("actor_uuid");
                        target      = actorFromIndex(index.parent().parent(), true);
                        target_uuid = ij.at("actor_uuid").get<Uuid>();
                    } else if (
                        type == "Video Track" or type == "Audio Track" or type == "Gap" or
                        type == "Clip") {
                        auto tindex = getTimelineIndex(index);
                        dropIndex   = tindex.parent();
                        target      = actorFromIndex(tindex.parent(), true);
                        target_uuid = UuidFromQUuid(tindex.data(actorUuidRole).toUuid());
                    } else {
                        spdlog::warn("UNHANDLED {}", type);
                    }

                    if (target and not media.empty()) {
                        bool local_mode = false;
                        // might be adding to end of playlist, which different
                        // check these uuids aren't already in playlist..
                        if (type == "Media")
                            local_mode = true;
                        else {
                            // just check first.
                            auto dup = search(
                                QVariant::fromValue(QUuidFromUuid(media[0].uuid())),
                                actorUuidRole,
                                SessionModel::index(0, 0, dropIndex));

                            if (dup.isValid())
                                local_mode = true;
                        }

                        if (local_mode) {
                            anon_send(
                                target,
                                playlist::move_media_atom_v,
                                vector_to_uuid_vector(media),
                                before);
                        } else {
                            if (proposedAction == Qt::MoveAction) {
                                // spdlog::warn("proposedAction == Qt::MoveAction");
                                // move media to new playlist
                                anon_send(
                                    session_actor_,
                                    playlist::move_media_atom_v,
                                    target_uuid,
                                    media_owner_uuid,
                                    vector_to_uuid_vector(media),
                                    before,
                                    false);
                            } else {
                                // spdlog::warn("proposedAction == Qt::CopyAction");
                                anon_send(
                                    session_actor_,
                                    playlist::copy_media_atom_v,
                                    target_uuid,
                                    vector_to_uuid_vector(media),
                                    false,
                                    before,
                                    false);
                            }
                        }

                        // post process timeline drops..
                        if (type == "Video Track" or type == "Audio Track" or type == "Gap" or
                            type == "Clip") {
                            auto track_actor = caf::actor();
                            auto row         = -1;

                            if (type == "Video Track" or type == "Audio Track")
                                track_actor = actorFromIndex(index, false);
                            else {
                                track_actor = actorFromIndex(index.parent(), false);
                                row         = index.row();
                            }

                            // append to track as clip.
                            // assuming media_id exists in timeline already.
                            for (const auto &i : media) {
                                auto new_uuid = utility::Uuid::generate();
                                auto new_item =
                                    self()->spawn<timeline::ClipActor>(i, "", new_uuid);
                                anon_send(
                                    track_actor,
                                    timeline::insert_item_atom_v,
                                    row,
                                    UuidActorVector({UuidActor(new_uuid, new_item)}));
                            }
                        }
                    }
                } else {

                    if (media_owner_name.empty()) {

                        // session will assign a new name
                        media_owner_name = "";

                    } else {
                        media_owner_name += " - copy";
                    }

                    auto uua = request_receive<UuidUuidActor>(
                        *sys,
                        session_actor_,
                        session::add_playlist_atom_v,
                        media_owner_name,
                        Uuid(),
                        false);

                    if (proposedAction == Qt::MoveAction) {
                        // move media to new playlist
                        anon_send(
                            session_actor_,
                            playlist::move_media_atom_v,
                            uua.second.uuid(),
                            media_owner_uuid,
                            vector_to_uuid_vector(media),
                            Uuid(),
                            false);
                    } else {
                        // copy media to new playlist.
                        anon_send(
                            session_actor_,
                            playlist::copy_media_atom_v,
                            uua.second.uuid(),
                            vector_to_uuid_vector(media),
                            false,
                            Uuid(),
                            false);
                    }
                }
            }

            for (const auto &i : new_media)
                results.push_back(QUuidFromUuid(i.uuid()));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}

QFuture<QList<QUuid>> SessionModel::handleContainerIdDropFuture(
    const int proposedAction_, const utility::JsonStore &jdrop, const QModelIndex &index) {

    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;
        UuidActorVector new_media;
        auto proposedAction = proposedAction_;

        try {
            // spdlog::warn(
            //     "handleDropFuture {} {} {}", proposedAction, jdrop.dump(2), index.isValid());
            auto valid_index = index.isValid();
            Uuid before;
            // item / parent
            std::vector<std::pair<Uuid, Uuid>> cont_uuids;
            Uuid target_uuid;
            Uuid target_parent_uuid;
            auto target_parent_actor = caf::actor();

            for (const auto &i : jdrop.at("xstudio/container-ids")) {
                // spdlog::warn("MOVE CONTAINER {}", to_string(i));
                // find container index

                auto cind = searchRecursive(QUuid::fromString(QStringFromStd(i)), idRole);
                if (cind.isValid()) {
                    const auto &cij = indexToData(cind);
                    // spdlog::info("{}", cij.dump(2));
                    auto item        = UuidFromQUuid(cind.data(containerUuidRole).toUuid());
                    auto parent_uuid = Uuid();
                    if (StdFromQString(cind.parent().data(typeRole).toString()) ==
                        "Container List")
                        parent_uuid =
                            UuidFromQUuid(cind.parent().parent().data(actorUuidRole).toUuid());
                    else
                        parent_uuid = UuidFromQUuid(cind.parent().data(actorUuidRole).toUuid());

                    cont_uuids.emplace_back(std::make_pair(item, parent_uuid));

                    if (not target_parent_actor) {
                        if (valid_index) {
                            // Moving or copying container to existing playlist, possibly
                            // itself.
                            const auto &ij = indexToData(index);
                            // spdlog::info("{}", ij.dump(2));
                            target_uuid = UuidFromQUuid(index.data(containerUuidRole).toUuid());
                            auto target_actor_uuid =
                                UuidFromQUuid(index.data(actorUuidRole).toUuid());

                            // moving to parent,  really means moving to end of list.
                            if (parent_uuid == target_actor_uuid) {
                                target_uuid = Uuid();
                                target_parent_uuid =
                                    UuidFromQUuid(index.data(actorUuidRole).toUuid());
                                target_parent_actor = actorFromQString(
                                    system(), index.data(actorRole).toString());
                            } else {
                                if (StdFromQString(index.parent().data(typeRole).toString()) ==
                                    "Container List") {
                                    target_parent_uuid = UuidFromQUuid(
                                        index.parent().parent().data(actorUuidRole).toUuid());
                                    target_parent_actor = actorFromQString(
                                        system(),
                                        index.parent().parent().data(actorRole).toString());
                                } else {
                                    target_parent_uuid = UuidFromQUuid(
                                        index.parent().data(actorUuidRole).toUuid());
                                    target_parent_actor = actorFromQString(
                                        system(), index.parent().data(actorRole).toString());
                                }
                            }
                        } else {
                            if (StdFromQString(cind.parent().data(typeRole).toString()) ==
                                "Container List") {
                                target_parent_uuid = UuidFromQUuid(
                                    cind.parent().parent().data(actorUuidRole).toUuid());
                                target_parent_actor = actorFromQString(
                                    system(),
                                    cind.parent().parent().data(actorRole).toString());
                            } else {
                                target_parent_uuid =
                                    UuidFromQUuid(cind.parent().data(actorUuidRole).toUuid());
                                target_parent_actor = actorFromQString(
                                    system(), cind.parent().data(actorRole).toString());
                            }
                        }
                    }
                }
            }

            if (not cont_uuids.empty()) {

                // spdlog::warn(
                //     "target {} {} {}",
                //     to_string(target_parent_actor),
                //     to_string(target_parent_uuid),
                //     to_string(target_uuid));

                if (target_parent_actor) {
                    for (const auto &i : cont_uuids) {
                        // spdlog::warn(
                        //     "source container p {} i {}",
                        //     to_string(i.second),
                        //     to_string(i.first));
                        if (i.second == target_parent_uuid) {
                            // spdlog::warn("move");
                            // moving inside parent.
                            auto new_item = request_receive<bool>(
                                *sys,
                                target_parent_actor,
                                playlist::move_container_atom_v,
                                i.first,
                                target_uuid,
                                false);
                        }
                    }
                }
            }

            for (const auto &i : new_media)
                results.push_back(QUuidFromUuid(i.uuid()));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}


QFuture<QList<QUuid>> SessionModel::handleUriListDropFuture(
    const int proposedAction_, const utility::JsonStore &jdrop, const QModelIndex &index) {

    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;
        UuidActorVector new_media;
        auto proposedAction = proposedAction_;

        try {
            // spdlog::warn(
            //     "handleDropFuture {} {} {}", proposedAction, jdrop.dump(2), index.isValid());
            auto valid_index = index.isValid();
            Uuid before;
            if (valid_index) {
                // Moving or copying Media to existing playlist, possibly itself.
                const auto &ij   = indexToData(index);
                auto target      = caf::actor();
                auto target_uuid = ij.value("actor_uuid", Uuid());
                const auto &type = ij.at("type").get<std::string>();
                auto sub_target  = caf::actor();

                // spdlog::warn("{}", type);

                std::string actor;
                if (type == "Playlist") {
                    actor  = ij.at("actor");
                    target = actorFromString(system(), actor);
                } else if (type == "ContactSheet") {
                    actor      = ij.at("actor");
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), actor);
                } else if (type == "Subset") {
                    actor      = ij.at("actor");
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), actor);
                } else if (type == "Timeline") {
                    actor      = ij.at("actor");
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), actor);
                } else if (
                    type == "Video Track" or type == "Audio Track" or type == "Gap" or
                    type == "Clip") {
                    auto tindex = getTimelineIndex(index);
                    auto pindex = tindex.parent();

                    sub_target = actorFromIndex(tindex, true);

                    target      = actorFromIndex(pindex, true);
                    target_uuid = UuidFromQUuid(pindex.data(actorUuidRole).toUuid());
                } else if (type == "Media") {
                    before = ij.at("actor_uuid");
                    target = actorFromIndex(index.parent().parent(), true);
                } else {
                    spdlog::warn("UNHANDLED {}", ij.at("type").get<std::string>());
                }

                if (target) {
                    for (const auto &path : jdrop.at("text/uri-list")) {
                        auto path_string = path.get<std::string>();
                        auto uri         = caf::make_uri(url_clean(path_string));
                        if (uri) {
                            // uri maybe timeline...
                            // hacky...
                            if (is_timeline_supported(*uri)) {
                                // spdlog::warn("LOAD TIMELINE {}", to_string(*uri));
                                new_media.push_back(request_receive<UuidActor>(
                                    *sys, target, session::import_atom_v, *uri, before, true));
                            } else {
                                auto new_media_tmp = request_receive<UuidActorVector>(
                                    *sys,
                                    target,
                                    playlist::add_media_atom_v,
                                    *uri,
                                    true,
                                    before);

                                new_media.insert(
                                    new_media.end(),
                                    new_media_tmp.begin(),
                                    new_media_tmp.end());
                            }
                        } else {
                            spdlog::warn(
                                "{} Invalid URI {}",
                                __PRETTY_FUNCTION__,
                                path.get<std::string>());
                        }
                    }

                    if (sub_target) {
                        for (const auto &i : new_media)
                            anon_send(sub_target, playlist::add_media_atom_v, i.uuid(), Uuid());

                        // post process timeline drops..
                        if (type == "Video Track" or type == "Audio Track" or type == "Gap" or
                            type == "Clip") {
                            auto track_actor = caf::actor();
                            auto row         = -1;

                            if (type == "Video Track" or type == "Audio Track")
                                track_actor = actorFromIndex(index, false);
                            else {
                                track_actor = actorFromIndex(index.parent(), false);
                                row         = index.row();
                            }

                            // append to track as clip.
                            // assuming media_id exists in timeline already.
                            for (const auto &i : new_media) {
                                auto new_uuid = utility::Uuid::generate();
                                auto new_item =
                                    self()->spawn<timeline::ClipActor>(i, "", new_uuid);
                                anon_send(
                                    track_actor,
                                    timeline::insert_item_atom_v,
                                    row,
                                    UuidActorVector({UuidActor(new_uuid, new_item)}));
                            }
                        }
                    }
                }
            } else {
                // create playlist ?
                std::vector<caf::uri> uris;
                for (const auto &path : jdrop.at("text/uri-list")) {
                    auto uri = caf::make_uri(url_clean(path));
                    if (uri)
                        uris.emplace_back(*uri);
                }
                anon_send(session_actor_, session::load_uris_atom_v, uris, false, true);
            }

            for (const auto &i : new_media)
                results.push_back(QUuidFromUuid(i.uuid()));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}

QFuture<QList<QUuid>> SessionModel::handleOtherDropFuture(
    const int proposedAction_, const utility::JsonStore &jdrop, const QModelIndex &index) {

    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;
        UuidActorVector new_media;
        auto proposedAction = proposedAction_;

        try {
            // spdlog::warn(
            //     "handleDropFuture {} {} {}", proposedAction, jdrop.dump(2), index.isValid());
            auto valid_index = index.isValid();
            Uuid before;

            // plugin and finaly text
            // forward to datasources for non file paths
            auto pm     = system().registry().template get<caf::actor>(plugin_manager_registry);
            auto target = caf::actor();
            auto sub_target = caf::actor();
            auto type       = std::string();

            if (valid_index) {
                const auto &ij = indexToData(index);
                type           = ij.at("type").get<std::string>();


                // spdlog::warn("{}", ij.at("type").get<std::string>());

                if (type == "Playlist") {
                    target = actorFromString(system(), ij.at("actor"));
                } else if (type == "Subset") {
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), ij.at("actor"));
                } else if (type == "ContactSheet") {
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), ij.at("actor"));
                } else if (type == "Timeline") {
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), ij.at("actor"));
                } else if (
                    type == "Video Track" or type == "Audio Track" or type == "Gap" or
                    type == "Clip") {
                    auto tindex = getTimelineIndex(index);
                    auto pindex = tindex.parent();

                    sub_target = actorFromIndex(tindex, true);

                    target = actorFromIndex(pindex, true);
                    // target_uuid = UuidFromQUuid(pindex.data(actorUuidRole).toUuid());
                } else if (type == "Media") {
                    before = ij.at("actor_uuid");
                    target = actorFromIndex(index.parent().parent(), true);
                } else {
                    spdlog::warn("UNHANDLED {}", ij.at("type").get<std::string>());
                }
            }

            if (not target) {
                auto uua = request_receive<UuidUuidActor>(
                    *sys,
                    session_actor_,
                    session::add_playlist_atom_v,
                    "Drag Drop",
                    Uuid(),
                    false);
                target = uua.second.actor();
            }


            try {
                auto media_rate = request_receive<FrameRate>(
                    *sys, session_actor_, session::media_rate_atom_v);

                auto plugin_media_tmp = request_receive<UuidActorVector>(
                    *sys, pm, data_source::use_data_atom_v, JsonStore(jdrop), media_rate, true);
                if (not plugin_media_tmp.empty()) {
                    // we've got a collection of actors..
                    // lets assume they are media... (WARNING this may not be the
                    // case...) create new playlist and add them...
                    for (const auto &i : plugin_media_tmp)
                        anon_send(target, playlist::add_media_atom_v, i, before);

                    new_media.insert(
                        new_media.end(), plugin_media_tmp.begin(), plugin_media_tmp.end());
                } else {
                    // try file load..
                    for (const auto &i : jdrop.at("text/plain")) {
                        auto uri = caf::make_uri(url_clean("file:" + i.get<std::string>()));
                        if (uri) {
                            auto new_media_tmp = request_receive<UuidActorVector>(
                                *sys, target, playlist::add_media_atom_v, *uri, true, before);

                            new_media.insert(
                                new_media.end(), new_media_tmp.begin(), new_media_tmp.end());
                        }
                    }
                }


                if (sub_target) {
                    for (const auto &i : new_media)
                        anon_send(sub_target, playlist::add_media_atom_v, i.uuid(), Uuid());

                    // post process timeline drops..
                    if (type == "Video Track" or type == "Audio Track" or type == "Gap" or
                        type == "Clip") {
                        auto track_actor = caf::actor();
                        auto row         = -1;

                        if (type == "Video Track" or type == "Audio Track")
                            track_actor = actorFromIndex(index, false);
                        else {
                            track_actor = actorFromIndex(index.parent(), false);
                            row         = index.row();
                        }

                        // append to track as clip.
                        // assuming media_id exists in timeline already.
                        for (const auto &i : new_media) {
                            auto new_uuid = utility::Uuid::generate();
                            auto new_item = self()->spawn<timeline::ClipActor>(i, "", new_uuid);
                            anon_send(
                                track_actor,
                                timeline::insert_item_atom_v,
                                row,
                                UuidActorVector({UuidActor(new_uuid, new_item)}));
                        }
                    }
                }

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            for (const auto &i : new_media)
                results.push_back(QUuidFromUuid(i.uuid()));

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}

QFuture<bool> SessionModel::importFuture(const QUrl &path, const QVariant &json) {
    return QtConcurrent::run([=]() {
        bool result = false;
        JsonStore js;

        if (json.isNull()) {
            try {
                js = utility::open_session(UriFromQUrl(path));
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
            spdlog::stopwatch sw;
            scoped_actor sys{system()};

            auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));

            request_receive<utility::UuidVector>(
                *sys, session_actor_, session::merge_session_atom_v, session);

            spdlog::info(
                "Session {} merged in {:.3} seconds.", StdFromQString(path.path()), sw);

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
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);

                if (j.at("type") == "MediaSource") {
                    scoped_actor sys{system()};

                    // get mediasource stream detail.
                    auto actor = actorFromString(system(), j.at("actor"));

                    auto ref = request_receive<utility::MediaReference>(
                        *sys, actor, media::media_reference_atom_v);

                    if (ref.frame_count()) {
                        auto middle_frame = (ref.frame_count() - 1) / 2;
                        auto requested_frame =
                            static_cast<int>(static_cast<float>(ref.frame_count() - 1) * frame);
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
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);

                if (j.at("type") == "MediaSource") {
                    scoped_actor sys{system()};

                    // get mediasource stream detail.
                    auto actor = actorFromString(system(), j.at("actor"));

                    auto ref = request_receive<utility::MediaReference>(
                        *sys, actor, media::media_reference_atom_v);

                    if (ref.frame_count() != 0.0) {
                        auto middle_frame = (ref.frame_count() - 1) / 2;

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
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
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

void SessionModel::gatherMediaFor(const QModelIndex &index, const QModelIndexList &selection) {
    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);

            if (j.at("type") == "Playlist" or j.at("type") == "ContactSheet" or j.at("type") == "Subset" or
                j.at("type") == "Timeline") {
                auto actor = actorFromString(system(), j.at("actor"));
                if (actor) {
                    UuidList uv;
                    for (const auto &i : selection)
                        uv.emplace_back(UuidFromQUuid(i.data(actorUuidRole).toUuid()));

                    anon_send(actor, media_hook::gather_media_sources_atom_v, uv);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

// special handing for media!!
QFuture<QVariant> SessionModel::getJSONObjectFuture(
    const QModelIndex &index, const QString &path, const bool includeSource) {
    return QtConcurrent::run([=]() {
        QVariant result;
        try {
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                if (not j.contains("placeholder")) {
                    auto actor = actorFromString(system(), j.at("actor"));
                    auto type  = j.at("type").get<std::string>();
                    if (actor) {
                        scoped_actor sys{system()};

                        try {
                            std::string path_string = StdFromQString(path);
                            if (type == "Media") {
                                auto jsn = request_receive<JsonStore>(
                                    *sys,
                                    actor,
                                    json_store::get_json_atom_v,
                                    Uuid(),
                                    path_string);

                                if (includeSource) {
                                    auto imageuuid =
                                        UuidFromQUuid(index.data(imageActorUuidRole).toUuid());
                                    auto audiouuid =
                                        UuidFromQUuid(index.data(audioActorUuidRole).toUuid());
                                    auto ijsn = request_receive<JsonStore>(
                                        *sys,
                                        actor,
                                        json_store::get_json_atom_v,
                                        imageuuid,
                                        "");
                                    jsn["metadata"]["image_source_metadata"] = ijsn;
                                    if (imageuuid == audiouuid)
                                        jsn["metadata"]["audio_source_metadata"] = ijsn;
                                    else {
                                        auto ajsn = request_receive<JsonStore>(
                                            *sys,
                                            actor,
                                            json_store::get_json_atom_v,
                                            audiouuid,
                                            "");
                                        jsn["metadata"]["audio_source_metadata"] = ajsn;
                                    }
                                }

                                result = mapFromValue(jsn);
                            } else {
                                auto jsn = request_receive<JsonStore>(
                                    *sys, actor, json_store::get_json_atom_v, path_string);

                                result = mapFromValue(jsn);
                            }
                        } catch (...) {
                        }
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return result;
    });
}

QFuture<QString> SessionModel::getJSONFuture(
    const QModelIndex &index, const QString &path, const bool includeSource) {
    return QtConcurrent::run([=]() {
        QString result;
        try {
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                auto actor        = actorFromString(system(), j.at("actor"));
                auto type         = j.at("type").get<std::string>();
                if (actor) {
                    scoped_actor sys{system()};

                    try {
                        std::string path_string = StdFromQString(path);
                        if (type == "Media") {
                            auto jsn = request_receive<JsonStore>(
                                *sys, actor, json_store::get_json_atom_v, Uuid(), path_string);

                            if (includeSource) {
                                auto imageuuid =
                                    UuidFromQUuid(index.data(imageActorUuidRole).toUuid());
                                auto audiouuid =
                                    UuidFromQUuid(index.data(audioActorUuidRole).toUuid());
                                auto ijsn = request_receive<JsonStore>(
                                    *sys, actor, json_store::get_json_atom_v, imageuuid, "");
                                jsn["metadata"]["image_source_metadata"] = ijsn;
                                if (imageuuid == audiouuid)
                                    jsn["metadata"]["audio_source_metadata"] = ijsn;
                                else {
                                    auto ajsn = request_receive<JsonStore>(
                                        *sys,
                                        actor,
                                        json_store::get_json_atom_v,
                                        audiouuid,
                                        "");
                                    jsn["metadata"]["audio_source_metadata"] = ajsn;
                                }
                            }

                            result = QStringFromStd(jsn.dump());
                        } else {
                            auto jsn = request_receive<JsonStore>(
                                *sys, actor, json_store::get_json_atom_v, path_string);

                            result = QStringFromStd(jsn.dump());
                        }
                    } catch (...) {
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return result;
    });
}

QFuture<bool> SessionModel::setJSONFuture(
    const QModelIndex &index, const QString &json, const QString &path) {
    return QtConcurrent::run([=]() {
        bool result = false;
        try {
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                auto actor        = actorFromString(system(), j.at("actor"));
                auto type         = j.at("type").get<std::string>();
                if (actor) {
                    scoped_actor sys{system()};

                    try {
                        if (type == "Media") {
                            result = request_receive<bool>(
                                *sys,
                                actor,
                                json_store::set_json_atom_v,
                                Uuid(),
                                JsonStore(nlohmann::json::parse(StdFromQString(json))),
                                StdFromQString(path));
                        } else {
                            result = request_receive<bool>(
                                *sys,
                                actor,
                                json_store::set_json_atom_v,
                                JsonStore(nlohmann::json::parse(StdFromQString(json))),
                                StdFromQString(path));
                        }
                    } catch (...) {
                    }
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return result;
    });
}

void SessionModel::sortByMediaDisplayInfo(
    const QModelIndex &index, const int sort_column_idx, const bool ascending) {
    try {

        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            auto actor        = actorFromString(system(), j.at("actor"));
            auto type         = j.at("type").get<std::string>();
            if (actor and (type == "Subset" or type == "ContactSheet" or type == "Playlist" or type == "Timeline")) {
                anon_send(
                    actor,
                    playlist::sort_by_media_display_info_atom_v,
                    sort_column_idx,
                    ascending);
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::setCurrentMediaContainer(const QModelIndex &index) {

    if (index.isValid()) {

        auto cuuid  = UuidFromQUuid(index.data(actorUuidRole).toUuid());
        auto cactor = actorFromQString(system(), index.data(actorRole).toString());

        scoped_actor sys{system()};
        request_receive<bool>(
            *sys,
            session_actor_,
            session::active_media_container_atom_v,
            UuidActor(cuuid, cactor));

        updateCurrentMediaContainerIndexFromBackend();
    }
}


void SessionModel::setViewportCurrentMediaContainerIndex(const QModelIndex &index) {

    try {

        if (index != current_playhead_owner_index_) {

            auto cuuid  = UuidFromQUuid(index.data(actorUuidRole).toUuid());
            auto cactor = actorFromQString(system(), index.data(actorRole).toString());

            // This is the 'viewed' playlist
            const bool viewed = true;

            scoped_actor sys{system()};
            request_receive<bool>(
                *sys,
                session_actor_,
                session::viewport_active_media_container_atom_v,
                UuidActor(cuuid, cactor));

            updateViewportCurrentMediaContainerIndexFromBackend();
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}
