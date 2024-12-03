// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
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
                    j.at("type") == "Timeline" or j.at("type") == "ContactSheet") {
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
                        j.at("type") == "Timeline" or j.at("type") == "Playlist" or j.at("type") == "ContactSheet") {
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
        if (not parent.isValid())
            throw std::runtime_error("invalid parent index");

        auto parent_type = StdFromQString(parent.data(typeRole).toString());

        if (parent_type == "Video Track" or parent_type == "Audio Track") {
            // clone indexes and insert..

            std::set<std::string> timeline_types(
                {"Gap", "Clip", "Stack", "Video Track", "Audio Track"});

            scoped_actor sys{system()};

            nlohmann::json &pj = indexToData(parent);

            auto pactor = pj.count("actor") and not pj.at("actor").is_null()
                              ? actorFromString(system(), pj.at("actor"))
                              : caf::actor();

            if (pactor) {
                // timeline item duplication.
                auto insertion_row = row;
                for (const auto &i : indexes) {
                    if (i.isValid()) {
                        auto item_type    = StdFromQString(i.data(typeRole).toString());
                        nlohmann::json &j = indexToData(i);
                        auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                ? actorFromString(system(), j.at("actor"))
                                                : caf::actor();

                        if (actor) {
                            auto actor_uuid = request_receive<UuidActor>(
                                *sys, actor, utility::duplicate_atom_v);

                            // reset state..
                            anon_send(actor_uuid.actor(), timeline::item_lock_atom_v, false);
                            anon_send(actor_uuid.actor(), plugin_manager::enable_atom_v, true);
                            // anon_send(actor_uuid.actor(), timeline::item_flag_atom_v, "");

                            auto insertion_json =
                                R"({"type": null, "id": null,  "placeholder": true, "actor": null})"_json;
                            insertion_json["type"] = item_type;
                            insertion_json["id"]   = actor_uuid.uuid();

                            insertion_json["actor"] =
                                actorToString(system(), actor_uuid.actor());

                            // spdlog::warn("{}", insertion_json.dump(2));

                            JSONTreeModel::insertRows(insertion_row, 1, parent, insertion_json);

                            anon_send(
                                pactor,
                                timeline::insert_item_atom_v,
                                insertion_row,
                                UuidActorVector({actor_uuid}));

                            result.push_back(index(insertion_row, 0, parent));

                            insertion_row++;
                        }
                    }
                }
            }
        } else {
            UuidVector media_uuids;
            Uuid target;

            auto media_parent = parent;
            if (parent.isValid()) {
                nlohmann::json &j = indexToData(parent);
                media_parent      = index(0, 0, parent);
                target            = j.at("actor_uuid");

                if (j.at("type") == "Playlist")
                    emit playlistsChanged();
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
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // wait for them to be valid..
    for (const auto &i : result) {
        while (i.data(SessionModel::Roles::placeHolderRole).toBool() == true) {
            QCoreApplication::processEvents(
                QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
        }
    }

    return result;
}

QModelIndexList SessionModel::moveRows(
    const QModelIndexList &indexes,
    const int q_row,
    const QModelIndex &parent,
    const bool doCopy) {

    QModelIndexList result;
    try {
        UuidVector media_uuids;
        Uuid target;
        Uuid source;
        auto row = q_row;

        auto media_parent = parent;
        nlohmann::json &j = indexToData(parent);

        // spdlog::warn("moveRows {}", j.dump(2));

        media_parent = index(0, 0, parent);
        target       = j.at("actor_uuid");

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
        if (row == -1) {
            row = rowCount(media_parent);
        }

        auto before    = Uuid();
        auto insertrow = index(row, 0, media_parent);
        if (insertrow.isValid()) {
            nlohmann::json &irj = indexToData(insertrow);
            before              = irj.at("actor_uuid");
        }

        if (not media_uuids.empty()) {
            scoped_actor sys{system()};
            auto new_media = utility::UuidVector();
            // send action to backend.
            if (doCopy) {
                new_media = request_receive<utility::UuidVector>(
                    *sys,
                    session_actor_,
                    playlist::copy_media_atom_v,
                    target,
                    media_uuids,
                    false,
                    before,
                    false);

            } else {
                new_media = request_receive<utility::UuidVector>(
                    *sys,
                    session_actor_,
                    playlist::move_media_atom_v,
                    target,
                    source,
                    media_uuids,
                    before,
                    false);
            }

            if (not new_media.empty()) {
                // can't tell if it's already in there..
                // so we end up with duff entries.
                // wait for the new media to appear so we can capture the indexes.
                for (const auto &i : new_media) {
                    while (true) {
                        auto mindex = search(
                            QVariant::fromValue(QUuidFromUuid(i)),
                            actorUuidRole,
                            media_parent,
                            0);
                        if (mindex.isValid()) {
                            result.push_back(mindex);
                            break;
                        }

                        QCoreApplication::processEvents(
                            QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents,
                            50);
                    }
                }
            }
        }

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
                type == "Playlist" or type == "ContactSheet") {
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
                            // if (not sync) {
                            anon_send(
                                actor, playlist::create_divider_atom_v, name, before, false);
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
                            anon_send(
                                actor, playlist::create_subset_atom_v, name, before, false);
                            result.push_back(index(row + i, 0, parent));
                        }
                    }
                } else if (type == "ContactSheet") {
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
                                "type": "ContactSheet", "placeholder": true, "container_uuid": null, "actor_uuid": null, "actor": null
                            })"_json);

                        // spdlog::warn(
                        //     "JSONTreeModel::insertRows Subset ({}, {}, parent);", row,
                        //     count);

                        for (auto i = 0; i < count; i++) {
                            anon_send(
                                actor, playlist::create_contact_sheet_atom_v, name, before, false);
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
                            anon_send(
                                actor,
                                playlist::create_timeline_atom_v,
                                name,
                                before,
                                false,
                                true);
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
                            anon_send(actor, session::add_playlist_atom_v, name, before, false);
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

                    // get timeline rate from parent.
                    auto rate = request_receive<FrameRate>(*sys, parent_actor, rate_atom_v);

                    // spdlog::warn("{}", rate.to_fps());

                    JSONTreeModel::insertRows(row, count, parent, insertion_json);

                    for (auto i = 0; i < count; i++) {
                        auto new_uuid = utility::Uuid::generate();
                        auto new_item = caf::actor();

                        if (type == "Video Track") {
                            new_item = self()->spawn<timeline::TrackActor>(
                                name.empty() ? "New Video Track" : name,
                                rate,
                                media::MediaType::MT_IMAGE,
                                new_uuid);
                        } else if (type == "Audio Track") {
                            new_item = self()->spawn<timeline::TrackActor>(
                                name.empty() ? "New Audio Track" : name,
                                rate,
                                media::MediaType::MT_AUDIO,
                                new_uuid);
                        } else if (type == "Stack") {
                            new_item = self()->spawn<timeline::StackActor>(
                                name.empty() ? "New Stack" : name, rate, new_uuid);
                        } else if (type == "Gap") {
                            auto duration = utility::FrameRateDuration(24, rate);
                            new_item      = self()->spawn<timeline::GapActor>(
                                name.empty() ? "Gap" : name, duration, new_uuid);
                        } else if (type == "Clip") {
                            new_item = self()->spawn<timeline::ClipActor>(
                                UuidActor(), name.empty() ? "New Clip" : name, new_uuid);
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

        if (sync) {
            for (const auto &i : result) {
                while (i.data(placeHolderRole).toBool() == true) {
                    QCoreApplication::processEvents(
                        QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    qDebug() << "result " << result << "\n";

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
    return;
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