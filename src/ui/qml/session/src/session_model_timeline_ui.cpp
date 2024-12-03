// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/timeline/timeline.hpp"
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

QRect SessionModel::timelineRect(const QModelIndexList &indexes) const {
    auto result     = QRect();
    auto box_inited = false;

    if (not indexes.empty()) {
        // get timeline object.
        auto timelineindex = getTimelineIndex(indexes[0]);
        if (timelineindex.isValid()) {
            // get actor and then item..
            auto tactor = actorFromQString(system(), timelineindex.data(actorRole).toString());
            if (timeline_lookup_.count(tactor)) {
                auto &item = timeline_lookup_.at(tactor);

                for (const auto &i : indexes) {
                    auto box = item.box(UuidFromQUuid(i.data(idRole).toUuid()));
                    if (box) {
                        if (box_inited) {
                            result.setCoords(
                                std::min(result.x(), box->first.first),
                                std::min(result.y(), box->first.second),
                                std::max(result.x() + result.width(), box->second.first),
                                std::max(result.y() + result.height(), box->second.second));
                        } else {
                            result.setCoords(
                                box->first.first,
                                box->first.second,
                                box->second.first,
                                box->second.second);
                            box_inited = true;
                        }
                    }
                }
            }
        }
    }
    return result;
}

QModelIndexList SessionModel::getIndexesByName(
    const QModelIndex &idx, const QString &name, const QString &type) const {
    auto results = QModelIndexList();

    if (idx.isValid()) {
        if ((type.isEmpty() or idx.data(typeRole).toString() == type) and
            idx.data(nameRole).toString().contains(name, Qt::CaseInsensitive)) {
            results.push_back(idx);
        }

        if (idx.model()->hasChildren(idx)) {
            for (auto i = 0; i < idx.model()->rowCount(idx); i++)
                results.append(getIndexesByName(idx.model()->index(i, 0, idx), name, type));
        }
    }

    return results;
}

QModelIndexList SessionModel::getTimelineClipIndexes(
    const QModelIndex &timelineIndex, const QModelIndex &mediaIndex) {
    auto result = QModelIndexList();

    auto media_uuid = QVariant();
    // get media uuid
    if (mediaIndex.isValid() and mediaIndex.data(typeRole).toString() == QString("Media"))
        media_uuid = mediaIndex.data(actorUuidRole);

    if (not media_uuid.isNull())
        result = searchRecursiveList(media_uuid, clipMediaUuidRole, timelineIndex, 0, -1);

    return result;
}

QVariantList SessionModel::mediaFrameToTimelineFrames(
    const QModelIndex &timelineIndex,
    const QModelIndex &mediaIndex,
    const int logicalMediaFrame,
    const bool skipDisabled) {

    // given the index of a media item, and a logical frame into that media,
    // we want to return the corresponding timeline logical frames - i.e. the
    // timeline frames where 'logicalMediaFrame' of the given media will be
    // on screen.

    auto result = QVariantList();

    // get the timeline actor
    auto timeline_actor = actorFromQString(system(), timelineIndex.data(actorRole).toString());

    // get the uuid of the media actor
    utility::Uuid media_uuid;
    if (mediaIndex.isValid() and mediaIndex.data(typeRole).toString() == QString("Media")) {
        media_uuid = UuidFromQUuid(mediaIndex.data(actorUuidRole).toUuid());
    }

    scoped_actor sys{system()};
    try {
        auto ri = request_receive<std::vector<int>>(
            *sys,
            timeline_actor,
            timeline::media_frame_to_timeline_frames_atom_v,
            media_uuid,
            logicalMediaFrame,
            skipDisabled);
        for (const auto &i : ri) {
            result.push_back(i);
        }
    } catch (...) {
    }
    return result;
}

Q_INVOKABLE QVariantList SessionModel::snapTo(
    const QModelIndex &ignore,
    const int cursor,
    const int clipStart,
    const int clipDuration,
    const int currentOffset,
    const int window,
    const QUuid &key) {

    auto result = QVariantList();

    static QVariantList search_these;
    static QUuid search_key;

    if (key.isNull() or key != search_key) {
        search_these = boundaryFramesInTimeline(QModelIndexList({getTimelineIndex(ignore)}));
        search_key   = key;
    }

    auto nearest = std::numeric_limits<int>::max();

    // snap start of clip to cursor
    auto cursor_start = std::abs((clipStart + currentOffset) - cursor);
    if (cursor_start < nearest and cursor_start < window) {
        nearest = cursor_start;
        result  = QVariantList({cursor - clipStart, QModelIndex(), true});
    }

    // snap end of clip to cursor
    auto cursor_end = std::abs((clipStart + clipDuration + currentOffset) - cursor);
    if (cursor_end < nearest and cursor_end < window) {
        nearest = cursor_end;
        result  = QVariantList({cursor - (clipStart + clipDuration), QModelIndex(), false});
    }

    for (const auto &i : search_these) {
        auto qlist = i.toList();
        auto mi    = qlist.at(0).toModelIndex();
        if (mi == ignore)
            continue;

        auto head1 = std::abs(clipStart + currentOffset - qlist.at(1).toInt());
        auto tail1 = std::abs(clipStart + currentOffset - qlist.at(2).toInt());
        auto head2 = std::abs(clipStart + clipDuration + currentOffset - qlist.at(1).toInt());
        auto tail2 = std::abs(clipStart + clipDuration + currentOffset - qlist.at(2).toInt());

        if (head1 < nearest and head1 < window) {
            nearest = head1;
            result  = QVariantList({qlist.at(1).toInt() - clipStart, mi, true});
        }

        if (tail1 < nearest and tail1 < window) {
            nearest = tail1;
            result  = QVariantList({qlist.at(2).toInt() - clipStart, mi, true});
        }

        if (head2 < nearest and head2 < window) {
            nearest = head2;
            result =
                QVariantList({qlist.at(1).toInt() - (clipStart + clipDuration), mi, false});
        }

        if (tail2 < nearest and tail2 < window) {
            nearest = tail2;
            result =
                QVariantList({qlist.at(2).toInt() - (clipStart + clipDuration), mi, false});
        }
    }

    return result;
}

QVariantList SessionModel::boundaryFramesInTimeline(const QModelIndexList &indexes) {
    auto result = QVariantList();

    if (not indexes.isEmpty() and indexes.at(0).isValid()) {
        // should all be from same timeline..
        auto tindex = getTimelineIndex(indexes.at(0));
        if (tindex.isValid()) {
            auto tactor = actorFromIndex(tindex);
            if (timeline_lookup_.count(tactor)) {
                auto titem = timeline_lookup_.at(tactor);

                for (const auto &i : indexes) {
                    if (i.isValid()) {
                        auto type = StdFromQString(i.data(typeRole).toString());

                        if (type == "Clip") {
                            auto puuid = UuidFromQUuid(i.parent().data(idRole).toUuid());
                            auto pitem = timeline::find_item(titem.children(), puuid);
                            if (pitem) {
                                const auto start = (*pitem)->frame_at_index(i.row());
                                result.push_back(QVariantList(
                                    {i, start, start + i.data(trimmedDurationRole).toInt()}));
                            }
                        } else if (
                            type == "Timeline" or type == "TimelineItem" or
                            type == "Audio Track" or type == "Video Track" or type == "Stack") {
                            for (auto row = 0; row < rowCount(i); row++) {
                                result.append(boundaryFramesInTimeline({index(row, 0, i)}));
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}


int SessionModel::startFrameInParent(const QModelIndex &timelineItemIndex) {
    auto result = 0;

    try {
        if (timelineItemIndex.isValid()) {
            const auto &j = indexToData(timelineItemIndex);
            if (j.count("active_range")) {
                auto p = timelineItemIndex.parent();
                auto t = getTimelineIndex(timelineItemIndex);

                if (p.isValid() and t.isValid()) {
                    auto tactor = actorFromIndex(t);
                    auto puuid  = UuidFromQUuid(p.data(idRole).toUuid());

                    if (timeline_lookup_.count(tactor)) {
                        auto pitem =
                            timeline::find_item(timeline_lookup_.at(tactor).children(), puuid);
                        if (pitem)
                            result = (*pitem)->frame_at_index(timelineItemIndex.row());
                    }
                }
            }
        }
    } catch (...) {
    }

    return result;
}


int SessionModel::getTimelineFrameFromClip(
    const QModelIndex &clipIndex, const int logicalMediaFrame) {
    auto result = -1;

    if (clipIndex.isValid()) {
        auto start  = clipIndex.data(trimmedStartRole).toInt();
        auto pstart = startFrameInParent(clipIndex);
        result      = pstart + (logicalMediaFrame - start);
    }

    return result;
}

QModelIndex
SessionModel::getTimelineClipIndex(const QModelIndex &timelineIndex, const int frame) {
    auto result = QModelIndex();
    auto tactor = actorFromQString(system(), timelineIndex.data(actorRole).toString());
    if (timeline_lookup_.count(tactor)) {
        auto &item = timeline_lookup_.at(tactor);

        scoped_actor sys{system()};
        try {
            auto ri = request_receive<timeline::ResolvedItem>(
                *sys,
                tactor,
                timeline::bake_atom_v,
                FrameRate(item.front().rate().to_flicks() * frame));
            auto cuuid = std::get<0>(ri).uuid();
            result     = searchRecursive(
                QVariant::fromValue(QUuidFromUuid(cuuid)), idRole, timelineIndex);
        } catch (...) {
        }
    }
    return result;
}

QModelIndexList SessionModel::getTimelineClipIndexesFromRect(
    const QModelIndex &timelineIndex,
    const int left,
    const int top,
    const int right,
    const int bottom,
    const double frameScale,
    const double trackScale,
    const timeline::ItemType type,
    const bool skipLocked) {

    auto result = QModelIndexList();

    auto tactor = actorFromQString(system(), timelineIndex.data(actorRole).toString());

    if (timeline_lookup_.count(tactor)) {
        const auto dleft   = static_cast<double>(left);
        const auto dtop    = static_cast<double>(top);
        const auto dright  = static_cast<double>(right);
        const auto dbottom = static_cast<double>(bottom);

        auto &item = timeline_lookup_.at(tactor);

        // use item to resolve rectangles for all clips ?
        auto clips = item.find_all_items(timeline::IT_CLIP, type);

        for (const auto &i : clips) {
            const auto box = *(item.box(i.get().uuid()));

            const auto cleft   = static_cast<double>(box.first.first) * frameScale;
            const auto ctop    = static_cast<double>(box.first.second) * trackScale;
            const auto cright  = static_cast<double>(box.second.first) * frameScale;
            const auto cbottom = static_cast<double>(box.second.second) * trackScale;

            if (skipLocked and i.get().locked())
                continue;

            if (cright <= dleft or cbottom <= dtop or cleft >= dright or ctop >= dbottom)
                continue;

            result.push_back(searchRecursive(
                QVariant::fromValue(QUuidFromUuid(i.get().uuid())), idRole, timelineIndex));
        }
    }

    return result;
}

QModelIndexList SessionModel::getTimelineVideoClipIndexesFromRect(
    const QModelIndex &timelineIndex,
    const int left,
    const int top,
    const int right,
    const int bottom,
    const double frameScale,
    const double trackScale,
    const bool skipLocked) {
    return getTimelineClipIndexesFromRect(
        timelineIndex,
        left,
        top,
        right,
        bottom,
        frameScale,
        trackScale,
        timeline::IT_VIDEO_TRACK,
        skipLocked);
}

QModelIndexList SessionModel::getTimelineAudioClipIndexesFromRect(
    const QModelIndex &timelineIndex,
    const int left,
    const int top,
    const int right,
    const int bottom,
    const double frameScale,
    const double trackScale,
    const bool skipLocked) {
    return getTimelineClipIndexesFromRect(
        timelineIndex,
        left,
        top,
        right,
        bottom,
        frameScale,
        trackScale,
        timeline::IT_AUDIO_TRACK,
        skipLocked);
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

QFuture<bool>
SessionModel::exportOTIO(const QModelIndex &timeline, const QUrl &path, const QString &type) {

    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            if (timeline == getTimelineIndex(timeline)) {
                nlohmann::json &j = indexToData(timeline);
                auto actor        = actorFromString(system(), j.at("actor"));
                if (actor) {
                    scoped_actor sys{system()};
                    result = request_receive<bool>(
                        *sys,
                        actor,
                        global_store::save_atom_v,
                        UriFromQUrl(path),
                        StdFromQString(type));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw; // QFuture catches it..
        }

        return result;
    });
}

QModelIndex SessionModel::getTimelineTrackIndex(const QModelIndex &index) const {
    try {
        if (index.isValid()) {
            auto type = StdFromQString(index.data(typeRole).toString());
            if (type == "Audio Track" or type == "Video Track")
                return index;
            else
                return getTimelineTrackIndex(index.parent());
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
                    *sys, actor, timeline::erase_item_at_frame_atom_v, frame, duration, false);
                result = true;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool SessionModel::replaceTimelineTrack(const QModelIndex &src, const QModelIndex &dst) {
    auto result = false;

    if (src.isValid() and dst.isValid() and src != dst and
        not dst.data(SessionModel::Roles::lockedRole).toBool()) {
        auto src_type = StdFromQString(src.data(typeRole).toString());
        auto dst_type = StdFromQString(dst.data(typeRole).toString());
        if ((src_type == "Audio Track" or src_type == "Video Track") and
            (dst_type == "Audio Track" or dst_type == "Video Track")) {
            // args are valid..
            // purge dst content.
            auto dactor = actorFromQString(system(), dst.data(actorRole).toString());
            if (dactor) {
                anon_send(dactor, timeline::erase_item_atom_v, 0, rowCount(dst), false);
                // JSONTreeModel::removeRows(0, rowCount(dst), dst);
                // wait for update ?
                while (rowCount(dst)) {
                    QCoreApplication::processEvents(
                        QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
                }
                // clone src clips into dst.
                auto items = QModelIndexList();
                for (auto i = 0; i < rowCount(src); i++)
                    items.push_back(index(i, 0, src));
                auto new_clips = copyRows(items, 0, dst);
                for (const auto &i : new_clips)
                    setData(i, "", flagColourRole);
                result = true;
            }
        }
    }
    return result;
}

QModelIndexList SessionModel::duplicateTimelineClipsTo(
    const QModelIndexList &indexes, const QModelIndex &trackIndex) {

    auto result = QModelIndexList();

    if (trackIndex.data(typeRole).toString() != QString("Video Track") and
        trackIndex.data(typeRole).toString() != QString("Audio Track"))
        return result;

    auto target_row = rowCount(trackIndex);

    for (const auto &i : indexes) {
        if (not i.isValid())
            continue;

        // clone clip and insert into new track..
        auto new_clip = copyRows(QModelIndexList({i}), target_row, trackIndex)[0];

        // wait for it ? Not sure we need to..
        target_row++;
        result.push_back(new_clip);
    }
    return result;
}


// clones clips in to new track above
// should we try and support multple clips on multiple tracks ?
// should this really be done here or in the timeline actor...?
QModelIndexList SessionModel::duplicateTimelineClips(
    const QModelIndexList &indexes,
    const QString &qTrackName,
    const QString &qTrackSuffix,
    const bool append) {
    // we only handle clips
    // order clips by row and sort under their tracks.
    auto result      = QModelIndexList();
    auto track_clips = std::map<QModelIndex, std::vector<QModelIndex>>();
    auto trackName   = StdFromQString(qTrackName);
    auto trackSuffix = StdFromQString(qTrackSuffix);

    auto expanded_indexed = QModelIndexList();

    for (const auto &i : indexes) {
        auto type = StdFromQString(i.data(typeRole).toString());
        if (type == "Clip")
            expanded_indexed.push_back(i);
        else if (type == "Audio Track" or type == "Video Track") {
            for (auto j = 0; j < rowCount(i); j++) {
                auto ind = index(j, 0, i);
                type     = StdFromQString(ind.data(typeRole).toString());
                if (type == "Clip")
                    expanded_indexed.push_back(ind);
            }
        }
    }


    for (const auto &i : expanded_indexed) {
        auto track_index = getTimelineTrackIndex(i);
        if (track_index.isValid()) {
            if (not track_clips.count(track_index))
                track_clips[track_index] = std::vector<QModelIndex>();
            track_clips[track_index].push_back(i);
        }
    }

    // We now need to sort tracks and clips so we don't mess the rows up
    auto sorted_track_clips = std::vector<std::pair<QModelIndex, std::vector<QModelIndex>>>();
    for (const auto &i : track_clips) {
        sorted_track_clips.push_back(std::make_pair(i.first, i.second));
        // sort clips..
        std::sort(
            sorted_track_clips.back().second.begin(),
            sorted_track_clips.back().second.end(),
            [](auto &a, auto &b) { return a.row() < b.row(); });
    }

    // sort by track row
    // account for audio track behaviour ? Inserting below ?
    std::sort(sorted_track_clips.begin(), sorted_track_clips.end(), [](auto &a, auto &b) {
        return a.first.row() > b.first.row();
    });

    // tracks and clips should now be sorted correctly.
    auto count = 0;
    for (const auto &i : sorted_track_clips) {

        auto track_type = StdFromQString(i.first.data(typeRole).toString());
        auto track_name = StdFromQString(i.first.data(nameRole).toString());

        if (not trackName.empty())
            track_name = trackName;

        if (not trackSuffix.empty())
            track_name += " " + trackSuffix;

        QModelIndex new_track_index;

        auto new_row = track_type == "Video Track"
                           ? (append ? 0 : i.first.row())
                           : (append ? rowCount(i.first.parent()) - count : i.first.row() + 1);

        count++;

        new_track_index = insertRowsSync(
            new_row,
            1,
            QStringFromStd(track_type),
            QStringFromStd(track_name),
            i.first.parent())[0];

        // new track created, now populate with gaps and duplicated clips.
        // we need to workout the gap required to insert before the clip...
        auto current_clip_index = 0;
        auto target_row         = 0;

        for (const auto &j : i.second) {
            // sum duration of items before this clip in list.
            auto leading_track_frames = 0;
            for (; current_clip_index < j.row(); current_clip_index++) {
                leading_track_frames +=
                    index(current_clip_index, 0, j.parent()).data(trimmedDurationRole).toInt();
            }

            if (leading_track_frames) {
                auto gap_index = insertRowsSync(
                    target_row,
                    1,
                    QStringFromStd("Gap"),
                    QStringFromStd("Gap"),
                    new_track_index)[0];

                // we need to wait until our new gap becomes valid..
                // process events for a small time then check if we've got a populated gap..
                while (gap_index.data(placeHolderRole).toBool() == true) {
                    QCoreApplication::processEvents(
                        QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
                }

                setData(
                    gap_index, QVariant::fromValue(leading_track_frames), activeDurationRole);
                // increment target track row.
                target_row++;
            }

            // clone clip and insert into new track..
            auto new_clip = copyRows(QModelIndexList({j}), target_row, new_track_index)[0];

            // wait for it ? Not sure we need to..
            target_row++;
            result.push_back(new_clip);
            current_clip_index = j.row() + 1;
        }
    }

    return result;
}


bool SessionModel::removeTimelineItems(const QModelIndexList &indexes, const bool isRipple) {
    auto result = false;
    try {

        // ignore indexes that are not timeline items..
        // be careful of invalidation, deletion order matters ?

        // simple operations.. deletion of tracks.
        // we're deleting items using rows..
        // The order matters, and we won't get model index updates to refresh the rows..
        // Order by descending row.
        auto sorted_indexes = std::vector<QModelIndex>(indexes.begin(), indexes.end());
        std::sort(
            sorted_indexes.begin(), sorted_indexes.end(), [](QModelIndex &a, QModelIndex &b) {
                return a.parent().row() > b.parent().row() or
                       (a.parent().row() == b.parent().row() and a.row() > b.row());
            });

        // for(const auto &i: sorted_indexes)
        //     spdlog::warn("{} {}", i.row(), i.parent().row());

        for (const auto &i : sorted_indexes) {
            if (i.isValid()) {
                auto locked = i.data(lockedRole).toBool();
                if (locked)
                    continue;
                auto name         = StdFromQString(i.data(nameRole).toString());
                auto type         = StdFromQString(i.data(typeRole).toString());
                auto actor        = actorFromQString(system(), i.data(actorRole).toString());
                auto parent_index = i.parent();

                // spdlog::warn("REMOVE {} {} {} {}", type, to_string(actor), i.row(), name);

                if (parent_index.isValid()) {
                    locked = parent_index.data(lockedRole).toBool();
                    if (locked)
                        continue;

                    caf::scoped_actor sys(system());
                    auto pactor =
                        actorFromQString(system(), parent_index.data(actorRole).toString());
                    auto uuid = UuidFromQUuid(i.data(JSONTreeModel::Roles::idRole).toUuid());

                    if (pactor) {
                        if (type == "Clip") {
                            // replace with gap
                            // get parent, and index.
                            // // find parent timeline.
                            // auto range = request_receive<utility::FrameRange>(
                            //     *sys, actor, timeline::trimmed_range_atom_v);

                            // auto uuid = Uuid::generate();
                            // auto gap  = self()->spawn<timeline::GapActor>(
                            //     "Gap", range.frame_duration(), uuid);
                            // request_receive<JsonStore>(
                            //     *sys,
                            //     pactor,
                            //     timeline::insert_item_atom_v,
                            //     row,
                            //     UuidActorVector({UuidActor(uuid, gap)}));
                            request_receive<JsonStore>(
                                *sys, pactor, timeline::erase_item_atom_v, uuid, not isRipple);
                        } else {
                            request_receive<JsonStore>(
                                *sys, pactor, timeline::erase_item_atom_v, uuid, false);
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

void SessionModel::add_processed_event(const utility::Uuid &uuid) {
    processed_events_.insert(uuid);
    processed_events_queue_.push(uuid);

    while (processed_events_queue_.size() > 1000) {
        processed_events_.erase(processed_events_queue_.front());
        processed_events_queue_.pop();
    }
}

bool SessionModel::wait_for_event(
    const utility::Uuid &uuid, const std::chrono::milliseconds &timeout) {
    auto result = processed_events_.count(uuid);

    if (not result) {
        const auto then = clock::now() + timeout;
        while (not result and clock::now() < then) {
            QCoreApplication::processEvents(
                QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
            result = processed_events_.count(uuid);
        }
    }

    return result;
}


// trigger actor creation
void SessionModel::item_event_callback(const utility::JsonStore &event, timeline::Item &item) {
    auto debug = false;
    try {
        // if (debug)
        //     spdlog::warn("timeline::event {}", event.dump(2));

        auto index = searchRecursive(
            QVariant::fromValue(QUuidFromUuid(event.at("uuid").get<Uuid>())),
            idRole,
            QModelIndex(),
            0,
            -1);

        if (not index.isValid())
            throw std::runtime_error(
                "Invalid item not found " + to_string(event.at("uuid").get<Uuid>()));

        switch (static_cast<timeline::ItemAction>(event.at("action"))) {
        case timeline::IA_INSERT:

            // check for place holder entry..
            if (debug)
                spdlog::warn("timeline::IA_INSERT {}", event.dump(2));
            {
                auto tree = indexToTree(index);
                if (tree) {
                    auto new_item = timeline::Item(event.at("item"), &system());
                    auto new_node = timelineItemToJson(new_item, system(), true);

                    auto replaced = false;
                    // check children..
                    auto place_row = 0;
                    for (auto &i : *tree) {
                        auto data = i.data();
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

                add_lookup(*indexToTree(index), index);
            }
            break;

        case timeline::IA_REMOVE:
            if (debug)
                spdlog::warn("timeline::IA_REMOVE {}", event.dump(2));
            JSONTreeModel::removeRows(event.at("index").get<int>(), 1, index);
            if (index.data(typeRole).toString() == QString("Stack")) {
                // refresh teack indexes
                emit dataChanged(
                    SessionModel::index(0, 0, index),
                    SessionModel::index(rowCount(index) - 1, 0, index),
                    QVector<int>({trackIndexRole}));
            }
            break;

        case timeline::IA_ENABLE:
            if (debug)
                spdlog::warn("timeline::IA_ENABLE {}", event.dump(2));
            if (indexToData(index).at("enabled").is_null() or
                indexToData(index).at("enabled") != event.value("value", true)) {
                indexToData(index)["enabled"] = event.value("value", true);
                emit dataChanged(index, index, QVector<int>({enabledRole}));
            }
            break;

        case timeline::IA_LOCK:
            if (debug)
                spdlog::warn("timeline::IA_LOCK {}", event.dump(2));
            if (indexToData(index).at("locked").is_null() or
                indexToData(index).at("locked") != event.value("value", true)) {
                indexToData(index)["locked"] = event.value("value", true);
                emit dataChanged(index, index, QVector<int>({lockedRole}));
            }
            break;

        case timeline::IA_NAME:
            if (debug)
                spdlog::warn("timeline::IA_NAME {}", event.dump(2));
            if (indexToData(index).at("name").is_null() or
                indexToData(index).at("name") != event.value("value", "")) {
                indexToData(index)["name"] = event.value("value", "");
                emit dataChanged(index, index, QVector<int>({nameRole}));
            }
            break;

        case timeline::IA_FLAG:
            if (debug)
                spdlog::warn("timeline::IA_FLAG {}", event.dump(2));
            if (indexToData(index).at("flag").is_null() or
                indexToData(index).at("flag") != event.value("value", "")) {
                indexToData(index)["flag"] = event.value("value", "");
                emit dataChanged(index, index, QVector<int>({flagColourRole}));
            }
            break;

        case timeline::IA_MARKER:
            if (debug)
                spdlog::warn("timeline::IA_MARKER {}", event.dump(2));
            if (indexToData(index).at("markers").is_null() or
                indexToData(index).at("markers") != event.value("value", R"([])"_json)) {
                indexToData(index)["markers"] = event.value("value", R"([])"_json);
                emit dataChanged(index, index, QVector<int>({markersRole}));
            }
            break;

        case timeline::IA_PROP:
            if (debug)
                spdlog::warn("timeline::IA_PROP {}", event.dump(2));
            if (indexToData(index).at("prop").is_null() or
                indexToData(index).at("prop") != event.at("value")) {
                indexToData(index)["prop"] = event.at("value");
                emit dataChanged(index, index, QVector<int>({propertyRole, clipMediaUuidRole}));
            }
            break;

        case timeline::IA_RANGE:
            if (debug)
                spdlog::warn("timeline::IA_RANGE {}", event.dump(2));
            {
                bool changed = false;
                if (event.at("value2") == true) {
                    if (indexToData(index).at("available_range").is_null() or
                        indexToData(index).at("available_range") != event.at("value")) {
                        indexToData(index)["available_range"] = event.at("value");
                        changed                               = true;
                    }
                } else {
                    if (not indexToData(index).at("available_range").is_null()) {
                        indexToData(index)["available_range"] = nullptr;
                        changed                               = true;
                    }
                }

                if (event.at("value4") == true) {
                    if (indexToData(index).at("active_range").is_null() or
                        indexToData(index).at("active_range") != event.at("value3")) {
                        indexToData(index)["active_range"] = event.at("value3");
                        changed                            = true;
                    }
                } else {
                    if (not indexToData(index).at("active_range").is_null()) {
                        indexToData(index)["active_range"] = nullptr;
                        changed                            = true;
                    }
                }

                if (changed)
                    emit dataChanged(
                        index,
                        index,
                        QVector<int>(
                            {trimmedDurationRole,
                             rateFPSRole,
                             activeDurationRole,
                             activeRangeValidRole,
                             trimmedStartRole,
                             activeStartRole}));
            }
            break;

        case timeline::IA_ACTIVE:
            if (debug)
                spdlog::warn("timeline::IA_ACTIVE {}", event.dump(2));

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
                             activeRangeValidRole,
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
                             activeRangeValidRole,
                             trimmedStartRole,
                             activeStartRole}));
                }
            }
            break;

        case timeline::IA_AVAIL:
            if (debug)
                spdlog::warn("timeline::IA_AVAIL {}", event.dump(2));

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
                             activeRangeValidRole,
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
                             activeRangeValidRole,
                             availableDurationRole,
                             trimmedStartRole,
                             availableStartRole}));
                }
            }
            break;

        case timeline::IA_SPLICE: {
            if (debug)
                spdlog::warn("timeline::IA_SPLICE {}", event.dump(2));

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
        } break;

        case timeline::IA_ADDR:
            if (debug)
                spdlog::warn("timeline::IA_ADDR {}", event.dump(2));
            // is the string actor valid here ?
            if (event.at("value").is_null() and not indexToData(index).at("actor").is_null()) {
                indexToData(index)["actor"] = nullptr;
                emit dataChanged(index, index, QVector<int>({actorRole}));
            } else if (
                event.at("value").is_string() and
                (not indexToData(index).at("actor").is_string() or
                 event.at("value") != indexToData(index).at("actor"))) {
                indexToData(index)["actor"] = event.at("value");
                emit dataChanged(index, index, QVector<int>({actorRole}));
                // add_lookup(*indexToTree(index), index);
            }
            break;

        case timeline::IA_NONE:
        default:
            break;
        }

        add_processed_event(event.at("event_id").get<Uuid>());

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
                    "prop": null,
                    "markers": null,
                    "name": null,
                    "placeholder": true,
                    "active_range": null,
                    "available_range": null,
                    "type": "Gap"
                })"_json;


                auto new_uuid = utility::Uuid::generate();
                auto duration =
                    utility::FrameRateDuration(frames, FrameRate(fps_to_flicks(rate)));
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
            auto event = request_receive<JsonStore>(
                *sys,
                track_actor,
                timeline::move_item_at_frame_atom_v,
                frame,
                duration,
                dest,
                insert,
                true);
            if (not wait_for_event(timeline::get_event_id(event).back()))
                spdlog::warn("event timed out");

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
        int align_to = startFrameInParent(indexes[0]);
        if (align_right)
            align_to += indexes[0].data(trimmedDurationRole).toInt();

        for (auto i = 1; i < indexes.size(); i++) {
            auto frame = startFrameInParent(indexes[i]);
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
                auto mind = searchRecursive(QUuid::fromString(QStringFromStd(i)), idRole);

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
                                    std::get<1>(i),
                                    false);

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


QModelIndex
SessionModel::bakeTimelineItems(const QModelIndexList &indexes, const QString &qtrackName) {
    auto result = QModelIndex();

    // indexes should contain tracks/clips
    if (not indexes.empty()) {
        auto tindex    = getTimelineIndex(indexes.at(0));
        auto sindex    = index(0, 0, index(2, 0, tindex));
        auto trindex   = getTimelineTrackIndex(indexes.at(0));
        auto rcount    = rowCount(sindex);
        auto trackName = StdFromQString(qtrackName);

        if (tindex.isValid() and sindex.isValid()) {
            auto timeline_actor = actorFromQString(system(), tindex.data(actorRole).toString());
            auto stack_actor    = actorFromQString(system(), sindex.data(actorRole).toString());

            if (trackName.empty()) {
                auto row = 0;
                for (const auto &i : indexes) {
                    auto trackindex = getTimelineTrackIndex(i);
                    if (trackindex.isValid() and trackindex.row() >= row) {
                        row       = trackindex.row();
                        trackName = StdFromQString(trackindex.data(nameRole).toString());
                    }
                }
            }

            if (timeline_actor and stack_actor) {
                scoped_actor sys{system()};
                // build list of uuids..

                auto uuids = UuidVector();
                for (const auto &i : indexes)
                    uuids.emplace_back(UuidFromQUuid(i.data(idRole).toUuid()));

                try {
                    auto ntrack = request_receive<UuidActor>(
                        *sys, timeline_actor, timeline::bake_atom_v, uuids);
                    if (not trackName.empty())
                        anon_send(ntrack.actor(), timeline::item_name_atom_v, trackName);

                    auto row = trindex.data(typeRole).toString() == "Video Track"
                                   ? trindex.row()
                                   : trindex.row() - 1;

                    try {
                        request_receive<JsonStore>(
                            *sys,
                            stack_actor,
                            timeline::insert_item_atom_v,
                            row,
                            UuidActorVector({ntrack}));
                    } catch (const std::exception &err) {
                        spdlog::warn("{} insert {}", __PRETTY_FUNCTION__, err.what());
                        throw;
                    }
                    // wait for track index.. to become valid ?
                    while (rowCount(sindex) == rcount) {
                        QCoreApplication::processEvents(
                            QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents,
                            50);
                    }
                    result = index(row, 0, sindex);

                    try {
                        request_receive<bool>(
                            *sys, timeline_actor, timeline::link_media_atom_v, false);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} link {}", __PRETTY_FUNCTION__, err.what());
                        throw;
                    }

                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }
        }
    }

    return result;
}

QModelIndexList SessionModel::modifyItemSelectionVertical(
    const QModelIndexList &items, const int up, const int down) {
    auto result = QModelIndexList();

    if (not items.empty()) {
        auto first_type = items[0].data(typeRole);
        auto parent     = items[0].parent();
        // simple...
        // find min , max

        auto min_a = 0;
        auto min_v = min_a;
        auto max_a = rowCount(parent) - 1;
        auto max_v = max_a;

        //  find bounds..
        for (auto i = 0; i < rowCount(parent); i++) {
            auto i_type = index(i, 0, parent).data(typeRole);
            if (i_type == "Video Track") {
                max_v = i;
                min_a = i + 1;
            } else {
                break;
            }
        }

        if (first_type == "Audio Track" or first_type == "Video Track") {

            auto selected_video = std::vector<std::pair<int, int>>();
            auto selected_audio = std::vector<std::pair<int, int>>();

            // order items by row true = video, false = audio
            std::map<int, bool> selection;

            for (const auto &i : items) {
                if (i.data(typeRole) == "Video Track")
                    selection[i.row()] = true;
                else
                    selection[i.row()] = false;
            }

            // iterate over selection..
            // populate ranges.
            auto is_video = true;
            auto start    = -1;
            auto end      = -1;

            for (const auto &i : selection) {
                if (start == -1) {
                    start = end = i.first;
                    is_video    = i.second;
                } else {
                    if (i.first != end + 1 or i.second != is_video) {
                        if (is_video)
                            selected_video.emplace_back(std::make_pair(start, end));
                        else
                            selected_audio.emplace_back(std::make_pair(start, end));

                        start = end = i.first;
                        is_video    = i.second;
                    } else {
                        end++;
                    }
                }
            }

            if (start != -1) {
                if (is_video)
                    selected_video.emplace_back(std::make_pair(start, end));
                else
                    selected_audio.emplace_back(std::make_pair(start, end));
            }

            if (up != 0) {
                for (auto &i : selected_video) {
                    auto new_start = i.first - up;
                    if (new_start >= min_v and new_start <= max_v)
                        i.first = new_start;
                    else
                        return items;
                }

                for (auto &i : selected_audio) {
                    auto new_start = i.first - up;
                    if (new_start >= min_a and new_start <= max_a)
                        i.first = new_start;
                    else
                        return items;
                }
            }

            if (down != 0) {
                for (auto &i : selected_video) {
                    auto new_end = i.second + down;
                    if (new_end <= max_v and new_end >= min_v)
                        i.second = new_end;
                    else
                        return items;
                }

                for (auto &i : selected_audio) {
                    auto new_end = i.second + down;
                    if (new_end <= max_a and new_end >= min_a)
                        i.second = new_end;
                    else
                        return items;
                }
            }

            for (const auto &i : selected_video) {
                for (auto r = i.first; r <= i.second; r++)
                    result.push_back(index(r, 0, parent));
            }

            for (const auto &i : selected_audio) {
                for (auto r = i.first; r <= i.second; r++)
                    result.push_back(index(r, 0, parent));
            }

        } else if (first_type == "Clip") {
            // even more complex...
            // build list of vertical regions.
            // get bounds of audio/video.
            auto stack_index    = getTimelineTrackIndex(items[0]).parent();
            auto timeline_index = getTimelineIndex(stack_index);
            auto tactor = actorFromQString(system(), timeline_index.data(actorRole).toString());

            if (timeline_lookup_.count(tactor)) {
                auto &titem = timeline_lookup_.at(tactor);

                auto min_a = 0;
                auto min_v = min_a;
                auto max_a = rowCount(stack_index) - 1;
                auto max_v = max_a;

                auto has_audio = false;
                auto has_video = false;

                //  find bounds..
                for (auto i = 0; i < rowCount(stack_index); i++) {
                    auto i_type = index(i, 0, stack_index).data(typeRole);
                    if (i_type == "Video Track") {
                        has_video = true;
                        max_v     = i;
                        min_a     = i + 1;
                    } else {
                        has_audio = true;
                        break;
                    }
                }
                // get selected clip boxes.
                auto video_selection_boxes = std::list<timeline::Box>();
                auto audio_selection_boxes = std::list<timeline::Box>();

                auto print_boxes = [](const std::list<timeline::Box> &boxes) {
                    for (const auto &i : boxes) {
                        spdlog::warn(
                            "l {} t {} r {} b {}",
                            i.first.first,
                            i.first.second,
                            i.second.first,
                            i.second.second);
                    }
                };

                for (const auto &i : items) {
                    if (getTimelineTrackIndex(i).data(typeRole) == "Video Track")
                        video_selection_boxes.emplace_back(
                            *(titem.box(UuidFromQUuid(i.data(idRole).toUuid()))));
                    else
                        audio_selection_boxes.emplace_back(
                            *(titem.box(UuidFromQUuid(i.data(idRole).toUuid()))));
                }

                auto sort_track_start = [](const timeline::Box &a, const timeline::Box &b) {
                    return a.first.second < b.first.second or
                           (a.first.second == b.first.second and a.first.first < b.first.first);
                };

                auto sort_start_track = [](const timeline::Box &a, const timeline::Box &b) {
                    return a.first.first < b.first.first or
                           (a.first.first == b.first.first and a.first.second < b.first.second);
                };

                auto sort_end_track = [](const timeline::Box &a, const timeline::Box &b) {
                    return a.second.first < b.second.first or
                           (a.second.first == b.second.first and
                            a.first.second < b.first.second);
                };

                video_selection_boxes.sort(sort_start_track);
                audio_selection_boxes.sort(sort_start_track);

                // if(not video_selection_boxes.empty()) {
                //     spdlog::warn("\nvideo");
                //     print_boxes(video_selection_boxes);
                // }
                // if(not audio_selection_boxes.empty()) {
                //     spdlog::warn("audio");
                //     print_boxes(audio_selection_boxes);
                // }

                auto merge_boxes = [=](std::list<timeline::Box> &boxes) {
                    auto done = false;

                    // merge vertical with same start
                    while (not done) {
                        done = true;
                        boxes.sort(sort_start_track);

                        for (auto it = boxes.begin(); it != boxes.end();) {
                            auto nit = std::next(it, 1);
                            if (nit != boxes.end()) {
                                // it left = nit left and it bottom = nit top
                                if (it->first.first == nit->first.first and
                                    it->second.second == nit->first.second) {
                                    // match but we'll need to deal with under / overhang.
                                    if (it->second.first < nit->second.first) {
                                        boxes.emplace_back(std::make_pair(
                                            std::make_pair(it->second.first, nit->first.second),
                                            std::make_pair(
                                                nit->second.first, nit->second.second)));
                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                        done = false;
                                        break;

                                    } else if (it->second.first > nit->second.first) {
                                        boxes.emplace_back(std::make_pair(
                                            std::make_pair(nit->second.first, it->first.second),
                                            std::make_pair(
                                                it->second.first, it->second.second)));

                                        it->second.first  = nit->second.first;
                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                        done = false;
                                        break;
                                    } else {
                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                    }
                                } else {
                                    it++;
                                }
                            } else {
                                it++;
                            }
                        }
                    }

                    // merge vertical with same end
                    done = false;
                    while (not done) {
                        done = true;
                        boxes.sort(sort_end_track);

                        for (auto it = boxes.begin(); it != boxes.end();) {
                            auto nit = std::next(it, 1);
                            if (nit != boxes.end()) {
                                // it back = nit back and it bottom = nit top
                                if (it->second.first == nit->second.first and
                                    it->second.second == nit->first.second) {
                                    // match but we'll need to deal with under / overhang.
                                    if (it->first.first < nit->first.first) {
                                        boxes.emplace_back(std::make_pair(
                                            std::make_pair(it->first.first, it->first.second),
                                            std::make_pair(
                                                nit->first.first, it->second.second)));
                                        it->first.first   = nit->first.first;
                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                        done = false;
                                        break;

                                    } else if (it->first.first > nit->first.first) {
                                        boxes.emplace_back(std::make_pair(
                                            std::make_pair(nit->first.first, nit->first.second),
                                            std::make_pair(
                                                it->first.first, nit->second.second)));

                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                        done = false;
                                        break;
                                    } else {
                                        it->second.second = nit->second.second;
                                        boxes.erase(nit);
                                    }
                                } else {
                                    it++;
                                }
                            } else {
                                it++;
                            }
                        }
                    }
                };

                merge_boxes(video_selection_boxes);
                merge_boxes(audio_selection_boxes);

                // if(not video_selection_boxes.empty()) {
                //     spdlog::warn("\nvideo");
                //     print_boxes(video_selection_boxes);
                // }
                // if(not audio_selection_boxes.empty()) {
                //     spdlog::warn("audio");
                //     print_boxes(audio_selection_boxes);
                // }

                // expand / contract boxes.
                for (auto &i : video_selection_boxes) {
                    i.first.second -= up;
                    if (i.first.second < min_v or i.first.second > max_v)
                        return items;

                    i.second.second += down;
                    if (i.second.second < min_v or i.second.second - 1 > max_v)
                        return items;
                }

                for (auto &i : audio_selection_boxes) {
                    i.first.second -= up;
                    if (i.first.second < min_a or i.first.second > max_a)
                        return items;

                    i.second.second += down;
                    if (i.second.second < min_a or i.second.second - 1 > max_a)
                        return items;
                }

                // if(not video_selection_boxes.empty()) {
                //     spdlog::warn("\nvideo");
                //     print_boxes(video_selection_boxes);
                // }
                // if(not audio_selection_boxes.empty()) {
                //     spdlog::warn("audio");
                //     print_boxes(audio_selection_boxes);
                // }

                // apply new selection..
                std::set<QModelIndex> new_selection;

                if (not video_selection_boxes.empty()) {
                    for (const auto &i : video_selection_boxes)
                        for (const auto &j : getTimelineVideoClipIndexesFromRect(
                                 timeline_index,
                                 i.first.first,
                                 i.first.second,
                                 i.second.first,
                                 i.second.second,
                                 1,
                                 1)) {
                            new_selection.insert(j);
                        }
                }

                if (not audio_selection_boxes.empty()) {
                    for (const auto &i : audio_selection_boxes)
                        for (const auto &j : getTimelineAudioClipIndexesFromRect(
                                 timeline_index,
                                 i.first.first,
                                 i.first.second,
                                 i.second.first,
                                 i.second.second,
                                 1,
                                 1)) {
                            new_selection.insert(j);
                        }
                }

                for (const auto &i : new_selection)
                    result.push_back(i);
            }
        }
    }

    return result;
}

QModelIndexList SessionModel::modifyItemSelectionHorizontal(
    const QModelIndexList &items, const int left, const int right) {
    // spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, left, right);

    auto rows = std::map<int, QModelIndexList>();
    auto dups = std::map<int, std::set<int>>();

    for (const auto &i : items) {
        if (i.data(typeRole) != "Clip")
            continue;

        const auto parent_row = i.parent().row();

        if (not dups.count(parent_row))
            dups[parent_row] = std::set<int>();
        if (dups[parent_row].count(i.row()))
            continue;
        dups[parent_row].insert(i.row());

        if (not rows.count(parent_row))
            rows.emplace(parent_row, QModelIndexList({i}));
        else
            rows[parent_row].push_back(i);
    }

    // sort clips
    for (auto &i : rows)
        std::sort(
            i.second.begin(), i.second.end(), [=](const QModelIndex &a, const QModelIndex &b) {
                return a.row() < b.row();
            });

    // for(const auto &i: rows) {
    //     for(const auto &ii: i.second) {
    //         spdlog::warn("{} {}", ii.row(), ii.parent().row());
    //     }
    // }

    auto right_count = right;
    // lets go..
    while (right_count > 0) {
        for (auto &i : rows) {
            // get right most clip.
            auto c            = i.second.last();
            auto row          = c.row() + 1;
            const auto parent = c.parent();
            while (row < rowCount(parent)) {
                auto ind = index(row, 0, parent);
                if (ind.data(typeRole) == "Clip") {
                    i.second.push_back(ind);
                    break;
                }
                row++;
            }
        }
        right_count--;
    }

    auto left_count = left;
    // lets go..
    while (left_count > 0) {
        for (auto &i : rows) {
            // get right most clip.
            auto c            = i.second.first();
            auto row          = c.row() - 1;
            const auto parent = c.parent();
            while (row >= 0) {
                auto ind = index(row, 0, parent);
                if (ind.data(typeRole) == "Clip") {
                    i.second.push_front(ind);
                    break;
                }
                row--;
            }
        }
        left_count--;
    }

    left_count  = left;
    right_count = right;

    while (left_count < 0) {
        for (auto &i : rows) {
            // get right most clip.
            if (i.second.size() > 1) {
                i.second.pop_front();
            }
        }
        left_count++;
    }

    while (right_count < 0) {
        for (auto &i : rows) {
            // get right most clip.
            if (i.second.size() > 1) {
                i.second.pop_back();
            }
        }
        right_count++;
    }


    auto result = QModelIndexList();
    // prune duplicates.
    for (const auto &i : rows)
        result += i.second;


    return result;
}

// return clips using media
void SessionModel::resetTimelineItemDragFlag(const QModelIndexList &items) {
    for (const auto &i : items) {
        if (not i.isValid())
            continue;
        auto orig_data                = mapFromValue(i.data(userDataRole));
        auto data                     = orig_data;
        data["show_drag_left"]        = false;
        data["show_drag_right"]       = false;
        data["show_drag_middle"]      = false;
        data["show_drag_left_left"]   = false;
        data["show_drag_right_right"] = false;
        data["show_rolling"]          = false;

        if (orig_data != data)
            setData(i, mapFromValue(data), userDataRole);
    }
}

void SessionModel::updateTimelineItemDragFlag(
    const QModelIndexList &items,
    const bool isRolling,
    const bool isRipple,
    const bool isOverwrite) {

    // spdlog::warn("updateTimelineItemDragFlag {} {} {} {}", isRolling, isRipple, isOverwrite,
    // items.size());

    for (const auto &i : items) {
        if (not i.isValid())
            continue;

        auto orig_data = mapFromValue(i.data(userDataRole));
        // spdlog::warn("{}", orig_data.dump(2));

        auto data = orig_data;

        if (isRolling) {
            data["show_rolling"] = true;
        } else {
            // if (not isOverwrite) {
            data["show_drag_left"]  = true;
            data["show_drag_right"] = true;
            // }
            data["show_drag_middle"] = true;

            if (not isRipple and not isOverwrite) {
                auto pre_index  = index(i.row() - 1, 0, i.parent());
                auto ante_index = index(i.row() + 1, 0, i.parent());

                if (pre_index.isValid() and
                    pre_index.data(typeRole).toString() == QString("Clip"))
                    data["show_drag_left_left"] = true;

                if (ante_index.isValid() and
                    ante_index.data(typeRole).toString() == QString("Clip"))
                    data["show_drag_right_right"] = true;
            }
        }

        if (orig_data != data) {
            // spdlog::warn("{}", data.dump(2));
            setData(i, mapFromValue(data), userDataRole);
        }
    }
}

void SessionModel::beginTimelineItemDrag(
    const QModelIndexList &items,
    const QString &mode,
    const bool isRipple,
    const bool isOverwrite) {
    for (const auto &i : items) {
        if (not i.isValid())
            continue;
        if (i.data(lockedRole).toBool())
            continue;
        if (i.parent().data(lockedRole).toBool())
            continue;

        auto orig_data = mapFromValue(i.data(userDataRole));
        auto data      = orig_data;

        // reset all
        data["adjust_duration"]          = 0;
        data["adjust_start"]             = 0;
        data["drag_value"]               = 0;
        data["is_adjusting_start"]       = false;
        data["is_adjusting_duration"]    = false;
        data["is_adjusting_preceeding"]  = false;
        data["is_adjusting_anteceeding"] = false;
        data["is_anteceeding_track"]     = false;
        data["adjust_track"]             = 0;
        data["adjust_preceeding_gap"]    = 0;
        data["adjust_anteceeding_gap"]   = 0;
        data["move_x"]                   = 0;
        data["move_Y"]                   = 0;
        data["is_floating"]              = false;

        if (mode == QString("roll")) {
            data["is_adjusting_start"] = true;
        } else if (mode == QString("leftleft")) {
            data["is_adjusting_start"]    = true;
            data["is_adjusting_duration"] = true;

            auto preceeding = index(i.row() - 1, 0, i.parent());
            auto orig_data2 = mapFromValue(preceeding.data(userDataRole));
            auto data2      = orig_data2;

            data2["adjust_duration"]       = 0;
            data2["is_adjusting_duration"] = true;

            if (orig_data2 != data2)
                setData(preceeding, mapFromValue(data2), userDataRole);
        } else if (mode == QString("rightright")) {
            data["is_adjusting_duration"] = true;

            auto anteceeding = index(i.row() + 1, 0, i.parent());
            auto orig_data2  = mapFromValue(anteceeding.data(userDataRole));
            auto data2       = orig_data2;

            data2["adjust_duration"]       = 0;
            data2["adjust_start"]          = 0;
            data2["drag_value"]            = 0;
            data2["is_adjusting_start"]    = true;
            data2["is_adjusting_duration"] = true;

            if (orig_data2 != data2)
                setData(anteceeding, mapFromValue(data2), userDataRole);
        } else if (mode == QString("left")) {
            data["is_adjusting_start"]    = true;
            data["is_adjusting_duration"] = true;

            if (isOverwrite) {
                data["is_floating"] = true;
            } else {
                auto preceeding = index(i.row() - 1, 0, i.parent());
                if (preceeding.isValid()) {
                    auto type = preceeding.data(typeRole).toString();
                    if (type == QString("Gap")) {
                        data["is_adjusting_preceeding"] = true;
                        auto orig_data2 = mapFromValue(preceeding.data(userDataRole));
                        auto data2      = orig_data2;

                        data2["adjust_duration"]       = 0;
                        data2["is_adjusting_duration"] = true;

                        if (orig_data2 != data2)
                            setData(preceeding, mapFromValue(data2), userDataRole);
                    }
                }
            }
        } else if (mode == QString("right")) {
            data["is_adjusting_duration"] = true;

            if (not isRipple) {
                if (isOverwrite) {
                    data["is_floating"] = true;
                } else {
                    auto anteceeding = index(i.row() + 1, 0, i.parent());
                    if (anteceeding.isValid()) {
                        auto type = anteceeding.data(typeRole).toString();
                        if (type == QString("Gap")) {
                            auto orig_data2 = mapFromValue(anteceeding.data(userDataRole));
                            auto data2      = orig_data2;

                            data["is_adjusting_anteceeding"] = true;

                            data2["adjust_duration"]       = 0;
                            data2["is_adjusting_duration"] = true;

                            if (orig_data2 != data2)
                                setData(anteceeding, mapFromValue(data2), userDataRole);
                        }
                    } else {
                        data["is_anteceeding_track"] = true;
                    }
                }
            }
        } else if (mode == QString("middle")) {
            if (isOverwrite) {
                data["is_floating"] = true;
            } else {
                auto preceeding  = index(i.row() - 1, 0, i.parent());
                auto anteceeding = index(i.row() + 1, 0, i.parent());

                auto preceeding_type  = preceeding.isValid()
                                            ? preceeding.data(typeRole).toString()
                                            : QString("Track");
                auto anteceeding_type = anteceeding.isValid()
                                            ? anteceeding.data(typeRole).toString()
                                            : QString("Track");

                if (preceeding_type == QString("Gap")) {
                    auto orig_data2 = mapFromValue(preceeding.data(userDataRole));
                    auto data2      = orig_data2;

                    data["is_adjusting_preceeding"] = true;
                    data2["adjust_duration"]        = 0;
                    data2["is_adjusting_duration"]  = true;

                    if (orig_data2 != data2)
                        setData(preceeding, mapFromValue(data2), userDataRole);
                } else {
                    data["is_adjusting_preceeding"] = true;
                }

                if (not isRipple) {
                    if (anteceeding_type == QString("Gap")) {
                        auto orig_data2 = mapFromValue(anteceeding.data(userDataRole));
                        auto data2      = orig_data2;

                        data["is_adjusting_anteceeding"] = true;
                        data2["adjust_duration"]         = 0;
                        data2["is_adjusting_duration"]   = true;

                        if (orig_data2 != data2)
                            setData(anteceeding, mapFromValue(data2), userDataRole);
                    } else if (anteceeding_type != QString("Track")) {
                        data["is_adjusting_anteceeding"] = true;
                    } else {
                        data["is_anteceeding_track"] = true;
                    }
                }
            }
        }

        if (orig_data != data)
            setData(i, mapFromValue(data), userDataRole);
    }
}

void SessionModel::updateTimelineItemDrag(
    const QModelIndexList &items,
    const QString &mode,
    int frameChange,
    int trackChange,
    const bool isRipple,
    const bool isOverwrite) {
    for (const auto &i : items) {
        if (not i.isValid())
            continue;
        if (i.data(lockedRole).toBool())
            continue;
        if (i.parent().data(lockedRole).toBool())
            continue;

        auto orig_data = mapFromValue(i.data(userDataRole));
        auto data      = orig_data;
        if (mode == QString("roll")) {
            auto trimmedStart      = i.data(trimmedStartRole).toInt();
            auto availableStart    = i.data(availableStartRole).toInt();
            auto trimmedDuration   = i.data(trimmedDurationRole).toInt();
            auto availableDuration = i.data(availableDurationRole).toInt();

            data["adjust_start"] = std::floor(
                std::min(
                    std::max(trimmedStart + frameChange, availableStart),
                    availableStart + availableDuration - trimmedDuration) -
                trimmedStart);
            data["drag_value"] = data["adjust_start"];

            if (orig_data != data)
                setData(i, mapFromValue(data), userDataRole);
        } else if (mode == QString("leftleft")) {
            auto preceeding = index(i.row() - 1, 0, i.parent());
            frameChange     = checkAdjust(i, frameChange, true);
            frameChange     = checkAdjust(preceeding, frameChange, true);

            draggingAdjust(i, frameChange);
            draggingAdjust(preceeding, frameChange);
        } else if (mode == QString("rightright")) {
            auto anteceeding = index(i.row() + 1, 0, i.parent());
            frameChange      = checkAdjust(i, frameChange, true);
            frameChange      = checkAdjust(anteceeding, frameChange, true);

            draggingAdjust(i, frameChange);
            draggingAdjust(anteceeding, frameChange);
        } else if (mode == QString("left")) {
            frameChange = checkAdjust(i, frameChange, false, true);

            if (isOverwrite) {
                data["move_x"] = frameChange;
            } else if (data["is_adjusting_preceeding"]) {
                auto preceeding = index(i.row() - 1, 0, i.parent());
                frameChange     = checkAdjust(preceeding, frameChange, false);
                draggingAdjust(preceeding, frameChange);
            } else {
                frameChange                   = std::max(0, frameChange);
                data["adjust_preceeding_gap"] = frameChange;
            }
            if (orig_data != data)
                setData(i, mapFromValue(data), userDataRole);

            draggingAdjust(i, frameChange);
        } else if (mode == QString("right")) {
            frameChange = checkAdjust(i, frameChange, true);
            if (not isOverwrite) {
                if (data["is_adjusting_anteceeding"]) {
                    auto anteceeding = index(i.row() + 1, 0, i.parent());
                    frameChange      = -checkAdjust(anteceeding, -frameChange, false);
                    draggingAdjust(anteceeding, -frameChange);
                } else if (not isRipple && not data["is_anteceeding_track"]) {
                    frameChange                    = std::min(0, frameChange);
                    data["adjust_anteceeding_gap"] = -frameChange;
                    if (orig_data != data)
                        setData(i, mapFromValue(data), userDataRole);
                }
            }

            draggingAdjust(i, frameChange);
        } else if (mode == QString("middle")) {
            auto preceeding  = index(i.row() - 1, 0, i.parent());
            auto anteceeding = index(i.row() + 1, 0, i.parent());
            auto preceeding_type =
                preceeding.isValid() ? preceeding.data(typeRole).toString() : QString("Track");
            auto anteceeding_type = anteceeding.isValid()
                                        ? anteceeding.data(typeRole).toString()
                                        : QString("Track");

            if (isOverwrite) {
            } else if (data["is_adjusting_preceeding"] and preceeding_type == QString("Gap"))
                frameChange = checkAdjust(preceeding, frameChange, false);
            else
                frameChange = std::max(0, frameChange);

            if (not isRipple) {
                if (isOverwrite) {
                } else if (
                    data["is_adjusting_anteceeding"] and anteceeding_type == QString("Gap"))
                    frameChange = -checkAdjust(anteceeding, -frameChange, false);
                else if (data["is_anteceeding_track"]) {
                } else
                    frameChange = std::min(0, frameChange);
            }

            if (not isRipple)
                data["move_x"] = frameChange;

            // iterate checking each row, then returning last valid row
            if (trackChange > 0) {
                auto last_valid = 0;
                auto ttype      = i.parent().data(typeRole);
                for (auto row = 0; row <= trackChange; row++) {
                    auto target_row = index(i.parent().row() + row, 0, i.parent().parent());
                    if (target_row.isValid() and target_row.data(typeRole) != ttype)
                        break;
                    last_valid = row;
                }
                trackChange = last_valid;
            } else if (trackChange < 0) {
                auto last_valid = 0;
                auto ttype      = i.parent().data(typeRole);
                for (auto row = 0; row <= std::abs(trackChange); row++) {
                    auto target_row = index(i.parent().row() - row, 0, i.parent().parent());
                    if (target_row.isValid() and target_row.data(typeRole) != ttype)
                        break;
                    last_valid = row;
                }
                trackChange = -last_valid;
            }

            if (isOverwrite) {
                data["move_y"] = trackChange;
            } else {
                data["move_y"] = 0;
            }

            if (isOverwrite) {
            } else if (data["is_adjusting_preceeding"] and preceeding_type == QString("Gap")) {
                draggingAdjust(preceeding, frameChange);
            } else if (data["is_adjusting_preceeding"]) {
                data["adjust_preceeding_gap"] = frameChange;
            }

            if (isOverwrite) {
            } else if (data["is_adjusting_anteceeding"] and anteceeding_type == QString("Gap"))
                draggingAdjust(anteceeding, -frameChange);
            else if (data["is_adjusting_anteceeding"])
                data["adjust_anteceeding_gap"] = -frameChange;

            // data["is_adjust_duration"] = true;
            data["drag_value"] = frameChange;
            if (orig_data != data)
                setData(i, mapFromValue(data), userDataRole);
        }
    }
}

void SessionModel::endTimelineItemDrag(
    const QModelIndexList &items, const QString &mode, const bool isOverwrite) {
    // items need to be persistent or they'll get messed up when we remove stuff.

    auto pitems = std::vector<QPersistentModelIndex>();
    for (const auto &i : items)
        pitems.emplace_back(QPersistentModelIndex(i));

    for (const auto &i : pitems) {
        if (not i.isValid())
            continue;
        if (i.data(lockedRole).toBool())
            continue;
        if (i.parent().data(lockedRole).toBool())
            continue;

        auto orig_data = mapFromValue(i.data(userDataRole));
        auto data      = orig_data;

        auto flushChange = [=]() mutable {
            data["adjust_duration"]          = 0;
            data["adjust_start"]             = 0;
            data["drag_value"]               = 0;
            data["is_adjusting_start"]       = false;
            data["is_adjusting_duration"]    = false;
            data["is_adjusting_preceeding"]  = false;
            data["is_adjusting_anteceeding"] = false;
            data["is_anteceeding_track"]     = false;
            data["adjust_preceeding_gap"]    = 0;
            data["adjust_anteceeding_gap"]   = 0;
            data["adjust_track"]             = 0;
            data["move_x"]                   = 0;
            data["move_Y"]                   = 0;
            data["is_floating"]              = false;


            if (orig_data != data) {
                setData(i, mapFromValue(data), userDataRole);
                orig_data = data;
            }
        };

        if (mode == QString("roll")) {
            auto trimmedStart = i.data(trimmedStartRole).toInt();
            setData(
                i,
                QVariant::fromValue(trimmedStart + data["adjust_start"].get<int>()),
                activeStartRole);
        } else if (mode == "leftleft") {
            auto preceeding      = index(i.row() - 1, 0, i.parent());
            auto orig_data2      = mapFromValue(preceeding.data(userDataRole));
            auto data2           = orig_data2;
            auto trimmedStart    = i.data(trimmedStartRole).toInt();
            auto trimmedDuration = i.data(trimmedDurationRole).toInt();
            auto startFrame      = trimmedStart + (not data["is_adjusting_start"].is_null() and
                                                      data["is_adjusting_start"]
                                                       ? data["adjust_start"].get<int>()
                                                       : 0);
            auto durationFrame =
                trimmedDuration +
                (not data["is_adjusting_duration"].is_null() and data["is_adjusting_duration"]
                     ? data["adjust_duration"].get<int>()
                     : 0);
            auto trimmedDuration2 = preceeding.data(trimmedDurationRole).toInt();
            auto durationFrame2 =
                trimmedDuration2 +
                (not data2["is_adjusting_duration"].is_null() and data2["is_adjusting_duration"]
                     ? data2["adjust_duration"].get<int>()
                     : 0);

            setData(i, startFrame, activeStartRole);
            setData(i, durationFrame, activeDurationRole);
            setData(preceeding, durationFrame2, activeDurationRole);

            data2["is_adjusting_start"]    = false;
            data2["is_adjusting_duration"] = false;
            data2["adjust_start"]          = 0;
            data2["adjust_duration"]       = 0;
            data2["drag_value"]            = 0;

            if (orig_data2 != data2)
                setData(preceeding, mapFromValue(data2), userDataRole);
        } else if (mode == "rightright") {
            auto anteceeding     = index(i.row() + 1, 0, i.parent());
            auto orig_data2      = mapFromValue(anteceeding.data(userDataRole));
            auto data2           = orig_data2;
            auto trimmedDuration = i.data(trimmedDurationRole).toInt();
            auto durationFrame =
                trimmedDuration +
                (not data["is_adjusting_duration"].is_null() and data["is_adjusting_duration"]
                     ? data["adjust_duration"].get<int>()
                     : 0);
            auto trimmedStart2    = anteceeding.data(trimmedStartRole).toInt();
            auto trimmedDuration2 = anteceeding.data(trimmedDurationRole).toInt();
            auto startFrame2 = trimmedStart2 + (not data2["is_adjusting_start"].is_null() and
                                                        data2["is_adjusting_start"]
                                                    ? data2["adjust_start"].get<int>()
                                                    : 0);
            auto durationFrame2 =
                trimmedDuration2 +
                (not data2["is_adjusting_duration"].is_null() and data2["is_adjusting_duration"]
                     ? data2["adjust_duration"].get<int>()
                     : 0);

            setData(i, durationFrame, activeDurationRole);
            setData(anteceeding, startFrame2, activeStartRole);
            setData(anteceeding, durationFrame2, activeDurationRole);

            data2["is_adjusting_start"]    = false;
            data2["is_adjusting_duration"] = false;
            data2["adjust_start"]          = 0;
            data2["adjust_duration"]       = 0;
            data2["drag_value"]            = 0;

            if (orig_data2 != data2)
                setData(anteceeding, mapFromValue(data2), userDataRole);
        } else if (mode == "left") {
            auto trimmedStart    = i.data(trimmedStartRole).toInt();
            auto trimmedDuration = i.data(trimmedDurationRole).toInt();
            auto startFrame      = trimmedStart + (not data["is_adjusting_start"].is_null() and
                                                      data["is_adjusting_start"]
                                                       ? data["adjust_start"].get<int>()
                                                       : 0);
            auto durationFrame =
                trimmedDuration +
                (not data["is_adjusting_duration"].is_null() and data["is_adjusting_duration"]
                     ? data["adjust_duration"].get<int>()
                     : 0);
            setData(i, startFrame, activeStartRole);
            setData(i, durationFrame, activeDurationRole);

            // remove material we overwrote
            if (isOverwrite) {
                // we got smaller, either extend or insert gap
                if (durationFrame < trimmedDuration) {
                    auto gap = trimmedDuration - durationFrame;
                    auto fps = i.data(rateFPSRole).toDouble();

                    auto previous = index(i.row() - 1, 0, i.parent());

                    if (not previous.isValid() or
                        previous.data(typeRole) != QVariant::fromValue(QString("Gap"))) {
                        flushChange();
                        insertTimelineGap(i.row(), i.parent(), gap, fps, "Gap");
                    } else {
                        auto gduration = previous.data(trimmedDurationRole).toInt();
                        setData(previous, gduration + gap, activeDurationRole);
                        setData(previous, gduration + gap, availableDurationRole);
                    }
                } else if (durationFrame > trimmedDuration and i.row()) {
                    // expanding..
                    // delete preceeding
                    auto start =
                        std::max(0, startFrameInParent(i) - (durationFrame - trimmedDuration));
                    flushChange();
                    removeTimelineItems(
                        getTimelineTrackIndex(i), start, startFrameInParent(i) - start);
                }
            }

            if (data["is_adjusting_preceeding"]) {
                auto preceeding       = index(i.row() - 1, 0, i.parent());
                auto orig_data2       = mapFromValue(preceeding.data(userDataRole));
                auto data2            = orig_data2;
                auto trimmedDuration2 = preceeding.data(trimmedDurationRole).toInt();
                auto durationFrame2 =
                    trimmedDuration2 + (not data2["is_adjusting_duration"].is_null() and
                                                data2["is_adjusting_duration"]
                                            ? data2["adjust_duration"].get<int>()
                                            : 0);

                if (durationFrame2 == 0) {
                    flushChange();
                    removeTimelineItems(QModelIndexList({preceeding}));
                } else {
                    setData(preceeding, durationFrame2, activeDurationRole);
                    setData(preceeding, durationFrame2, availableDurationRole);
                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    if (orig_data2 != data2)
                        setData(preceeding, mapFromValue(data2), userDataRole);
                }
            } else if (data["adjust_preceeding_gap"] > 0) {
                auto gap = data["adjust_preceeding_gap"].get<int>();
                auto fps = i.data(rateFPSRole).toDouble();
                flushChange();
                insertTimelineGap(i.row(), i.parent(), gap, fps, "Gap");
            }
        } else if (mode == "right") {
            auto trimmedDuration = i.data(trimmedDurationRole).toInt();
            auto durationFrame =
                trimmedDuration +
                (not data["is_adjusting_duration"].is_null() and data["is_adjusting_duration"]
                     ? data["adjust_duration"].get<int>()
                     : 0);
            setData(i, durationFrame, activeDurationRole);

            // remove material we overwrote
            if (isOverwrite) {
                // only remove if we're not the last item.
                if (durationFrame > trimmedDuration and rowCount(i.parent()) - 1 != i.row()) {
                    flushChange();

                    removeTimelineItems(
                        getTimelineTrackIndex(i),
                        startFrameInParent(i) + durationFrame,
                        durationFrame - trimmedDuration);
                } else if (
                    durationFrame < trimmedDuration and rowCount(i.parent()) - 1 != i.row()) {
                    auto gap = trimmedDuration - durationFrame;
                    auto fps = i.data(rateFPSRole).toDouble();

                    // check next item isn't a gap..
                    auto next_item = index(i.row() + 1, 0, i.parent());
                    if (next_item.data(typeRole) == QVariant::fromValue(QString("Gap"))) {
                        auto gduration = next_item.data(trimmedDurationRole).toInt();
                        setData(next_item, gduration + gap, activeDurationRole);
                        setData(next_item, gduration + gap, availableDurationRole);
                    } else {
                        flushChange();
                        insertTimelineGap(i.row() + 1, i.parent(), gap, fps, "Gap");
                    }
                }
            }

            if (data["is_adjusting_anteceeding"]) {
                auto anteceeding      = index(i.row() + 1, 0, i.parent());
                auto orig_data2       = mapFromValue(anteceeding.data(userDataRole));
                auto data2            = orig_data2;
                auto trimmedDuration2 = anteceeding.data(trimmedDurationRole).toInt();
                auto durationFrame2 =
                    trimmedDuration2 + (not data2["is_adjusting_duration"].is_null() and
                                                data2["is_adjusting_duration"]
                                            ? data2["adjust_duration"].get<int>()
                                            : 0);

                if (durationFrame2 == 0) {
                    removeTimelineItems(QModelIndexList({anteceeding}));
                } else {
                    setData(anteceeding, durationFrame2, activeDurationRole);
                    setData(anteceeding, durationFrame2, availableDurationRole);

                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    if (orig_data2 != data2)
                        setData(anteceeding, mapFromValue(data2), userDataRole);
                }
            } else if (data["adjust_anteceeding_gap"] > 0) {
                auto gap = data["adjust_anteceeding_gap"].get<int>();
                auto fps = i.data(rateFPSRole).toDouble();

                insertTimelineGap(i.row() + 1, i.parent(), gap, fps, "Gap");
            }
        } else if (mode == "middle") {
            auto preceeding  = index(i.row() - 1, 0, i.parent());
            auto anteceeding = index(i.row() + 1, 0, i.parent());
            auto preceeding_type =
                preceeding.isValid() ? preceeding.data(typeRole).toString() : QString("Track");
            auto anteceeding_type = anteceeding.isValid()
                                        ? anteceeding.data(typeRole).toString()
                                        : QString("Track");

            if (isOverwrite) {
                auto frame_offset = data["move_x"].get<int>();
                auto track_offset = data["move_y"].get<int>();
                auto duration     = i.data(trimmedDurationRole).toInt();
                auto start        = startFrameInParent(i);

                // order of indexes is important,
                // check for correct order ..
                if (i == pitems.front()) {
                    auto sorted = QModelIndexList(items.begin(), items.end());

                    if (frame_offset > 0) {
                        std::sort(
                            sorted.begin(), sorted.end(), [](QModelIndex &a, QModelIndex &b) {
                                return a.row() > b.row();
                            });

                        if (sorted != items) {
                            endTimelineItemDrag(sorted, mode, isOverwrite);
                            return;
                        }
                    } else {
                        std::sort(
                            sorted.begin(), sorted.end(), [](QModelIndex &a, QModelIndex &b) {
                                return a.row() < b.row();
                            });

                        if (sorted != items) {
                            endTimelineItemDrag(sorted, mode, isOverwrite);
                            return;
                        }
                    }
                }

                // ouch...
                if (data["is_adjusting_preceeding"] and preceeding_type == QString("Gap")) {
                    auto data2 = mapFromValue(preceeding.data(userDataRole));
                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    setData(preceeding, mapFromValue(data2), userDataRole);
                }
                if (data["is_adjusting_anteceeding"] and anteceeding_type == QString("Gap")) {
                    auto data2 = mapFromValue(anteceeding.data(userDataRole));
                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    setData(anteceeding, mapFromValue(data2), userDataRole);
                }
                flushChange();

                // this involves a ton of track modifications..
                if (track_offset) {
                    auto target_track_index =
                        index(i.parent().row() + track_offset, 0, i.parent().parent());

                    if (track_offset < 0) {
                        for (auto tr = 0; tr >= track_offset; tr--) {
                            target_track_index =
                                index(i.parent().row() + tr, 0, i.parent().parent());

                            if (not target_track_index.isValid()) {
                                // create new track
                                auto type_role = i.parent().data(typeRole).toString();

                                target_track_index = insertRowsSync(
                                    type_role == "Video Track" ? 0
                                                               : rowCount(i.parent().parent()),
                                    1,
                                    type_role,
                                    type_role,
                                    i.parent().parent())[0];
                            }
                        }
                    } else if (track_offset > 0) {
                        for (auto tr = 0; tr <= track_offset; tr++) {
                            target_track_index =
                                index(i.parent().row() + tr, 0, i.parent().parent());

                            if (not target_track_index.isValid()) {
                                // create new track
                                auto type_role = i.parent().data(typeRole).toString();

                                target_track_index = insertRowsSync(
                                    type_role == "Video Track" ? 0
                                                               : rowCount(i.parent().parent()),
                                    1,
                                    type_role,
                                    type_role,
                                    i.parent().parent())[0];
                            }
                        }
                    }

                    auto new_clips =
                        duplicateTimelineClipsTo(QModelIndexList({i}), target_track_index);
                    moveRangeTimelineItems(
                        getTimelineTrackIndex(new_clips[0].parent()),
                        startFrameInParent(new_clips[0]),
                        duration,
                        start + frame_offset,
                        false);

                    removeTimelineItems(QModelIndexList({i}));
                    // wait for index to become invalidated..
                    while (i.isValid()) {
                        QCoreApplication::processEvents(
                            QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents,
                            50);
                    }

                } else if (frame_offset)
                    moveRangeTimelineItems(
                        getTimelineTrackIndex(i.parent()),
                        start,
                        duration,
                        start + frame_offset,
                        false);
            } else {
                auto delete_preceeding    = false;
                auto delete_anteceeding   = false;
                auto adjustPreceedingGap  = data["adjust_preceeding_gap"].get<int>();
                auto adjustAnteceedingGap = data["adjust_anteceeding_gap"].get<int>();
                auto insert_preceeding =
                    data["is_adjusting_preceeding"] and adjustPreceedingGap;
                auto insert_anteceeding =
                    data["is_adjusting_anteceeding"] and adjustAnteceedingGap;

                // adjust duration of preceeding gap or delete
                if (data["is_adjusting_preceeding"] and preceeding_type == QString("Gap")) {
                    auto trimmedDuration = preceeding.data(trimmedDurationRole).toInt();
                    auto data2           = mapFromValue(preceeding.data(userDataRole));
                    auto preceedingDurationFrame =
                        trimmedDuration + (not data2["is_adjusting_duration"].is_null() and
                                                   data2["is_adjusting_duration"]
                                               ? data2["adjust_duration"].get<int>()
                                               : 0);
                    if (preceedingDurationFrame) {
                        setData(preceeding, preceedingDurationFrame, activeDurationRole);
                        setData(preceeding, preceedingDurationFrame, availableDurationRole);
                    } else {
                        delete_preceeding = true;
                    }

                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    setData(preceeding, mapFromValue(data2), userDataRole);
                }

                // adjust duration of anteceeding gap or delete
                if (data["is_adjusting_anteceeding"] and anteceeding_type == QString("Gap")) {
                    auto trimmedDuration = anteceeding.data(trimmedDurationRole).toInt();
                    auto data2           = mapFromValue(anteceeding.data(userDataRole));
                    auto anteceedingDurationFrame =
                        trimmedDuration + (not data2["is_adjusting_duration"].is_null() and
                                                   data2["is_adjusting_duration"]
                                               ? data2["adjust_duration"].get<int>()
                                               : 0);
                    if (anteceedingDurationFrame) {
                        setData(anteceeding, anteceedingDurationFrame, activeDurationRole);
                        setData(anteceeding, anteceedingDurationFrame, availableDurationRole);
                    } else {
                        delete_anteceeding = true;
                    }
                    data2["is_adjusting_duration"] = false;
                    data2["adjust_duration"]       = 0;
                    setData(anteceeding, mapFromValue(data2), userDataRole);
                }

                flushChange();
                // spdlog::warn("insert_preceeding {} delete_preceeding {} insert_anteceeding {}
                // delete_anteceeding {}", insert_preceeding, delete_preceeding,
                // insert_anteceeding, delete_anteceeding );

                // some operations are moves
                if (insert_preceeding and delete_anteceeding)
                    moveTimelineItem(i, 1);
                else if (delete_preceeding and insert_anteceeding)
                    moveTimelineItem(i, -1);
                else {
                    if (delete_preceeding)
                        removeTimelineItems(QModelIndexList({preceeding}));

                    if (delete_anteceeding)
                        removeTimelineItems(QModelIndexList({anteceeding}));

                    if (insert_preceeding) {
                        auto fps = i.data(rateFPSRole).toDouble();
                        insertTimelineGap(i.row(), i.parent(), adjustPreceedingGap, fps, "Gap");
                    }

                    if (insert_anteceeding) {
                        auto fps = i.data(rateFPSRole).toDouble();
                        insertTimelineGap(
                            i.row() + 1, i.parent(), adjustAnteceedingGap, fps, "Gap");
                    }
                }
            }
        }

        flushChange();
    }
}


void SessionModel::draggingAdjust(const QModelIndex &item, const int frameChange) {
    if (item.isValid()) {
        auto orig_data = mapFromValue(item.data(userDataRole));
        auto data      = orig_data;

        auto type_role = item.data(typeRole).toString();
        if (type_role == QString("Clip")) {
            auto doffset = frameChange;

            if (not data["is_adjusting_start"].is_null() and data["is_adjusting_start"]) {
                data["adjust_start"] = frameChange;
                data["drag_value"]   = frameChange;
                doffset              = -doffset;
            }

            if (not data["is_adjusting_duration"].is_null() and data["is_adjusting_duration"]) {
                data["adjust_duration"] = doffset;
                data["drag_value"]      = doffset;
            }
        } else if (type_role == QString("Gap")) {
            data["adjust_duration"] = frameChange;
        }

        if (orig_data != data)
            setData(item, mapFromValue(data), userDataRole);
    }
}

int SessionModel::checkAdjust(
    const QModelIndex &item,
    const int frameChange,
    const bool lockDuration,
    const bool lockEnd) {
    auto result = frameChange;

    if (item.isValid()) {
        auto type_role = item.data(typeRole).toString();
        auto data      = mapFromValue(item.data(userDataRole));

        if (type_role == QString("Clip")) {
            auto trimmedStart      = item.data(trimmedStartRole).toInt();
            auto trimmedDuration   = item.data(trimmedDurationRole).toInt();
            auto availableStart    = item.data(availableStartRole).toInt();
            auto availableDuration = item.data(availableDurationRole).toInt();

            auto doffset = frameChange;

            if (not data["is_adjusting_start"].is_null() and data["is_adjusting_start"]) {
                auto tmp = std::min(
                    availableStart + availableDuration - 1,
                    std::max(trimmedStart + frameChange, availableStart));

                if (lockEnd and tmp > trimmedStart + trimmedDuration)
                    tmp = trimmedStart + trimmedDuration - 1;

                if (trimmedStart != tmp - frameChange)
                    return checkAdjust(item, tmp - trimmedStart);

                // if adjusting duration as well
                doffset = -doffset;
            }


            if (lockDuration and not data["is_adjusting_duration"].is_null() and
                data["is_adjusting_duration"]) {
                auto startFrame =
                    not data["is_adjusting_start"].is_null() and data["is_adjusting_start"]
                        ? data["adjust_start"].get<int>()
                        : trimmedStart;
                auto tmp = std::max(
                    1,
                    std::min(
                        trimmedDuration + doffset,
                        availableDuration - (startFrame - availableStart)));

                if (trimmedDuration != tmp - doffset) {
                    if (not data["is_adjusting_start"].is_null() and data["is_adjusting_start"])
                        return checkAdjust(item, -(tmp - trimmedDuration));
                    else
                        return checkAdjust(item, tmp - trimmedDuration);
                }
            }
        } else if (type_role == QString("Gap")) {
            auto trimmedDuration = item.data(trimmedDurationRole).toInt();
            auto tmp             = std::max(0, trimmedDuration + frameChange);

            if (trimmedDuration != tmp - frameChange)
                result = checkAdjust(item, tmp - trimmedDuration);
        }
    }

    return result;
}
