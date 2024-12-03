// SPDX-License-Identifier: Apache-2.0

#include "xstudio/session/session_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"
#include "xstudio/ui/qml/caf_response_ui.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSignalSpy>
#include <QImage>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;


SessionModel::SessionModel(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    auto role_names = std::vector<std::string>(
        {{"activeDurationRole"},
         {"activeRangeValidRole"},
         {"activeStartRole"},
         {"actorRole"},
         {"actorUuidRole"},
         {"audioActorUuidRole"},
         {"availableDurationRole"},
         {"availableStartRole"},
         {"bitDepthRole"},
         {"bookmarkUuidsRole"},
         {"busyRole"},
         {"clipMediaUuidRole"},
         {"containerUuidRole"},
         {"enabledRole"},
         {"errorRole"},
         {"expandedRole"},
         {"flagColourRole"},
         {"flagTextRole"},
         {"formatRole"},
         {"groupActorRole"},
         {"imageActorUuidRole"},
         {"lockedRole"},
         {"markersRole"},
         {"mediaCountRole"},
         {"mediaDisplayInfoRole"},
         {"mediaStatusRole"},
         {"metadataChangedRole"},
         {"mtimeRole"},
         {"nameRole"},
         {"notificationRole"},
         {"pathRole"},
         {"pathShakeRole"},
         {"pixelAspectRole"},
         {"placeHolderRole"},
         {"propertyRole"},
         {"rateFPSRole"},
         {"resolutionRole"},
         {"selectionRole"},
         {"thumbnailImageRole"},
         {"thumbnailURLRole"},
         {"timecodeAsFramesRole"},
         {"trackIndexRole"},
         {"trimmedDurationRole"},
         {"trimmedStartRole"},
         {"typeRole"},
         {"uuidRole"},
         {"userDataRole"}});

    setRoleNames(role_names);
    request_handler_ = new QThreadPool(this);
    request_handler_->setMaxThreadCount(8);
}

void SessionModel::fetchMore(const QModelIndex &parent) {
    try {
        if (canFetchMore(parent)) {
            const auto &j = indexToData(parent);
            requestData(
                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                idRole,
                parent,
                parent,
                JSONTreeModel::Roles::childrenRole);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


QModelIndexList SessionModel::searchRecursiveListBase(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {

    QModelIndexList results;
    auto cached_result = true;

    if (role == idRole) {
        auto uuid = UuidFromQUuid(value.toUuid());
        if (uuid.is_null()) {
            return QModelIndexList();
        }

        auto it = id_uuid_lookup_.find(uuid);
        if (it != std::end(id_uuid_lookup_)) {
            // spdlog::info("found {}", to_string(uuid));
            for (auto iit = std::begin(it->second); iit != std::end(it->second);) {
                if (iit->isValid()) {
                    results.push_back(*iit);
                    iit++;
                    // spdlog::info("is valid");
                } else {
                    // spdlog::info("isn't valid");
                    iit = it->second.erase(iit);
                }
            }
        } else {
            // spdlog::warn("not cached idRole");
            cached_result = false;
            results =
                JSONTreeModel::searchRecursiveListBase(value, role, parent, start, hits, depth);
            for (const auto &i : results) {
                add_id_uuid_lookup(uuid, i);
            }
        }
    } else if (role == actorUuidRole or role == containerUuidRole) {
        auto uuid = UuidFromQUuid(value.toUuid());
        if (uuid.is_null()) {
            return QModelIndexList();
        }

        // if (role == actorUuidRole) {
        auto it = uuid_lookup_.find(uuid);
        if (it != std::end(uuid_lookup_)) {
            // spdlog::info("found {}", to_string(uuid));
            for (auto iit = std::begin(it->second); iit != std::end(it->second);) {
                if (iit->isValid()) {
                    results.push_back(*iit);
                    iit++;
                    // spdlog::info("is valid");
                } else {
                    // spdlog::info("isn't valid");
                    iit = it->second.erase(iit);
                }
            }
        } else {
            // spdlog::warn("not cached actorUuidRole / containerUuidRole");
            cached_result = false;
            results =
                JSONTreeModel::searchRecursiveListBase(value, role, parent, start, hits, depth);
            for (const auto &i : results) {
                add_uuid_lookup(uuid, i);
            }
        }
        // }
    } else if (role == actorRole) {
        auto str = StdFromQString(value.toString());

        if (str.empty())
            return QModelIndexList();

        auto it = string_lookup_.find(str);
        if (it != std::end(string_lookup_)) {
            for (auto iit = std::begin(it->second); iit != std::end(it->second);) {
                if (iit->isValid()) {
                    results.push_back(*iit);
                    iit++;
                } else {
                    iit = it->second.erase(iit);
                }
            }
        } else {
            //  back populate..
            cached_result = false;
            // spdlog::warn("not cached actorRole {}",
            // StdFromQString(parent.data(typeRole).toString()));
            results =
                JSONTreeModel::searchRecursiveListBase(value, role, parent, start, hits, depth);
            for (const auto &i : results) {
                add_string_lookup(str, i);
            }
        }
    } else {
        // spdlog::error("No lookup {}", StdFromQString(roleName(role)));
    }


    if (results.empty()) {
        cached_result = false;
        results =
            JSONTreeModel::searchRecursiveListBase(value, role, parent, start, hits, depth);
    } else if (cached_result) {
        // spdlog::info("have cached result {} {}", parent.isValid(), depth);

        // make sure results exist under parent..
        if (parent.isValid()) {
            for (auto it = results.begin(); it != results.end();) {
                if (not isChildOf(parent, *it)) {
                    it = results.erase(it);
                } else
                    it++;
            }
        }

        if (depth != -1) {
            for (auto it = results.begin(); it != results.end();) {
                if (depthOfChild(parent, *it) > depth) {
                    it = results.erase(it);
                } else
                    it++;
            }
        }
    }

    return results;
}

int SessionModel::depthOfChild(const QModelIndex &parent, const QModelIndex &child) const {
    auto depth = 0;

    auto p = child.parent();
    while (p.isValid()) {
        if (p == parent)
            return depth;
        p = p.parent();
        depth++;
    }

    if (not parent.isValid())
        return depth;

    return -1;
}


bool SessionModel::isChildOf(const QModelIndex &parent, const QModelIndex &child) const {
    auto result = false;

    auto p = child;
    while (p.isValid()) {
        if (p == parent) {
            result = true;
            break;
        }
        p = p.parent();
    }

    return result;
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    START_SLOW_WATCHER()

    // spdlog::warn("SessionModel::data {} {}", index.row(), StdFromQString(roleName(role)));

    // spdlog::warn("SessionModel::data {} {} {}", index.row(), indexToData(index).dump(2),
    // StdFromQString(roleName(role)));

    try {
        if (index.isValid()) {
            const auto &j = indexToData(index);

            switch (role) {
            case Roles::typeRole:
                if (j.count("type"))
                    result = QString::fromStdString(j.at("type"));
                break;

            case Roles::mediaCountRole:
                if (j.count("media_count"))
                    result = QVariant::fromValue(j.at("media_count").get<int>());
                break;

            case Roles::errorRole:
                if (j.count("error_count"))
                    result = QVariant::fromValue(j.at("error_count").get<int>());
                break;

            case JSONTreeModel::Roles::idRole:
                if (j.count("id"))
                    result = QVariant::fromValue(QUuidFromUuid(j.at("id")));
                break;

            case Roles::placeHolderRole:
                if (j.count("placeholder"))
                    result = QVariant::fromValue(j.at("placeholder").get<bool>());
                else
                    result = QVariant::fromValue(false);
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
                            index,
                            role);
                    } else {
                        auto uri = caf::make_uri(j.at("path"));
                        if (uri)
                            result = QVariant::fromValue(QUrlFromUri(*uri));
                    }
                }
                break;

                // case Roles::metadataChangedRole: {
                //         auto tmp = Uuid::generate();
                //         result = QVariant::fromValue(QUuidFromUuid(tmp));
                //     }
                //     break;

            case Roles::pathShakeRole:
                if (j.count("path_shake")) {
                    if (j.at("path_shake").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        auto uri = caf::make_uri(j.at("path_shake"));
                        if (uri)
                            result = QVariant::fromValue(QUrlFromUri(*uri));
                    }
                }
                break;

            case Roles::timecodeAsFramesRole:
                if (j.count("timecode_as_frames")) {
                    if (j.at("timecode_as_frames").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        result = QVariant::fromValue(j.at("timecode_as_frames").get<int>());
                    }
                }
                break;

            case Roles::bookmarkUuidsRole: {
                auto type = j.value("type", "");
                if (type == "Media" && j.count("bookmark_uuids")) {
                    if (j.at("bookmark_uuids").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        const auto &uuids = j.at("bookmark_uuids");
                        if (uuids.is_array()) {
                            QList<QVariant> rt;
                            for (auto p = uuids.begin(); p != uuids.end(); ++p) {
                                rt.append(QVariant(QUuidFromUuid((*p).get<utility::Uuid>())));
                            }
                            result = QVariant(rt);
                        } else {
                            result = json_to_qvariant(uuids);
                        }
                    }
                }
            } break;

            case Roles::trackIndexRole: {
                auto type = j.value("type", "");
                if (type == "Audio Track" or type == "Video Track") {
                    // get parent.
                    const auto pj = indexToFullData(index.parent(), 1);
                    const auto id = j.value("id", "");

                    auto vcount = 0;
                    auto acount = 0;
                    auto tindex = 0;
                    auto found  = false;

                    for (const auto &i : pj.at("children")) {
                        auto ttype = i.value("type", "");
                        auto tid   = i.value("id", "");
                        if (ttype == "Video Track") {
                            if (tid == id)
                                found = true;
                            if (not found and type == ttype)
                                tindex++;
                            vcount++;
                        } else {
                            if (tid == id)
                                found = true;
                            if (not found and type == ttype)
                                tindex++;
                            acount++;
                        }
                    }
                    if (type == "Audio Track")
                        result = tindex + 1;
                    else
                        result = vcount - tindex;
                }
            } break;

            case Roles::rateFPSRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(0.0);
                } else if (j.count("rate")) {
                    if (j.at("rate").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        result = QVariant::fromValue(j.at("rate").get<FrameRate>().to_fps());
                    }
                } else if (j.count("active_range") or j.count("available_range")) {
                    // timeline..
                    if (j.count("active_range") and j.at("active_range").is_object()) {
                        auto fr = j.value("active_range", FrameRange());
                        result  = QVariant::fromValue(fr.rate().to_fps());
                    } else if (
                        j.count("available_range") and j.at("available_range").is_object()) {
                        auto fr = j.value("available_range", FrameRange());
                        result  = QVariant::fromValue(fr.rate().to_fps());
                    } else if (j.count("available_range") or j.count("active_range")) {
                        result = QVariant::fromValue(0.0);
                    }
                }
                break;

            case Roles::bitDepthRole:
                if (j.count("bit_depth")) {
                    if (j.at("bit_depth").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    } else {
                        result = QString::fromStdString(j.at("bit_depth"));
                    }
                }
                break;

            case clipMediaUuidRole:
                if (j.count("prop") and j.at("prop").count("media_uuid")) {
                    result = QVariant::fromValue(QUuidFromUuid(j.at("prop").at("media_uuid")));
                }
                break;

            case propertyRole:
                if (j.count("prop")) {
                    result = QVariant::fromValue(mapFromValue(j.at("prop")));
                }
                break;

            case notificationRole:
                if (j.count("notification")) {
                    auto type = j.value("type", "");
                    if (j.at("notification").is_null()) {
                        if (type == "Audio Track" or type == "Video Track")
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                index,
                                role);
                        else
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                                actorUuidRole,
                                getPlaylistIndex(index),
                                index,
                                role);
                    } else {
                        result = QVariant::fromValue(mapFromValue(j.at("notification")));
                    }
                }
                break;

            case Roles::resolutionRole:
                if (j.count("resolution")) {
                    if (j.at("resolution").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    } else {
                        result = QString::fromStdString(j.at("resolution"));
                    }
                }
                break;

            case Roles::pixelAspectRole:
                if (j.count("pixel_aspect")) {
                    if (j.at("pixel_aspect").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    } else {
                        result = QVariant::fromValue(j.at("pixel_aspect").get<double>());
                    }
                }
                break;

            case Roles::formatRole:
                if (j.count("format")) {
                    if (j.at("format").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    } else {
                        result = QString::fromStdString(j.at("format"));
                    }
                }
                break;

            case Roles::mtimeRole:
                if (j.count("mtime") and not j.at("mtime").is_null()) {
                    result = QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            j.at("mtime").get<fs::file_time_type>().time_since_epoch())
                            .count()));
                }
                break;

            case Roles::mediaStatusRole:
                if (j.count("media_status")) {
                    if (j.at("media_status").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    else
                        result = QVariant::fromValue(QStringFromStd(to_string(
                            static_cast<media::MediaStatus>(j.at("media_status").get<int>()))));
                }
                break;

            case Roles::mediaDisplayInfoRole: {
                if (j.count("media_display_info")) {

                    if (j.at("media_display_info").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            getPlaylistIndex(index),
                            index,
                            role);
                    else
                        result = json_to_qvariant(j.at("media_display_info"));
                }
            } break;

            case Roles::audioActorUuidRole:
                if (j.count("audio_actor_uuid")) {
                    if (j.at("audio_actor_uuid").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
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
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("actor"));
                } else if (j.count("actor_owner")) {
                    if (j.at("actor_owner").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("actor_owner"));
                }

                break;

            case Roles::groupActorRole:
                if (j.count("group_actor")) {
                    if (j.at("group_actor").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
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
                            index,
                            role);
                    else {
                        result = QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid")));
                    }
                }
                break;

            case Roles::uuidRole:
                if (j.count("uuid"))
                    result = QVariant::fromValue(QUuidFromUuid(j.at("uuid")));
                break;
            case Roles::expandedRole:
                if (j.count("expanded")) {
                    if (j.at("expanded").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("actor_uuid"))),
                            actorUuidRole,
                            index,
                            index,
                            role);
                    else {
                        result = json_to_qvariant(j.at("expanded"));
                    }
                }
                break;
            case Roles::thumbnailImageRole: {
                auto type = j.value("type", "");
                if (type == "Media" && j.count("actor") and not j.at("actor").is_null()) {
                    auto actor_str = j.at("actor");
                    auto p         = media_thumbnails_.find(QStringFromStd(actor_str));
                    if (p != media_thumbnails_.end()) {
                        result = p.value();
                    } else {
                        auto actor = actorFromString(system(), actor_str);
                        if (actor)
                            anon_send(actor, media_reader::get_thumbnail_atom_v, 0.5f);
                    }
                }
            } break;

            case Roles::thumbnailURLRole:
                if (j.count("thumbnail_url")) {
                    if (j.at("thumbnail_url").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
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
                if (j.count("container_uuid") and not j.at("container_uuid").is_null())
                    result = QVariant::fromValue(QUuidFromUuid(j.at("container_uuid")));
                break;

            case Roles::selectionRole:
                if (j.count("playhead_selection")) {
                    if (j.at("playhead_selection").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        const auto &uuids = j.at("playhead_selection");
                        if (uuids.is_array()) {
                            QList<QVariant> rt;
                            for (auto p = uuids.begin(); p != uuids.end(); ++p) {
                                rt.append(QVariant(QUuidFromUuid((*p).get<utility::Uuid>())));
                            }
                            result = QVariant(rt);
                        } else {
                            result = json_to_qvariant(uuids);
                        }
                    }
                }
                break;

            case Roles::flagColourRole:
                if (j.count("placeholder")) {
                    result = QString("");
                } else if (j.count("flag")) {
                    if (j.at("flag").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else {
                        result = QString::fromStdString(j.at("flag"));
                    }
                } else if (j.count("type") and j.at("type") == "MediaSource") {
                    // helper for media Sources.
                    result = index.parent().data(role);
                }
                break;

            case Roles::flagTextRole:
                if (j.count("flag_text")) {
                    if (j.at("flag_text").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else
                        result = QString::fromStdString(j.at("flag_text"));
                } else if (j.count("type") and j.at("type") == "MediaSource") {
                    // helper for media Sources.
                    result = index.parent().data(role);
                }
                break;


            case Roles::enabledRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(true);
                } else if (j.count("enabled")) {
                    result = QVariant::fromValue(j.value("enabled", true));
                }
                break;

            case Roles::lockedRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(false);
                } else if (j.count("locked")) {
                    result = QVariant::fromValue(j.value("locked", false));
                }
                break;

            case Roles::markersRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(mapFromValue(R"([])"_json));
                } else if (j.count("markers")) {
                    result =
                        QVariant::fromValue(mapFromValue(j.value("markers", R"([])"_json)));
                }
                break;


            case Roles::activeRangeValidRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(true);
                } else if (
                    j.count("active_range") and j.at("active_range").is_object() and
                    j.count("available_range") and j.at("available_range").is_object()) {
                    try {
                        const auto active = j.value("active_range", FrameRange());
                        const auto avail  = j.value("available_range", FrameRange());
                        result = QVariant::fromValue(avail.intersect(active) == active);
                    } catch (...) {
                        result = QVariant::fromValue(true);
                    }
                } else {
                    result = QVariant::fromValue(true);
                }
                break;

            case Roles::userDataRole:
                if (j.count("user_data"))
                    result = mapFromValue(j.at("user_data"));
                else
                    result = mapFromValue(R"({})"_json);
                break;

            case Roles::trimmedStartRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(0);
                } else if (j.count("active_range") and j.at("active_range").is_object()) {
                    auto fr = j.value("active_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_start().frames());
                } else if (j.count("available_range") and j.at("available_range").is_object()) {
                    auto fr = j.value("available_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_start().frames());
                } else if (j.count("available_range") or j.count("active_range")) {
                    result = QVariant::fromValue(0);
                }
                break;

                // case Roles::parentStartRole:
                //     // requires access to parent item.
                //     if (j.count("placeholder")) {
                //         result = QVariant::fromValue(0);
                //     } else if (j.count("active_range")) {
                //         spdlog::warn("PARENTSTARTROLE {}", index.row());
                //         auto p = index.parent();
                //         auto t = getTimelineIndex(index);

                //         if (p.isValid() and t.isValid()) {
                //             auto tactor = actorFromIndex(t);
                //             auto puuid  = UuidFromQUuid(p.data(idRole).toUuid());

                //             if (timeline_lookup_.count(tactor)) {
                //                 auto pitem = timeline::find_item(
                //                     timeline_lookup_.at(tactor).children(), puuid);
                //                 if (pitem)
                //                     result =
                //                         QVariant::fromValue((*pitem)->frame_at_index(index.row()));
                //             }
                //         } else
                //             result = QVariant::fromValue(0);
                //     }
                //     break;

            case Roles::trimmedDurationRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(0);
                } else if (j.count("active_range") and j.at("active_range").is_object()) {
                    auto fr = j.value("active_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_duration().frames());
                } else if (j.count("available_range") and j.at("available_range").is_object()) {
                    auto fr = j.value("available_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_duration().frames());
                } else if (j.count("available_range") or j.count("active_range")) {
                    result = QVariant::fromValue(0);
                }
                break;

            case Roles::activeStartRole:
                if (j.count("active_range")) {
                    if (j.at("active_range").is_object()) {
                        auto fr = j.value("active_range", FrameRange());
                        result  = QVariant::fromValue(fr.frame_start().frames());
                    } else {
                        result = QVariant::fromValue(0);
                    }
                }
                break;


            case Roles::activeDurationRole:
                if (j.count("active_range")) {
                    if (j.at("active_range").is_object()) {
                        auto fr = j.value("active_range", FrameRange());
                        result  = QVariant::fromValue(fr.frame_duration().frames());
                    } else {
                        result = QVariant::fromValue(0);
                    }
                }
                break;

            case Roles::availableDurationRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(0);
                } else if (j.count("available_range")) {
                    if (j.at("available_range").is_object()) {
                        auto fr = j.value("available_range", FrameRange());
                        result  = QVariant::fromValue(fr.frame_duration().frames());
                    } else {
                        result = QVariant::fromValue(0);
                    }
                }
                break;

            case Roles::availableStartRole:
                if (j.count("placeholder")) {
                    result = QVariant::fromValue(0);
                } else if (j.count("available_range")) {
                    if (j.at("available_range").is_object()) {
                        auto fr = j.value("available_range", FrameRange());
                        result  = QVariant::fromValue(fr.frame_start().frames());
                    } else {
                        result = QVariant::fromValue(0);
                    }
                }
                break;

            case Qt::DisplayRole:
            case Roles::nameRole:
                if (j.count("placeholder")) {
                    result = QString("");
                } else if (j.count("name")) {
                    if (j.at("name").is_null())
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    else
                        result = QString::fromStdString(j.at("name"));
                }
                break;
            case JSONTreeModel::Roles::childrenRole:
                if (j.count("children")) {
                    if (j.at("children").is_null()) {
                        // stop more requests being sent..
                        // j["children"] = R"([])"_json;
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else
                        result = QVariantMapFromJson(j.at("children"));
                }
                break;
            default: {
                result = JSONTreeModel::data(index, role);
            } break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            StdFromQString(roleName(role)),
            index.row());
    }

    CHECK_SLOW_WATCHER()

    return result;
}


bool SessionModel::setData(const QModelIndex &index, const QVariant &qvalue, int role) {
    bool result = false;
    QVector<int> roles({role});
    START_SLOW_WATCHER()

    try {
        if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            auto value        = mapFromValue(qvalue);
            auto type  = j.count("type") ? j.at("type").get<std::string>() : std::string();
            auto actor = j.count("actor") and not j.at("actor").is_null()
                             ? actorFromString(system(), j.at("actor"))
                             : (j.count("actor_owner") and not j.at("actor_owner").is_null()
                                    ? actorFromString(system(), j.at("actor_owner"))
                                    : caf::actor());

            switch (role) {

            case busyRole:
                if (j.count("busy") and j["busy"] != value) {
                    j["busy"] = value;
                    result    = true;
                }
                break;

            case metadataChangedRole:
                result = true;
                break;

            case errorRole:
                if (j.count("error_count") and j["error_count"] != value) {
                    j["error_count"] = value;
                    result           = true;
                }
                break;

            case idRole:
                if (j.count("id") and j["id"] != value) {
                    j["id"] = value;
                    result  = true;
                }
                break;


            case Roles::clipMediaUuidRole:
                if (type == "Clip") {
                    // find media actor..
                    auto timeline = getTimelineIndex(index);
                    // find media in timeline..
                    auto media_index = searchRecursive(qvalue, actorUuidRole, timeline);

                    if (media_index.isValid()) {
                        nlohmann::json &mj = indexToData(media_index);
                        auto ua            = UuidActor(
                            value,
                            mj.count("actor") and not mj.at("actor").is_null()
                                           ? actorFromString(system(), mj.at("actor"))
                                           : (mj.count("actor_owner") and
                                           not mj.at("actor_owner").is_null()
                                                  ? actorFromString(system(), mj.at("actor_owner"))
                                                  : caf::actor()));

                        if (ua.actor()) {
                            j["prop"]["media_uuid"] = value;

                            if (actor)
                                anon_send(actor, timeline::link_media_atom_v, ua);
                            result = true;
                        }
                    }
                }
                break;

            case Roles::userDataRole:
                j["user_data"] = value;
                result         = true;
                break;

            case activeStartRole:
                if (j.count("active_range")) {
                    auto fr = FrameRange();
                    // has range adjust duration..
                    if (j.at("active_range").is_object()) {
                        fr = j.value("active_range", FrameRange());
                        if (fr.frame_start().frames() != value) {
                            fr.set_start(FrameRate(fr.rate() * value.get<int>()));
                            result = true;
                        }
                    } else {
                        fr = j.value("available_range", FrameRange());
                        if (fr.frame_start().frames() != value) {
                            fr.set_start(FrameRate(fr.rate() * value.get<int>()));
                            result = true;
                        }
                    }

                    if (result) {
                        j["active_range"] = fr;
                        // probably pointless, as this will trigger from the backend update
                        roles.push_back(trimmedStartRole);
                        roles.push_back(activeRangeValidRole);
                        if (actor)
                            anon_send(actor, timeline::active_range_atom_v, fr);
                    }
                }
                break;

            case Roles::markersRole:
                if (j.count("markers")) {
                    if (j.at("markers") != value.at("children")) {
                        j["markers"] = value.at("children");
                        result       = true;
                        if (actor) {
                            auto markers = std::vector<timeline::Marker>();
                            for (const auto &i : j.at("markers"))
                                markers.emplace_back(timeline::Marker(JsonStore(i)));
                            anon_send(actor, timeline::item_marker_atom_v, markers);
                        }
                    }
                }
                break;

            case propertyRole:
                if (j.count("prop")) {
                    if (j.at("prop") != value) {
                        j["prop"] = value;
                        result    = true;
                        roles.push_back(clipMediaUuidRole);
                        if (actor)
                            anon_send(actor, timeline::item_prop_atom_v, JsonStore(value));
                    }
                }
                break;

            case activeDurationRole:
                if (j.count("active_range")) {
                    auto fr = FrameRange();
                    // has range adjust duration..
                    if (j.at("active_range").is_object()) {
                        fr = j.value("active_range", FrameRange());
                        if (fr.frame_duration().frames() != value) {
                            fr.set_duration(FrameRate(fr.rate() * value.get<int>()));
                            result = true;
                        }
                    } else {
                        fr = j.value("available_range", FrameRange());
                        if (fr.frame_duration().frames() != value) {
                            fr.set_duration(FrameRate(fr.rate() * value.get<int>()));
                            result = true;
                        }
                    }

                    if (result) {
                        j["active_range"] = fr;
                        // probably pointless, as this will trigger from the backend update
                        roles.push_back(trimmedDurationRole);
                        roles.push_back(activeRangeValidRole);
                        if (actor)
                            anon_send(actor, timeline::active_range_atom_v, fr);
                    }
                }
                break;

            case availableStartRole:
                if (j.count("available_range")) {
                    auto fr = j.value("available_range", FrameRange());
                    if (fr.frame_start().frames() != value) {
                        fr.set_start(FrameRate(fr.rate() * value.get<int>()));
                        result = true;
                    }

                    if (result) {
                        j["available_range"] = fr;
                        // probably pointless, as this will trigger from the backend update
                        roles.push_back(trimmedStartRole);
                        roles.push_back(activeRangeValidRole);

                        if (actor)
                            anon_send(actor, timeline::available_range_atom_v, fr);
                    }
                }
                break;

            case availableDurationRole:
                if (j.count("available_range")) {
                    auto fr = j.value("available_range", FrameRange());
                    if (fr.frame_duration().frames() != value) {
                        fr.set_duration(FrameRate(fr.rate() * value.get<int>()));
                        result = true;
                    }

                    if (result) {
                        j["available_range"] = fr;
                        // probably pointless, as this will trigger from the backend update
                        roles.push_back(trimmedDurationRole);
                        roles.push_back(activeRangeValidRole);
                        if (actor)
                            anon_send(actor, timeline::available_range_atom_v, fr);
                    }
                }
                break;

            case enabledRole:
                if (j.count("enabled") and j["enabled"] != value) {
                    j["enabled"] = value;
                    result       = true;
                    if (type == "Clip" or type == "Gap" or type == "Audio Track" or
                        type == "Video Track" or type == "Stack") {
                        anon_send(actor, plugin_manager::enable_atom_v, value.get<bool>());
                    }
                }
                break;

            case lockedRole:
                if (j.count("locked") and j["locked"] != value) {
                    j["locked"] = value;
                    result      = true;
                    if (type == "Clip" or type == "Gap" or type == "Audio Track" or
                        type == "Video Track" or type == "Stack" or type == "Timeline") {
                        anon_send(actor, timeline::item_lock_atom_v, value.get<bool>());
                    }
                }
                break;

            case mediaCountRole:
                if (j.count("media_count") and j.at("media_count") != value) {
                    j["media_count"] = value;
                    result           = true;
                }
                break;

            case placeHolderRole:
                if (j.count("placeholder") and j.at("placeholder") != value) {
                    j["placeholder"] = value;
                    result           = true;
                }
                break;

            case containerUuidRole:
                if (j.count("container_uuid") and j.at("container_uuid") != value) {
                    j["container_uuid"] = value;
                    add_lookup(*indexToTree(index), index);
                    result = true;
                }
                break;
            case actorUuidRole:
                if (j.count("actor_uuid") and j.at("actor_uuid") != value) {
                    j["actor_uuid"] = value;
                    add_lookup(*indexToTree(index), index);
                    result = true;
                }
                break;

            case actorRole:
                if (j.count("actor") and j.at("actor") != value) {
                    j["actor"] = value;
                    add_lookup(*indexToTree(index), index);
                    result = true;
                }
                break;

            case thumbnailURLRole:
                if (j.count("thumnail_url") and j.at("thumnail_url") != value) {
                    j["thumnail_url"] = value;
                    result            = true;
                }
                break;

            case imageActorUuidRole:
                if (j.count("image_actor_uuid") and j["image_actor_uuid"] != value) {
                    j["image_actor_uuid"] = value;
                    result                = true;
                    if (type == "MediaSource") {
                        anon_send(
                            actor,
                            media::current_media_stream_atom_v,
                            media::MT_IMAGE,
                            j["image_actor_uuid"].get<utility::Uuid>());
                    } else if (type == "Media") {
                        anon_send(
                            actor,
                            media::current_media_source_atom_v,
                            j["image_actor_uuid"].get<utility::Uuid>(),
                            media::MT_IMAGE);
                    }
                }
                break;

            case rateFPSRole:
                if (j.count("rate") and actor) {
                    double fps = 0.1;

                    if (value.is_string())
                        fps = std::stod(value.get<std::string>());
                    else if (value.is_number_integer())
                        fps = static_cast<double>(value.get<int>());
                    else
                        fps = value.get<double>();

                    auto fr = FrameRate(1.0 / (fps > 0.1 ? fps : 0.1));
                    auto r  = json(fr);

                    if (j["rate"] != r) {
                        // spdlog::warn("rateFPSRole {} {}", j["rate"].dump(), r.dump());
                        j["rate"] = r;
                        result    = true;

                        if (type == "MediaSource") {
                            scoped_actor sys{system()};

                            auto mr = request_receive<MediaReference>(
                                *sys, actor, media::media_reference_atom_v);
                            mr.set_rate(fr);
                            anon_send(actor, media::media_reference_atom_v, mr);
                        } else if (type == "Session") {
                            anon_send(session_actor_, session::media_rate_atom_v, fr);
                        }
                    }
                }
                break;

            case nameRole:
                if (j.count("name") and j["name"] != value) {
                    if ((type == "Session" or type == "ContactSheet" or type == "Subset" or type == "Timeline" or
                         type == "Playlist") and
                        actor) {
                        // spdlog::warn("Send update {} {}", j["name"], value);
                        anon_send(actor, utility::name_atom_v, value.get<std::string>());
                        j["name"] = value;
                        result    = true;
                        emit playlistsChanged();

                    } else if (type == "ContainerDivider") {
                        auto p = index.parent();
                        if (p.isValid()) {
                            nlohmann::json &pj = indexToData(p);
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
                                // spdlog::warn("Send update {} {}", j["name"], value);
                                anon_send(
                                    pactor,
                                    playlist::rename_container_atom_v,
                                    value.get<std::string>(),
                                    j.at("container_uuid").get<Uuid>());
                                j["name"] = value;
                                result    = true;
                            }
                        }
                    } else if (
                        type == "Clip" or type == "Gap" or type == "Stack" or
                        type == "Audio Track" or type == "Video Track") {
                        anon_send(actor, timeline::item_name_atom_v, value.get<std::string>());
                        j["name"] = value;
                        result    = true;
                    }
                }
                break;
            case flagTextRole:
                if (j.count("flag_text") and j["flag_text"] != value) {
                    if (type == "Media") {
                        if (index.isValid()) {
                            nlohmann::json &j = indexToData(index);
                            auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                    ? actorFromString(system(), j.at("actor"))
                                                    : caf::actor();
                            if (actor) {
                                // spdlog::warn("Send update {} {}", j["flag"], value);
                                anon_send(
                                    actor,
                                    playlist::reflag_container_atom_v,
                                    std::make_tuple(
                                        std::optional<std::string>(),
                                        std::optional<std::string>(value.get<std::string>())));
                                j["flag_text"] = value;
                                result         = true;
                            }
                        }
                    }
                }
                break;

            case flagColourRole:
                if (j.count("flag") and j["flag"] != value) {
                    if (type == "Media") {
                        nlohmann::json &j = indexToData(index);
                        auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                ? actorFromString(system(), j.at("actor"))
                                                : caf::actor();
                        if (actor) {
                            // spdlog::warn("Send update {} {}", j["flag"], value);
                            anon_send(
                                actor,
                                playlist::reflag_container_atom_v,
                                std::make_tuple(
                                    std::optional<std::string>(value.get<std::string>()),
                                    std::optional<std::string>()));
                            j["flag"] = value;
                            result    = true;
                        }
                    } else if (
                        type == "ContainerDivider" or type == "Subset" or type == "Timeline" or
                        type == "Playlist" or type == "ContactSheet") {
                        auto p = index.parent();
                        if (p.isValid()) {
                            nlohmann::json &pj = indexToData(p);
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
                                // spdlog::warn("Send update {} {}", j["flag"], value);
                                anon_send(
                                    pactor,
                                    playlist::reflag_container_atom_v,
                                    value.get<std::string>(),
                                    j.at("container_uuid").get<Uuid>());
                                j["flag"] = value;
                                result    = true;
                            }
                        }
                    } else if (
                        type == "Clip" or type == "Gap" or type == "Audio Track" or
                        type == "Video Track" or type == "Stack") {
                        nlohmann::json &j = indexToData(index);
                        auto actor        = j.count("actor") and not j.at("actor").is_null()
                                                ? actorFromString(system(), j.at("actor"))
                                                : caf::actor();
                        if (actor) {
                            anon_send(
                                actor, timeline::item_flag_atom_v, value.get<std::string>());
                            j["flag"] = value;
                            result    = true;
                        }
                    }
                }
                break;

            case expandedRole:
                if (type == "Playlist" && j.count("expanded") and j["expanded"] != value) {
                    nlohmann::json &j = indexToData(index);
                    auto actor        = j.count("actor") and not j.at("actor").is_null()
                                            ? actorFromString(system(), j.at("actor"))
                                            : caf::actor();
                    if (actor) {
                        // spdlog::warn("Send update {} {}", j["flag"], value);
                        anon_send(actor, playlist::expanded_atom_v, value.get<bool>());
                        j["expanded"] = value;
                        result        = true;
                    }
                }
                break;

            default:
                throw std::runtime_error("Unsupported Role");
                break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        spdlog::warn(
            "{} {} {}",
            index.row(),
            StdFromQString(roleName(role)),
            mapFromValue(qvalue).dump());
    }

    if (result)
        emit dataChanged(index, index, roles);

    CHECK_SLOW_WATCHER_FAST()

    return result;
}

bool SessionModel::removeRows(int row, int count, const QModelIndex &parent) {
    return removeRows(row, count, false, parent);
}