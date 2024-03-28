// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/tag/tag.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
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

Q_INVOKABLE bool
SessionModel::removeRows(int row, int count, const bool deep, const QModelIndex &parent) {
    // check if we can delete it..
    auto can_delete = false;
    auto result     = false;
    auto media      = false;
    // spdlog::warn("removeRows {} {}", row, count);

    try {
        for (auto i = row; i < row + count; i++) {
            auto index = SessionModel::index(i, 0, parent);
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                if (j.at("type") == "ContainerDivider" or j.at("type") == "Subset" or
                    j.at("type") == "Timeline") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    if (pactor) {
                        if (j.at("type") == "ContainerDivider" and pactor == session_actor_) {
                            anon_send(
                                pactor,
                                playlist::remove_container_atom_v,
                                j.at("container_uuid").get<Uuid>());
                        } else {
                            // spdlog::warn("Send remove {} {}",
                            // j.at("type").get<std::string>(),
                            // j["container_uuid"].get<std::string>());
                            anon_send(
                                pactor,
                                playlist::remove_container_atom_v,
                                j.at("container_uuid").get<Uuid>(),
                                deep);
                        }
                        can_delete = true;
                    }
                } else if (j.at("type") == "Playlist") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    if (pactor) {
                        // spdlog::warn("Send remove {}", j["container_uuid"]);
                        anon_send(
                            pactor,
                            playlist::remove_container_atom_v,
                            j.at("container_uuid").get<Uuid>());
                        can_delete = true;
                        emit playlistsChanged();
                    }

                } else if (j.at("type") == "Media") {
                    auto pactor = actorFromIndex(index.parent(), true);
                    media       = true;
                    if (pactor) {
                        // spdlog::warn("Send remove {}", j["container_uuid"]);
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

    if (can_delete) {
        result = JSONTreeModel::removeRows(row, count, parent);

        if (media) {
            // spdlog::warn("mediaCountRole {}", rowCount(parent));
            setData(parent.parent(), QVariant::fromValue(rowCount(parent)), mediaCountRole);
        }
    }

    return result;
}


bool SessionModel::duplicateRows(int row, int count, const QModelIndex &parent) {
    auto can_duplicate = false;
    auto result        = false;
    // spdlog::warn("duplicateRows {} {}", row, count);

    std::set<std::string> timeline_types(
        {"Gap", "Clip", "Stack", "Video Track", "Audio Track"});

    if (not parent.isValid())
        return false;

    try {
        auto before      = Uuid();
        auto first_index = SessionModel::index(row, 0, parent);
        auto first_type  = std::string();

        if (first_index.isValid()) {
            nlohmann::json &j = indexToData(first_index);
            first_type        = j.at("type");
        }

        if (timeline_types.count(first_type)) {
            scoped_actor sys{system()};

            nlohmann::json &pj = indexToData(parent);

            auto pactor = pj.count("actor") and not pj.at("actor").is_null()
                              ? actorFromString(system(), pj.at("actor"))
                              : caf::actor();
            if (pactor) {
                // timeline item duplication.
                for (auto i = row; i < row + count; i++) {
                    auto index = SessionModel::index(i, 0, parent);
                    if (index.isValid()) {
                        nlohmann::json &j = indexToData(index);
                        auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                ? actorFromString(system(), j.at("actor"))
                                                : caf::actor();

                        if (actor) {
                            auto actor_uuid = request_receive<UuidActor>(
                                *sys, actor, utility::duplicate_atom_v);
                            // add to parent, next to original..
                            // mess up next ?
                            request_receive<JsonStore>(
                                *sys,
                                pactor,
                                timeline::insert_item_atom_v,
                                i,
                                UuidActorVector({actor_uuid}));
                        }
                    }
                }
            }
        } else {

            try {
                auto before_ind = SessionModel::index(row + 1, 0, parent);
                if (before_ind.isValid()) {
                    nlohmann::json &j = indexToData(before_ind);
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
                if (index.isValid()) {
                    nlohmann::json &j = indexToData(index);

                    if (j.at("type") == "ContainerDivider" or j.at("type") == "Subset" or
                        j.at("type") == "Timeline" or j.at("type") == "Playlist") {
                        auto pactor = actorFromIndex(index.parent(), true);

                        if (pactor) {
                            // spdlog::warn("Send Duplicate {}", j["container_uuid"]);
                            anon_send(
                                pactor,
                                playlist::duplicate_container_atom_v,
                                j.at("container_uuid").get<Uuid>(),
                                before,
                                false);
                            can_duplicate = true;
                            emit playlistsChanged();
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
        if (parent.isValid()) {
            nlohmann::json &j = indexToData(parent);
            // spdlog::warn("copyRows {}", j.dump(2));

            media_parent = index(0, 0, parent);
            target       = j.at("actor_uuid");

            if (j.at("type") == "Playlist")
                emit playlistsChanged();
        } else {
            throw std::runtime_error("invalid parent index");
        }

        for (auto &i : indexes) {
            if (i.isValid()) {
                nlohmann::json &j = indexToData(i);
                if (j.at("type") == "Media")
                    media_uuids.emplace_back(j.at("actor_uuid"));
            }
        }

        // insert dummy entries.
        auto before    = Uuid();
        auto insertrow = index(row, 0, media_parent);
        if (insertrow.isValid()) {
            nlohmann::json &irj = indexToData(insertrow);
            before              = irj.at("actor_uuid");
        }

        JSONTreeModel::insertRows(
            row,
            media_uuids.size(),
            media_parent,
            R"({"type": "Media", "placeholder": true})"_json);
        for (size_t i = 0; i < media_uuids.size(); i++) {
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
        nlohmann::json &j = indexToData(parent);
        // spdlog::warn("moveRows {}", j.dump(2));

        if (j.at("type") == "Playlist") {
            media_parent = index(0, 0, parent);
            target       = j.at("actor_uuid");
            // emit playlistsChanged();
        } else {
            target = j.at("container_uuid");
        }

        for (auto &i : indexes) {
            if (i.isValid()) {
                nlohmann::json &j = indexToData(i);
                if (j.at("type") == "Media") {
                    media_uuids.emplace_back(j.at("actor_uuid"));
                    // spdlog::warn("media uuid {}", j.at("actor_uuid").get<std::string>());
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
        if (insertrow.isValid()) {
            nlohmann::json &irj = indexToData(insertrow);
            before              = irj.at("actor_uuid");
        }

        JSONTreeModel::insertRows(
            row,
            media_uuids.size(),
            media_parent,
            R"({"type": "Media", "placeholder": true})"_json);
        for (size_t i = 0; i < media_uuids.size(); i++) {
            result.push_back(index(row + i, 0, media_parent));
        }

        // move media
        // spdlog::warn("moveRows {}", j.dump(2));
        // spdlog::warn("moveRows target {} source {}", to_string(target), to_string(source));

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
        if (parent.isValid()) {
            const auto type = StdFromQString(qtype);
            const auto name = StdFromQString(qname);
            scoped_actor sys{system()};
            nlohmann::json &j = indexToData(parent);

            // spdlog::warn("SessionModel::insertRows {} {} {} {}", row, count, type, name);
            // spdlog::warn("{}", j.dump(2));

            if (type == "ContainerDivider" or type == "Subset" or type == "Timeline" or
                type == "Playlist") {
                auto before    = Uuid();
                auto insertrow = index(row, 0, parent);
                auto actor     = caf::actor();

                if (insertrow.isValid()) {
                    nlohmann::json &irj = indexToData(insertrow);
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
                        nlohmann::json &pj = indexToData(parent);
                        // spdlog::warn("divider parent {}", pj.dump(2));

                        if (before.is_null())
                            row = rowCount(parent);

                        JSONTreeModel::insertRows(
                            row,
                            count,
                            parent,
                            R"({"type": "ContainerDivider", "placeholder": true, "container_uuid": null})"_json);

                        for (auto i = 0; i < count; i++) {
                            if (not sync) {
                                anon_send(
                                    actor,
                                    playlist::create_divider_atom_v,
                                    name,
                                    before,
                                    false);
                            } else {
                                auto new_item = request_receive<Uuid>(
                                    *sys,
                                    actor,
                                    playlist::create_divider_atom_v,
                                    name,
                                    before,
                                    false);

                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item),
                                    containerUuidRole);
                                // container_uuid
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

                        JSONTreeModel::insertRows(
                            row,
                            count,
                            parent,
                            R"({
                                "type": "Subset", "placeholder": true, "container_uuid": null, "actor_uuid": null, "actor": null
                            })"_json);

                        // spdlog::warn(
                        //     "JSONTreeModel::insertRows Subset ({}, {}, parent);", row,
                        //     count);

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

                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.first),
                                    containerUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.second.uuid()),
                                    actorUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    actorToQString(system(), new_item.second.actor()),
                                    actorRole);
                            }
                            result.push_back(index(row + i, 0, parent));
                        }
                    }
                } else if (type == "Timeline") {
                    actor = j.count("actor_owner") and not j.at("actor_owner").is_null()
                                ? actorFromString(system(), j.at("actor_owner"))
                                : caf::actor();

                    if (actor) {
                        if (before.is_null())
                            row = rowCount(parent);

                        JSONTreeModel::insertRows(
                            row,
                            count,
                            parent,
                            R"({
                                "type": "Timeline", "placeholder": true, "container_uuid": null, "actor_uuid": null, "actor": null
                            })"_json);

                        // spdlog::warn(
                        //     "JSONTreeModel::insertRows Subset ({}, {}, parent);", row,
                        //     count);

                        for (auto i = 0; i < count; i++) {
                            if (not sync) {
                                anon_send(
                                    actor,
                                    playlist::create_timeline_atom_v,
                                    name,
                                    before,
                                    false);
                            } else {
                                auto new_item = request_receive<UuidUuidActor>(
                                    *sys,
                                    actor,
                                    playlist::create_timeline_atom_v,
                                    name,
                                    before,
                                    false);

                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.first),
                                    containerUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.second.uuid()),
                                    actorUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    actorToQString(system(), new_item.second.actor()),
                                    actorRole);
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
                            row,
                            count,
                            parent,
                            R"({"type": "Playlist", "placeholder": true, "container_uuid": null, "actor_uuid": null, "actor": null})"_json);

                        // spdlog::warn(
                        //     "JSONTreeModel::insertRows Playlist ({}, {}, parent);", row,
                        //     count);

                        for (auto i = 0; i < count; i++) {
                            if (not sync) {
                                anon_send(
                                    actor, session::add_playlist_atom_v, name, before, false);
                            } else {
                                auto new_item = request_receive<UuidUuidActor>(
                                    *sys,
                                    actor,
                                    session::add_playlist_atom_v,
                                    name,
                                    before,
                                    false);

                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.first),
                                    containerUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    QUuidFromUuid(new_item.second.uuid()),
                                    actorUuidRole);
                                setData(
                                    index(row + i, 0, parent),
                                    actorToQString(system(), new_item.second.actor()),
                                    actorRole);
                            }
                            // spdlog::warn("ROW {}, {}", row + i, data_.dump(2));
                            result.push_back(index(row + i, 0, parent));
                        }
                        emit playlistsChanged();
                    }
                }
            } else if (
                type == "Gap" or type == "Clip" or type == "Stack" or type == "Audio Track" or
                type == "Video Track") {
                auto parent_actor = j.count("actor") and not j.at("actor").is_null()
                                        ? actorFromString(system(), j.at("actor"))
                                        : caf::actor();
                if (parent_actor) {
                    auto insertion_json =
                        R"({"type": null, "id": null,  "placeholder": true, "actor": null})"_json;
                    insertion_json["type"] = type;

                    JSONTreeModel::insertRows(row, count, parent, insertion_json);

                    for (auto i = 0; i < count; i++) {
                        auto new_uuid = utility::Uuid::generate();
                        auto new_item = caf::actor();

                        if (type == "Video Track") {
                            new_item = self()->spawn<timeline::TrackActor>(
                                "New Video Track", media::MediaType::MT_IMAGE, new_uuid);
                        } else if (type == "Audio Track") {
                            new_item = self()->spawn<timeline::TrackActor>(
                                "New Audio Track", media::MediaType::MT_AUDIO, new_uuid);
                        } else if (type == "Stack") {
                            new_item =
                                self()->spawn<timeline::StackActor>("New Stack", new_uuid);
                        } else if (type == "Gap") {
                            auto duration = utility::FrameRateDuration(
                                24, FrameRate(timebase::k_flicks_24fps));
                            new_item = self()->spawn<timeline::GapActor>(
                                "New Gap", duration, new_uuid);
                        } else if (type == "Clip") {
                            new_item = self()->spawn<timeline::ClipActor>(
                                UuidActor(), "New Clip", new_uuid);
                        }

                        // hopefully add to parent..
                        try {
                            request_receive<JsonStore>(
                                *sys,
                                parent_actor,
                                timeline::insert_item_atom_v,
                                row,
                                UuidActorVector({UuidActor(new_uuid, new_item)}));
                            setData(index(row + i, 0, parent), QUuidFromUuid(new_uuid), idRole);

                            setData(
                                index(row + i, 0, parent),
                                actorToQString(system(), new_item),
                                actorRole);

                            result.push_back(index(row + i, 0, parent));
                        } catch (...) {
                            // failed to insert, kill it..
                            self()->send_exit(new_item, caf::exit_reason::user_shutdown);
                        }
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

void SessionModel::updateErroredCount(const QModelIndex &media_index) {
    try {
        if (media_index.isValid()) {
            auto media_list_index = media_index.parent();
            if (media_list_index.isValid() and
                media_list_index.data(typeRole).toString() == QString("Media List")) {
                auto parent = media_list_index.parent();
                // either subgroup or playlist
                if (parent.isValid() and not parent.data(errorRole).isNull()) {
                    // count statuses of media..
                    auto errors = 0;
                    for (auto i = 0; i < rowCount(media_list_index); i++) {
                        auto mind = index(i, 0, media_list_index);
                        if (mind.isValid() and
                            mind.data(mediaStatusRole).toString() != QString("Online"))
                            errors++;
                    }
                    setData(parent, QVariant::fromValue(errors), errorRole);
                }
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}