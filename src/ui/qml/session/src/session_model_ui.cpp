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

caf::actor SessionModel::actorFromIndex(const QModelIndex &index, const bool try_parent) {
    auto result = caf::actor();

    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            result            = j.count("actor") and not j.at("actor").is_null()
                                    ? actorFromString(system(), j.at("actor"))
                                    : caf::actor();

            if (not result and try_parent) {
                QModelIndex pindex = index.parent();
                if (pindex.isValid()) {
                    nlohmann::json &pj = indexToData(pindex);
                    result             = pj.count("actor") and not pj.at("actor").is_null()
                                             ? actorFromString(system(), pj.at("actor"))
                                             : caf::actor();
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

utility::Uuid
SessionModel::actorUuidFromIndex(const QModelIndex &index, const bool try_parent) {
    auto result = utility::Uuid();

    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            result            = j.count("actor_uuid") and not j.at("actor_uuid").is_null()
                                    ? j.at("actor_uuid").get<Uuid>()
                                    : utility::Uuid();

            if (result.is_null() and try_parent) {
                QModelIndex pindex = index.parent();
                if (pindex.isValid()) {
                    nlohmann::json &pj = indexToData(pindex);
                    result = pj.count("actor_uuid") and not pj.at("actor_uuid").is_null()
                                 ? pj.at("actor_uuid").get<Uuid>()
                                 : utility::Uuid();
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}


void SessionModel::forcePopulate(
    const nlohmann::json &item, const nlohmann::json::json_pointer &search_hint) {
    if (item.count("group_actor") and not item.at("group_actor").is_null()) {
        auto grp = actorFromString(system(), item.at("group_actor").get<std::string>());
        if (grp) {
            anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
            // spdlog::error("join grp {} {} {}",
            //     ijc.back().at("type").is_string() ?
            //     ijc.back().at("type").get<std::string>() : "",
            //     ijc.back().at("name").is_string() ?
            //     ijc.back().at("name").get<std::string>() : "",
            //     to_string(grp));
        }

    } else if (
        item.count("actor") and not item.at("actor").is_null() and
        item.at("group_actor").is_null()) {
        // spdlog::info("request group {}", i);
        requestData(
            QVariant::fromValue(QUuidFromUuid(item.at("id"))),
            Roles::idRole,
            search_hint,
            item,
            Roles::groupActorRole);
    }

    const auto type = item.count("type") ? item.at("type").get<std::string>() : std::string();

    // spdlog::warn("{} {}", type, item.dump(2));
    // if subset or playlist, trigger auto population of children
    if (type == "Playlist" or type == "Subset" or type == "Timeline") {
        try {
            requestData(
                QVariant::fromValue(QUuidFromUuid(item.at("children").at(0).at("id"))),
                Roles::idRole,
                search_hint,
                item.at("children").at(0),
                Roles::childrenRole);
            requestData(
                QVariant::fromValue(QUuidFromUuid(item.at("children").at(1).at("id"))),
                Roles::idRole,
                search_hint,
                item.at("children").at(1),
                Roles::childrenRole);

            if (type == "Playlist")
                requestData(
                    QVariant::fromValue(QUuidFromUuid(item.at("children").at(2).at("id"))),
                    Roles::idRole,
                    search_hint,
                    item.at("children").at(2),
                    Roles::childrenRole);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
}

void SessionModel::refreshId(nlohmann::json &ij) {
    if (ij.count("id"))
        ij["id"] = Uuid::generate();
}

void SessionModel::processChildren(
    const nlohmann::json &rj, nlohmann::json &ij, const QModelIndex &index) {
    QVector<int> roles({Roles::childrenRole});
    auto changed = false;
    START_SLOW_WATCHER()

    // spdlog::info("current {}", data_.dump(2));
    // spdlog::info("new {}", rj.dump(2));
    // spdlog::info("old {}", ij.dump(2));
    // spdlog::info("target {}", indexToData(index).dump(2));
    const auto type = ij.count("type") ? ij.at("type").get<std::string>() : std::string();

    try {
        if (type == "Session" or type == "Container List" or type == "Media" or
            type == "Media List" or type == "PlayheadSelection" or type == "MediaSource") {

            // point to result  children..
            auto rjc = nlohmann::json();

            if (rj.is_array())
                rjc = rj;
            else
                rjc = rj.at("children");

            if (rjc.is_null())
                rjc = R"([])"_json;

            // init local children if null
            if (ij.at("children").is_null()) {
                ij["children"] = R"([])"_json;

                // // special handling for nested..
                // if (type == "Session" or type == "Container List") {
                //     if(rj.at(0).count("container_uuid")) {
                //         ij["container_uuid"] = rj.at(0).at("container_uuid");
                //         roles.append(containerUuidRole);
                //     }

                //     ij["flag"] = rj.at(0).at("flag");
                //     roles.append(flagRole);
                // }
                // changed = true;
            }

            // point to children of index we're updating
            auto &ijc = ij["children"];

            // compare children, append / insert / remove..
            // spdlog::info("processChildren {} new {} old {}",
            // type.get<std::string>(), rjc.size(), ijc.size());

            // set up comparison key
            auto compare_key = "";
            if (type == "Session" or type == "Container List") {
                emit playlistsChanged();
                compare_key = "container_uuid";
            } else if (type == "Media List" or type == "Media")
                compare_key = "actor_uuid";
            else if (type == "PlayheadSelection")
                compare_key = "uuid";
            else if (type == "MediaSource") {
                compare_key = "type";
                // spdlog::warn("{}", rj.dump(2));
            }

            // remove delete entries
            // build lookup.
            std::set<Uuid> rju;
            for (size_t i = 0; i < rjc.size(); i++) {
                if (rjc.at(i).count(compare_key))
                    rju.insert(rjc.at(i).at(compare_key).get<Uuid>());
            }


            bool removal_done = false;
            while (not removal_done) {
                removal_done = true;

                size_t start_index = 0;
                size_t count       = 0;

                for (size_t i = 0; i < ijc.size(); i++) {
                    if (ijc.at(i).count(compare_key) and not ijc.at(i).count("placeholder") and
                        not rju.count(ijc.at(i).at(compare_key))) {

                        if (start_index + count == i) {
                            count++;
                        } else if (count) {
                            // non sequential, remove previous.
                            JSONTreeModel::removeRows(start_index, count, index);
                            count        = 0;
                            removal_done = false;
                            changed      = true;
                        }

                        if (not count) {
                            count       = 1;
                            start_index = i;
                        }
                    }
                }

                // chaser
                if (count) {
                    JSONTreeModel::removeRows(start_index, count, index);
                    removal_done = false;
                    changed      = true;
                }
            }

            // build uuid/index map for old.
            std::map<Uuid, size_t> iju;
            for (size_t i = 0; i < ijc.size(); i++) {
                if (ijc.at(i).count(compare_key) and not ijc.at(i).count("placeholder"))
                    iju[ijc.at(i).at(compare_key).get<Uuid>()] = i;
            }


            // whenever we insert new items, refresh the id field
            // as we maybe inserting the same data in multiple places
            // and the ID needs to be unique..
            // this currently only applies to Media List/ Media / Media Source.

            // process new / changed.
            for (size_t i = 0; i < rjc.size(); i++) {

                // APPEND NEW ENTRIES
                if (i >= ijc.size()) {
                    // if appending we can process them all in one hit..
                    // spdlog::warn("append {} {}",
                    // mapFromValue(index.data(JSONPointerRole)).dump(), i);
                    auto start = ijc.size();

                    beginInsertRows(index, i, rjc.size() - 1);
                    for (; i < rjc.size(); i++) {
                        ijc.push_back(rjc.at(i));
                        refreshId(ijc.back());
                    }
                    endInsertRows();

                    emit dataChanged(
                        SessionModel::index(start, 0, index),
                        SessionModel::index(ijc.size() - 1, 0, index),
                        QVector<int>());

                    auto hint = getIndexPath(index.internalId());
                    for (auto ii = start; ii < ijc.size(); ++ii)
                        forcePopulate(ijc[ii], hint);

                    changed = true;
                    break;
                } else {
                    if (ijc.at(i).count("placeholder") or not ijc.at(i).count(compare_key)) {
                        // OVERWRITE PLACE HOLDER
                        ijc.at(i) = rjc.at(i);
                        refreshId(ijc.at(i));

                        forcePopulate(ijc.at(i), getIndexPath(index.internalId()));

                        // force updated of replace child.
                        emit dataChanged(
                            SessionModel::index(i, 0, index),
                            SessionModel::index(i, 0, index),
                            QVector<int>());

                        changed = true;
                    } else if (
                        ijc.at(i).count(compare_key) and rjc.at(i).count(compare_key) and
                        ijc.at(i).at(compare_key) == rjc.at(i).at(compare_key)) {
                        // skip..
                        // spdlog::warn("skip {}", i);
                    } else if (not iju.count(rjc.at(i).at(compare_key))) {
                        // insert
                        // spdlog::warn("insert {}", i);
                        beginInsertRows(index, i, i);
                        ijc.insert(ijc.begin() + i, rjc.at(i));
                        endInsertRows();

                        refreshId(ijc.at(i));

                        emit dataChanged(
                            SessionModel::index(i, 0, index),
                            SessionModel::index(i, 0, index),
                            QVector<int>());

                        forcePopulate(ijc.at(i), getIndexPath(index.internalId()));

                        changed = true;
                    } else {
                        // re-order done later
                    }
                }
            }

            // handle reordering
            // all rows exist but in wrong order..

            try {
                auto ordered = false;
                while (not ordered) {
                    ordered = true;
                    for (size_t i = 0; i < rjc.size(); i++) {
                        // spdlog::warn("{} {}",i,rjc.at(i).at("name").dump());

                        if (ijc.at(i).count(compare_key) and rjc.at(i).count(compare_key) and
                            ijc.at(i).at(compare_key) != rjc.at(i).at(compare_key)) {
                            ordered = false;
                            // find actual index of model row
                            // spdlog::warn("{} -> {}", iju[rjc.at(i).at(compare_key)], i);
                            JSONTreeModel::moveRows(
                                index, iju[rjc.at(i).at(compare_key)], 1, index, i);
                            // for update of item, as listview seems to get confused..

                            emit dataChanged(
                                SessionModel::index(i, 0, index),
                                SessionModel::index(i, 0, index),
                                QVector<int>());

                            // rebuild iju
                            iju.clear();
                            for (size_t i = 0; i < ijc.size(); i++) {
                                if (ijc.at(i).count(compare_key) and
                                    not ijc.at(i).count("placeholder"))
                                    iju[ijc.at(i).at(compare_key).get<Uuid>()] = i;
                            }
                            break;
                        }
                    }
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} reorder {}", __PRETTY_FUNCTION__, err.what());
            }
        } else {
            spdlog::warn("SessionModel::processChildren unhandled {}\n{}", type, rj.dump(2));
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        spdlog::warn("{}", ij.dump(2));
        spdlog::warn("{}", rj.dump(2));
    }

    // spdlog::warn("{}", data_.dump(2));

    if (changed) {
        // update totals.
        if (type == "Media List" and ij.at("children").is_array()) {
            // spdlog::warn("mediaCountRole {}", ij.at("children").size());
            setData(
                index.parent(), QVariant::fromValue(ij.at("children").size()), mediaCountRole);
        }

        emit dataChanged(index, index, roles);
    }
    CHECK_SLOW_WATCHER_FAST()
}

// optimised for finding the media index..

QModelIndexList SessionModel::search_recursive_media(
    const nlohmann::json &searchValue,
    const std::string &searchKey,
    const nlohmann::json::json_pointer &path,
    const nlohmann::json &root,
    const int hits) const {

    QModelIndexList results;
    START_SLOW_WATCHER()

    if (root.value(searchKey, nlohmann::json()) == searchValue)
        results.push_back(pointerToIndex(path));

    // if we find a result in a child, don't scan siblings.
    // we only get duplicates at the playlist level.
    // only multi scan at playlist level.

    // don't scan media children
    if (root.value("type", "") == "Media")
        return results;

    // recurse..
    if (hits == -1 or results.size() < hits) {
        const nlohmann::json *children = nullptr;

        if (root.is_array())
            children = &root;
        else if (root.contains(children_) and root.at(children_).is_array()) {
            children = &root.at(children_);
        }

        if (children) {
            auto row = 0;
            for (const auto &i : *children) {
                // no media here..
                if (i.value("type", "") == "Container List" or
                    i.value("type", "") == "PlayheadSelection")
                    continue;

                auto more_result = search_recursive_fast(
                    searchValue,
                    searchKey,
                    path / children_ / std::to_string(row),
                    i,
                    hits == -1 ? -1 : hits - results.size());
                row++;

                results.append(more_result);
                if (hits != -1 and results.size() >= hits)
                    break;

                if (not results.empty() and root.value("type", "") == "Session")
                    break;
            }
        }
    }
    CHECK_SLOW_WATCHER()

    return results;
}


QModelIndexList SessionModel::search_recursive_fast(
    const nlohmann::json &searchValue,
    const std::string &searchKey,
    const nlohmann::json::json_pointer &path,
    const nlohmann::json &root,
    const int hits) const {
    QModelIndexList results;
    START_SLOW_WATCHER()

    if (root.value(searchKey, nlohmann::json()) == searchValue)
        results.push_back(pointerToIndex(path));

    // if we find a result in a child, don't scan siblings.
    // we only get duplicates at the playlist level.
    // only multi scan at playlist level.

    // recurse..
    if (hits == -1 or results.size() < hits) {
        const nlohmann::json *children = nullptr;

        if (root.is_array())
            children = &root;
        else if (root.contains(children_) and root.at(children_).is_array()) {
            children = &root.at(children_);
        }

        if (children) {
            auto row = 0;
            for (const auto &i : *children) {
                auto more_result = search_recursive_fast(
                    searchValue,
                    searchKey,
                    path / children_ / std::to_string(row),
                    i,
                    hits == -1 ? -1 : hits - results.size());
                row++;

                results.append(more_result);
                if (hits != -1 and results.size() >= hits)
                    break;

                if (not results.empty() and root.value("type", "") == "Session")
                    break;
            }
        }
    }
    CHECK_SLOW_WATCHER()

    return results;
}

void SessionModel::receivedDataSlot(
    const QVariant &search_value,
    const int search_role,
    const QString &search_hint,
    const int role,
    const QString &result) {

    try {
        receivedData(
            mapFromValue(search_value),
            search_role,
            nlohmann::json::json_pointer(StdFromQString(search_hint)),
            role,
            json::parse(StdFromQString(result)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        // spdlog::warn("{}", data_.dump(2));
    }
    auto inflight = std::make_tuple(search_value, search_role, role);
    if (in_flight_requests_.count(inflight)) {
        in_flight_requests_.erase(inflight);
    }
}

void SessionModel::receivedData(
    const nlohmann::json &search_value,
    const int search_role,
    const nlohmann::json::json_pointer &search_hint,
    const int role,
    const nlohmann::json &result) {

    START_SLOW_WATCHER()

    try {
        // because media can be shared, this is inefficient..
        auto hits = (search_role == actorUuidRole or search_role == actorRole) ? -1 : 1;

        // auto indexes = search_recursive(
        //     search_value, search_role, QModelIndex(), 0, hits);

        std::map<int, std::string> role_to_search(
            {{Roles::actorUuidRole, "actor_uuid"},
             {Roles::actorRole, "actor"},
             {Roles::containerUuidRole, "container_uuid"},
             {Roles::idRole, "id"}});

        auto hint = search_hint;
        if (hint == nlohmann::json::json_pointer() or not data_.contains(hint))
            hint = nlohmann::json::json_pointer("/0");

        auto indexes = search_recursive_fast(
            search_value, role_to_search[search_role], hint, data_.at(hint), hits);

        std::map<int, std::string> role_to_key({
            {Roles::pathRole, "path"},
            {Roles::rateFPSRole, "rate"},
            {Roles::formatRole, "format"},
            {Roles::resolutionRole, "resolution"},
            {Roles::pixelAspectRole, "pixel_aspect"},
            {Roles::bitDepthRole, "bit_depth"},
            {Roles::mtimeRole, "mtime"},
            {Roles::nameRole, "name"},
            {Roles::actorUuidRole, "actor_uuid"},
            {Roles::actorRole, "actor"},
            {Roles::audioActorUuidRole, "audio_actor_uuid"},
            {Roles::imageActorUuidRole, "image_actor_uuid"},
            {Roles::mediaStatusRole, "media_status"},
            {Roles::flagRole, "flag"},
            {Roles::thumbnailURLRole, "thumbnail_url"},
        });

        for (auto &index : indexes) {

            if (index.isValid()) {
                QVector<int> roles({role});
                nlohmann::json &j = indexToData(index);

                switch (role) {
                default:
                    if (role_to_key.count(role)) {
                        if (j.count(role_to_key[role]) and j.at(role_to_key[role]) != result) {
                            j[role_to_key[role]] = result;
                            emit dataChanged(index, index, roles);
                        }
                    }
                    break;
                case Roles::thumbnailURLRole:
                    if (role_to_key.count(role)) {
                        if (j.count(role_to_key[role]) and j.at(role_to_key[role]) != result) {
                            if (result != "RETRY")
                                j[role_to_key[role]] = result;
                            emit dataChanged(index, index, roles);
                        }
                    }
                    break;

                case Roles::groupActorRole:
                    if (j.count("group_actor") and j.at("group_actor") != result) {
                        j["group_actor"] = result;
                        auto grp         = actorFromString(system(), result.get<std::string>());
                        if (grp) {
                            anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());
                            // spdlog::error(
                            //     "join grp {} {} {}",
                            //     j.at("type").is_string() ? j.at("type").get<std::string>() :
                            //     "", j.at("name").is_string() ?
                            //     j.at("name").get<std::string>() : "", to_string(grp));
                        }
                        //  should we join ??
                        emit dataChanged(index, index, roles);
                        // spdlog::warn("{}", data_.dump(2));
                    }
                    break;

                case Roles::childrenRole:
                    processChildren(result, j, index);
                    break;
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    // spdlog::warn("{}", data_.dump(2));
    CHECK_SLOW_WATCHER()
}

// if(can_duplicate)
//     result = JSONTreeModel::insertRows(row, count, parent);

void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const nlohmann::json::json_pointer &search_hint,
    const QModelIndex &index,
    const int role) const {
    // dispatch call to backend to retrieve missing data.

    // spdlog::warn("{} {}", role, StdFromQString(roleName(role)));
    try {
        requestData(search_value, search_role, search_hint, indexToData(index), role);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const nlohmann::json::json_pointer &search_hint,
    const nlohmann::json &data,
    const int role) const {
    // dispatch call to backend to retrieve missing data.
    auto inflight = std::make_tuple(search_value, search_role, role);

    if (not in_flight_requests_.count(inflight)) {
        in_flight_requests_.emplace(inflight);

        // spdlog::warn("request {} {} {} {}",
        //     mapFromValue(search_value).dump(2),
        //     StdFromQString(roleName(search_role)),
        //     data.dump(2),
        //     StdFromQString(roleName(role))
        // );

        auto tmp = new CafResponse(
            search_value,
            search_role,
            search_hint,
            data,
            role,
            StdFromQString(roleName(role)),
            request_handler_);

        connect(tmp, &CafResponse::received, this, &SessionModel::receivedDataSlot);
    }
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
        result["type"]     = tree.type();
        result["name"]     = tree.name();
        result["children"] = R"([])"_json;

        // playlists and dividers..
        for (const auto &i : tree.children_ref()) {
            if (i.value().type() == "ContainerDivider") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "flag": null,
                    "type": null
                })"_json);
                n.erase("children");
                n["type"]           = i.value().type();
                n["name"]           = i.value().name();
                n["flag"]           = i.value().flag();
                n["actor_uuid"]     = i.value().uuid();
                n["container_uuid"] = i.uuid();
                result["children"].emplace_back(n);
            } else if (i.value().type() == "Subset" or i.value().type() == "Timeline") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "group_actor": null,
                    "flag": null,
                    "type": null,
                    "actor": null,
                    "busy": false,
                    "media_count": 0
                })"_json);

                n["type"]           = i.value().type();
                n["name"]           = i.value().name();
                n["flag"]           = i.value().flag();
                n["actor_uuid"]     = i.value().uuid();
                n["container_uuid"] = i.uuid();
                if (uuid_actors.count(i.value().uuid()))
                    n["actor"] = actorToString(sys, uuid_actors.at(i.value().uuid()));

                n["children"] = R"([])"_json;

                n["children"].push_back(
                    createEntry(R"({"type": "Media List", "actor_owner": null})"_json));
                n["children"][0]["actor_owner"] = n["actor"];

                n["children"].push_back(createEntry(
                    R"({"type": "PlayheadSelection", "name": null, "actor_owner": null, "actor_uuid": null, "actor": null, "group_actor": null})"_json));
                n["children"][1]["actor_owner"] = n["actor"];

                result["children"].emplace_back(n);
            } else {
                spdlog::warn("{} invalid type {}", __PRETTY_FUNCTION__, i.value().type());
            }
        }

        // spdlog::warn("playlistTreeToJson {}",result.dump(2));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

nlohmann::json SessionModel::sessionTreeToJson(
    const utility::PlaylistTree &tree,
    actor_system &sys,
    const utility::UuidActorMap &uuid_actors) {
    auto result = createEntry(R"({
        "name": null
    })"_json);

    try {
        result["type"]     = tree.type();
        result["name"]     = tree.name();
        result["children"] = R"([])"_json;

        // playlists and dividers..
        for (const auto &i : tree.children_ref()) {
            if (i.value().type() == "ContainerDivider") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "flag": null,
                    "type": null
                })"_json);
                n.erase("children");
                n["type"]           = i.value().type();
                n["name"]           = i.value().name();
                n["flag"]           = i.value().flag();
                n["actor_uuid"]     = i.value().uuid();
                n["container_uuid"] = i.uuid();
                result["children"].emplace_back(n);
            } else if (i.value().type() == "Playlist") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "group_actor": null,
                    "flag": null,
                    "type": null,
                    "actor": null,
                    "busy": false,
                    "media_count": 0
                })"_json);

                n["type"]           = i.value().type();
                n["name"]           = i.value().name();
                n["flag"]           = i.value().flag();
                n["actor_uuid"]     = i.value().uuid();
                n["container_uuid"] = i.uuid();
                if (uuid_actors.count(i.value().uuid()))
                    n["actor"] = actorToString(sys, uuid_actors.at(i.value().uuid()));

                n["children"] = R"([])"_json;

                n["children"].push_back(
                    createEntry(R"({"type": "Media List", "actor_owner": null})"_json));
                n["children"][0]["actor_owner"] = n["actor"];

                n["children"].push_back(createEntry(
                    R"({"type": "PlayheadSelection", "name": null, "actor_owner": null, "actor_uuid": null, "actor": null, "group_actor": null})"_json));
                n["children"][1]["actor_owner"] = n["actor"];

                n["children"].push_back(
                    createEntry(R"({"type": "Container List", "actor_owner": null})"_json));
                n["children"][2]["actor_owner"] = n["actor"];

                result["children"].emplace_back(n);
            } else {
                spdlog::warn("{} invalid type {}", __PRETTY_FUNCTION__, i.value().type());
            }
        }

        // spdlog::warn("sessionTreeToJson {}",result.dump(2));

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

    if (detail.type_ == "MediaStream") {
        result.erase("children");
    }

    if (detail.type_ == "Media" or detail.type_ == "MediaSource") {
        result["audio_actor_uuid"] = nullptr;
        result["image_actor_uuid"] = nullptr;
        result["media_status"]     = nullptr;
        if (detail.type_ == "Media") {
            result["flag"] = nullptr;
        } else if (detail.type_ == "MediaSource") {
            result["thumbnail_url"] = nullptr;
            result["rate"]          = nullptr;
            result["path"]          = nullptr;
            result["resolution"]    = nullptr;
            result["pixel_aspect"]  = nullptr;
            result["bit_depth"]     = nullptr;
            result["format"]        = nullptr;
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


void SessionModel::moveSelectionByIndex(const QModelIndex &index, const int offset) {
    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);

            if (j.at("type") == "PlayheadSelection") {
                auto actor = actorFromString(system(), j.at("actor"));
                if (actor) {
                    anon_send(actor, playhead::select_next_media_atom_v, offset);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::updateSelection(const QModelIndex &index, const QModelIndexList &selection) {
    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            // spdlog::warn("{}", j.dump(2));

            if (j.at("type") == "PlayheadSelection") {
                auto actor = actorFromString(system(), j.at("actor"));
                if (actor) {
                    UuidList uv;
                    for (const auto &i : selection)
                        uv.emplace_back(UuidFromQUuid(i.data(actorUuidRole).toUuid()));

                    anon_send(actor, playlist::select_media_atom_v, uv);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            spdlog::warn("{}", j.dump(2));
        }
    }
}
