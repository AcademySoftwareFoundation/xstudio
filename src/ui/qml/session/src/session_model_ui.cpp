// SPDX-License-Identifier: Apache-2.0

#include "xstudio/media/media.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/tag/tag.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/ui/qml/caf_response_ui.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"

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

void SessionModel::add_id_uuid_lookup(const utility::Uuid &uuid, const QModelIndex &index) {
    if (not uuid.is_null()) {
        if (not id_uuid_lookup_.count(uuid))
            id_uuid_lookup_[uuid] = std::set<QPersistentModelIndex>();
        id_uuid_lookup_[uuid].insert(index);
    }
}

void SessionModel::add_uuid_lookup(const utility::Uuid &uuid, const QModelIndex &index) {
    if (not uuid.is_null()) {
        if (not uuid_lookup_.count(uuid))
            uuid_lookup_[uuid] = std::set<QPersistentModelIndex>();
        uuid_lookup_[uuid].insert(index);
    }
}

void SessionModel::add_string_lookup(const std::string &str, const QModelIndex &index) {
    if (not str.empty()) {
        if (not string_lookup_.count(str))
            string_lookup_[str] = std::set<QPersistentModelIndex>();
        string_lookup_[str].insert(index);
    }
}

void SessionModel::add_lookup(const utility::JsonTree &tree, const QModelIndex &index) {

    // add actorUuidLookup
    if (tree.data().count("id") and not tree.data().at("id").is_null())
        add_id_uuid_lookup(tree.data().at("id").get<Uuid>(), index);
    if (tree.data().count("actor_uuid") and not tree.data().at("actor_uuid").is_null())
        add_uuid_lookup(tree.data().at("actor_uuid").get<Uuid>(), index);
    if (tree.data().count("container_uuid") and not tree.data().at("container_uuid").is_null())
        add_uuid_lookup(tree.data().at("container_uuid").get<Uuid>(), index);
    if (tree.data().count("actor") and not tree.data().at("actor").is_null())
        add_string_lookup(tree.data().at("actor").get<std::string>(), index);

    auto ind = 0;
    for (const auto &i : tree) {
        add_lookup(i, SessionModel::index(ind, 0, index));
        ind++;
    }
}


caf::actor SessionModel::actorFromIndex(const QModelIndex &index, const bool try_parent) const {
    auto result = caf::actor();

    try {
        if (index.isValid()) {
            const nlohmann::json &j = indexToData(index);
            result                  = j.count("actor") and not j.at("actor").is_null()
                                          ? actorFromString(system(), j.at("actor"))
                                          : caf::actor();

            if (not result and try_parent) {
                QModelIndex pindex = index.parent();
                if (pindex.isValid()) {
                    const nlohmann::json &pj = indexToData(pindex);
                    result = pj.count("actor") and not pj.at("actor").is_null()
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
    const utility::JsonTree &tree, const QPersistentModelIndex &search_hint) {
    auto tjson = tree.data();

    if (tjson.count("group_actor") and not tjson.at("group_actor").is_null()) {
        auto grp = actorFromString(system(), tjson.at("group_actor").get<std::string>());
        if (grp)
            anon_send(grp, broadcast::join_broadcast_atom_v, as_actor());

    } else if (
        tjson.count("actor") and not tjson.at("actor").is_null() and
        tjson.at("group_actor").is_null()) {
        // spdlog::info("request group {}", i);
        requestData(
            QVariant::fromValue(QUuidFromUuid(tjson.at("id"))),
            Roles::idRole,
            search_hint,
            tjson,
            Roles::groupActorRole);
    }

    const auto type = tjson.count("type") ? tjson.at("type").get<std::string>() : std::string();

    // spdlog::warn("{} {}", type, item.dump(2));
    // if subset or playlist, trigger auto population of children
    if (type == "Playlist" or type == "Subset" or type == "Timeline") {
        try {
            requestData(
                QVariant::fromValue(QUuidFromUuid(tree.child(0)->data().at("id"))),
                Roles::idRole,
                search_hint,
                tree.child(0)->data(),
                Roles::childrenRole);
            requestData(
                QVariant::fromValue(QUuidFromUuid(tree.child(1)->data().at("id"))),
                Roles::idRole,
                search_hint,
                tree.child(1)->data(),
                Roles::childrenRole);

            if (type == "Playlist")
                requestData(
                    QVariant::fromValue(QUuidFromUuid(tree.child(2)->data().at("id"))),
                    Roles::idRole,
                    search_hint,
                    tree.child(2)->data(),
                    Roles::childrenRole);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
}

utility::Uuid SessionModel::refreshId(nlohmann::json &ij) {
    utility::Uuid result;

    if (ij.count("id")) {
        result.generate_in_place();
        ij["id"] = result;
    }

    return result;
}

void SessionModel::processChildren(const nlohmann::json &rj, const QModelIndex &parent_index) {
    QVector<int> roles({Roles::childrenRole});
    auto changed = false;
    START_SLOW_WATCHER()

    auto ptree = indexToTree(parent_index);

    const auto type = ptree->data().count("type") ? ptree->data().at("type").get<std::string>()
                                                  : std::string();

    // spdlog::warn("processChildren {} {} {}", type, ptree->data().dump(2), rj.dump(2));
    // spdlog::warn("processChildren {}", tree_to_json( *ptree,"children").dump(2));
    // spdlog::warn("processChildren {}", rj.dump(2));
    if (type == "MediaSource" and rj.at(0).at("children").empty() and
        rj.at(1).at("children").empty()) {
        // spdlog::warn("RETRY {}", rj.dump(2));
        // force retry
        emit dataChanged(parent_index, parent_index, roles);
        return;
    }


    try {
        if (type == "Session" or type == "Container List" or type == "Media" or
            type == "Clip" or type == "Media List" or type == "PlayheadSelection" or
            type == "MediaSource") {

            // point to result  children..
            auto rjc = nlohmann::json();

            if (rj.is_array())
                rjc = rj;
            else
                rjc = rj.at("children");

            if (rjc.is_null())
                rjc = R"([])"_json;

            // flag we now have valid children..
            if (ptree->data().at("children").is_null())
                ptree->data()["children"] = R"([])"_json;

            // set up comparison key
            auto compare_key = "";
            if (type == "Session" or type == "Container List") {
                emit playlistsChanged();
                compare_key = "container_uuid";
            } else if (type == "Media List" or type == "Media" or type == "Clip")
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
            for (auto &i : rjc) {
                if (i.count(compare_key))
                    rju.insert(i.at(compare_key).get<Uuid>());
            }

            bool removal_done = false;
            while (not removal_done) {
                removal_done = true;

                size_t start_index = 0;
                size_t count       = 0;

                for (size_t i = 0; i < ptree->size(); i++) {
                    auto cjson = ptree->child(i)->data();

                    if (cjson.count(compare_key) and not cjson.count("placeholder") and
                        not rju.count(cjson.at(compare_key))) {

                        if (start_index + count == i) {
                            count++;
                        } else if (count) {
                            // non sequential, remove previous.
                            JSONTreeModel::removeRows(start_index, count, parent_index);
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
                    JSONTreeModel::removeRows(start_index, count, parent_index);
                    removal_done = false;
                    changed      = true;
                }
            }

            // build uuid/index map for old.
            std::map<Uuid, size_t> iju;
            for (size_t i = 0; i < ptree->size(); i++) {
                auto cjson = ptree->child(i)->data();
                if (cjson.count(compare_key) and not cjson.count("placeholder"))
                    iju[cjson.at(compare_key).get<Uuid>()] = i;
            }


            // whenever we insert new items, refresh the id field
            // as we maybe inserting the same data in multiple places
            // and the ID needs to be unique..
            // this currently only applies to Media List/ Media / Media Source.

            // process new / changed.
            for (size_t i = 0; i < rjc.size(); i++) {

                // APPEND NEW ENTRIES
                if (i >= ptree->size()) {
                    // if appending we can process them all in one hit..
                    auto start = ptree->size();

                    beginInsertRows(parent_index, i, rjc.size() - 1);
                    for (; i < rjc.size(); i++) {

                        auto new_child =
                            ptree->insert(ptree->end(), json_to_tree(rjc.at(i), "children"));
                        refreshId(new_child->data());

                        add_lookup(*new_child, index(i, 0, parent_index));
                    }
                    endInsertRows();

                    try {
                        emit dataChanged(
                            SessionModel::index(start, 0, parent_index),
                            SessionModel::index(ptree->size() - 1, 0, parent_index),
                            QVector<int>());
                    } catch (const std::exception &err) {
                        spdlog::warn("{} reorder {}", __PRETTY_FUNCTION__, err.what());
                    }

                    // FIX ME ***************************************************
                    for (auto ii = start; ii < ptree->size(); ++ii)
                        forcePopulate(*(ptree->child(ii)), parent_index);

                    changed = true;
                    break;
                } else {
                    auto cjson = ptree->child(i)->data();
                    if (cjson.count("placeholder") or not cjson.count(compare_key)) {
                        // OVERWRITE PLACE HOLDER

                        // we can't erase..
                        // or we'd invalidate the index..
                        // auto oldpos = ptree->erase(ptree->child(i));
                        auto childit    = ptree->child(i);
                        auto new_item   = json_to_tree(rjc.at(i), "children");
                        childit->data() = new_item.data();
                        childit->clear();
                        childit->splice(childit->end(), new_item.base());
                        refreshId(childit->data());

                        add_lookup(*childit, index(i, 0, parent_index));

                        // FIX ME ***************************************************
                        forcePopulate(*childit, parent_index);

                        try {
                            auto changed_index = SessionModel::index(i, 0, parent_index);
                            emit dataChanged(changed_index, changed_index, QVector<int>());
                        } catch (const std::exception &err) {
                            spdlog::warn("{} reorder {}", __PRETTY_FUNCTION__, err.what());
                        }

                        changed = true;
                    } else if (
                        // skip..
                        cjson.count(compare_key) and rjc.at(i).count(compare_key) and
                        cjson.at(compare_key) == rjc.at(i).at(compare_key)) {

                    } else if (not iju.count(rjc.at(i).at(compare_key))) {
                        // insert
                        beginInsertRows(parent_index, i, i);
                        auto new_child =
                            ptree->insert(ptree->child(i), json_to_tree(rjc.at(i), "children"));
                        refreshId(new_child->data());

                        add_lookup(*new_child, index(i, 0, parent_index));

                        endInsertRows();

                        try {
                            emit dataChanged(
                                SessionModel::index(i, 0, parent_index),
                                SessionModel::index(i, 0, parent_index),
                                QVector<int>());
                        } catch (const std::exception &err) {
                            spdlog::warn("{} reorder {}", __PRETTY_FUNCTION__, err.what());
                        }

                        // FIX ME ***************************************************
                        forcePopulate(*new_child, parent_index);

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

                        auto cjson = ptree->child(i)->data();

                        if (cjson.count(compare_key) and rjc.at(i).count(compare_key) and
                            cjson.at(compare_key) != rjc.at(i).at(compare_key)) {
                            ordered = false;
                            // find actual index of model row
                            // spdlog::warn("{} -> {}", iju[rjc.at(i).at(compare_key)], i);
                            JSONTreeModel::moveRows(
                                parent_index,
                                iju[rjc.at(i).at(compare_key)],
                                1,
                                parent_index,
                                i);
                            // for update of item, as listview seems to get confused..

                            try {
                                emit dataChanged(
                                    SessionModel::index(i, 0, parent_index),
                                    SessionModel::index(i, 0, parent_index),
                                    QVector<int>());
                            } catch (const std::exception &err) {
                                spdlog::warn("{} reorder {}", __PRETTY_FUNCTION__, err.what());
                            }

                            // rebuild iju
                            iju.clear();
                            for (size_t i = 0; i < ptree->size(); i++) {
                                auto cjson = ptree->child(i)->data();
                                if (cjson.count(compare_key) and not cjson.count("placeholder"))
                                    iju[cjson.at(compare_key).get<Uuid>()] = i;
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
        spdlog::warn("{}", ptree->data().dump(2));
        spdlog::warn("{}", rj.dump(2));
    }


    if (changed) {
        // update totals.
        auto children = ptree->data().at("children");
        if (type == "Media List") {
            if (children.is_array()) {

                setData(
                    parent_index.parent(),
                    QVariant::fromValue(int(children.size())),
                    mediaCountRole);

            } else {
                setData(parent_index.parent(), QVariant::fromValue(0), mediaCountRole);
            }
        }

        emit dataChanged(parent_index, parent_index, roles);
    }

    emit jsonChanged();

    CHECK_SLOW_WATCHER_FAST()
}

void SessionModel::finishedDataSlot(
    const QVariant &search_value, const int search_role, const int role) {

    auto inflight = mapFromValue(search_value).dump() + std::to_string(search_role) + "-" +
                    std::to_string(role);
    if (in_flight_requests_.count(inflight)) {
        in_flight_requests_.erase(inflight);
    }
}

void SessionModel::receivedDataSlot(
    const QVariant &search_value,
    const int search_role,
    const QPersistentModelIndex &search_hint,
    const int role,
    const QString &result) {

    try {
        receivedData(
            mapFromValue(search_value),
            search_role,
            search_hint,
            role,
            json::parse(StdFromQString(result)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::receivedData(
    const nlohmann::json &search_value,
    const int search_role,
    const QPersistentModelIndex &search_hint,
    const int role,
    const nlohmann::json &result) {

    START_SLOW_WATCHER()

    try {
        // because media can be shared, this is inefficient..
        auto hits = (search_role == actorUuidRole or search_role == actorRole) ? -1 : 1;

        QVariant sv;
        if (search_role == Roles::actorRole) {
            sv = QVariant::fromValue(QStringFromStd(search_value.get<std::string>()));
        } else {
            sv = QVariant::fromValue(QUuidFromUuid(search_value.get<Uuid>()));
        }

        auto search_index = QModelIndex();
        auto search_start = 0;

        if (search_hint.isValid()) {
            search_index = search_hint.parent();
            search_start = search_index.row();
        }

        auto indexes = search_recursive_list(sv, search_role, search_index, search_start, hits);

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
            {Roles::flagColourRole, "flag"},
            {Roles::flagTextRole, "flag_text"},
            {Roles::selectionRole, "playhead_selection"},
            {Roles::thumbnailURLRole, "thumbnail_url"},
            {Roles::metadataSet0Role, "metadata_set0"},
            {Roles::metadataSet1Role, "metadata_set1"},
            {Roles::metadataSet2Role, "metadata_set2"},
            {Roles::metadataSet3Role, "metadata_set3"},
            {Roles::metadataSet4Role, "metadata_set4"},
            {Roles::metadataSet5Role, "metadata_set5"},
            {Roles::metadataSet6Role, "metadata_set6"},
            {Roles::metadataSet7Role, "metadata_set7"},
            {Roles::metadataSet8Role, "metadata_set8"},
            {Roles::metadataSet9Role, "metadata_set9"},
            {Roles::metadataSet10Role, "metadata_set10"},
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
                        } else if (not j.count(role_to_key[role])) {
                            j[role_to_key[role]] = result;
                            emit dataChanged(index, index, roles);
                        }
                    }
                    break;

                //  this might be inefficient, we need a new event for media changing underneath
                //  us
                case Roles::mediaStatusRole:
                    if (j.count(role_to_key[role]) and j.at(role_to_key[role]) != result) {
                        j[role_to_key[role]] = result;

                        if (j.count(role_to_key[Roles::thumbnailURLRole])) {
                            roles.push_back(Roles::thumbnailURLRole);
                            j[role_to_key[Roles::thumbnailURLRole]] = nullptr;
                        }
                        emit dataChanged(index, index, roles);

                        // update error counts in parents.
                        updateErroredCount(index);

                    } else {
                        // force update of thumbnail..
                        if (j.count(role_to_key[Roles::thumbnailURLRole])) {
                            roles.clear();
                            roles.push_back(Roles::thumbnailURLRole);
                            j[role_to_key[Roles::thumbnailURLRole]] = nullptr;
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
                            // if type is playlist request children, to make sure we're in sync.
                            if (j.at("type").is_string() and j.at("type") == "Playlist") {
                                auto media_list_index = SessionModel::index(0, 0, index);
                                if (media_list_index.isValid()) {
                                    const nlohmann::json &jj = indexToData(media_list_index);
                                    // spdlog::warn("{} {}",
                                    //     StdFromQString(media_list_index.data(typeRole).toString()),
                                    //     jj.at("id").dump()
                                    // );
                                    requestData(
                                        QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                        idRole,
                                        index,
                                        media_list_index,
                                        childrenRole);
                                }
                            }


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

                case JSONTreeModel::Roles::JSONTextRole:
                    if (j.at("type") == "TimelineItem") {
                        // this is an init setup..
                        auto owner =
                            actorFromString(system(), j.at("actor_owner").get<std::string>());
                        timeline_lookup_.emplace(
                            std::make_pair(owner, timeline::Item(result, &system())));

                        timeline_lookup_[owner].bind_item_event_func(
                            [this](const utility::JsonStore &event, timeline::Item &item) {
                                item_event_callback(event, item);
                            },
                            true);

                        // rebuild json
                        auto jsn =
                            timelineItemToJson(timeline_lookup_.at(owner), system(), true);
                        // spdlog::info("construct timeline object {}", jsn.dump(2));
                        // root is myself
                        auto node     = indexToTree(index);
                        auto new_node = json_to_tree(jsn, children_);

                        node->splice(node->end(), new_node.base());

                        // update root..
                        j[children_] = nlohmann::json::array();

                        // spdlog::error("{} {}", j["id"], jsn["uuid"]);

                        j["id"]              = jsn["id"];
                        j["actor"]           = jsn["actor"];
                        j["enabled"]         = jsn["enabled"];
                        j["transparent"]     = jsn["transparent"];
                        j["active_range"]    = jsn["active_range"];
                        j["available_range"] = jsn["available_range"];
                        emit dataChanged(index, index, QVector<int>());
                    }
                    break;

                case Roles::childrenRole:
                    processChildren(result, index);
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

void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const QPersistentModelIndex &search_hint,
    const QModelIndex &index,
    const int role,
    const std::map<int, std::string> &metadata_paths) const {
    // dispatch call to backend to retrieve missing data.

    // spdlog::warn("{} {}", role, StdFromQString(roleName(role)));
    try {
        requestData(
            search_value, search_role, search_hint, indexToData(index), role, metadata_paths);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void SessionModel::requestData(
    const QVariant &search_value,
    const int search_role,
    const QPersistentModelIndex &search_hint,
    const nlohmann::json &data,
    const int role,
    const std::map<int, std::string> &metadata_paths) const {
    // dispatch call to backend to retrieve missing data.
    auto inflight = mapFromValue(search_value).dump() + std::to_string(search_role) + "-" +
                    std::to_string(role);

    if (not in_flight_requests_.count(inflight)) {
        in_flight_requests_.emplace(inflight);
        auto tmp = new CafResponse(
            search_value,
            search_role,
            search_hint,
            data,
            role,
            StdFromQString(roleName(role)),
            metadata_paths,
            request_handler_);

        connect(tmp, &CafResponse::received, this, &SessionModel::receivedDataSlot);
        connect(tmp, &CafResponse::finished, this, &SessionModel::finishedDataSlot);
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
        "flag_text": null,
        "name": null
    })"_json);

    try {
        result["type"]     = tree.type();
        result["name"]     = tree.name();
        result["children"] = R"([])"_json;

        // playlists and dividers..
        for (const auto &i : tree.children_ref()) {
            const auto type = i.value().type();
            if (type == "ContainerDivider") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "flag": null,
                    "type": null
                })"_json);
                n.erase("children");
                n["type"]           = type;
                n["name"]           = i.value().name();
                n["flag"]           = i.value().flag();
                n["actor_uuid"]     = i.value().uuid();
                n["container_uuid"] = i.uuid();
                result["children"].emplace_back(n);
            } else if (type == "Subset" or type == "Timeline") {
                auto n = createEntry(R"({
                    "name": null,
                    "actor_uuid": null,
                    "container_uuid": null,
                    "group_actor": null,
                    "flag": null,
                    "flag_text": null,
                    "type": null,
                    "actor": null,
                    "busy": false,
                    "media_count": 0,
                    "error_count": 0
                })"_json);

                n["type"]           = type;
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
                    R"({"type": "PlayheadSelection", "playhead_selection": null, "name": null, "actor_owner": null, "actor_uuid": null, "actor": null, "group_actor": null})"_json));
                n["children"][1]["actor_owner"] = n["actor"];

                if (type == "Timeline") {
                    n["children"].push_back(createEntry(
                        R"({"type": "TimelineItem", "name": null, "actor_owner": null})"_json));
                    n["children"][2]["actor_owner"] = n["actor"];
                }

                n["children"].push_back(createEntry(
                    R"({"type": "Playhead", "actor": null, "actor_owner": null, "actor_uuid": null})"_json));
                n["children"].back()["actor_owner"] = n["actor"];

                result["children"].emplace_back(n);
            } else {
                spdlog::warn("{} invalid type {}", __PRETTY_FUNCTION__, type);
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
                    "media_count": 0,
                    "error_count": 0
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
                    R"({"type": "PlayheadSelection", "playhead_selection": null, "name": null, "actor_owner": null, "actor_uuid": null, "actor": null, "group_actor": null})"_json));
                n["children"][1]["actor_owner"] = n["actor"];

                n["children"].push_back(
                    createEntry(R"({"type": "Container List", "actor_owner": null})"_json));
                n["children"][2]["actor_owner"] = n["actor"];

                n["children"].push_back(createEntry(
                    R"({"type": "Playhead", "actor_owner": null, "actor_uuid": null})"_json));
                n["children"].back()["actor_owner"] = n["actor"];

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
        result["metadata_set0"]    = nullptr;
        result["metadata_set1"]    = nullptr;
        result["metadata_set2"]    = nullptr;
        result["metadata_set3"]    = nullptr;
        result["metadata_set4"]    = nullptr;
        result["metadata_set5"]    = nullptr;
        result["metadata_set6"]    = nullptr;
        result["metadata_set7"]    = nullptr;
        result["metadata_set8"]    = nullptr;
        result["metadata_set9"]    = nullptr;
        result["metadata_set10"]   = nullptr;
        result["audio_actor_uuid"] = nullptr;
        result["image_actor_uuid"] = nullptr;
        result["media_status"]     = nullptr;
        if (detail.type_ == "Media") {
            result["flag"]      = nullptr;
            result["flag_text"] = nullptr;
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

            if (j.at("type") == "PlayheadSelection" && j.at("actor").is_string()) {
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


nlohmann::json SessionModel::timelineItemToJson(
    const timeline::Item &item, caf::actor_system &sys, const bool recurse) {
    auto result = R"({})"_json;

    result["id"]    = item.uuid();
    result["actor"] = actorToString(sys, item.actor());
    result["type"]  = to_string(item.item_type());
    result["name"]  = item.name();
    result["flag"]  = item.flag();
    result["prop"]  = item.prop();

    result["active_range"]    = nullptr;
    result["available_range"] = nullptr;

    auto active_range    = item.active_range();
    auto available_range = item.available_range();

    switch (item.item_type()) {
    case timeline::IT_NONE:
        break;

    case timeline::IT_GAP:
    case timeline::IT_AUDIO_TRACK:
    case timeline::IT_VIDEO_TRACK:
    case timeline::IT_STACK:
    case timeline::IT_TIMELINE:
    case timeline::IT_CLIP:
        result["enabled"]     = item.enabled();
        result["transparent"] = item.transparent();
        if (active_range)
            result["active_range"] = *active_range;
        if (available_range)
            result["available_range"] = *available_range;
        break;
    }

    if (recurse)
        switch (item.item_type()) {
        case timeline::IT_NONE:
        case timeline::IT_GAP:
            result["children"] = nlohmann::json::array();
            break;

        case timeline::IT_CLIP:
            result["children"] = nlohmann::json::array();
            break;

        case timeline::IT_AUDIO_TRACK:
        case timeline::IT_VIDEO_TRACK:
        case timeline::IT_STACK:
        case timeline::IT_TIMELINE:
            result["children"] = nlohmann::json::array();
            if (recurse) {
                int index = 0;
                for (const auto &i : item.children()) {
                    result["children"].push_back(timelineItemToJson(i, sys, recurse));
                    index++;
                }
            }
            break;
        }

    return result;
}
