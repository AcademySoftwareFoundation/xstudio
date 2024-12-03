// SPDX-License-Identifier: Apache-2.0
// #include "shotgun_model_ui.hpp"
// #include "data_source_shotgun_ui.hpp"
// #include "../data_source_shotgun.hpp"
// #include "../data_source_shotgun_query_engine.hpp"

#include "model_ui.hpp"

// #include "xstudio/utility/string_helpers.hpp"
// #include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/json_store.hpp"
// #include "xstudio/atoms.hpp"

#include <QDateTime>

using namespace xstudio;
using namespace xstudio::utility;
// using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;
// using namespace xstudio::global_store;

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

nlohmann::json ShotBrowserSequenceModel::sortByName(const nlohmann::json &jsn) {
    // this needs
    auto result = sort_by(jsn, nlohmann::json::json_pointer("/name"));
    for (auto &item : result) {
        if (item.count("children")) {
            item["children"] = sortByName(item.at("children"));
        }
    }

    return result;
}

nlohmann::json ShotBrowserSequenceModel::flatToTree(const nlohmann::json &src) {
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


QVariant ShotBrowserListModel::data(const QModelIndex &index, int role) const {
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

        case JSONTreeModel::Roles::idRole:
            try {
                if (j.at("id").is_number())
                    result = j.at("id").get<int>();
                else
                    result = QString::fromStdString(j.at("id").get<std::string>());
            } catch (...) {
            }
            break;

        case Roles::typeRole:
            if (j.contains("type") and j.at("type").is_string())
                result = QString::fromStdString(j.at("type").get<std::string>());
            break;

        case Roles::descriptionRole:
            if (j.contains("attributes") and j.at("attributes").contains("sg_description") and
                j.at("attributes").at("sg_description").is_string())
                result = QString::fromStdString(
                    j.at("attributes").at("sg_description").get<std::string>());
            break;

        case Roles::divisionRole:
            if (j.contains("attributes") and j.at("attributes").contains("sg_division") and
                j.at("attributes").at("sg_division").is_string())
                result = QString::fromStdString(
                    j.at("attributes").at("sg_division").get<std::string>());
            break;

        case Roles::projectStatusRole:
            if (j.contains("attributes") and
                j.at("attributes").contains("sg_project_status") and
                j.at("attributes").at("sg_project_status").is_string())
                result = QString::fromStdString(
                    j.at("attributes").at("sg_project_status").get<std::string>());
            break;

        case Roles::createdRole:
            if (j.contains("attributes") and j.at("attributes").contains("created_at") and
                j.at("attributes").at("created_at").is_string())
                result = QDateTime::fromString(
                    QStringFromStd(j.at("attributes").at("created_at").get<std::string>()),
                    Qt::ISODate);
            break;

        case Roles::nameRole:
        case Qt::DisplayRole:
            try {
                if (j.count("name"))
                    result = QString::fromStdString(j.at("name"));
                else if (j.count("attributes") and j.at("attributes").count("code"))
                    result = QString::fromStdString(j.at("attributes").at("code"));
                else if (j.count("attributes") and j.at("attributes").count("name"))
                    result = QString::fromStdString(j.at("attributes").at("name"));
            } catch (...) {
            }
            break;

        default:
            result = JSONTreeModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}

QVariant ShotBrowserFilterModel::get(const QModelIndex &item, const QString &role) const {
    auto index = mapToSource(item);
    return dynamic_cast<ShotBrowserListModel *>(sourceModel())->get(index, role);
}

QModelIndex ShotBrowserFilterModel::searchRecursive(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int depth) {
    auto smodel = dynamic_cast<ShotBrowserListModel *>(sourceModel());
    return mapFromSource(
        smodel->searchRecursive(value, role, mapToSource(parent), start, depth));
}

QVariant
ShotBrowserSequenceFilterModel::get(const QModelIndex &item, const QString &role) const {
    auto index = mapToSource(item);
    return dynamic_cast<ShotBrowserListModel *>(sourceModel())->get(index, role);
}

QModelIndex ShotBrowserSequenceFilterModel::searchRecursive(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int depth) {
    auto smodel = dynamic_cast<ShotBrowserListModel *>(sourceModel());
    return mapFromSource(
        smodel->searchRecursive(value, role, mapToSource(parent), start, depth));
}


QVariant ShotBrowserSequenceModel::data(const QModelIndex &index, int role) const {
    auto result                      = QVariant();
    const static auto sg_status_list = json::json_pointer("/attributes/sg_status_list");
    const static auto sg_unit        = json::json_pointer("/relationships/sg_unit/data/name");

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case Roles::statusRole:
            if (j.contains(sg_status_list))
                result = QString::fromStdString(j.at(sg_status_list).get<std::string>());
            break;

        case Roles::unitRole:
            if (j.contains(sg_unit))
                result = QString::fromStdString(j.at(sg_unit).get<std::string>());
            break;


        default:
            result = ShotBrowserListModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}

void ShotBrowserSequenceFilterModel::setHideStatus(const QStringList &value) {
    std::set<QString> new_value;

    for (const auto &i : value)
        new_value.insert(i);

    if (new_value != hide_status_) {
        hide_status_ = new_value;
        emit hideStatusChanged();
        invalidateFilter();
        emit filterChanged();
    }
}

QStringList ShotBrowserSequenceFilterModel::hideStatus() const {
    auto result = QStringList();

    for (const auto &i : hide_status_)
        result.push_back(i);

    return result;
}


bool ShotBrowserSequenceFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    auto result       = true;
    auto source_index = sourceModel()->index(source_row, 0, source_parent);

    if (not hide_status_.empty() and source_index.isValid() and
        hide_status_.count(
            source_index.data(ShotBrowserSequenceModel::Roles::statusRole).toString()))
        return false;

    if (not filter_unit_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto value        = index.data(ShotBrowserSequenceModel::Roles::unitRole);
        auto no_unit      = QVariant::fromValue(QString("No Unit"));
        auto shot_type    = QVariant::fromValue(QString("Shot"));

        for (const auto &i : filter_unit_)
            if (i == value)
                return false;
            else if (
                i == no_unit and
                source_index.data(ShotBrowserListModel::Roles::typeRole) == shot_type and
                (value.isNull() or value.toString() == QString()))
                return false;
    }

    if (hide_empty_ and source_index.isValid() and
        source_index.data(ShotBrowserListModel::Roles::typeRole) !=
            QVariant::fromValue(QString("Shot"))) {
        auto rc = sourceModel()->rowCount(source_index);

        // check all our children haven't been filtered..
        auto has_child = false;
        for (auto i = 0; i < rc and not has_child; i++)
            has_child = filterAcceptsRow(i, source_index);

        if (not has_child)
            return false;
    }

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool ShotBrowserFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    // check level
    if (not selection_filter_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        if (not selection_filter_.contains(index))
            return false;
    }

    if (not filter_division_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto value        = index.data(ShotBrowserListModel::Roles::divisionRole);

        if (not value.isNull()) {
            for (const auto &i : filter_division_)
                if (i == value)
                    return false;
        }
    }

    if (not filter_project_status_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto value        = index.data(ShotBrowserListModel::Roles::projectStatusRole);

        if (not value.isNull()) {
            for (const auto &i : filter_project_status_)
                if (i == value)
                    return false;
        }
    }

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}