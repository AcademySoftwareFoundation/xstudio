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


SessionModel::SessionModel(QObject *parent) : super(parent) {
    tag_manager_ = new TagManagerUI(this);
    init(CafSystemObject::get_actor_system());

    auto role_names = std::vector<std::string>({
        {"activeDurationRole"},
        {"activeStartRole"},
        {"actorRole"},
        {"actorUuidRole"},
        {"audioActorUuidRole"},
        {"availableDurationRole"},
        {"availableStartRole"},
        {"bitDepthRole"},
        {"busyRole"},
        {"childrenRole"},
        {"clipMediaUuidRole"},
        {"containerUuidRole"},
        {"enabledRole"},
        {"errorRole"},
        {"flagColourRole"},
        {"flagTextRole"},
        {"formatRole"},
        {"groupActorRole"},
        {"idRole"},
        {"imageActorUuidRole"},
        {"mediaCountRole"},
        {"mediaStatusRole"},
        {"metadataSet0Role"},
        {"metadataSet10Role"},
        {"metadataSet1Role"},
        {"metadataSet2Role"},
        {"metadataSet3Role"},
        {"metadataSet4Role"},
        {"metadataSet5Role"},
        {"metadataSet6Role"},
        {"metadataSet7Role"},
        {"metadataSet8Role"},
        {"metadataSet9Role"},
        {"mtimeRole"},
        {"nameRole"},
        {"parentStartRole"},
        {"pathRole"},
        {"pixelAspectRole"},
        {"placeHolderRole"},
        {"rateFPSRole"},
        {"resolutionRole"},
        {"selectionRole"},
        {"thumbnailURLRole"},
        {"trackIndexRole"},
        {"trimmedDurationRole"},
        {"trimmedStartRole"},
        {"typeRole"},
        {"uuidRole"},
    });

    setRoleNames(role_names);
    request_handler_ = new QThreadPool(this);
}

void SessionModel::fetchMore(const QModelIndex &parent) {
    try {
        if (parent.isValid() and canFetchMore(parent)) {
            const auto &j = indexToData(parent);

            requestData(
                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                idRole,
                parent,
                parent,
                Roles::childrenRole);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


QModelIndexList SessionModel::search_recursive_list_base(
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
            cached_result = false;
            results       = JSONTreeModel::search_recursive_list_base(
                value, role, parent, start, hits, depth);
            for (const auto &i : results) {
                add_id_uuid_lookup(uuid, i);
            }
        }
    }
    if (role == actorUuidRole or role == containerUuidRole) {
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
            cached_result = false;
            results       = JSONTreeModel::search_recursive_list_base(
                value, role, parent, start, hits, depth);
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
            results       = JSONTreeModel::search_recursive_list_base(
                value, role, parent, start, hits, depth);
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
            JSONTreeModel::search_recursive_list_base(value, role, parent, start, hits, depth);
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

            case Roles::idRole:
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
                if (j.count("rate")) {
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
                    } else
                        result = json_to_qvariant(j.at("playhead_selection"));
                }
                break;

            case Roles::flagColourRole:
                if (j.count("flag")) {
                    if (j.at("flag").is_null()) {
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            role);
                    } else
                        result = QString::fromStdString(j.at("flag"));
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
                }
                break;


            case Roles::enabledRole:
                if (j.count("enabled")) {
                    result = QVariant::fromValue(j.value("enabled", true));
                }
                break;

            case Roles::trimmedStartRole:
                if (j.count("active_range") and j.at("active_range").is_object()) {
                    auto fr = j.value("active_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_start().frames());
                } else if (j.count("available_range") and j.at("available_range").is_object()) {
                    auto fr = j.value("available_range", FrameRange());
                    result  = QVariant::fromValue(fr.frame_start().frames());
                } else if (j.count("available_range") or j.count("active_range")) {
                    result = QVariant::fromValue(0);
                }
                break;

            case Roles::parentStartRole:
                // requires access to parent item.
                if (j.count("active_range")) {
                    auto p = index.parent();
                    auto t = getTimelineIndex(index);

                    if (p.isValid() and t.isValid()) {
                        auto tactor = actorFromIndex(t);
                        auto puuid  = UuidFromQUuid(p.data(idRole).toUuid());

                        if (timeline_lookup_.count(tactor)) {
                            auto pitem = timeline::find_item(
                                timeline_lookup_.at(tactor).children(), puuid);
                            if (pitem)
                                result =
                                    QVariant::fromValue((*pitem)->frame_at_index(index.row()));
                        }
                    } else
                        result = QVariant::fromValue(0);
                }
                break;

            case Roles::trimmedDurationRole:
                if (j.count("active_range") and j.at("active_range").is_object()) {
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
                if (j.count("available_range")) {
                    if (j.at("available_range").is_object()) {
                        auto fr = j.value("available_range", FrameRange());
                        result  = QVariant::fromValue(fr.frame_duration().frames());
                    } else {
                        result = QVariant::fromValue(0);
                    }
                }
                break;

            case Roles::availableStartRole:
                if (j.count("available_range")) {
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
                if (j.count("name")) {
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
            case Roles::childrenRole:
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
                // are we looking for one of the flexible metadata set roles?
                int did = role - Roles::metadataSet0Role;
                if (did >= 0 && did <= 9) {
                    const std::string key = fmt::format("metadata_set{}", did);
                    if (j.count(key)) {
                        if (j.at(key).is_null() && !metadata_sets_.empty()) {

                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                index,
                                role,
                                metadata_sets_.find(did)->second);

                        } else {
                            result = json_to_qvariant(j.at(key));
                        }
                    }

                } else {
                    result = JSONTreeModel::data(index, role);
                }
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
                             : caf::actor();

            switch (role) {

            case busyRole:
                if (j.count("busy") and j["busy"] != value) {
                    j["busy"] = value;
                    result    = true;
                }
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
                        if (actor)
                            anon_send(actor, timeline::active_range_atom_v, fr);
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
                    if ((type == "Session" or type == "Subset" or type == "Timeline" or
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
                        type == "Playlist") {
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

            default:
                throw std::runtime_error("Unsupported");
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

    if (result) {
        emit dataChanged(index, index, roles);
    }
    CHECK_SLOW_WATCHER_FAST()

    return result;
}

bool SessionModel::removeRows(int row, int count, const QModelIndex &parent) {
    return removeRows(row, count, false, parent);
}

void SessionModel::updateMetadataSelection(const int slot, QStringList metadata_paths) {

    // This SLOT lets us decide which Media metadata fields are put into one
    // of the metadataSet0Role, metadataSet1Role etc...
    // A 'slot' of 2 would correspond to metadataSet2Role, for example

    // When this is set-up, the  metadataSetXRole will be an array of metadata
    // VALUES whose metadata KEYS (or PATHS) are defined by metadata_paths.
    // Empry strings are allowed and the array element will be empty

    int idx = 0;
    metadata_sets_[slot].clear();
    for (const auto &path : metadata_paths) {
        metadata_sets_[slot][idx++] = StdFromQString(path);
    }
}
