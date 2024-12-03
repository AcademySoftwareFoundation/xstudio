// SPDX-License-Identifier: Apache-2.0

#include "result_model_ui.hpp"
#include "shotbrowser_engine_ui.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

#include <QProcess>
#include <qdebug.h>

using namespace std::chrono_literals;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;


ShotBrowserResultModel::ShotBrowserResultModel(QObject *parent) : JSONTreeModel(parent) {
    setRoleNames(std::vector<std::string>(
        {"addressingRole",
         "artistRole",
         "assetRole",
         "attachmentsRole",
         "authorRole",
         "clientFilenameRole",
         "clientNoteRole",
         "contentRole",
         "createdByRole",
         "createdDateRole",
         "dateSubmittedToClientRole",
         "departmentRole",
         "detailRole",
         "frameRangeRole",
         "frameSequenceRole",
         "linkedVersionsRole",
         "loginRole",
         "movieRole",
         "nameRole",
         "noteCountRole",
         "noteTypeRole",
         "onSiteChn",
         "onSiteLon",
         "onSiteMtl",
         "onSiteMum",
         "onSiteSyd",
         "onSiteVan",
         "otioRole",
         "pipelineStatusFullRole",
         "pipelineStatusRole",
         "pipelineStepRole",
         "playlistNameRole",
         "playlistTypeRole",
         "productionStatusRole",
         "projectIdRole",
         "projectRole",
         "resultRowRole",
         "sequenceRole",
         "shotRole",
         "siteRole",
         "stageRole",
         "stalkUuidRole",
         "subjectRole",
         "submittedToDailiesRole",
         "tagRole",
         "thumbRole",
         "twigNameRole",
         "twigTypeRole",
         "typeRole",
         "URLRole",
         "versionCountRole",
         "versionNameRole",
         "versionRole"}));
}

void ShotBrowserResultModel::setEmpty() {
    result_data_ = R"({})"_json;
    setModelData(R"([])"_json);
    setCanBeGrouped(false);
}

int ShotBrowserResultModel::page() const { return getResultValue("/page", 0); }

int ShotBrowserResultModel::maxResult() const { return getResultValue("/max_result", 0); }

int ShotBrowserResultModel::executionMilliseconds() const {
    return getResultValue("/execution_ms", 0);
}

bool ShotBrowserResultModel::truncated() const {
    return getResultValue("/context/truncated", false);
}

void ShotBrowserResultModel::setIsGrouped(const bool value) {
    if (can_be_grouped_ and value != is_grouped_) {
        was_grouped_ = value;
        is_grouped_  = value;
        emit isGroupedChanged();

        if (is_grouped_)
            groupBy();
        else
            unGroupBy();
    }
}

void ShotBrowserResultModel::setCanBeGrouped(const bool value) {
    if (value != can_be_grouped_) {
        can_be_grouped_ = value;
        emit canBeGroupedChanged();
    }

    if (can_be_grouped_ and was_grouped_)
        is_grouped_ = true;
    else
        is_grouped_ = false;

    emit isGroupedChanged();
}


QUuid ShotBrowserResultModel::presetId() const {
    return QUuidFromUuid(getResultValue("/preset_id", Uuid()));
}
QUuid ShotBrowserResultModel::groupId() const {
    return QUuidFromUuid(getResultValue("/group_id", Uuid()));
}

QString ShotBrowserResultModel::audioSource() const {
    return QStringFromStd(getResultValue("/context/audio_source", std::string("")));
}

QString ShotBrowserResultModel::visualSource() const {
    return QStringFromStd(getResultValue("/context/visual_source", std::string("")));
}

QString ShotBrowserResultModel::entity() const {
    return QStringFromStd(getResultValue("/entity", std::string("")));
}

QString ShotBrowserResultModel::flagColour() const {
    return QStringFromStd(getResultValue("/context/flag_text", std::string("")));
}

QString ShotBrowserResultModel::flagText() const {
    return QStringFromStd(getResultValue("/context/flag_colour", std::string("")));
}

QDateTime ShotBrowserResultModel::requestedAt() const {
    return QDateTime::fromSecsSinceEpoch(getResultValue("/context/epoc", 0));
}

QVariantMap ShotBrowserResultModel::env() const {
    return QVariantMapFromJson(getResultValue("/env", R"({})"_json));
}

QVariantMap ShotBrowserResultModel::context() const {
    return QVariantMapFromJson(getResultValue("/context", R"({})"_json));
}

QFuture<bool> ShotBrowserResultModel::setResultDataFuture(const QStringList &data) {
    return QtConcurrent::run([=]() {
        try {
            if (data.isEmpty())
                setEmpty();
            else {
                auto results = R"([])"_json;
                try {
                    auto have_set_result_data = false;
                    for (const auto &i : data) {
                        if (i.isEmpty())
                            continue;
                        auto jsn_data = nlohmann::json::parse(StdFromQString(i));
                        if (not have_set_result_data) {
                            result_data_         = jsn_data;
                            have_set_result_data = true;
                        }
                        const auto &items = jsn_data.at("result").at("data");


                        results.insert(results.end(), items.begin(), items.end());
                    }
                } catch (const std::exception &err) {
                    spdlog::warn(
                        "{} {} {}", __PRETTY_FUNCTION__, err.what(), StdFromQString(data[0]));
                }

                // inject helper order index..
                for (size_t i = 0; i < results.size(); i++) {
                    results[i]["result_row"] = i;
                }

                setModelData(results);
                if (entity() == "Versions") {
                    if (not can_be_grouped_)
                        setCanBeGrouped(true);

                    if (is_grouped_) {
                        groupBy();
                    }
                } else
                    setCanBeGrouped(false);
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), StdFromQString(data[0]));
            setEmpty();
        }
        emit stateChanged();
        return true;
    });
}


QVariant ShotBrowserResultModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();
    try {
        const auto &j = indexToData(index);
        switch (role) {
        case submittedToDailiesRole:
            if (not j.at("attributes").at("sg_submit_dailies").is_null())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("sg_submit_dailies")), Qt::ISODate);
            else if (not j.at("attributes").at("sg_submit_dailies_chn").is_null())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("sg_submit_dailies_chn")),
                    Qt::ISODate);
            else if (not j.at("attributes").at("sg_submit_dailies_mtl").is_null())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("sg_submit_dailies_mtl")),
                    Qt::ISODate);
            else if (not j.at("attributes").at("sg_submit_dailies_van").is_null())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("sg_submit_dailies_van")),
                    Qt::ISODate);
            else if (not j.at("attributes").at("sg_submit_dailies_mum").is_null())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("sg_submit_dailies_mum")),
                    Qt::ISODate);
            break;

        case Roles::departmentRole:
            result = QString::fromStdString(
                j.at("relationships").at("sg_department_unit").at("data").value("name", ""));
            break;

        case Roles::pipelineStepRole:
            result = QString::fromStdString(j.at("attributes").value("sg_pipeline_step", ""));
            break;
        case Roles::frameRangeRole:
            result = QString::fromStdString(j.at("attributes").value("frame_range", ""));
            break;
        case Roles::versionRole:
            result = j.at("attributes").value("sg_dneg_version", 0);
            break;

        case Roles::pipelineStatusRole:
            result = QString::fromStdString(j.at("attributes").value("sg_status_list", ""));
            break;

        case Roles::stageRole:
            result = QString::fromStdString(
                j.at("relationships").at("sg_client_send_stage").at("data").value("name", ""));
            break;

        case Roles::pipelineStatusFullRole: {
            result = QString::fromStdString(
                QueryEngine::resolve_attribute_value(
                    "Pipeline Status",
                    j.at("attributes").at("sg_status_list"),
                    ShotBrowserEngine::instance()->queryEngine().lookup())
                    .at("name")
                    .get<std::string>());
        } break;

        case Roles::productionStatusRole:
            result =
                QString::fromStdString(j.at("attributes").value("sg_production_status", ""));
            break;

        case Roles::dateSubmittedToClientRole:
            result = QDateTime::fromString(
                QStringFromStd(j.at("attributes").value("sg_date_submitted_to_client", "")),
                Qt::ISODate);
            break;

        case Roles::addressingRole: {
            auto tmp = QStringList();
            for (const auto &i : j.at("relationships").at("addressings_to").at("data"))
                tmp.append(QStringFromStd(i.at("name").get<std::string>()));
            result = tmp;
        } break;

        case Roles::noteTypeRole:
            result = QString::fromStdString(j.at("attributes").value("sg_note_type", ""));
            break;

        case Roles::resultRowRole:
            result = j.value("result_row", -1);
            break;

        case Roles::typeRole:
            result = QString::fromStdString(j.value("type", ""));
            break;

        case Roles::clientNoteRole:
            result = j.at("attributes").value("client_note", false);
            break;

        case Roles::clientFilenameRole:
            result = QString::fromStdString(j.at("attributes").value("sg_client_filename", ""));
            break;

        case Roles::subjectRole:
            result = QString::fromStdString(j.at("attributes").value("subject", ""));
            break;

        case Roles::contentRole:
            result = QString::fromStdString(j.at("attributes").value("content", ""));
            break;

        case Roles::linkedVersionsRole: {
            auto v = QVariantList();
            for (const auto &i : j.at("relationships").at("note_links").at("data")) {
                if (i.at("type").get<std::string>() == "Version") {
                    v.append(QVariant::fromValue(i.at("id").get<int>()));
                    break;
                }
            }
            result = v;
        } break;

        case Roles::versionNameRole:
            for (const auto &i : j.at("relationships").at("note_links").at("data")) {
                if (i.at("type").get<std::string>() == "Version") {
                    result = QString::fromStdString(i.at("name").get<std::string>());
                    break;
                }
            }
            break;

        case Roles::shotRole:
            if (j.at("relationships").at("entity").at("data").at("type") == "Shot")
                result = QString::fromStdString(
                    j.at("relationships").at("entity").at("data").at("name"));
            break;

        case Roles::assetRole:
            if (j.at("relationships").at("entity").at("data").at("type") == "Asset")
                result = QString::fromStdString(
                    j.at("relationships").at("entity").at("data").at("name"));
            break;

        case Roles::attachmentsRole:
            result = mapFromValue(j.at("relationships").at("attachments").at("data"));
            break;

        case Roles::movieRole:
            result = QString::fromStdString(j.at("attributes").at("sg_path_to_movie"));
            break;

        case Roles::otioRole: {
            auto path     = j.at("attributes").value("sg_path_to_frames", std::string());
            auto last_dot = path.find_last_of(".");
            path          = path.substr(0, last_dot);

            QVariantList otios;
            try {
                auto uri = posix_path_to_uri(path + "_optimised.otio");
                otios.append(QVariant::fromValue(QUrlFromUri(uri)));
            } catch (...) {
            }
            try {
                auto uri = posix_path_to_uri(path + ".otio");
                otios.append(QVariant::fromValue(QUrlFromUri(uri)));
            } catch (...) {
            }
            result = QVariant::fromValue(otios);
        } break;

        case Roles::sequenceRole: {
            if (j.at("relationships").at("entity").at("data").value("type", "") == "Sequence") {
                result = QString::fromStdString(
                    j.at("relationships").at("entity").at("data").value("name", ""));
            } else {
                auto name = QueryEngine::get_sequence_name(
                    j.at("relationships").at("project").at("data").value("id", 0),
                    j.at("relationships").at("entity").at("data").value("id", 0),
                    ShotBrowserEngine::instance()->queryEngine().lookup());
                if (not name.empty())
                    result = QString::fromStdString(name.at(0));
                else
                    result = QString();
            }
        } break;

        case Roles::frameSequenceRole:
            result = QString::fromStdString(j.at("attributes").at("sg_path_to_frames"));
            break;

        case Roles::noteCountRole:
            try {
                result = static_cast<int>(j.at("relationships").at("notes").at("data").size());
            } catch (...) {
                auto req      = JsonStore(GetFields);
                req["id"]     = j.at("id");
                req["entity"] = j.at("type");
                req["fields"].push_back("notes");
                // send request.
                ShotBrowserEngine::instance()->requestData(index, role, req);
                result = static_cast<int>(-1);
            }
            break;

        case Roles::versionCountRole:
            try {
                result =
                    static_cast<int>(j.at("relationships").at("versions").at("data").size());
            } catch (...) {
                auto req      = JsonStore(GetFields);
                req["id"]     = j.at("id");
                req["entity"] = j.at("type");
                req["fields"].push_back("versions");
                // send request.
                ShotBrowserEngine::instance()->requestData(index, role, req);
                result = static_cast<int>(-1);
            }
            break;

        case Roles::siteRole:
            result = QString::fromStdString(j.at("attributes").value("sg_location", ""));
            break;

        case Roles::onSiteMum:
            try {
                result = j.at("attributes").at("sg_on_disk_mum") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_mum") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;
        case Roles::onSiteMtl:
            try {
                result = j.at("attributes").at("sg_on_disk_mtl") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_mtl") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;
        case Roles::onSiteVan:
            try {
                result = j.at("attributes").at("sg_on_disk_van") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_van") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;
        case Roles::onSiteChn:
            try {
                result = j.at("attributes").at("sg_on_disk_chn") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_chn") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;
        case Roles::onSiteLon:
            try {
                result = j.at("attributes").at("sg_on_disk_lon") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_lon") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;

        case Roles::onSiteSyd:
            try {
                result = j.at("attributes").at("sg_on_disk_syd") == "Full"      ? 2
                         : j.at("attributes").at("sg_on_disk_syd") == "Partial" ? 1
                                                                                : 0;
            } catch (...) {
                result = 0;
            }
            break;


        case Roles::twigNameRole:
            result = QString::fromStdString(j.at("attributes").value("sg_twig_name", ""));
            break;

        case Roles::tagRole: {
            auto tmp = QStringList();
            for (const auto &i : j.at("relationships").at("tags").at("data")) {
                auto name = QStringFromStd(i.at("name").get<std::string>());
                name.replace(QRegExp("\\.REFERENCE$"), "");
                tmp.append(name);
            }
            result = tmp;
        } break;

        case Roles::twigTypeRole:
            result = QString::fromStdString(j.at("attributes").value("sg_twig_type", ""));
            break;

        case Roles::stalkUuidRole:
            result = QUuidFromUuid(utility::Uuid(j.at("attributes").at("sg_ivy_dnuuid")));
            break;

        case Roles::URLRole:
            result = QStringFromStd(std::string(fmt::format(
                "http://shotgun/detail/{}/{}",
                j.at("type").get<std::string>(),
                j.at("id").get<int>())));
            break;

        case Roles::artistRole: {
            auto name = std::string("pending");
            if (j.at("attributes").count("sg_artist") and
                j.at("attributes").at("sg_artist").is_string())
                name = j.at("attributes").at("sg_artist").get<std::string>();

            if (name == "pending" or name == "") {
                // request name from shot.
                auto req = JsonStore(GetVersionArtist);
                for (const auto &i : j.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Version") {
                        req["version_id"] = i.at("id");
                        break;
                    }
                }
                if (req["version_id"].is_null()) {
                    name = "unknown";
                } else {
                    // send request.
                    ShotBrowserEngine::instance()->requestData(index, role, req);
                }
            }
            result = QString::fromStdString(name);
        } break;

        case Roles::nameRole:
        case Qt::DisplayRole:
            if (j.count("nameRole"))
                result = QString::fromStdString(j.at("nameRole"));
            else if (j.count("name"))
                result = QString::fromStdString(j.at("name"));
            else
                result = QString::fromStdString(
                    j.at("attributes")
                        .value(
                            "name",
                            j.at("attributes")
                                .value("code", j.at("attributes").value("subject", ""))));
            break;

        case JSONTreeModel::Roles::idRole:
            try {
                if (j.at("id").is_number())
                    result = j.at("id").get<int>();
                else
                    result = QString::fromStdString(j.at("id").get<std::string>());
            } catch (...) {
            }
            break;

        case JSONTreeModel::Roles::JSONRole:
        case Roles::detailRole:
            result = QVariantMapFromJson(j);
            break;

        case JSONTreeModel::Roles::JSONTextRole:
            result = QString::fromStdString(j.dump(2));
            break;

        case Roles::loginRole:
            result = QString::fromStdString(j.at("attributes").at("login").get<std::string>());
            break;

        case Roles::playlistTypeRole:
            result =
                QString::fromStdString(j.at("attributes").at("sg_type").get<std::string>());
            break;

        case Roles::createdByRole:
        case Roles::authorRole:
            result = QString::fromStdString(
                j.at("relationships").at("created_by").at("data").value("name", ""));
            break;

        case Roles::projectRole:
            result = QString::fromStdString(
                j.at("relationships").at("project").at("data").value("name", ""));
            break;

        case Roles::projectIdRole:
            result = j.at("relationships").at("project").at("data").value("id", 0);
            break;

        case Roles::createdDateRole:
            result = QDateTime::fromString(
                QStringFromStd(j.at("attributes").value("created_at", "")), Qt::ISODate);
            break;

        case Roles::thumbRole:
            // result = "qrc:/feather_icons/film.svg";
            if (j.at("type") == "Playlist") {
                result = QStringFromStd(fmt::format(
                    "image://shotgrid/thumbnail/{}/{}", j.value("type", ""), j.value("id", 0)));
            } else if (
                j.at("attributes").count("image") and
                not j.at("attributes").at("image").is_null())
                result = QStringFromStd(fmt::format(
                    "image://shotgrid/thumbnail/{}/{}", j.value("type", ""), j.value("id", 0)));
            else {
                for (const auto &i : j.at("relationships").at("note_links").at("data")) {
                    if (i.at("type") == "Version") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgrid/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                        break;
                    } else if (i.at("type") == "Shot") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgrid/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                    } else if (result.isNull() and i.at("type") == "Playlist") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgrid/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                    }
                }
            }
            break;


            break;

        default:
            result = JSONTreeModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

void ShotBrowserResultModel::fetchMore(const QModelIndex &parent) {
    try {
        if (canFetchMore(parent)) {
            auto type = mapFromValue(parent.data(Roles::typeRole));
            if (type == "Playlist") {
                // issue async request for the children..
                auto future = ShotBrowserEngine::instance()->getPlaylistVersionsFuture(
                    mapFromValue(parent.data(JSONTreeModel::Roles::idRole)));

                auto jsn        = nlohmann::json::parse(StdFromQString(future.result()));
                auto tmp        = R"({"children":null})"_json;
                tmp["children"] = jsn.at("data").at("relationships").at("versions").at("data");
                setData(parent, mapFromValue(tmp), JSONTreeModel::Roles::childrenRole);
                // spdlog::warn("{}", tmp["children"].dump(2));
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

bool ShotBrowserResultModel::setData(
    const QModelIndex &index, const QVariant &value, int role) {
    QVector<int> roles({role});
    auto result = false;

    try {
        auto &j = indexToData(index);
        switch (role) {

        case Roles::artistRole: {
            auto data = mapFromValue(value);
            if (j["attributes"]["sg_artist"] !=
                data.at("relationships").at("user").at("data").at("name")) {
                j["attributes"]["sg_artist"] =
                    data.at("relationships").at("user").at("data").at("name");
                result = true;
                emit dataChanged(index, index, roles);
            }
        } break;

        case Roles::noteCountRole: {
            auto data = mapFromValue(value);
            auto jp   = nlohmann::json::json_pointer("/relationships/notes/data");
            auto dp   = nlohmann::json::json_pointer("/data/relationships/notes/data");

            if (not j.contains(jp))
                j["relationships"]["notes"] = R"({"data":null})"_json;

            if (j.at(jp) != data.at(dp)) {
                j["relationships"]["notes"]["data"] = data.at(dp);
                result                              = true;
                emit dataChanged(index, index, roles);
            }
        } break;

        case Roles::versionCountRole: {
            auto data = mapFromValue(value);
            auto jp   = nlohmann::json::json_pointer("/relationships/versions/data");
            auto dp   = nlohmann::json::json_pointer("/data/relationships/versions/data");

            if (not j.contains(jp))
                j["relationships"]["versions"] = R"({"data":null})"_json;

            if (j.at(jp) != data.at(dp)) {
                j["relationships"]["versions"]["data"] = data.at(dp);

                if (j["relationships"]["versions"]["data"].empty())
                    j["children"] = R"([])"_json;
                else
                    j["children"] = nullptr;

                result = true;
                emit dataChanged(index, index, roles);
            }
        } break;

        default:
            result = JSONTreeModel::setData(index, value, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            StdFromQString(value.toString()));
    }

    return result;
}


void ShotBrowserResultModel::groupBy() {
    // reorder rows injecting into parent with version max for stalk.
    // build lookup for items.
    std::map<QVariant, std::map<int, utility::JsonTree *>> vmap;

    for (auto i = 0; i < rowCount(); i++) {
        auto index   = ShotBrowserResultModel::index(i, 0);
        auto name    = index.data(Roles::twigNameRole);
        auto version = index.data(Roles::versionRole).toInt();

        if (not vmap.count(name))
            vmap[name] = std::map<int, utility::JsonTree *>();

        vmap[name][version] = indexToTree(index);
    }

    beginResetModel();

    // iterate over map
    for (const auto &[key, items] : vmap) {
        if (items.size() == 1) {
            continue;
        } else {
            // select largest version
            auto pnode = items.crbegin()->second;

            // reparent others to this item..
            for (const auto &[cversion, cnode] : items) {
                if (cnode == pnode)
                    continue;

                auto row_in_parent = cnode->index();
                moveNodes(cnode->parent(), row_in_parent, row_in_parent, pnode, 0);
            }
        }
    }

    endResetModel();
}

void ShotBrowserResultModel::unGroupBy() {
    // yay unparent....
    std::map<int, utility::JsonTree *> vmap;

    // find children and add to map
    for (auto i = 0; i < rowCount(); i++) {
        auto pindex = ShotBrowserResultModel::index(i, 0);

        // has children
        auto ccount = rowCount(pindex);
        if (ccount) {
            for (auto ii = 0; ii < ccount; ii++) {
                auto cindex = ShotBrowserResultModel::index(ii, 0, pindex);
                vmap[cindex.data(resultRowRole).toInt()] = indexToTree(cindex);
            }
        }
    }

    beginResetModel();

    // move children back to root
    for (const auto &[crow, cnode] : vmap) {
        auto row_in_parent = cnode->index();
        moveNodes(
            cnode->parent(), row_in_parent, row_in_parent, cnode->parent()->parent(), crow);
    }

    endResetModel();
}

void ShotBrowserResultFilterModel::setSortByNaturalOrder(const bool value) {
    if (ordering_ != OM_NATURAL and value) {
        ordering_ = OM_NATURAL;
        emit sortByChanged();
        invalidate();
    }
}

void ShotBrowserResultFilterModel::setSortByCreationDate(const bool value) {
    if (ordering_ != OM_DATE and value) {
        ordering_ = OM_DATE;
        emit sortByChanged();
        invalidate();
    }
}

void ShotBrowserResultFilterModel::setSortByShotName(const bool value) {
    if (ordering_ != OM_SHOT and value) {
        ordering_ = OM_SHOT;
        emit sortByChanged();
        invalidate();
    }
}

void ShotBrowserResultFilterModel::setSortInAscending(const bool value) {
    if (sortInAscending_ != value) {
        sortInAscending_ = value;
        emit sortInAscendingChanged();
        invalidate();
    }
}

bool ShotBrowserResultFilterModel::lessThan(
    const QModelIndex &source_left, const QModelIndex &source_right) const {
    auto result = false;

    switch (ordering_) {
    case OM_SHOT: {
        auto compare_shot =
            source_left.data(ShotBrowserResultModel::Roles::shotRole)
                .toString()
                .compare(source_right.data(ShotBrowserResultModel::Roles::shotRole).toString());
        if (compare_shot == 0) {
            result = source_left.data(ShotBrowserResultModel::Roles::nameRole).toString() <
                     source_right.data(ShotBrowserResultModel::Roles::nameRole).toString();
        } else
            result = compare_shot < 0;
    } break;

    case OM_DATE: {
        result = source_left.data(ShotBrowserResultModel::Roles::createdDateRole).toDateTime() <
                 source_right.data(ShotBrowserResultModel::Roles::createdDateRole).toDateTime();
    } break;

    case OM_NATURAL: {
    default:
        result = source_left.row() < source_right.row();
    } break;
    }

    if (sortInAscending_)
        result = !result;

    return result;
}

void ShotBrowserResultFilterModel::setFilterChn(const bool value) {
    if (value != filterChn_) {
        filterChn_ = value;
        emit filterChnChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterLon(const bool value) {
    if (value != filterLon_) {
        filterLon_ = value;
        emit filterLonChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterMtl(const bool value) {
    if (value != filterMtl_) {
        filterMtl_ = value;
        emit filterMtlChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterMum(const bool value) {
    if (value != filterMum_) {
        filterMum_ = value;
        emit filterMumChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterVan(const bool value) {
    if (value != filterVan_) {
        filterVan_ = value;
        emit filterVanChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterSyd(const bool value) {
    if (value != filterSyd_) {
        filterSyd_ = value;
        emit filterSydChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterPipeStep(const QString &value) {
    if (value != filterPipeStep_) {
        filterPipeStep_ = value;
        emit filterPipeStepChanged();
        invalidateFilter();
    }
}
void ShotBrowserResultFilterModel::setFilterName(const QString &value) {
    if (value != filterName_) {
        filterName_ = value;
        emit filterNameChanged();
        invalidateFilter();
    }
}


bool ShotBrowserResultFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    auto result       = true;
    auto source_index = sourceModel()->index(source_row, 0, source_parent);

    if (source_index.isValid()) {
        if (source_index.data(ShotBrowserResultModel::Roles::typeRole).toString() ==
            "Version") {
            if (filterChn_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteChn).toInt())
                result = false;
            else if (
                filterLon_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteLon).toInt())
                result = false;
            else if (
                filterMtl_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteMtl).toInt())
                result = false;
            else if (
                filterMum_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteMum).toInt())
                result = false;
            else if (
                filterVan_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteVan).toInt())
                result = false;
            else if (
                filterSyd_ and
                not source_index.data(ShotBrowserResultModel::Roles::onSiteSyd).toInt())
                result = false;
            else if (
                not filterPipeStep_.isEmpty() and
                filterPipeStep_ !=
                    source_index.data(ShotBrowserResultModel::Roles::pipelineStepRole)
                        .toString())
                result = false;
        }
        // only apply name filter to parent, not children.

        if (result and not filterName_.isEmpty() and not source_index.parent().isValid() and
            not source_index.data(ShotBrowserResultModel::Roles::nameRole)
                    .toString()
                    .contains(filterName_, Qt::CaseInsensitive))
            result = false;
    }

    return result;
}


#include "result_model_ui.moc"
