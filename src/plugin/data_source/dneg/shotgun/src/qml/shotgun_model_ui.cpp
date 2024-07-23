// SPDX-License-Identifier: Apache-2.0
#include "shotgun_model_ui.hpp"
#include "../data_source_shotgun.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/atoms.hpp"

#include <QQmlExtensionPlugin>
#include <QDateTime>
#include <qdebug.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;
using namespace xstudio::global_store;

namespace {
void dumpNames(const nlohmann::json &jsn, const int depth) {
    if (jsn.is_array()) {
        for (const auto &item : jsn) {
            dumpNames(item, depth);
        }
    } else {
        spdlog::warn("{:>{}} {}", " ", depth * 4, jsn.value("name", "unnamed"));
        if (jsn.count("children") and jsn.at("children").is_array()) {
            for (const auto &item : jsn.at("children")) {
                dumpNames(item, depth + 1);
            }
        }
    }
}
} // namespace

nlohmann::json ShotgunSequenceModel::sortByName(const nlohmann::json &jsn) {
    // this needs
    auto result = sort_by(jsn, nlohmann::json::json_pointer("/name"));
    for (auto &item : result) {
        if (item.count("children")) {
            item["children"] = sortByName(item.at("children"));
        }
    }

    return result;
}

nlohmann::json ShotgunSequenceModel::flatToTree(const nlohmann::json &src) {
    // manipulate data into tree structure.
    // spdlog::warn("{}", src.size());

    auto result = R"([])"_json;
    std::map<size_t, nlohmann::json::json_pointer> seqs;

    // spdlog::warn("{}", src.dump(2));

    try {

        if (src.is_array()) {
            auto done = false;

            while (not done) {
                done = true;

                for (auto seq : src) {
                    try {
                        auto id = seq.at("id").get<int>();
                        // already logged ?
                        // if already there then skip

                        if (not seqs.count(id)) {
                            seq["name"]    = seq.at("attributes").at("code");
                            auto parent_id = seq.at("relationships")
                                                 .at("sg_parent")
                                                 .at("data")
                                                 .at("id")
                                                 .get<int>();

                            // no parent add to results. and store pointer to results entry.
                            if (parent_id == id) {
                                // spdlog::warn("new root level item {}",
                                // seq["name"].get<std::string>());

                                auto &shots = seq["relationships"]["shots"]["data"];
                                if (shots.is_array())
                                    seq["children"] =
                                        sort_by(shots, nlohmann::json::json_pointer("/name"));
                                else
                                    seq["children"] = R"([])"_json;

                                seq["parent_id"] = parent_id;
                                seq["relationships"].erase("shots");
                                seq["relationships"].erase("sg_parent");

                                // store in result
                                // and pointer to entry.
                                result.emplace_back(seq);

                                seqs.emplace(std::make_pair(
                                    id,
                                    nlohmann::json::json_pointer(
                                        std::string("/") + std::to_string(result.size() - 1))));

                                done = false;

                            } else if (seqs.count(parent_id)) {
                                // parent exists
                                // spdlog::warn("add to parent {} {}", parent_id,
                                // seq["name"].get<std::string>());

                                auto parent_pointer = seqs[parent_id];

                                auto &shots = seq["relationships"]["shots"]["data"];
                                if (shots.is_array())
                                    seq["children"] =
                                        sort_by(shots, nlohmann::json::json_pointer("/name"));
                                else
                                    seq["children"] = R"([])"_json;

                                seq["parent_id"] = parent_id;
                                seq["relationships"].erase("shots");
                                seq["relationships"].erase("sg_parent");

                                result[parent_pointer]["children"].emplace_back(seq);
                                // spdlog::warn("{}", result[parent_pointer].dump(2));

                                // add path to new entry..
                                seqs.emplace(std::make_pair(
                                    id,
                                    parent_pointer /
                                        nlohmann::json::json_pointer(
                                            std::string("/children/") +
                                            std::to_string(
                                                result[parent_pointer]["children"].size() -
                                                1))));
                                done = false;
                            }
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            }

            // un parented sequences
            auto count = 0;
            // unresolved..
            for (auto unseq : src) {
                try {
                    auto id = unseq.at("id").get<int>();
                    // already logged ?
                    if (not seqs.count(id)) {
                        unseq["name"] = unseq.at("attributes").at("code");

                        auto parent_id =
                            unseq["relationships"]["sg_parent"]["data"]["id"].get<int>();
                        // no parent
                        auto &shots = unseq["relationships"]["shots"]["data"];

                        spdlog::warn("{} {}", id, parent_id);

                        if (shots.is_array())
                            unseq["children"] =
                                sort_by(shots, nlohmann::json::json_pointer("/name"));
                        else
                            unseq["children"] = R"([])"_json;

                        unseq["parent_id"] = parent_id;
                        unseq["relationships"].erase("shots");
                        unseq["relationships"].erase("sg_parent");
                        result.emplace_back(unseq);
                        seqs.emplace(std::make_pair(
                            id,
                            nlohmann::json::json_pointer(
                                std::string("/") + std::to_string(result.size() - 1))));
                        count++;
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }

                if (count)
                    spdlog::warn("{} unresolved sequences.", count);
            }

            result = sortByName(result);
            // dumpNames(result, 0);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // sort results..

    // spdlog::warn("{}", result.dump(2));

    return result;
}


QVariant ShotgunSequenceModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToFullData(index);

        switch (role) {
        case JSONTreeModel::Roles::JSONRole:
            result = QVariantMapFromJson(j);
            break;

        case JSONTreeModel::Roles::JSONTextRole:
            result = QString::fromStdString(j.dump(2));
            break;

        case Roles::idRole:
            try {
                if (j.at("id").is_number())
                    result = j.at("id").get<int>();
                else
                    result = QString::fromStdString(j.at("id").get<std::string>());
            } catch (...) {
            }
            break;

        case Roles::typeRole:
            try {
                result = QString::fromStdString(j.at("type").get<std::string>());
            } catch (...) {
            }
            break;

        case Roles::nameRole:
        case Qt::DisplayRole:
            try {
                if (j.count("name"))
                    result = QString::fromStdString(j.at("name"));
                else if (j.count("attributes"))
                    result = QString::fromStdString(j.at("attributes").at("code"));
            } catch (...) {
            }
            break;

        default:
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}


bool ShotgunFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

    static const QString qtrue("true");
    static const QString qfalse("false");
    // check level
    if (not selection_filter_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        if (not selection_filter_.contains(index))
            return false;
    }

    if (not roleFilterMap_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        for (const auto &[k, v] : roleFilterMap_) {
            if (not v.isEmpty()) {
                if (v == "None" and k == ShotgunListModel::Roles::pipelineStepRole)
                    continue;
                try {
                    auto qv = sourceModel()->data(index, k).toString();

                    if (v == qtrue or v == qfalse) {
                        if (v == qtrue and not sourceModel()->data(index, k).toBool())
                            return false;
                        else if (v == qfalse and sourceModel()->data(index, k).toBool())
                            return false;

                    } else if (v != qv)
                        return false;
                } catch (...) {
                }
            }
        }
    }
    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool ShotgunFilterModel::lessThan(
    const QModelIndex &source_left, const QModelIndex &source_right) const {
    auto left_selected  = selection_sort_.contains(source_left);
    auto right_selected = selection_sort_.contains(source_right);

    if (left_selected and not right_selected)
        return sortOrder() == Qt::AscendingOrder;

    if (not left_selected and right_selected)
        return sortOrder() == Qt::DescendingOrder;

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QString ShotgunFilterModel::getRoleFilter(const QString &role) const {
    auto id = getRoleId(role);
    QString result;

    if (roleFilterMap_.count(id))
        result = roleFilterMap_.at(id);

    return result;
}

void ShotgunFilterModel::setRoleFilter(const QString &filter, const QString &role) {
    auto id = getRoleId(role);
    if (not roleFilterMap_.count(id) or roleFilterMap_.at(id) != filter) {
        roleFilterMap_[id] = filter;
        invalidateFilter();
    }
}

int ShotgunFilterModel::search(const QVariant &value, const QString &role, const int start) {
    auto smodel = dynamic_cast<ShotgunListModel *>(sourceModel());
    int role_id = -1;
    auto row    = -1;

    QHashIterator<int, QByteArray> it(smodel->roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    auto indexes = match(createIndex(start, 0), role_id, value);

    if (not indexes.empty())
        row = indexes[0].row();

    return row;
}

QVariant ShotgunFilterModel::get(int row, const QString &role) {
    // map row to source..
    auto src_index = mapToSource(index(row, 0));
    auto smodel    = dynamic_cast<ShotgunListModel *>(sourceModel());
    return smodel->get(src_index.row(), role);
}

void ShotgunListModel::populate(const utility::JsonStore &data) {
    // dirty update..
    beginResetModel();
    data_ = data;
    endResetModel();
    emit lengthChanged();
}

utility::JsonStore ShotgunListModel::getQueryValue(
    const std::string &type, const utility::JsonStore &value, const int project_id) const {
    // look for map
    auto _type        = type;
    auto mapped_value = utility::JsonStore();

    if (query_value_cache_ == nullptr)
        return mapped_value;

    if (_type == "Author" || _type == "Recipient")
        _type = "User";

    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    try {
        auto val = value.get<std::string>();
        if (query_value_cache_->count(_type)) {
            if (query_value_cache_->at(_type).count(val)) {
                mapped_value = query_value_cache_->at(_type).at(val);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", _type, __PRETTY_FUNCTION__, err.what(), value.dump(2));
    }

    return mapped_value;
}

void ShotgunListModel::append(const QVariant &data) {
    auto jsn = mapFromValue(data);

    // no exact duplicates..
    for (const auto &i : data_)
        if (i == jsn)
            return;

    auto rows = rowCount();
    beginInsertRows(QModelIndex(), rows, rows);
    data_.push_back(jsn);
    endInsertRows();
}


int ShotgunListModel::search(const QVariant &value, const QString &role, const int start) {
    int role_id = -1;
    auto row    = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    auto indexes = match(createIndex(start, 0), role_id, value, 1, Qt::MatchFixedString);

    if (not indexes.empty())
        row = indexes[0].row();

    return row;
}

QVariant ShotgunListModel::get(int row, const QString &role) {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    return data(createIndex(row, 0), role_id);
}

QVariant ShotgunListModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();
    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {
            case Roles::nameRole:
            case Qt::DisplayRole:
                if (data.count("nameRole"))
                    result = QString::fromStdString(data.at("nameRole"));
                else if (data.count("name"))
                    result = QString::fromStdString(data.at("name"));
                else
                    result = QString::fromStdString(
                        data.at("attributes")
                            .value("name", data.at("attributes").value("code", "")));
                break;

            case Roles::idRole:
                try {
                    if (data.at("id").is_number())
                        result = data.at("id").get<int>();
                    else
                        result = QString::fromStdString(data.at("id").get<std::string>());
                } catch (...) {
                }
                break;

            case Roles::JSONRole:
            case Roles::detailRole:
                result = QVariantMapFromJson(data);
                break;

            case Roles::JSONTextRole:
                result = QString::fromStdString(data.dump(2));
                break;

            case Roles::loginRole:
                result = QString::fromStdString(
                    data.at("attributes").at("login").get<std::string>());
                break;

            case Roles::createdByRole:
            case Roles::authorRole:
                result = QString::fromStdString(
                    data.at("relationships").at("created_by").at("data").value("name", ""));
                break;

            case Roles::projectRole:
                result = QString::fromStdString(
                    data.at("relationships").at("project").at("data").value("name", ""));
                break;

            case Roles::projectIdRole:
                result = data.at("relationships").at("project").at("data").value("id", 0);
                break;

            case Roles::createdDateRole:
                result = QDateTime::fromString(
                    QStringFromStd(data.at("attributes").value("created_at", "")), Qt::ISODate);
                break;

            case Roles::thumbRole:
                result = "qrc:/feather_icons/film.svg";
                break;

            default:
                break;
            }
        }
    } catch (...) {
    }

    return result;
}

// {
//    "attributes": {
//      "code": "033_jts_seq"
//    },
//    "id": 288766,
//    "links": {
//      "self": "/api/v1/entity/sequences/288766"
//    },
//    "relationships": {
//      "shots": {
//        "data": [
//          {
//            "id": 1079204,
//            "name": "033_jts_9250",
//            "type": "Shot"
//          },


JsonStore ShotModel::getSequence(const int project_id, const int shot_id) const {
    if (sequence_map_) {
        if (sequence_map_->count(project_id)) {
            auto jsn = (*sequence_map_)[project_id]->modelData();

            for (const auto &i : jsn) {
                for (const auto &j : i.at("relationships").at("shots").at("data")) {
                    if (j.at("id") == shot_id) {
                        JsonStore result(R"({"attributes": {"code":null}, "id": 0})"_json);
                        result["id"]                 = i.at("id");
                        result["attributes"]["code"] = i.at("attributes").at("code");
                        return result;
                    }
                }
            }
        }
    }

    return JsonStore();
}

QVariant ReferenceModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {
            case Roles::resultTypeRole:
                result = "Reference";
                break;

            default:
                return ShotModel::data(index, role);
                break;
            }
        }
    } catch (...) {
    }

    return result;
}

QVariant MediaActionModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {
            case Roles::resultTypeRole:
                result = "MediaAction";
                break;

            default:
                return ShotModel::data(index, role);
                break;
            }
        }
    } catch (...) {
    }

    return result;
}

QVariant ShotModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {

            case submittedToDailiesRole:
                if (not data.at("attributes").at("sg_submit_dailies").is_null())
                    result = QDateTime::fromString(
                        QStringFromStd(data.at("attributes").at("sg_submit_dailies")),
                        Qt::ISODate);
                else if (not data.at("attributes").at("sg_submit_dailies_chn").is_null())
                    result = QDateTime::fromString(
                        QStringFromStd(data.at("attributes").at("sg_submit_dailies_chn")),
                        Qt::ISODate);
                else if (not data.at("attributes").at("sg_submit_dailies_mtl").is_null())
                    result = QDateTime::fromString(
                        QStringFromStd(data.at("attributes").at("sg_submit_dailies_mtl")),
                        Qt::ISODate);
                else if (not data.at("attributes").at("sg_submit_dailies_van").is_null())
                    result = QDateTime::fromString(
                        QStringFromStd(data.at("attributes").at("sg_submit_dailies_van")),
                        Qt::ISODate);
                else if (not data.at("attributes").at("sg_submit_dailies_mum").is_null())
                    result = QDateTime::fromString(
                        QStringFromStd(data.at("attributes").at("sg_submit_dailies_mum")),
                        Qt::ISODate);
                break;

            case Roles::pipelineStepRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_pipeline_step", ""));
                break;
            case Roles::frameRangeRole:
                result = QString::fromStdString(data.at("attributes").value("frame_range", ""));
                break;
            case Roles::versionRole:
                result = data.at("attributes").value("sg_dneg_version", 0);
                break;

            case Roles::pipelineStatusRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_status_list", ""));
                break;

            case Roles::pipelineStatusFullRole:
                result = QString::fromStdString(
                    getQueryValue(
                        "Pipeline Status", data.at("attributes").at("sg_status_list"), -1)
                        .get<std::string>());
                break;

            case Roles::productionStatusRole:
                result = QString::fromStdString(
                    data.at("attributes").value("sg_production_status", ""));
                break;

            case Roles::dateSubmittedToClientRole:
                result = QDateTime::fromString(
                    QStringFromStd(
                        data.at("attributes").value("sg_date_submitted_to_client", "")),
                    Qt::ISODate);
                break;

            case Roles::shotRole:
                if (data.at("relationships").at("entity").at("data").at("type") == "Shot")
                    result = QString::fromStdString(
                        data.at("relationships").at("entity").at("data").at("name"));
                break;

            case Roles::assetRole:
                if (data.at("relationships").at("entity").at("data").at("type") == "Asset")
                    result = QString::fromStdString(
                        data.at("relationships").at("entity").at("data").at("name"));
                break;

            case Roles::movieRole:
                result = QString::fromStdString(data.at("attributes").at("sg_path_to_movie"));
                break;

            case Roles::sequenceRole: {
                if (data.at("relationships").at("entity").at("data").value("type", "") ==
                    "Sequence") {
                    result = QString::fromStdString(
                        data.at("relationships").at("entity").at("data").value("name", ""));
                } else {
                    auto seq_data = getSequence(
                        data.at("relationships").at("project").at("data").value("id", 0),
                        data.at("relationships").at("entity").at("data").value("id", 0));
                    result = QString::fromStdString(seq_data.at("attributes").at("code"));
                }
            } break;

            case Roles::frameSequenceRole:
                result = QString::fromStdString(data.at("attributes").at("sg_path_to_frames"));
                break;

            case Roles::noteCountRole:
                try {
                    result = static_cast<int>(
                        data.at("relationships").at("notes").at("data").size());
                } catch (...) {
                    result = static_cast<int>(0);
                }
                break;

            case Roles::onSiteMum:
                try {
                    result = data.at("attributes").at("sg_on_disk_mum") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_mum") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;
            case Roles::onSiteMtl:
                try {
                    result = data.at("attributes").at("sg_on_disk_mtl") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_mtl") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;
            case Roles::onSiteVan:
                try {
                    result = data.at("attributes").at("sg_on_disk_van") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_van") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;
            case Roles::onSiteChn:
                try {
                    result = data.at("attributes").at("sg_on_disk_chn") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_chn") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;
            case Roles::onSiteLon:
                try {
                    result = data.at("attributes").at("sg_on_disk_lon") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_lon") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;

            case Roles::onSiteSyd:
                try {
                    result = data.at("attributes").at("sg_on_disk_syd") == "Full"      ? 2
                             : data.at("attributes").at("sg_on_disk_syd") == "Partial" ? 1
                                                                                       : 0;
                } catch (...) {
                    result = 0;
                }
                break;


            case Roles::twigNameRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_twig_name", ""));
                break;

            case Roles::tagRole: {
                auto tmp = QStringList();
                for (const auto &i : data.at("relationships").at("tags").at("data")) {
                    auto name = QStringFromStd(i.at("name").get<std::string>());
                    name.replace(QRegExp("\\.REFERENCE$"), "");
                    tmp.append(name);
                }
                result = tmp;
            } break;

            case Roles::twigTypeRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_twig_type", ""));
                break;

            case Roles::stalkUuidRole:
                result =
                    QUuidFromUuid(utility::Uuid(data.at("attributes").at("sg_ivy_dnuuid")));
                break;

            case Roles::URLRole:
                result = QStringFromStd(std::string(
                    fmt::format("http://shotgun/detail/Version/{}", data.at("id").get<int>())));
                break;

            case Roles::resultTypeRole:
                result = "Shot";
                break;

            case Roles::thumbRole:
                if (not data.at("attributes").at("image").is_null())
                    result = QStringFromStd(fmt::format(
                        "image://shotgun/thumbnail/{}/{}",
                        data.value("type", ""),
                        data.value("id", 0)));
                break;

            default:
                return ShotgunListModel::data(index, role);
                break;
            }
        }
    } catch (const std::exception &err) {
        // spdlog::warn("{}", err.what());
    }

    return result;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {
            case Roles::itemCountRole:
                try {
                    result = static_cast<int>(
                        data.at("relationships").at("versions").at("data").size());
                } catch (...) {
                    result = static_cast<int>(-1);
                }
                break;

            case Roles::typeRole:
                result = QString::fromStdString(data.at("attributes").value("sg_type", ""));
                break;

            case Roles::departmentRole:
                result = QString::fromStdString(data.at("relationships")
                                                    .at("sg_department_unit")
                                                    .at("data")
                                                    .value("name", ""));
                break;

            case Roles::siteRole:
                result = QString::fromStdString(data.at("attributes").value("sg_location", ""));
                break;

            case Roles::noteCountRole:
                try {
                    result = static_cast<int>(
                        data.at("relationships").at("notes").at("data").size());
                } catch (...) {
                    result = static_cast<int>(0);
                }
                break;

            case Roles::resultTypeRole:
                result = "Playlist";
                break;

            case Roles::createdDateRole:
                result = QDateTime::fromString(
                    QStringFromStd(data.at("attributes").value("created_at", "")), Qt::ISODate);
                break;

            case Roles::URLRole:
                result = QStringFromStd(std::string(fmt::format(
                    "http://shotgun/detail/Playlist/{}", data.at("id").get<int>())));
                break;

            case Roles::thumbRole:
                result = QStringFromStd(fmt::format(
                    "image://shotgun/thumbnail/{}/{}",
                    data.value("type", ""),
                    data.value("id", 0)));
                break;

            default:
                return ShotgunListModel::data(index, role);
                break;
            }
        }
    } catch (...) {
    }

    return result;
}

QVariant NoteModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.row() > -1 and index.row() < static_cast<int>(data_.size())) {
            const auto &data = data_[index.row()];

            switch (role) {
            case Roles::nameRole:
            case Qt::DisplayRole: {
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type") == "Version") {
                        result = QStringFromStd(i.at("name"));
                        break;
                    } else if (i.at("type") == "Shot") {
                        result = QStringFromStd(i.at("name"));
                    } else if (result.isNull() and i.at("type") == "Playlist") {
                        result = QStringFromStd(i.at("name"));
                    }
                }
            } break;

            case Roles::subjectRole:
                result = QString::fromStdString(data.at("attributes").value("subject", ""));
                break;

            case Roles::contentRole:
                result = QString::fromStdString(data.at("attributes").value("content", ""));
                break;

            case Roles::siteRole:
                result = QString::fromStdString(data.at("attributes").value("sg_location", ""));
                break;

            case Roles::pipelineStepRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_pipeline_step", ""));
                break;

            case Roles::noteTypeRole:
                result =
                    QString::fromStdString(data.at("attributes").value("sg_note_type", ""));
                break;

            case Roles::clientNoteRole:
                result = data.at("attributes").value("client_note", false);
                break;

            case Roles::URLRole:
                result = QStringFromStd(std::string(
                    fmt::format("http://shotgun/detail/Note/{}", data.at("id").get<int>())));
                break;

            case Roles::addressingRole: {
                auto tmp = QStringList();
                for (const auto &i : data.at("relationships").at("addressings_to").at("data"))
                    tmp.append(QStringFromStd(i.at("name").get<std::string>()));
                result = tmp;
            } break;

            case Roles::artistRole:
                result = QString::fromStdString(data.at("attributes").value("sg_artist", ""));
                break;

            case Roles::shotNameRole:
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Shot") {
                        result = QString::fromStdString(i.at("name").get<std::string>());
                        break;
                    }
                }
                break;

            case Roles::twigNameRole:
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Version") {
                        static std::regex remove_version_re(R"(_[VvSs]\d+$)");
                        result = QString::fromStdString(std::regex_replace(
                            i.at("name").get<std::string>(), remove_version_re, ""));
                        break;
                    }
                }
                break;

            case Roles::playlistNameRole:
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Playlist") {
                        result = QString::fromStdString(i.at("name").get<std::string>());
                        break;
                    }
                }
                break;

            case Roles::linkedVersionsRole: {
                auto v = QVariantList();
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Version") {
                        v.append(QVariant::fromValue(i.at("id").get<int>()));
                        break;
                    }
                }
                result = v;
            } break;

            case Roles::versionNameRole:
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type").get<std::string>() == "Version") {
                        result = QString::fromStdString(i.at("name").get<std::string>());
                        break;
                    }
                }
                break;

            case Roles::resultTypeRole:
                result = "Note";
                break;

            case Roles::thumbRole:
                for (const auto &i : data.at("relationships").at("note_links").at("data")) {
                    if (i.at("type") == "Version") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgun/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                        break;
                    } else if (i.at("type") == "Shot") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgun/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                    } else if (result.isNull() and i.at("type") == "Playlist") {
                        result = QStringFromStd(fmt::format(
                            "image://shotgun/thumbnail/{}/{}",
                            i.value("type", ""),
                            i.value("id", 0)));
                    }
                }
                break;

            default:
                return ShotgunListModel::data(index, role);
                break;
            }
        }
    } catch (const std::exception & /*err*/) {
        // spdlog::warn("{}", err.what());
    }

    return result;
}

QVariant EditModel::data(const QModelIndex &index, int role) const {
    if (role == Roles::resultTypeRole)
        return "Edit";

    return ShotgunListModel::data(index, role);
}


void ShotgunTreeModel::populate(const utility::JsonStore &data) {
    // dirty update..
    setModelData(data);

    setActivePreset();
    emit lengthChanged();

    checkForActiveFilter();
    checkForActiveLiveLink();
}

void ShotgunTreeModel::checkForActiveLiveLink() {
    try {
        bool active = false;
        if (active_preset_ != -1 and active_preset_ < static_cast<int>(data_.size())) {

            auto active_node = data_.begin();
            std::advance(active_node, active_preset_);

            for (const auto &j : *active_node) {
                if (j.data().value("enabled", false) and j.data().value("livelink", false)) {
                    active = true;
                    break;
                }
            }
        }

        if (active != has_active_live_link_) {
            has_active_live_link_ = active;
            emit hasActiveLiveLinkChanged();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotgunTreeModel::checkForActiveFilter() {

    try {
        bool active = false;
        if (active_preset_ != -1 and active_preset_ < static_cast<int>(data_.size())) {

            auto active_node = data_.begin();
            std::advance(active_node, active_preset_);

            for (const auto &j : *active_node) {
                if (j.data().at("enabled")) {
                    active = true;
                    break;
                }
            }
        }

        if (active != has_active_filter_) {
            has_active_filter_ = active;
            emit hasActiveFilterChanged();
        }
    } catch (...) {
    }
}


bool ShotgunTreeModel::removeRows(int row, int count, const QModelIndex &parent) {
    auto result = JSONTreeModel::removeRows(row, count, parent);

    if (result) {
        if (active_preset_ >= row) {
            setActivePreset(std::max(active_preset_ - count, -1));
        }

        checkForActiveFilter();
        checkForActiveLiveLink();
    }

    return result;
}

bool ShotgunTreeModel::insert(int row, const QModelIndex &parent, const QVariant &data) {
    bool result = insertRows(row, 1, parent);

    if (result) {
        if (not data.isNull()) {
            setData(index(row, 0, parent), data, JSONTreeModel::Roles::JSONRole);
        }

        checkForActiveFilter();
        checkForActiveLiveLink();
    }

    return result;
}

void ShotgunTreeModel::clearLoaded() {
    setActivePreset();
    checkForActiveFilter();
    checkForActiveLiveLink();
}

void ShotgunTreeModel::setType(const int row, const QString &type) {
    if (row < static_cast<int>(data_.size())) {
        set(row, type, "typeRole", QModelIndex());
    }
}


void ShotgunTreeModel::clearLiveLinks() {
    try {
        auto i_ind = 0;
        for (const auto &i : data_) {
            auto j_ind = 0;
            for (const auto &j : i) {
                if (j.data().value("livelink", false)) {
                    set(j_ind, "", "argRole", index(i_ind, 0, QModelIndex()));
                }
                j_ind++;
            }
            i_ind++;
        }

        // for (size_t i = 0; i < data_.size(); i++) {
        //     for (size_t j = 0; j < data_.at(i).at(children_).size(); j++) {
        //         if (data_.at(i).at(children_).at(j).value("livelink", false))
        //             set(j, "", "argRole", index(i, 0, QModelIndex()));
        //     }
        // }
    } catch (...) {
    }
}

void ShotgunTreeModel::clearExpanded() {
    for (size_t i = 0; i < data_.size(); i++) {
        set(i, false, "expandedRole", QModelIndex());
    }
}

QVariant ShotgunTreeModel::get(int row, const QModelIndex &parent, const QString &role) const {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    return data(index(row, 0, parent), role_id);
}

QVariant ShotgunTreeModel::get(const QModelIndex &index, const QString &role) const {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    return data(index, role_id);
}


int ShotgunTreeModel::search(
    const QVariant &value, const QString &role, const QModelIndex &parent, const int start) {
    int role_id = -1;
    auto row    = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    auto indexes = match(index(start, 0, parent), role_id, value);

    if (not indexes.empty())
        row = indexes[0].row();

    return row;
}

bool ShotgunTreeModel::set(
    int row, const QVariant &value, const QString &role, const QModelIndex &parent) {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    return setData(index(row, 0, parent), value, role_id);
}

JsonStore ShotgunTreeModel::getSequence(const int project_id, const int shot_id) const {
    if (sequence_map_) {
        if (sequence_map_->count(project_id)) {
            auto jsn = (*sequence_map_)[project_id]->modelData();

            for (const auto &i : jsn) {
                for (const auto &j : i.at("relationships").at("shots").at("data")) {
                    if (j.at("id") == shot_id) {
                        JsonStore result(R"({"attributes": {"code":null}, "id": 0})"_json);
                        result["id"]                 = i.at("id");
                        result["attributes"]["code"] = i.at("attributes").at("code");
                        return result;
                    }
                }
            }
        }
    }

    return JsonStore();
}

QVariant ShotgunTreeModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case JSONTreeModel::Roles::JSONRole:
            result = QVariantMapFromJson(indexToFullData(index));
            break;

        case JSONTreeModel::Roles::JSONTextRole:
            result = QString::fromStdString(indexToFullData(index).dump(2));
            break;

        case Roles::enabledRole:
            result = j.at("enabled").get<bool>();
            break;

        case Roles::termRole:
            result = QString::fromStdString(j.at("term"));
            break;

        case Roles::liveLinkRole:
            try {
                result = j.at("livelink").get<bool>();
            } catch (...) {
            }
            break;

        case Roles::negationRole:
            try {
                result = j.at("negated").get<bool>();
            } catch (...) {
            }
            break;

        case Roles::argRole:
            result = QString::fromStdString(j.at("value"));
            break;

        case Roles::detailRole:
            if (j.count("detail"))
                result = QVariantMapFromJson(j.at("detail"));
            break;

        case Roles::idRole:
            try {
                if (j.at("id").is_number())
                    result = j.at("id").get<int>();
                else
                    result = QString::fromStdString(j.at("id").get<std::string>());
            } catch (...) {
            }
            break;

        case Roles::nameRole:
        case Qt::DisplayRole:
            result = QString::fromStdString(j.at("name"));
            break;

        case Roles::typeRole:
            result = QString::fromStdString(j.value("type", "user"));
            break;

        case Roles::expandedRole:
            result = j.at("expanded").get<bool>();
            break;

        case Roles::queriesRole:
            // return index..
            result = index;
            break;

        default:
            break;
        }
    } catch (const std::exception &err) {

        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalId());
    }

    return result;
}


bool ShotgunTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result      = false;
    bool sync_change = true;

    QVector<int> roles({role});

    try {
        nlohmann::json &j = indexToData(index);
        result            = true;
        switch (role) {

        case JSONTreeModel::Roles::JSONRole: {
            // more involved..
            nlohmann::json jval;

            if (std::string(value.typeName()) == "QJSValue") {
                jval = nlohmann::json::parse(
                    QJsonDocument::fromVariant(value.value<QJSValue>().toVariant())
                        .toJson(QJsonDocument::Compact)
                        .constData());
            } else {
                jval = nlohmann::json::parse(QJsonDocument::fromVariant(value)
                                                 .toJson(QJsonDocument::Compact)
                                                 .constData());
            }

            // we now need to update / replace the TreeNode..
            auto new_node = json_to_tree(jval, "queries");
            auto old_node = indexToTree(index);
            // remove old children
            old_node->clear();
            // replace data..
            old_node->data() = new_node.data();
            // copy children
            old_node->splice(old_node->end(), new_node.base());

            refreshLiveLinks();
            checkForActiveFilter();
            checkForActiveLiveLink();
            roles.clear();
        } break;

        case Roles::enabledRole:
            j["enabled"] = value.toBool();
            checkForActiveFilter();
            checkForActiveLiveLink();
            break;

        case Roles::liveLinkRole:
            j["livelink"] = value.toBool();
            if (value.toBool())
                updateLiveLink(index);

            checkForActiveLiveLink();
            break;

        case Roles::negationRole:
            j["negated"] = value.toBool();
            break;

        case Roles::idRole:
            if (value.type() == QVariant::String) {
                j["id"] = value.toByteArray();
            } else {
                j["id"] = value.toInt();
            }
            break;

        case Roles::termRole:
            if (j.value("livelink", false) or j.value("dynamic", false))
                sync_change = false;
            j["term"] = value.toByteArray();
            checkForActiveLiveLink();
            break;

        case Roles::argRole:
            if (j.value("livelink", false) or j.value("dynamic", false))
                sync_change = false;

            j["value"] = value.toByteArray();
            checkForActiveLiveLink();
            break;

        case Roles::detailRole:
            if (value.isNull())
                j["detail"] = nullptr;
            else
                j["detail"] = nlohmann::json::parse(
                    QJsonDocument::fromVariant(value.value<QJSValue>().toVariant())
                        .toJson(QJsonDocument::Compact)
                        .constData());
            break;

        case Roles::nameRole:
            j["name"] = value.toByteArray();
            break;

        case Roles::typeRole:
            j["type"] = value.toByteArray();
            break;

        case Roles::expandedRole:
            sync_change   = false;
            j["expanded"] = value.toBool();
            break;

        default:
            result = false;
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            StdFromQString(value.toString()));
        result = false;
    }

    if (result) {
        // ignore these state changes. We don't want to sync these to disk.
        if (sync_change)
            emit ShotgunTreeModel::modelChanged();

        emit dataChanged(index, index, roles);
    }

    return result;
}

void ShotgunTreeModel::updateLiveLinks(const utility::JsonStore &data) {
    live_link_data_ = data;

    auto shot = QStringFromStd(
        applyLiveLinkValue(JsonStore(R"({"term":"Shot"})"_json), live_link_data_));
    auto sequence = QStringFromStd(
        applyLiveLinkValue(JsonStore(R"({"term":"Sequence"})"_json), live_link_data_));

    if (active_shot_ != shot) {
        active_shot_ = shot;
        emit activeShotChanged();
    }

    if (active_seq_ != sequence) {
        active_seq_ = sequence;
        emit activeSeqChanged();
    }

    refreshLiveLinks();
}

void ShotgunTreeModel::refreshLiveLinks() {

    try {
        auto i_ind = 0;
        for (const auto &i : data_) {
            auto j_ind = 0;
            for (const auto &j : i) {
                if (j.data().value("livelink", false)) {
                    updateLiveLink(index(j_ind, 0, index(i_ind, 0, QModelIndex())));
                }
                j_ind++;
            }
            i_ind++;
        }
    } catch (...) {
    }
}

QVariant ShotgunTreeModel::applyLiveLink(const QVariant &preset, const QVariant &livelink) {
    JsonStore result;

    try {
        auto preset_jsn = JsonStore(nlohmann::json::parse(
            QJsonDocument::fromVariant(preset.value<QJSValue>().toVariant())
                .toJson(QJsonDocument::Compact)
                .constData()));

        auto livelink_jsn =
            JsonStore(nlohmann::json::parse(StdFromQString(livelink.value<QString>())));

        result = applyLiveLink(preset_jsn, livelink_jsn);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        return preset;
    }

    return QVariantMapFromJson(result);
}

int ShotgunTreeModel::getProjectId(const QVariant &livelink) const {
    int result = -1;

    try {
        auto livelink_jsn =
            JsonStore(nlohmann::json::parse(StdFromQString(livelink.value<QString>())));

        result = livelink_jsn.at("metadata")
                     .at("shotgun")
                     .at("version")
                     .at("relationships")
                     .at("project")
                     .at("data")
                     .at("id")
                     .get<int>();

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


JsonStore ShotgunTreeModel::applyLiveLink(const JsonStore &preset, const JsonStore &livelink) {
    JsonStore result = preset;

    auto shot =
        QStringFromStd(applyLiveLinkValue(JsonStore(R"({"term":"Shot"})"_json), livelink));
    auto sequence =
        QStringFromStd(applyLiveLinkValue(JsonStore(R"({"term":"Sequence"})"_json), livelink));

    if (active_shot_ != shot) {
        active_shot_ = shot;
        emit activeShotChanged();
    }

    if (active_seq_ != sequence) {
        active_seq_ = sequence;
        emit activeSeqChanged();
    }

    try {
        if (not result.is_null()) {
            for (int j = 0; j < static_cast<int>(result.at(children_).size()); j++) {
                if (result.at(children_).at(j).value("livelink", false)) {
                    result[children_][j]["value"] =
                        applyLiveLinkValue(result.at(children_).at(j), livelink);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

JsonStore
ShotgunTreeModel::applyLiveLinkValue(const JsonStore &query, const JsonStore &livelink) {
    JsonStore result("");

    try {
        if (not query.is_null() and not livelink.is_null()) {
            auto term = query.value("term", "");

            if (livelink.contains(json::json_pointer("/metadata/shotgun/version"))) {
                if (term == "Version Name") {
                    result = livelink.at(
                        json::json_pointer("/metadata/shotgun/version/attributes/code"));
                } else if (term == "Older Version" or term == "Newer Version") {
                    auto val = livelink
                                   .at(json::json_pointer(
                                       "/metadata/shotgun/version/attributes/sg_dneg_version"))
                                   .get<long>();
                    result = nlohmann::json(std::to_string(val));
                } else if (term == "Author" or term == "Recipient") {
                    result = livelink.at(json::json_pointer(
                        "/metadata/shotgun/version/relationships/user/data/name"));
                } else if (term == "Shot") {
                    result = livelink.at(json::json_pointer(
                        "/metadata/shotgun/version/relationships/entity/data/name"));
                } else if (term == "Twig Name") {
                    result = nlohmann::json(
                        std::string("^") +
                        livelink
                            .at(json::json_pointer(
                                "/metadata/shotgun/version/attributes/sg_twig_name"))
                            .get<std::string>() +
                        std::string("$"));
                } else if (term == "Pipeline Step") {
                    result = livelink.at(json::json_pointer(
                        "/metadata/shotgun/version/attributes/sg_pipeline_step"));
                } else if (term == "Twig Type") {
                    result = livelink.at(json::json_pointer(
                        "/metadata/shotgun/version/attributes/sg_twig_type"));
                } else if (term == "Sequence") {
                    auto type = livelink.at(json::json_pointer(
                        "/metadata/shotgun/version/relationships/entity/data/type"));
                    if (type == "Sequence") {
                        result = livelink.at(json::json_pointer(
                            "/metadata/shotgun/version/relationships/entity/data/name"));
                    } else {
                        auto project_id =
                            livelink
                                .at(json::json_pointer(
                                    "/metadata/shotgun/version/relationships/project/data/id"))
                                .get<int>();
                        auto shot_id =
                            livelink
                                .at(json::json_pointer(
                                    "/metadata/shotgun/version/relationships/entity/data/id"))
                                .get<int>();
                        auto seq_data = getSequence(project_id, shot_id);
                        if (not seq_data.is_null())
                            result = seq_data.at("attributes").at("code");
                    }
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}", __PRETTY_FUNCTION__, err.what(), query.dump(2), livelink.dump(2));
    }

    return result;
}

void ShotgunTreeModel::setActivePreset(const int row) {
    if (active_preset_ != row) {
        active_preset_ = row;
        checkForActiveFilter();
        checkForActiveLiveLink();

        emit activePresetChanged();
        if (active_preset_ >= 0) {
            try {
                if (active_preset_ < static_cast<int>(data_.size())) {
                    auto row = data_.begin();
                    std::advance(row, active_preset_);
                    if (not row->empty()) {
                        auto jsn = row->front().data();
                        if (jsn.at("term") == "Shot" and jsn.value("livelink", false) and
                            active_shot_ != QStringFromStd(jsn.at("value"))) {
                            active_shot_ = QStringFromStd(jsn.at("value"));
                            emit activeShotChanged();
                        } else if (
                            jsn.at("term") == "Sequence" and jsn.value("livelink", false) and
                            active_seq_ != QStringFromStd(jsn.at("value"))) {
                            active_seq_ = QStringFromStd(jsn.at("value"));
                            emit activeSeqChanged();
                        }
                    }
                }
            } catch (...) {
            }
        }
    }
}

void ShotgunTreeModel::updateLiveLink(const QModelIndex &index) {
    // spdlog::warn("updateLiveLink {}", live_link_data_.dump(2));
    try {
        auto jsn    = indexToData(index);
        auto value  = jsn.at("value");
        auto result = applyLiveLinkValue(jsn, live_link_data_);

        if (result != value) {
            set(index.row(),
                QVariant::fromValue(QStringFromStd(result)),
                QStringFromStd("argRole"),
                index.parent());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        set(index.row(),
            QVariant::fromValue(QString("")),
            QStringFromStd("argRole"),
            index.parent());
    }
}

#include "shotgun_model_ui.moc"


// Sequence

//   "metadata": {
//     "shotgun": {
//       "version": {
//         "attributes": {
//           "code": "O_078_las_0670_lighting_slap_v009",
//           "created_at": "2022-09-19T10:33:09Z",
//           "frame_count": 49,
//           "frame_range": "1041-1089",
//           "sg_date_submitted_to_client": null,
//           "sg_dneg_version": 9,
//           "sg_ivy_dnuuid": "47c697dd-9e23-43b2-a068-b6d7e016af5c",
//           "sg_on_disk_chn": "None",
//           "sg_on_disk_lon": "Full",
//           "sg_on_disk_mtl": "None",
//           "sg_on_disk_mum": "None",
//           "sg_on_disk_van": "None",
//           "sg_path_to_frames":
//           "/jobs/MEG2/078_las_0670/OUT/O_078_las_0670_lighting_slap_v009/4222x1768/O_078_las_0670_lighting_slap_v009.####.exr",
//           "sg_path_to_movie":
//           "/jobs/MEG2/078_las_0670/movie/out/O_078_las_0670_lighting_slap_v009/O_078_las_0670_lighting_slap_v009.dneg.mov",
//           "sg_pipeline_step": "Lighting",
//           "sg_production_status": "rev",
//           "sg_status_list": "rev",
//           "sg_twig_name": "O_078_las_0670_lighting_slap",
//           "sg_twig_type_code": "out"
//         },
//         "id": 104526415,
//         "links": {
//           "self": "/api/v1/entity/versions/104526415"
//         },
//         "relationships": {
//           "created_by": {
//             "data": {
//               "id": 26981,
//               "name": "Nessa Zhang Mingfang",
//               "type": "HumanUser"
//             },
//             "links": {
//               "related": "/api/v1/entity/human_users/26981",
//               "self": "/api/v1/entity/versions/104526415/relationships/created_by"
//             }
//           },
//           "entity": {
//             "data": {
//               "id": 1222548,
//               "name": "078_las_0670",
//               "type": "Shot"
//             },
//             "links": {
//               "related": "/api/v1/entity/shots/1222548",
//               "self": "/api/v1/entity/versions/104526415/relationships/entity"
//             }
//           },
//           "notes": {
//             "data": [
//               {
//                 "id": 24872042,
//                 "name": "MEG2 : Dailies : O_078_las_0670_lighting_slap_v009 : london - Submit
//                 to Dailies : O_078_las_0670_lighting_slap_v009\nupdated visor comp", "type":
//                 "Note"
//               }
//             ],
//             "links": {
//               "self": "/api/v1/entity/versions/104526415/relationships/notes"
//             }
//           },
//           "project": {
//             "data": {
//               "id": 1419,
//               "name": "MEG2",
//               "type": "Project"
//             },
//             "links": {
//               "related": "/api/v1/entity/projects/1419",
//               "self": "/api/v1/entity/versions/104526415/relationships/project"
//             }
//           },
//           "user": {
//             "data": {
//               "id": 26981,
//               "name": "Nessa Zhang Mingfang",
//               "type": "HumanUser"
//             },
//             "links": {
//               "related": "/api/v1/entity/human_users/26981",
//               "self": "/api/v1/entity/versions/104526415/relationships/user"
//             }
//           }
//         },
//         "type": "Version"
//       }
//     }
//   }
// }
