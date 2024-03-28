// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/timeline/timeline.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/clip_actor.hpp"
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

void SessionModel::setTimelineFocus(
    const QModelIndex &timeline, const QModelIndexList &indexes) const {
    try {
        UuidVector uuids;

        if (timeline.isValid()) {
            auto actor = actorFromQString(system(), timeline.data(actorRole).toString());

            for (auto &i : indexes) {
                uuids.emplace_back(UuidFromQUuid(i.data(idRole).toUuid()));
            }

            anon_send(actor, timeline::focus_atom_v, uuids);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


QModelIndex SessionModel::getTimelineIndex(const QModelIndex &index) const {
    try {
        if (index.isValid()) {
            auto type = StdFromQString(index.data(typeRole).toString());
            if (type == "Timeline")
                return index;
            else
                return getTimelineIndex(index.parent());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QModelIndex();
}

bool SessionModel::removeTimelineItems(
    const QModelIndex &track_index, const int frame, const int duration) {
    auto result = false;
    try {
        if (track_index.isValid()) {
            auto type  = StdFromQString(track_index.data(typeRole).toString());
            auto actor = actorFromQString(system(), track_index.data(actorRole).toString());

            if (type == "Audio Track" or type == "Video Track") {
                scoped_actor sys{system()};
                request_receive<JsonStore>(
                    *sys, actor, timeline::erase_item_at_frame_atom_v, frame, duration);
                result = true;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


bool SessionModel::removeTimelineItems(const QModelIndexList &indexes) {
    auto result = false;
    try {

        // ignore indexes that are not timeline items..
        // be careful of invalidation, deletion order matters ?

        // simple operations.. deletion of tracks.
        for (const auto &i : indexes) {
            if (i.isValid()) {
                auto type         = StdFromQString(i.data(typeRole).toString());
                auto actor        = actorFromQString(system(), i.data(actorRole).toString());
                auto parent_index = i.parent();

                if (parent_index.isValid()) {

                    caf::scoped_actor sys(system());
                    auto pactor =
                        actorFromQString(system(), parent_index.data(actorRole).toString());
                    auto row = i.row();

                    if (type == "Clip") {
                        // replace with gap
                        // get parent, and index.
                        // find parent timeline.
                        auto range = request_receive<utility::FrameRange>(
                            *sys, actor, timeline::trimmed_range_atom_v);

                        if (pactor) {
                            auto uuid = Uuid::generate();
                            auto gap  = self()->spawn<timeline::GapActor>(
                                "Gap", range.frame_duration(), uuid);
                            request_receive<JsonStore>(
                                *sys,
                                pactor,
                                timeline::insert_item_atom_v,
                                row,
                                UuidActorVector({UuidActor(uuid, gap)}));
                            request_receive<JsonStore>(
                                *sys, pactor, timeline::erase_item_atom_v, row + 1);
                        }
                    } else {
                        if (pactor) {
                            request_receive<JsonStore>(
                                *sys, pactor, timeline::erase_item_atom_v, row);
                        }
                    }
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QFuture<bool> SessionModel::undoFuture(const QModelIndex &index) {
    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                auto actor        = actorFromString(system(), j.at("actor"));
                auto type         = j.at("type").get<std::string>();
                if (actor and type == "Timeline") {
                    scoped_actor sys{system()};
                    result = request_receive<bool>(
                        *sys,
                        actor,
                        history::undo_atom_v,
                        utility::sys_time_duration(std::chrono::milliseconds(500)));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}

QFuture<bool> SessionModel::redoFuture(const QModelIndex &index) {
    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                auto actor        = actorFromString(system(), j.at("actor"));
                auto type         = j.at("type").get<std::string>();
                if (actor and type == "Timeline") {
                    scoped_actor sys{system()};
                    result = request_receive<bool>(
                        *sys,
                        actor,
                        history::redo_atom_v,
                        utility::sys_time_duration(std::chrono::milliseconds(500)));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}

// trigger actor creation
void SessionModel::item_event_callback(const utility::JsonStore &event, timeline::Item &item) {
    try {
        auto index = search_recursive(
            QVariant::fromValue(QUuidFromUuid(event.at("uuid").get<Uuid>())),
            idRole,
            QModelIndex(),
            0,
            -1);

        switch (static_cast<timeline::ItemAction>(event.at("action"))) {
        case timeline::IT_INSERT:

            // check for place holder entry..
            // spdlog::warn("timeline::IT_INSERT {}", event.dump(2));
            if (index.isValid()) {
                auto tree = indexToTree(index);
                if (tree) {
                    auto new_item = timeline::Item(event.at("item"), &system());
                    auto new_node = timelineItemToJson(new_item, system(), true);

                    auto replaced = false;
                    // check children..
                    for (auto &i : *tree) {
                        auto place_row = 0;
                        auto data      = i.data();
                        if (data.count("placeholder") and data.at("id") == new_node.at("id")) {
                            i.data() = new_node;
                            replaced = true;
                            emit dataChanged(
                                SessionModel::index(place_row, 0, index),
                                SessionModel::index(place_row, 0, index),
                                QVector<int>({}));
                            break;
                        }
                        place_row++;
                    }

                    if (not replaced) {
                        auto new_tree = json_to_tree(new_node, children_);
                        auto row      = event.at("index").get<int>();
                        beginInsertRows(index, row, row);
                        tree->insert(tree->child(row), new_tree);
                        endInsertRows();
                    }
                }

                if (index.data(typeRole).toString() == QString("Stack")) {
                    // refresh teack indexes
                    emit dataChanged(
                        SessionModel::index(0, 0, index),
                        SessionModel::index(rowCount(index) - 1, 0, index),
                        QVector<int>({trackIndexRole}));
                }
            }
            break;

        case timeline::IT_REMOVE:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_REMOVE {}", event.dump(2));
                JSONTreeModel::removeRows(event.at("index").get<int>(), 1, index);
                if (index.data(typeRole).toString() == QString("Stack")) {
                    // refresh teack indexes
                    emit dataChanged(
                        SessionModel::index(0, 0, index),
                        SessionModel::index(rowCount(index) - 1, 0, index),
                        QVector<int>({trackIndexRole}));
                }
            }
            break;

        case timeline::IT_ENABLE:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_ENABLE {}", event.dump(2));
                if (indexToData(index).at("enabled").is_null() or
                    indexToData(index).at("enabled") != event.value("value", true)) {
                    indexToData(index)["enabled"] = event.value("value", true);
                    emit dataChanged(index, index, QVector<int>({enabledRole}));
                }
            }
            break;

        case timeline::IT_NAME:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_NAME {}", event.dump(2));
                if (indexToData(index).at("name").is_null() or
                    indexToData(index).at("name") != event.value("value", "")) {
                    indexToData(index)["name"] = event.value("value", "");
                    emit dataChanged(index, index, QVector<int>({nameRole}));
                }
            }
            break;

        case timeline::IT_FLAG:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_NAME {}", event.dump(2));
                if (indexToData(index).at("flag").is_null() or
                    indexToData(index).at("flag") != event.value("value", "")) {
                    indexToData(index)["flag"] = event.value("value", "");
                    emit dataChanged(index, index, QVector<int>({flagColourRole}));
                }
            }
            break;

        case timeline::IT_PROP:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_NAME {}", event.dump(2));
                if (indexToData(index).at("prop").is_null() or
                    indexToData(index).at("prop") != event.value("value", "")) {
                    indexToData(index)["prop"] = event.value("value", "");
                    emit dataChanged(index, index, QVector<int>({clipMediaUuidRole}));
                }
            }
            break;

        case timeline::IT_ACTIVE:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_ACTIVE {}", event.dump(2));

                if (event.at("value2") == true) {
                    if (indexToData(index).at("active_range").is_null() or
                        indexToData(index).at("active_range") != event.at("value")) {
                        indexToData(index)["active_range"] = event.at("value");
                        emit dataChanged(
                            index,
                            index,
                            QVector<int>(
                                {trimmedDurationRole,
                                 rateFPSRole,
                                 activeDurationRole,
                                 trimmedStartRole,
                                 activeStartRole}));
                    }
                } else {
                    if (not indexToData(index).at("active_range").is_null()) {
                        indexToData(index)["active_range"] = nullptr;
                        emit dataChanged(
                            index,
                            index,
                            QVector<int>(
                                {trimmedDurationRole,
                                 rateFPSRole,
                                 activeDurationRole,
                                 trimmedStartRole,
                                 activeStartRole}));
                    }
                }
            }
            break;

        case timeline::IT_AVAIL:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_AVAIL {}", event.dump(2));

                if (event.at("value2") == true) {
                    if (indexToData(index).at("available_range").is_null() or
                        indexToData(index).at("available_range") != event.at("value")) {
                        indexToData(index)["available_range"] = event.at("value");
                        emit dataChanged(
                            index,
                            index,
                            QVector<int>(
                                {trimmedDurationRole,
                                 rateFPSRole,
                                 availableDurationRole,
                                 trimmedStartRole,
                                 availableStartRole}));
                    }
                } else {
                    if (not indexToData(index).at("available_range").is_null()) {
                        indexToData(index)["available_range"] = nullptr;
                        emit dataChanged(
                            index,
                            index,
                            QVector<int>(
                                {trimmedDurationRole,
                                 rateFPSRole,
                                 availableDurationRole,
                                 trimmedStartRole,
                                 availableStartRole}));
                    }
                }
            }
            break;

        case timeline::IT_SPLICE:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_SPLICE {}", event.dump(2));

                auto frst  = event.at("first").get<int>();
                auto count = event.at("count").get<int>();
                auto dst   = event.at("dst").get<int>();

                // massage values if they'll not work with qt..
                if (dst >= frst and dst <= frst + count - 1) {
                    dst = frst + count;
                    // spdlog::warn("FAIL ?");
                }

                JSONTreeModel::moveRows(index, frst, count, index, dst);

                if (index.data(typeRole).toString() == QString("Stack")) {
                    // refresh teack indexes
                    emit dataChanged(
                        SessionModel::index(0, 0, index),
                        SessionModel::index(rowCount(index) - 1, 0, index),
                        QVector<int>({trackIndexRole}));
                }
            }
            break;

        case timeline::IT_ADDR:
            if (index.isValid()) {
                // spdlog::warn("timeline::IT_ADDR {}", event.dump(2));
                // is the string actor valid here ?
                if (event.at("value").is_null() and
                    not indexToData(index).at("actor").is_null()) {
                    indexToData(index)["actor"] = nullptr;
                    emit dataChanged(index, index, QVector<int>({actorRole}));
                } else if (
                    event.at("value").is_string() and
                    (not indexToData(index).at("actor").is_string() or
                     event.at("value") != indexToData(index).at("actor"))) {
                    indexToData(index)["actor"] = event.at("value");
                    emit dataChanged(index, index, QVector<int>({actorRole}));
                }
            }
            break;

        case timeline::IA_NONE:
        default:
            break;
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

QModelIndex SessionModel::insertTimelineGap(
    const int row,
    const QModelIndex &parent,
    const int frames,
    const double rate,
    const QString &qname) {
    auto result = QModelIndex();

    try {
        if (parent.isValid()) {
            const auto name = StdFromQString(qname);
            scoped_actor sys{system()};
            nlohmann::json &j = indexToData(parent);

            auto parent_actor = j.count("actor") and not j.at("actor").is_null()
                                    ? actorFromString(system(), j.at("actor"))
                                    : caf::actor();
            if (parent_actor) {
                auto insertion_json = R"({
                    "actor": null,
                    "enabled": true,
                    "id": null,
                    "name": null,
                    "placeholder": true,
                    "active_range": null,
                    "available_range": null,
                    "type": "Gap"
                })"_json;


                auto new_uuid = utility::Uuid::generate();
                auto duration = utility::FrameRateDuration(frames, FrameRate(1.0 / rate));
                auto new_item = self()->spawn<timeline::GapActor>(name, duration, new_uuid);

                insertion_json["actor"]           = actorToString(system(), new_item);
                insertion_json["id"]              = new_uuid;
                insertion_json["name"]            = name;
                insertion_json["available_range"] = utility::FrameRange(duration);

                JSONTreeModel::insertRows(row, 1, parent, insertion_json);

                // {
                //   "active_range": {
                //     "duration": 8085000000,
                //     "rate": 29400000,
                //     "start": 0
                //   },
                //   "actor":
                //   "00000000000001F9010000256B6541E50248C6C675AF42C5E8F50EA28AC388D2D0",
                //   "available_range": {
                //     "duration": 0,
                //     "rate": 0,
                //     "start": 0
                //   },
                //   "children": [],
                //   "enabled": true,
                //   "flag": "",
                //   "id": "b5b2bc54-d8e3-49c1-b1ef-94f2b3dae89f",
                //   "name": "HELLO GAP",
                //   "prop": null,
                //   "transparent": true,
                //   "type": "Gap"
                // },


                // hopefully add to parent..
                try {
                    request_receive<JsonStore>(
                        *sys,
                        parent_actor,
                        timeline::insert_item_atom_v,
                        row,
                        UuidActorVector({UuidActor(new_uuid, new_item)}));

                    result = index(row, 0, parent);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    // failed to insert, kill it..
                    self()->send_exit(new_item, caf::exit_reason::user_shutdown);
                }
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QModelIndex SessionModel::insertTimelineClip(
    const int row,
    const QModelIndex &parent,
    const QModelIndex &mediaIndex,
    const QString &qname) {
    auto result = QModelIndex();

    try {
        if (parent.isValid()) {
            const auto name = StdFromQString(qname);
            scoped_actor sys{system()};
            nlohmann::json &j = indexToData(parent);

            auto parent_actor = j.count("actor") and not j.at("actor").is_null()
                                    ? actorFromString(system(), j.at("actor"))
                                    : caf::actor();
            if (parent_actor) {
                auto insertion_json =
                    R"({"type": "Clip", "id": null,  "placeholder": true, "actor": null})"_json;

                JSONTreeModel::insertRows(row, 1, parent, insertion_json);

                auto new_uuid = utility::Uuid::generate();
                // get media ..
                auto media_uuid = UuidFromQUuid(mediaIndex.data(actorUuidRole).toUuid());
                auto media_actor =
                    actorFromQString(system(), mediaIndex.data(actorRole).toString());

                auto new_item = self()->spawn<timeline::ClipActor>(
                    UuidActor(media_uuid, media_actor), name, new_uuid);

                // hopefully add to parent..
                try {
                    request_receive<JsonStore>(
                        *sys,
                        parent_actor,
                        timeline::insert_item_atom_v,
                        row,
                        UuidActorVector({UuidActor(new_uuid, new_item)}));
                    setData(index(row, 0, parent), QUuidFromUuid(new_uuid), idRole);

                    setData(
                        index(row, 0, parent), actorToQString(system(), new_item), actorRole);

                    result = index(row, 0, parent);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    // failed to insert, kill it..
                    self()->send_exit(new_item, caf::exit_reason::user_shutdown);
                }
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QModelIndex SessionModel::splitTimelineClip(const int frame, const QModelIndex &index) {
    auto result = QModelIndex();

    // only makes sense in Clip, Gap / Track ?

    try {
        auto parent_index = index.parent();

        if (index.isValid() and parent_index.isValid()) {
            nlohmann::json &pj = indexToData(parent_index);

            auto parent_actor = pj.count("actor") and not pj.at("actor").is_null()
                                    ? actorFromString(system(), pj.at("actor"))
                                    : caf::actor();

            scoped_actor sys{system()};
            request_receive<JsonStore>(
                *sys, parent_actor, timeline::split_item_atom_v, index.row(), frame);
            result = SessionModel::index(index.row() + 1, 0, parent_index);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool SessionModel::moveTimelineItem(const QModelIndex &index, const int distance) {
    auto result        = false;
    auto real_distance = (distance == 1 ? 2 : distance);

    // stop mixing of audio and video tracks.
    // as this upsets the DelegateModel

    try {
        auto parent_index = index.parent();
        if (index.isValid() and parent_index.isValid() and (index.row() + distance >= 0) and
            (index.row() + distance < rowCount(parent_index))) {
            auto block_move = false;
            auto type       = StdFromQString(index.data(typeRole).toString());
            if (distance > 0 and type == "Video Track") {
                // check next entry..
                auto ntype =
                    StdFromQString(SessionModel::index(index.row() + 1, 0, index.parent())
                                       .data(typeRole)
                                       .toString());
                if (ntype != "Video Track")
                    block_move = true;
            } else if (distance < 0 and type == "Audio Track") {
                auto ntype =
                    StdFromQString(SessionModel::index(index.row() - 1, 0, index.parent())
                                       .data(typeRole)
                                       .toString());
                if (ntype != "Audio Track")
                    block_move = true;
            }

            if (not block_move) {
                nlohmann::json &pj = indexToData(parent_index);

                auto parent_actor = pj.count("actor") and not pj.at("actor").is_null()
                                        ? actorFromString(system(), pj.at("actor"))
                                        : caf::actor();

                scoped_actor sys{system()};
                request_receive<JsonStore>(
                    *sys,
                    parent_actor,
                    timeline::move_item_atom_v,
                    index.row(),
                    1,
                    index.row() + real_distance);

                result = true;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool SessionModel::moveRangeTimelineItems(
    const QModelIndex &track_index,
    const int frame,
    const int duration,
    const int dest,
    const bool insert) {
    auto result = false;

    try {
        if (track_index.isValid()) {
            nlohmann::json &pj = indexToData(track_index);

            auto track_actor = pj.count("actor") and not pj.at("actor").is_null()
                                   ? actorFromString(system(), pj.at("actor"))
                                   : caf::actor();

            scoped_actor sys{system()};
            request_receive<JsonStore>(
                *sys,
                track_actor,
                timeline::move_item_at_frame_atom_v,
                frame,
                duration,
                dest,
                insert);

            result = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool SessionModel::alignTimelineItems(const QModelIndexList &indexes, const bool align_right) {
    auto result = false;

    if (indexes.size() > 1) {
        // index 0 is item to align to with respect to the track.
        int align_to = indexes[0].data(parentStartRole).toInt();
        if (align_right)
            align_to += indexes[0].data(trimmedDurationRole).toInt();

        for (auto i = 1; i < indexes.size(); i++) {
            auto frame = indexes[i].data(parentStartRole).toInt();
            if (align_right)
                frame += indexes[i].data(trimmedDurationRole).toInt();

            if (align_to != frame) {
                auto duration = indexes[i].data(trimmedDurationRole).toInt();

                if (align_right) {
                    setData(indexes[i], duration + (align_to - frame), activeDurationRole);
                } else {
                    // can't align to start
                    if (indexes[i].row()) {
                        auto start      = indexes[i].data(trimmedStartRole).toInt();
                        auto prev_index = index(indexes[i].row() - 1, 0, indexes[i].parent());
                        auto prev_duration = prev_index.data(trimmedDurationRole).toInt();

                        setData(
                            prev_index, prev_duration + (align_to - frame), activeDurationRole);
                        setData(indexes[i], start + (align_to - frame), activeStartRole);
                        setData(indexes[i], duration - (align_to - frame), activeDurationRole);
                    }
                }
            }
        }
    }

    return result;
}

QFuture<QList<QUuid>> SessionModel::handleTimelineIdDropFuture(
    const int proposedAction_, const utility::JsonStore &jdrop, const QModelIndex &index) {

    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;
        auto proposedAction = proposedAction_;

        auto dropIndex = index;

        // UuidActorVector new_media;

        try {
            // spdlog::warn(
            //     "handleTimelineIdDropFuture {} {} {}",
            //     proposedAction,
            //     jdrop.dump(2),
            //     index.isValid());
            auto valid_index = index.isValid();

            // build list of media actor uuids

            using item_tuple =
                std::tuple<QModelIndex, Uuid, caf::actor, caf::actor, std::string>;

            std::vector<item_tuple> items;

            for (const auto &i : jdrop.at("xstudio/timeline-ids")) {
                // find media index
                auto mind = search_recursive(QUuid::fromString(QStringFromStd(i)), idRole);

                if (mind.isValid()) {
                    auto item_uuid         = UuidFromQUuid(mind.data(idRole).toUuid());
                    auto item_actor        = actorFromIndex(mind);
                    auto item_parent_actor = actorFromIndex(mind.parent());

                    auto item_type = StdFromQString(mind.data(typeRole).toString());

                    items.push_back(std::make_tuple(
                        mind, item_uuid, item_actor, item_parent_actor, item_type));
                }
            }

            std::sort(items.begin(), items.end(), [&](item_tuple a, item_tuple b) {
                return std::get<0>(a).row() < std::get<0>(b).row();
            });

            // valid desination ?
            if (valid_index) {
                auto before_type         = StdFromQString(index.data(typeRole).toString());
                auto before_uuid         = UuidFromQUuid(index.data(idRole).toUuid());
                auto before_parent       = index.parent();
                auto before_parent_actor = actorFromIndex(index.parent());
                auto before_actor        = actorFromIndex(index);

                // spdlog::warn(
                //     "BEFORE {} {} {} {}",
                //     before_type,
                //     to_string(before_uuid),
                //     to_string(before_actor),
                //     to_string(before_parent_actor));

                // this can get tricky...
                // as index rows will change underneath us..

                // check before type is timeline..
                if (timeline::TIMELINE_TYPES.count(before_type)) {
                    auto timeline_index = getTimelineIndex(index);

                    // spdlog::warn("{}",before_type);

                    for (const auto &i : items) {
                        const auto item_index = std::get<0>(i);
                        const auto item_type  = std::get<4>(i);

                        // spdlog::warn("->{}",item_type);


                        auto item_timeline_index = getTimelineIndex(item_index);

                        if (timeline_index == item_timeline_index) {
                            auto item_uuid = std::get<1>(i);

                            // move inside container
                            if (before_parent == item_index.parent()) {
                                // get actor..
                                request_receive<JsonStore>(
                                    *sys,
                                    actorFromIndex(item_index.parent()),
                                    timeline::move_item_atom_v,
                                    item_uuid,
                                    1,
                                    before_uuid);
                            } else if (index == item_index.parent()) {
                                // get actor..
                                request_receive<JsonStore>(
                                    *sys,
                                    actorFromIndex(index),
                                    timeline::move_item_atom_v,
                                    item_uuid,
                                    1,
                                    Uuid());
                            } else {
                                // this needs to be an atomic operation, or we end up with two
                                // items with the same id. this happens when the operation is
                                // reversed by redo.

                                auto new_item = request_receive<UuidActor>(
                                    *sys, std::get<2>(i), duplicate_atom_v);

                                request_receive<JsonStore>(
                                    *sys,
                                    std::get<3>(i),
                                    timeline::erase_item_atom_v,
                                    std::get<1>(i));

                                // we should be able to insert this..
                                // make sure before is a container...

                                if (before_type == "Clip" or before_type == "Gap") {
                                    request_receive<JsonStore>(
                                        *sys,
                                        before_parent_actor,
                                        timeline::insert_item_atom_v,
                                        before_uuid,
                                        UuidActorVector({new_item}));
                                } else {
                                    request_receive<JsonStore>(
                                        *sys,
                                        before_actor,
                                        timeline::insert_item_atom_v,
                                        Uuid(),
                                        UuidActorVector({new_item}));
                                }
                            }
                        } else {
                            spdlog::warn("timelines don't match");
                        }
                    }
                } else {
                    // target isn't a timeline
                }
            } else {
                // target is undefined
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}
