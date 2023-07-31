// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/tag/tag.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"
#include "xstudio/ui/qml/caf_response_ui.hpp"

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
                    auto iind = search_recursive(i.data(imageActorUuidRole), actorUuidRole, i);
                    auto aind = search_recursive(i.data(audioActorUuidRole), actorUuidRole, i);

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

            try {
                auto actor =
                    request_receive<caf::actor>(*sys, session_actor_, tag::get_tag_atom_v);
                tag_manager_->set_backend(actor);
                emit tagsChanged();

            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }

            // clear lookup..
            uuid_lookup_.clear();
            string_lookup_.clear();

            setModelData(data);
            emit playlistsChanged();

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

QFuture<QList<QUuid>> SessionModel::handleDropFuture(
    const int proposedAction_, const QVariantMap &drop, const QModelIndex &index) {

    try {
        auto jdrop = dropToJsonStore(drop);
        if (jdrop.count("xstudio/media-ids"))
            return handleMediaIdDropFuture(proposedAction_, jdrop, index);
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

        try {
            // spdlog::warn(
            //     "handleDropFuture {} {} {}", proposedAction, jdrop.dump(2), index.isValid());
            auto valid_index = index.isValid();
            Uuid before;
            // build list of media actor uuids
            UuidVector media_uuids;
            Uuid media_owner_uuid;
            std::string media_owner_name;

            for (const auto &i : jdrop.at("xstudio/media-ids")) {
                // find media index
                auto mind = search_recursive(QUuid::fromString(QStringFromStd(i)), idRole);
                if (mind.isValid()) {
                    if (media_owner_uuid.is_null()) {
                        auto p = mind.parent().parent();
                        if (p.isValid()) {
                            media_owner_name = StdFromQString(p.data(nameRole).toString());
                            media_owner_uuid = UuidFromQUuid(p.data(actorUuidRole).toUuid());
                        }
                    }
                    media_uuids.emplace_back(UuidFromQUuid(mind.data(actorUuidRole).toUuid()));
                }
            }
            if (not media_uuids.empty()) {
                if (valid_index) {
                    // Moving or copying Media to existing playlist, possibly itself.
                    const auto &ij = indexToData(index);
                    // spdlog::warn("{}", ij.at("type").get<std::string>());
                    auto target      = caf::actor();
                    auto target_uuid = ij.at("actor_uuid").get<Uuid>();

                    if (ij.at("type") == "Playlist") {
                        target = actorFromString(system(), ij.at("actor"));
                    } else if (ij.at("type") == "Subset") {
                        target = actorFromIndex(index.parent(), true);
                    } else if (ij.at("type") == "Timeline") {
                        target = actorFromIndex(index.parent(), true);
                    } else if (ij.at("type") == "Media") {
                        before = ij.at("actor_uuid");
                        target = actorFromIndex(index.parent().parent(), true);
                    } else {
                        spdlog::warn("UNHANDLED {}", ij.at("type").get<std::string>());
                    }

                    if (target and not media_uuids.empty()) {
                        bool local_mode = false;
                        // might be adding to end of playlist, which different
                        // check these uuids aren't already in playlist..
                        if (ij.at("type") == "Media")
                            local_mode = true;
                        else {
                            // just check first.
                            auto dup = search(
                                QVariant::fromValue(QUuidFromUuid(media_uuids[0])),
                                actorUuidRole,
                                SessionModel::index(0, 0, index));
                            if (dup.isValid())
                                local_mode = true;
                        }

                        if (local_mode) {
                            anon_send(target, playlist::move_media_atom_v, media_uuids, before);
                        } else {
                            if (proposedAction == Qt::MoveAction) {
                                // move media to new playlist
                                anon_send(
                                    session_actor_,
                                    playlist::move_media_atom_v,
                                    target_uuid,
                                    media_owner_uuid,
                                    media_uuids,
                                    before,
                                    false);
                            } else {
                                anon_send(
                                    session_actor_,
                                    playlist::copy_media_atom_v,
                                    target_uuid,
                                    media_uuids,
                                    false,
                                    before,
                                    false);
                            }
                        }
                    }
                } else {
                    // Moving or copying Media to new playlist

                    if (media_owner_name.empty())
                        media_owner_name = "Untitled Playlist";
                    else
                        media_owner_name += " - copy";

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
                            media_uuids,
                            Uuid(),
                            false);
                    } else {
                        // copy media to new playlist.
                        anon_send(
                            session_actor_,
                            playlist::copy_media_atom_v,
                            uua.second.uuid(),
                            media_uuids,
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
                auto cind = search_recursive(QUuid::fromString(QStringFromStd(i)), idRole);
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
                auto target_uuid = ij.at("actor_uuid").get<Uuid>();
                const auto &type = ij.at("type").get<std::string>();

                if (type == "Playlist") {
                    target = actorFromString(system(), ij.at("actor"));
                } else if (type == "Subset") {
                    target = actorFromIndex(index.parent(), true);
                } else if (type == "Timeline") {
                    target = actorFromIndex(index.parent(), true);
                } else if (type == "Media") {
                    before = ij.at("actor_uuid");
                    target = actorFromIndex(index.parent().parent(), true);
                } else {
                    spdlog::warn("UNHANDLED {}", ij.at("type").get<std::string>());
                }

                if (target) {
                    for (const auto &path : jdrop.at("text/uri-list")) {
                        auto uri = caf::make_uri(url_clean(path.get<std::string>()));
                        if (uri) {
                            // uri maybe timeline...
                            // hacky...
                            if (is_timeline_supported(*uri)) {
                                // spdlog::warn("LOAD TIMELINE {}", to_string(*uri));
                                new_media.push_back(request_receive<UuidActor>(
                                    *sys, target, session::import_atom_v, *uri, before));
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

                    if (type == "Subset") {
                        auto sub_actor = actorFromString(system(), ij.at("actor"));
                        if (sub_actor) {
                            for (const auto &i : new_media)
                                anon_send(
                                    sub_actor, playlist::add_media_atom_v, i.uuid(), Uuid());
                        }
                    } else if (type == "Timeline") {
                        auto sub_actor = actorFromString(system(), ij.at("actor"));
                        if (sub_actor) {
                            for (const auto &i : new_media)
                                anon_send(
                                    sub_actor, playlist::add_media_atom_v, i.uuid(), Uuid());
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
                anon_send(session_actor_, session::load_uris_atom_v, uris, false);
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

            if (valid_index) {
                const auto &ij = indexToData(index);
                // spdlog::warn("{}", ij.at("type").get<std::string>());

                if (ij.at("type") == "Playlist") {
                    target = actorFromString(system(), ij.at("actor"));
                } else if (ij.at("type") == "Subset") {
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), ij.at("actor"));
                } else if (ij.at("type") == "Timeline") {
                    target     = actorFromIndex(index.parent(), true);
                    sub_target = actorFromString(system(), ij.at("actor"));
                } else if (ij.at("type") == "Media") {
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

            if (j.at("type") == "Playlist" or j.at("type") == "Subset" or
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

QFuture<QString> SessionModel::getJSONFuture(const QModelIndex &index, const QString &path) {
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
                        if (type == "Media") {
                            auto jsn = request_receive<JsonStore>(
                                *sys,
                                actor,
                                json_store::get_json_atom_v,
                                Uuid(),
                                StdFromQString(path));

                            result = QStringFromStd(jsn.dump());
                        } else {
                            auto jsn = request_receive<JsonStore>(
                                *sys, actor, json_store::get_json_atom_v, StdFromQString(path));

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

void SessionModel::sortAlphabetically(const QModelIndex &index) {
    try {

        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            auto actor        = actorFromString(system(), j.at("actor"));
            auto type         = j.at("type").get<std::string>();
            if (actor and (type == "Subset" or type == "Playlist" or type == "Timeline")) {
                anon_send(actor, playlist::sort_alphabetically_atom_v);
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::setCurrentPlaylist(const QModelIndex &index) {
    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            auto actor        = actorFromString(system(), j.at("actor"));
            auto type         = j.at("type").get<std::string>();
            if (session_actor_ and actor and
                (type == "Subset" or type == "Playlist" or type == "Timeline")) {
                scoped_actor sys{system()};
                anon_send(session_actor_, session::current_playlist_atom_v, actor);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::setPlayheadTo(const QModelIndex &index) {
    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            auto actor        = actorFromString(system(), j.at("actor"));
            auto type         = j.at("type").get<std::string>();
            if (actor and (type == "Subset" or type == "Playlist" or type == "Timeline")) {
                auto ph_events =
                    system().registry().template get<caf::actor>(global_playhead_events_actor);
                scoped_actor sys{system()};
                try {
                    auto playhead = request_receive<UuidActor>(
                                        *sys, actor, playlist::create_playhead_atom_v)
                                        .actor();
                    anon_send(ph_events, viewport::viewport_playhead_atom_v, playhead);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}
