// SPDX-License-Identifier: Apache-2.0

#include "xstudio/conform/conformer.hpp"
#include "xstudio/ui/qml/conform_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"

CAF_PUSH_WARNINGS
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

ConformEngineUI::ConformEngineUI(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    setRoleNames(std::vector<std::string>({"nameRole"}));
}


void ConformEngineUI::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(caf::drop);

    // join conform engine events.
    try {
        scoped_actor sys{system()};
        utility::print_on_create(as_actor(), "ConformEngineUI");
        auto conform_manager = system().registry().template get<caf::actor>(conform_registry);

        try {
            auto uuids =
                request_receive<UuidVector>(*sys, conform_manager, json_store::sync_atom_v);
            conform_uuid_ = uuids[0];

            // get system presets
            auto data = request_receive<JsonStore>(
                *sys, conform_manager, json_store::sync_atom_v, conform_uuid_);
            setModelData(data);

            // join events.
            if (conform_events_) {
                try {
                    request_receive<bool>(
                        *sys, conform_events_, broadcast::leave_broadcast_atom_v, as_actor());
                } catch (const std::exception &) {
                }
                conform_events_ = caf::actor();
            }
            try {
                conform_events_ =
                    request_receive<caf::actor>(*sys, conform_manager, get_event_group_atom_v);
                request_receive<bool>(
                    *sys, conform_events_, broadcast::join_broadcast_atom_v, as_actor());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        // setModelData(tree);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::event_atom,
                conform::conform_tasks_atom,
                const std::vector<std::string> &) {},

            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {
                try {
                    if (uuid == conform_uuid_)
                        receiveEvent(event);

                    // spdlog::warn("{}", modelData().dump(2));
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &g) {
                caf::aout(self())
                    << "ConformEngineUI down: " << to_string(g.source) << std::endl;
            }};
    });
}


QVariant ConformEngineUI::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case Roles::nameRole:
        case Qt::DisplayRole:
            try {
                if (j.count("name"))
                    result = QString::fromStdString(j.at("name"));
            } catch (...) {
            }
            break;

        default:
            result = JSONTreeModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}

QFuture<bool> ConformEngineUI::conformPrepareSequenceFuture(
    const QModelIndex &sequenceIndex, const bool onlyCreateConfrom) const {
    auto future = QFuture<bool>();

    try {
        if (not sequenceIndex.isValid())
            throw std::runtime_error("Invalid Sequence");
        if (sequenceIndex.data(SessionModel::Roles::typeRole).toString() != QString("Timeline"))
            throw std::runtime_error("Invalid Sequence type");

        auto sequence_actor = actorFromString(
            system(),
            StdFromQString(sequenceIndex.data(SessionModel::Roles::actorRole).toString()));
        auto sequence_uuid =
            UuidFromQUuid(sequenceIndex.data(SessionModel::Roles::actorUuidRole).toUuid());

        if (not sequence_actor)
            throw std::runtime_error("Invalid sequence actor");

        future = QtConcurrent::run([=]() {
            auto result = false;

            // populate data into conform request.. ?
            // or should conformer do the heavy lifting..
            auto conform_manager =
                system().registry().template get<caf::actor>(conform_registry);
            scoped_actor sys{system()};

            // always inserts next to old items ?
            // how would this work with timelines ?
            try {
                result = request_receive<bool>(
                    *sys,
                    conform_manager,
                    conform::conform_atom_v,
                    UuidActor(sequence_uuid, sequence_actor),
                    onlyCreateConfrom);

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
        });

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        future = QtConcurrent::run([=]() {
            auto result = false;
            return result;
        });
    }

    return future;
}

QFuture<QList<QUuid>> ConformEngineUI::conformItemsFuture(
    const QString &qtask,
    const QModelIndex &container,
    const QModelIndex &item,
    const bool fanOut,
    const bool removeSource) const {
    const auto task = StdFromQString(qtask);

    // get container detail
    QModelIndexList items;
    auto nofity_item = false;

    // maybe media or clip, or track
    auto item_type = StdFromQString(item.data(SessionModel::Roles::typeRole).toString());
    auto smodel =
        qobject_cast<SessionModel *>(const_cast<QAbstractItemModel *>(container.model()));

    if (item_type == "Media") {
        items.push_back(item);
    } else if (
        item_type == "Clip" and not item.data(SessionModel::Roles::lockedRole).toBool() and
        not item.parent().data(SessionModel::Roles::lockedRole).toBool()) {
        items.push_back(item);
    } else if (
        (item_type == "Video Track" or item_type == "Audio Track") and
        not item.data(SessionModel::Roles::lockedRole).toBool()) {
        nofity_item = true;

        item_type = "Clip";
        // iterate of track clips, adding those that are not locked.
        for (int i = 0; i < item.model()->rowCount(item); i++) {
            auto ind = item.model()->index(i, 0, item);

            if (ind.isValid() and
                ind.data(SessionModel::Roles::typeRole).toString() == "Clip" and
                not ind.data(SessionModel::Roles::lockedRole).toBool()) {
                items.push_back(ind);
            }
        }
    }

    auto jmodel = qobject_cast<JSONTreeModel *>(smodel);

    auto cactor = actorFromString(
        system(), StdFromQString(container.data(SessionModel::Roles::actorRole).toString()));
    auto cuuid = UuidFromQUuid(container.data(SessionModel::Roles::actorUuidRole).toUuid());

    auto playlist_index = smodel->getPlaylistIndex(container);

    auto pactor = actorFromString(
        system(),
        StdFromQString(playlist_index.data(SessionModel::Roles::actorRole).toString()));
    auto puuid =
        UuidFromQUuid(playlist_index.data(SessionModel::Roles::actorUuidRole).toUuid());

    auto item_uav  = UuidActorVector();
    auto before_uv = UuidVector();

    for (const auto &i : items) {
        auto media_index = QModelIndex();

        auto iactor = actorFromString(
            system(), StdFromQString(i.data(SessionModel::Roles::actorRole).toString()));

        auto iuuid = Uuid();
        if (item_type == "Media") {
            media_index = i;
            iuuid       = UuidFromQUuid(i.data(SessionModel::Roles::actorUuidRole).toUuid());
        } else if (item_type == "Clip") {
            // get media actor from clip..
            // there MAY NOT BE ONE..
            // get meda uuid..
            iuuid      = UuidFromQUuid(i.data(JSONTreeModel::Roles::idRole).toUuid());
            auto muuid = UuidFromQUuid(i.data(SessionModel::Roles::clipMediaUuidRole).toUuid());
            // find media actor
            if (not muuid.is_null()) {
                // find in playlist
                auto mlist = container.model()->index(0, 0, container);
                if (mlist.isValid()) {
                    media_index = jmodel->searchRecursive(
                        item.data(SessionModel::Roles::clipMediaUuidRole),
                        SessionModel::Roles::actorUuidRole,
                        mlist,
                        0,
                        0);
                }
            }
        }
        // get uuid of next media..
        auto buuid = Uuid();
        if (media_index.isValid()) {
            auto next_index =
                media_index.model()->sibling(media_index.row() + 1, 0, media_index);
            if (next_index.isValid())
                buuid =
                    UuidFromQUuid(next_index.data(SessionModel::Roles::actorUuidRole).toUuid());
        }

        if (not iuuid.is_null()) {
            item_uav.emplace_back(UuidActor(iuuid, iactor));
            before_uv.push_back(buuid);
        }
    }

    // spdlog::warn(
    //     "ConformEngineUI::conformItemsFuture task {} p {} {} c {} {} {} {} {} {}",
    //     task,
    //     to_string(puuid),
    //     to_string(pactor),
    //     to_string(cuuid),
    //     to_string(cactor),
    //     item_type,
    //     to_string(iuuid),
    //     to_string(iactor),
    //     remove);

    if (item_uav.size() > 1 and fanOut) {
        return QtConcurrent::run([=]() {
            auto result = QList<QUuid>();

            auto smodel = qobject_cast<SessionModel *>(
                const_cast<QAbstractItemModel *>(container.model()));

            auto pending = std::vector<QFuture<QList<QUuid>>>();

            QUuid notify_uuid;

            if (nofity_item)
                notify_uuid = smodel->progressRangeNotification(
                    item, "Conforming Track {}", 0, item_uav.size());

            for (size_t i = 0; i < item_uav.size(); i++) {
                pending.emplace_back(conformItemsFuture(
                    task,
                    utility::UuidActorVector({item_uav.at(i)}),
                    UuidActor(puuid, pactor),
                    UuidActor(cuuid, cactor),
                    item_type,
                    utility::UuidVector({before_uv.at(i)}),
                    removeSource));
            }

            if (nofity_item) {
                std::vector<bool> is_done(item_uav.size());
                auto done = 0;
                while (done != item_uav.size()) {
                    for (size_t i = 0; i < is_done.size(); i++) {
                        if (not is_done[i] and pending[i].isResultReadyAt(0)) {
                            is_done[i] = true;
                            done++;
                            smodel->updateProgressNotification(item, notify_uuid, done);
                        }
                    }
                    // sleep..
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }

            for (auto &i : pending) {
                auto r = i.result();
                result.append(r);
            }

            if (nofity_item)
                smodel->removeNotification(item, notify_uuid);

            return result;
        });
    }

    QUuid notify_uuid;

    if (nofity_item)
        notify_uuid = smodel->processingNotification(item, "Conforming Track");

    return conformItemsFuture(
        task,
        item_uav,
        UuidActor(puuid, pactor),
        UuidActor(cuuid, cactor),
        item_type,
        before_uv,
        removeSource,
        QPersistentModelIndex(item),
        notify_uuid);
}


QFuture<QList<QUuid>> ConformEngineUI::conformToSequenceFuture(
    const QModelIndex &_playlistIndex,
    const QModelIndexList &_mediaIndexes,
    const QModelIndex &sequenceIndex,
    const QModelIndex &conformTrackIndex,
    const bool replace,
    const QString &newTrackName) const {

    auto playlistIndex = _playlistIndex;
    auto mediaIndexes  = _mediaIndexes;

    auto future = QFuture<QList<QUuid>>();

    try {
        if (mediaIndexes.empty())
            throw std::runtime_error("Empty media list");

        if (not playlistIndex.isValid())
            throw std::runtime_error("Invalid Playlist");
        if (playlistIndex.data(SessionModel::Roles::typeRole).toString() != QString("Playlist"))
            throw std::runtime_error("Invalid Playlist type");


        if (not sequenceIndex.isValid())
            throw std::runtime_error("Invalid Sequence");
        if (sequenceIndex.data(SessionModel::Roles::typeRole).toString() != QString("Timeline"))
            throw std::runtime_error("Invalid Sequence type");


        auto conformTrackUuidActor = UuidActor();

        if (conformTrackIndex.isValid()) {
            auto ctuuid =
                UuidFromQUuid(conformTrackIndex.data(JSONTreeModel::Roles::idRole).toUuid());
            auto ctactor = actorFromString(
                system(),
                StdFromQString(
                    conformTrackIndex.data(SessionModel::Roles::actorRole).toString()));

            if (ctactor and not ctuuid.is_null())
                conformTrackUuidActor = UuidActor(ctuuid, ctactor);
        }

        // safe ??
        auto sessionModel = qobject_cast<SessionModel *>(
            const_cast<QAbstractItemModel *>(playlistIndex.model()));
        auto sequencePlaylistIndex = sessionModel->getPlaylistIndex(sequenceIndex);

        if (sequencePlaylistIndex != playlistIndex) {
            playlistIndex = sequencePlaylistIndex;
            mediaIndexes  = sessionModel->copyRows(
                mediaIndexes,
                sessionModel->rowCount(sessionModel->index(0, 0, playlistIndex)),
                playlistIndex);
        }

        auto playlist_actor = actorFromString(
            system(),
            StdFromQString(playlistIndex.data(SessionModel::Roles::actorRole).toString()));
        auto playlist_uuid =
            UuidFromQUuid(playlistIndex.data(SessionModel::Roles::actorUuidRole).toUuid());

        auto media_list = utility::UuidActorVector();
        for (const auto &i : mediaIndexes) {
            auto media_actor = actorFromString(
                system(), StdFromQString(i.data(SessionModel::Roles::actorRole).toString()));
            auto media_uuid =
                UuidFromQUuid(i.data(SessionModel::Roles::actorUuidRole).toUuid());
            if (media_actor)
                media_list.emplace_back(UuidActor(media_uuid, media_actor));
        }

        if (not playlist_actor)
            throw std::runtime_error("Invalid playlist actor");

        auto sequence_actor = actorFromString(
            system(),
            StdFromQString(sequenceIndex.data(SessionModel::Roles::actorRole).toString()));
        auto sequence_uuid =
            UuidFromQUuid(sequenceIndex.data(SessionModel::Roles::actorUuidRole).toUuid());

        if (not sequence_actor)
            throw std::runtime_error("Invalid sequence actor");

        future = QtConcurrent::run([=]() {
            auto result = QList<QUuid>();

            // populate data into conform request.. ?
            // or should conformer do the heavy lifting..
            auto conform_manager =
                system().registry().template get<caf::actor>(conform_registry);
            scoped_actor sys{system()};

            // always inserts next to old items ?
            // how would this work with timelines ?
            try {
                auto operations                  = JsonStore(conform::ConformOperationsJSON);
                operations["create_media"]       = false;
                operations["remove_media"]       = false;
                operations["insert_media"]       = true;
                operations["replace_clip"]       = replace;
                operations["new_track_name"]     = StdFromQString(newTrackName);
                operations["remove_failed_clip"] = true;

                auto reply = request_receive<conform::ConformReply>(
                    *sys,
                    conform_manager,
                    conform::conform_atom_v,
                    operations,
                    UuidActor(playlist_uuid, playlist_actor),
                    UuidActor(sequence_uuid, sequence_actor),
                    conformTrackUuidActor,
                    media_list);

                for (const auto &i : reply.items_) {
                    if (i) {
                        for (const auto &j : *i)
                            result.push_back(QUuidFromUuid(std::get<0>(j)));
                    }
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
        });

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        future = QtConcurrent::run([=]() {
            auto result = QList<QUuid>();
            return result;
        });
    }

    return future;
}


QFuture<QList<QUuid>> ConformEngineUI::conformItemsFuture(
    const std::string &task,
    const utility::UuidActorVector &items,
    const utility::UuidActor &playlist,
    const utility::UuidActor &container,
    const std::string &item_type,
    const utility::UuidVector &before,
    const bool removeSource,
    const QPersistentModelIndex &notifyIndex,
    const QUuid &notifyUuid) const {

    return QtConcurrent::run([=]() {
        auto result = QList<QUuid>();

        if (items.empty())
            return result;

        // populate data into conform request.. ?
        // or should conformer do the heavy lifting..
        auto conform_manager = system().registry().template get<caf::actor>(conform_registry);
        scoped_actor sys{system()};

        // always inserts next to old items ?
        // how would this work with timelines ?
        try {
            auto operations                  = JsonStore(conform::ConformOperationsJSON);
            operations["create_media"]       = true;
            operations["insert_media"]       = true;
            operations["replace_clip"]       = true;
            operations["remove_media"]       = removeSource;
            operations["remove_failed_clip"] = removeSource;

            auto reply = request_receive<conform::ConformReply>(
                *sys,
                conform_manager,
                conform::conform_atom_v,
                task,
                operations,
                playlist,
                container,
                item_type,
                items,
                before);

            for (const auto &i : reply.items_) {
                if (i) {
                    for (const auto &j : *i) {
                        result.push_back(QUuidFromUuid(std::get<0>(j)));
                    }
                }
            }

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        if (not notifyUuid.isNull() and notifyIndex.isValid()) {
            auto smodel = qobject_cast<SessionModel *>(
                const_cast<QAbstractItemModel *>(notifyIndex.model()));
            smodel->removeNotification(notifyIndex, notifyUuid);
        }

        return result;
    });
}

QFuture<QList<QUuid>> ConformEngineUI::conformToNewSequenceFuture(
    const QModelIndexList &mediaIndexes,
    const QString &qtask,
    const QModelIndex &playlistIndex) const {

    auto media = UuidActorVector();

    auto src_playlist_index = QPersistentModelIndex();
    SessionModel *smodel = nullptr;

    // get media uuidactors
    for (const auto &i : mediaIndexes) {
        if (i.data(SessionModel::Roles::typeRole) != "Media")
            continue;

        if(smodel == nullptr) {
            smodel = qobject_cast<SessionModel *>(
                const_cast<QAbstractItemModel *>(i.model()));

            src_playlist_index = QPersistentModelIndex(smodel->getPlaylistIndex(i));
        }

        auto media_actor = actorFromString(
            system(), StdFromQString(i.data(SessionModel::Roles::actorRole).toString()));
        auto media_uuid = UuidFromQUuid(i.data(SessionModel::Roles::actorUuidRole).toUuid());

        if (media_actor) {
            media.emplace_back(UuidActor(media_uuid, media_actor));
        }
    }

    // get target playlist
    auto target_playlist = UuidActor();
    if (playlistIndex.isValid()) {
        auto playlist_actor = actorFromString(
            system(),
            StdFromQString(playlistIndex.data(SessionModel::Roles::actorRole).toString()));
        auto playlist_uuid =
            UuidFromQUuid(playlistIndex.data(SessionModel::Roles::actorUuidRole).toUuid());

        target_playlist = UuidActor(playlist_uuid, playlist_actor);
    }

    auto nquuid = QUuid();
    if(smodel and not media.empty())
        nquuid = smodel->progressRangeNotification(
            src_playlist_index, "Conforming Media", 0, media.size());

    auto task = StdFromQString(qtask);

    return QtConcurrent::run([=]() {
        auto result = QList<QUuid>();

        if (not media.empty()) {
            auto conform_manager =
                system().registry().template get<caf::actor>(conform_registry);
            scoped_actor sys{system()};
            try {

                auto reply = request_receive<
                    std::vector<std::optional<std::pair<std::string, caf::uri>>>>(
                    *sys, conform_manager, conform::conform_atom_v, media);

                auto seq_to_media = std::map<caf::uri, std::vector<UuidActor>>();
                auto seq_to_name  = std::map<caf::uri, std::string>();

                auto count = 0;
                auto media_done = 0;

                for (const auto &i : reply) {
                    if (i) {
                        if (not seq_to_media.count(i->second))
                            seq_to_media[i->second] = std::vector<UuidActor>();

                        seq_to_media[i->second].push_back(media[count]);
                        seq_to_name[i->second] = i->first;
                    }
                    // else
                    //     spdlog::warn("NO RESULT");
                    count++;
                }

                // for(const auto &i: seq_to_name) {
                //     spdlog::warn("{} {}", to_string(i.first), i.second);
                // }

                // for(const auto &i: seq_to_media) {
                //     spdlog::warn("{} {}", to_string(i.first), i.second.size());
                // }

                if (not seq_to_media.empty()) {
                    // find session actor
                    auto session = request_receive<caf::actor>(
                        *sys,
                        system().registry().template get<caf::actor>(studio_registry),
                        session::session_atom_v);


                    for (const auto &i : seq_to_media) {
                        auto playlist       = target_playlist;
                        auto playlist_media = i.second;

                        // we've got a path for a timeline and a list of media actors.
                        // spdlog::warn("{} {}", to_string(i.first), i.second.size());

                        // create new playlist
                        if (not playlist.actor()) {
                            playlist = request_receive<std::pair<utility::Uuid, UuidActor>>(
                                           *sys,
                                           session,
                                           session::add_playlist_atom_v,
                                           seq_to_name.at(i.first),
                                           Uuid(),
                                           false)
                                           .second;

                            // clone media in to new playlist
                            request_receive<UuidVector>(
                                *sys,
                                session,
                                playlist::copy_media_atom_v,
                                playlist.uuid(),
                                vector_to_uuid_vector(playlist_media),
                                false,
                                Uuid(),
                                false);
                            // as playlist will only contain this media just get it all..
                            playlist_media = request_receive<UuidActorVector>(
                                *sys, playlist.actor(), playlist::get_media_atom_v);
                        }

                        auto timeline = request_receive<UuidActor>(
                            *sys,
                            playlist.actor(),
                            session::import_atom_v,
                            i.first,
                            Uuid(),
                            true);

                        // generate conform track.
                        request_receive<bool>(
                            *sys, conform_manager, conform::conform_atom_v, timeline, false);

                        // if task is supplied add conformed track.
                        if (not task.empty()) {
                            // duplicate nominated conform track
                            // always track 0 ?
                            try {
                                auto timeline_item = request_receive<timeline::Item>(
                                    *sys, timeline.actor(), timeline::item_atom_v);
                                auto conform_track_uuid = timeline_item.prop().value(
                                    "conform_track_uuid", utility::Uuid());

                                // iterate over tracks
                                for (int j = 0;
                                     j < static_cast<int>(timeline_item.front().size());
                                     j++) {
                                    if ((*timeline_item.front().item_at_index(j))->uuid() ==
                                        conform_track_uuid) {

                                        // duplicate
                                        auto dup = request_receive<UuidActor>(
                                            *sys,
                                            (*timeline_item.front().item_at_index(j))->actor(),
                                            duplicate_atom_v);
                                        request_receive<JsonStore>(
                                            *sys,
                                            dup.actor(),
                                            timeline::item_lock_atom_v,
                                            false);
                                        request_receive<JsonStore>(
                                            *sys,
                                            dup.actor(),
                                            timeline::item_name_atom_v,
                                            task);

                                        auto dupitem = request_receive<timeline::Item>(
                                            *sys, dup.actor(), timeline::item_atom_v);
                                        auto citems = UuidActorVector();

                                        for (const auto &ditem : dupitem) {
                                            if (ditem.item_type() == timeline::IT_CLIP) {
                                                request_receive<JsonStore>(
                                                    *sys,
                                                    ditem.actor(),
                                                    timeline::item_flag_atom_v,
                                                    "");
                                                citems.emplace_back(ditem.uuid_actor());
                                            }
                                        }

                                        // insert above
                                        auto inserted = request_receive<JsonStore>(
                                            *sys,
                                            timeline_item.front().actor(),
                                            timeline::insert_item_atom_v,
                                            j,
                                            UuidActorVector({dup}));

                                        // get clips from dup
                                        conformItemsFuture(
                                            task,
                                            citems,
                                            playlist,
                                            timeline,
                                            "Clip",
                                            UuidVector(),
                                            true)
                                            .result();
                                        break;
                                    }
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }

                        // conform media to timeline..
                        auto operations            = JsonStore(conform::ConformOperationsJSON);
                        operations["create_media"] = false;
                        operations["remove_media"] = false;
                        operations["insert_media"] = true;
                        operations["replace_clip"] = false;
                        operations["new_track_name"]     = "Conformed Media";
                        operations["remove_failed_clip"] = true;

                        auto reply = request_receive<conform::ConformReply>(
                            *sys,
                            conform_manager,
                            conform::conform_atom_v,
                            operations,
                            playlist,
                            timeline,
                            UuidActor(),
                            playlist_media);
                        if(not nquuid.isNull()) {
                            media_done += playlist_media.size();
                            smodel->updateProgressNotification(src_playlist_index, nquuid, media_done);
                        }
                    }
                } else {
                    if(not nquuid.isNull())
                        smodel->warnNotification(src_playlist_index, "No sequence found for media", 10, nquuid);

                    throw std::runtime_error("No sequence found for media");
                }


            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                if(not nquuid.isNull())
                    smodel->warnNotification(src_playlist_index, err.what(), 10, nquuid);

                throw;
            }
        }

        // return uuid of each timeline containing copied media ?

        if(not nquuid.isNull())
            smodel->infoNotification(src_playlist_index, "Media Conformed to Sequences", 5, nquuid);

        return result;
    });
}

QFuture<QList<QUuid>> ConformEngineUI::conformTracksToSequenceFuture(
    const QModelIndexList &trackIndexes, const QModelIndex &sequenceIndex) const {

    try {
        if (not sequenceIndex.isValid())
            throw std::runtime_error("Invalid destination timeline");

        auto smodel = qobject_cast<SessionModel *>(
            const_cast<QAbstractItemModel *>(sequenceIndex.model()));

        auto stype =
            StdFromQString(sequenceIndex.data(SessionModel::Roles::typeRole).toString());
        if (stype != "Timeline")
            throw std::runtime_error("Invalid destination timeline " + stype);

        auto source_playlist = UuidActor();
        auto source_timeline = UuidActor();
        auto tracks          = UuidActorVector();

        // need to sort these..
        auto sortedTrackIndexes =
            std::vector<QModelIndex>(trackIndexes.begin(), trackIndexes.end());
        std::sort(
            sortedTrackIndexes.begin(),
            sortedTrackIndexes.end(),
            [](QModelIndex &a, QModelIndex &b) { return a.row() > b.row(); });

        for (const auto &i : sortedTrackIndexes) {
            if (i.isValid()) {
                auto type = StdFromQString(i.data(SessionModel::Roles::typeRole).toString());
                if (type == "Video Track" or type == "Audio Track") {
                    auto source_track = UuidActor(
                        UuidFromQUuid(i.data(SessionModel::Roles::actorUuidRole).toUuid()),
                        actorFromString(
                            system(),
                            StdFromQString(i.data(SessionModel::Roles::actorRole).toString())));

                    if (not source_timeline.actor()) {
                        auto source_timeline_index = smodel->getTimelineIndex(i);
                        if (source_timeline_index.isValid()) {
                            auto source_playlist_index =
                                smodel->getPlaylistIndex(source_timeline_index);
                            source_timeline = UuidActor(
                                UuidFromQUuid(source_timeline_index
                                                  .data(SessionModel::Roles::actorUuidRole)
                                                  .toUuid()),
                                actorFromString(
                                    system(),
                                    StdFromQString(source_timeline_index
                                                       .data(SessionModel::Roles::actorRole)
                                                       .toString())));

                            source_playlist = UuidActor(
                                UuidFromQUuid(source_playlist_index
                                                  .data(SessionModel::Roles::actorUuidRole)
                                                  .toUuid()),
                                actorFromString(
                                    system(),
                                    StdFromQString(source_playlist_index
                                                       .data(SessionModel::Roles::actorRole)
                                                       .toString())));
                        }
                    }

                    tracks.emplace_back(source_track);
                } else {
                    spdlog::warn("{} Invalid track type {}", __PRETTY_FUNCTION__, type);
                }
            }
        }

        auto playlistIndex   = smodel->getPlaylistIndex(sequenceIndex);
        auto target_playlist = UuidActor(
            UuidFromQUuid(playlistIndex.data(SessionModel::Roles::actorUuidRole).toUuid()),
            actorFromString(
                system(),
                StdFromQString(playlistIndex.data(SessionModel::Roles::actorRole).toString())));

        auto target_timeline = UuidActor(
            UuidFromQUuid(sequenceIndex.data(SessionModel::Roles::actorUuidRole).toUuid()),
            actorFromString(
                system(),
                StdFromQString(sequenceIndex.data(SessionModel::Roles::actorRole).toString())));

        if (not target_timeline.actor())
            throw std::runtime_error("Invalid destination actor");

        if (not source_timeline.actor())
            throw std::runtime_error("Invalid source actor");

        if (not source_playlist.actor())
            throw std::runtime_error("Invalid source playlist");

        if (tracks.empty())
            throw std::runtime_error("No source tracks");

        return QtConcurrent::run([=]() {
            auto result = QList<QUuid>();
            auto conform_manager =
                system().registry().template get<caf::actor>(conform_registry);
            scoped_actor sys{system()};

            try {
                auto reply = request_receive<conform::ConformReply>(
                    *sys,
                    conform_manager,
                    conform::conform_atom_v,
                    source_playlist,
                    source_timeline,
                    tracks,
                    target_playlist,
                    target_timeline,
                    UuidActor());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
        });

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QtConcurrent::run([=]() {
        auto result = QList<QUuid>();
        return result;
    });
}

QFuture<QList<QUuid>> ConformEngineUI::conformFindRelatedFuture(
    const QString &key, const QModelIndex &clipIndex, const QModelIndex &sequenceIndex) const {

    try {
        if (not clipIndex.isValid() or
            clipIndex.data(SessionModel::Roles::typeRole).toString() != "Clip")
            throw std::runtime_error("Invalid clip");
        if (not sequenceIndex.isValid() or
            sequenceIndex.data(SessionModel::Roles::typeRole).toString() != "Timeline")
            throw std::runtime_error("Invalid sequence");

        return QtConcurrent::run([=]() {
            auto result = QList<QUuid>();
            auto conform_manager =
                system().registry().template get<caf::actor>(conform_registry);
            scoped_actor sys{system()};

            auto clip_ua = UuidActor(
                UuidFromQUuid(clipIndex.data(JSONTreeModel::Roles::idRole).toUuid()),
                actorFromString(
                    system(),
                    StdFromQString(clipIndex.data(SessionModel::Roles::actorRole).toString())));

            auto sequence_ua = UuidActor(
                UuidFromQUuid(sequenceIndex.data(SessionModel::Roles::actorUuidRole).toUuid()),
                actorFromString(
                    system(),
                    StdFromQString(
                        sequenceIndex.data(SessionModel::Roles::actorRole).toString())));

            try {
                auto reply = request_receive<UuidActorVector>(
                    *sys,
                    conform_manager,
                    conform::conform_atom_v,
                    StdFromQString(key),
                    clip_ua,
                    sequence_ua);

                auto sessionModel = qobject_cast<SessionModel *>(
                    const_cast<QAbstractItemModel *>(sequenceIndex.model()));

                for (const auto &i : reply)
                    result.push_back(QUuidFromUuid(i.uuid()));


            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            return result;
        });


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QtConcurrent::run([=]() {
        auto result = QList<QUuid>();
        return result;
    });
}
