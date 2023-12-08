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

    setRoleNames(std::vector<std::string>({
        {"actorRole"},         {"actorUuidRole"},   {"audioActorUuidRole"},
        {"bitDepthRole"},      {"busyRole"},        {"childrenRole"},
        {"containerUuidRole"}, {"flagRole"},        {"formatRole"},
        {"groupActorRole"},    {"idRole"},          {"imageActorUuidRole"},
        {"mediaCountRole"},    {"mediaStatusRole"}, {"mtimeRole"},
        {"nameRole"},          {"pathRole"},        {"pixelAspectRole"},
        {"placeHolderRole"},   {"rateFPSRole"},     {"resolutionRole"},
        {"thumbnailURLRole"},  {"typeRole"},        {"uuidRole"},
    }));

    request_handler_ = new QThreadPool(this);
}


QModelIndexList SessionModel::search_recursive_list_base(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {

    QModelIndexList results;

    if (role == idRole or role == actorUuidRole or role == containerUuidRole) {
        auto uuid = UuidFromQUuid(value.toUuid());
        if (uuid.is_null()) {
            return QModelIndexList();
        }

        if (role == idRole or role == actorUuidRole) {
            auto it = uuid_lookup_.find(uuid);
            if (it != std::end(uuid_lookup_)) {
                for (auto iit = std::begin(it->second); iit != std::end(it->second);) {
                    if (iit->isValid()) {
                        results.push_back(*iit);
                        iit++;
                    } else {
                        iit = it->second.erase(iit);
                    }
                }
            } else {
                results = JSONTreeModel::search_recursive_list_base(
                    value, role, parent, start, hits, depth);
                for (const auto &i : results) {
                    add_uuid_lookup(uuid, i);
                }
            }
        }
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
            results = JSONTreeModel::search_recursive_list_base(
                value, role, parent, start, hits, depth);
            for (const auto &i : results) {
                add_string_lookup(str, i);
            }
        }
    } else {
        // spdlog::error("No lookup {}", StdFromQString(roleName(role)));
    }

    if (results.empty())
        results =
            JSONTreeModel::search_recursive_list_base(value, role, parent, start, hits, depth);
    else {
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

            case Roles::flagRole:
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
            default:
                result = JSONTreeModel::data(index, role);
                break;
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
                    }
                }
                break;
            case flagRole:
                if (j.count("flag") and j["flag"] != value) {
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
                                        std::optional<std::string>(value.get<std::string>()),
                                        std::optional<std::string>()));
                                j["flag"] = value;
                                result    = true;
                            }
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
