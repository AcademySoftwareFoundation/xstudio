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

void add_subtype(std::set<std::string> &types, const nlohmann::json &value) {
    if (value != "No Type")
        types.insert(value);
}
} // namespace

nlohmann::json ShotBrowserSequenceModel::sortByNameAndType(const nlohmann::json &jsn) {
    // this needs
    auto result = jsn;
    if (result.is_array()) {
        std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) -> bool {
            try {
                if (a.at("type") == b.at("type")) {
                    // if (a.at("subtype") == b.at("subtype"))
                    return a.at("name") < b.at("name");
                    // else
                    //     return a.at("subtype") < b.at("subtype");
                }
                return a.at("type") < b.at("type");
            } catch (const std::exception &err) {
                spdlog::warn("{}", err.what());
            }
            return false;
        });
    }

    for (auto &item : result) {
        if (item.count("children")) {
            item["children"] = sortByNameAndType(item.at("children"));
        }
    }

    return result;
}

nlohmann::json
ShotBrowserSequenceModel::flatToAssetTree(const nlohmann::json &src, QStringList &_types) {
    // manipulate data into tree structure.
    // spdlog::warn("{}", src.size());
    auto result = R"([])"_json;
    std::map<std::string, nlohmann::json::json_pointer> assets;
    auto done    = false;
    auto changed = true;

    try {
        while (not done and changed) {
            changed = false;
            done    = true;
            for (const auto &i : src) {
                if (not i.at("attributes").at("sg_asset_name").is_null()) {
                    const auto path = i.at("attributes").at("sg_asset_name").get<std::string>();

                    if (not assets.count(path)) {

                        auto parent = path.substr(
                            0,
                            path.size() - path.find_last_of("/") == std::string::npos
                                ? 0
                                : path.find_last_of("/"));
                        // add root level
                        if (path == parent) {
                            auto asset        = i;
                            asset["name"]     = asset.at("attributes").at("code");
                            asset["hidden"]   = false;
                            asset["children"] = json::array();

                            result.emplace_back(asset);
                            assets.insert(std::make_pair(
                                path,
                                nlohmann::json::json_pointer(
                                    "/" + std::to_string(result.size() - 1))));
                            changed = true;
                        } else {
                            // find parent..
                            if (not assets.count(parent)) {
                                done = false;
                            } else {
                                auto asset        = i;
                                asset["name"]     = asset.at("attributes").at("code");
                                asset["children"] = json::array();
                                asset["hidden"]   = false;

                                result[assets[parent]]["children"].emplace_back(asset);

                                assets.emplace(std::make_pair(
                                    path,
                                    assets[parent] /
                                        nlohmann::json::json_pointer(
                                            std::string("/children/") +
                                            std::to_string(
                                                result[assets[parent]]["children"].size() -
                                                1))));
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        // result = sortByNameAndType(result);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // sort results..
    _types.clear();
    // for (auto i : types) {
    //     _types.push_back(QStringFromStd(i));
    // }
    // spdlog::warn("{}", result.dump(2));

    return result;
}

nlohmann::json
ShotBrowserSequenceModel::flatToTree(const nlohmann::json &src, QStringList &_types) {
    // manipulate data into tree structure.
    // spdlog::warn("{}", src.size());
    const static auto sg_shot_type     = json::json_pointer("/attributes/sg_shot_type");
    const static auto sg_sequence_type = json::json_pointer("/attributes/sg_sequence_type");

    auto result = R"([])"_json;
    std::map<size_t, nlohmann::json::json_pointer> seqs;

    // spdlog::warn("{}", src.dump(2));

    auto types = std::set<std::string>();

    try {

        if (src.is_array()) {
            auto done = false;

            while (not done) {
                done = true;

                for (auto seq : src.at(1)) {
                    try {
                        auto id = seq.at("id").get<int>();
                        // already logged ?
                        // if already there then skip

                        if (not seqs.count(id)) {
                            seq["name"]   = seq.at("attributes").at("code");
                            seq["hidden"] = false;

                            seq["subtype"] = seq.at(sg_sequence_type).is_null()
                                                 ? "No Type"
                                                 : seq.at(sg_sequence_type);
                            add_subtype(types, seq["subtype"]);


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
                                if (shots.is_array()) {
                                    seq["children"] = seq["relationships"]["shots"]["data"];
                                    for (auto &i : seq["children"]) {
                                        i["hidden"] = false;
                                        if (i.at("type") == "Shot" and not i.count("subtype")) {
                                            i["subtype"] = i.at(sg_shot_type).is_null()
                                                               ? "No Type"
                                                               : i.at(sg_shot_type);
                                            add_subtype(types, i["subtype"]);
                                        }
                                    }
                                    // seq["children"] =
                                    //     sort_by(shots,
                                    //     nlohmann::json::json_pointer("/name"));
                                } else
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
                                if (shots.is_array()) {
                                    seq["children"] = seq["relationships"]["shots"]["data"];
                                    for (auto &i : seq["children"]) {
                                        i["hidden"] = false;
                                        if (i.at("type") == "Shot" and not i.count("subtype")) {
                                            i["subtype"] = i.at(sg_shot_type).is_null()
                                                               ? "No Type"
                                                               : i.at(sg_shot_type);
                                            add_subtype(types, i["subtype"]);
                                        }
                                    }
                                    // sort_by(shots, nlohmann::json::json_pointer("/name"));
                                } else
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
            for (auto unseq : src.at(1)) {
                try {
                    auto id = unseq.at("id").get<int>();
                    // already logged ?
                    if (not seqs.count(id)) {
                        unseq["name"]    = unseq.at("attributes").at("code");
                        unseq["hidden"]  = false;
                        unseq["subtype"] = unseq.at(sg_sequence_type).is_null()
                                               ? "No Type"
                                               : unseq.at(sg_sequence_type);
                        add_subtype(types, unseq["subtype"]);


                        auto parent_id =
                            unseq["relationships"]["sg_parent"]["data"]["id"].get<int>();
                        // no parent
                        auto &shots = unseq["relationships"]["shots"]["data"];

                        if (shots.is_array()) {
                            unseq["children"] = unseq["relationships"]["shots"]["data"];
                            for (auto &i : unseq["children"]) {
                                i["hidden"] = false;

                                if (i.at("type") == "Shot" and not i.count("subtype")) {
                                    i["subtype"] = i.at(sg_shot_type).is_null()
                                                       ? "No Type"
                                                       : i.at(sg_shot_type);
                                    add_subtype(types, i["subtype"]);
                                }
                            }
                            // sort_by(shots, nlohmann::json::json_pointer("/name"));
                        } else
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

            // result is now tree of sequence shots.
            // add in any episodes..
            auto remove_seqs = std::vector<size_t>();

            for (auto ep : src.at(0)) {
                // spdlog::warn("{}", ep.dump(2));
                ep["name"]      = ep.at("attributes").at("code");
                auto parent_id  = ep.at("id").get<int>();
                ep["parent_id"] = parent_id;
                ep["children"]  = json::array();
                ep["type"]      = "Episode";
                ep["subtype"]   = "Episode";
                ep["hidden"]    = false;


                // we now need to reparent any sequences.
                for (const auto &i : ep.at("relationships").at("sg_sequences").at("data")) {
                    auto seqid = i.at("id").get<int>();
                    auto secit = seqs.find(seqid);

                    if (secit != std::end(seqs)) {
                        // copy into our children
                        ep["children"].push_back(result.at(secit->second));

                        if (result.at(secit->second).at("parent_id") == seqid) {
                            // remove from results.
                            remove_seqs.push_back(seqid);
                        }
                        seqs.erase(secit);
                    }
                }

                ep["children"] = sort_by(ep["children"], nlohmann::json::json_pointer("/name"));

                result.push_back(ep);
            }

            for (const auto &id : remove_seqs) {
                for (auto it = result.begin(); it != result.end(); ++it) {
                    if (it->at("id") == id) {
                        result.erase(it);
                        break;
                    }
                }
            }


            result = sortByNameAndType(result);
            // dumpNames(result, 0);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // sort results..
    _types.clear();
    for (auto i : types) {
        _types.push_back(QStringFromStd(i));
    }
    // spdlog::warn("{}", result.dump(2));

    return result;
}

bool ShotBrowserListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        // nlohmann::json &j = indexToData(index);
        switch (role) {

        case Roles::hiddenRole: {

            result = baseSetData(index, value, "hidden", QVector<int>({role}), true);

            std::function<void(const QModelIndex &)> changedChild =
                [&](const QModelIndex &parent) {
                    for (auto i = 0; i < rowCount(parent); i++) {
                        auto child = ShotBrowserListModel::index(i, 0, parent);
                        changedChild(child);
                    }

                    emit dataChanged(
                        ShotBrowserListModel::index(0, 0, parent),
                        ShotBrowserListModel::index(rowCount(parent) - 1, 0, parent),
                        QVector<int>({ShotBrowserListModel::Roles::parentHiddenRole}));
                };
            if (result) {
                // enable parents.
                if (not value.toBool() and index.parent().isValid()) {
                    setData(index.parent(), false, Roles::hiddenRole);
                }

                changedChild(index);
            }
        } break;

        default:
            result = JSONTreeModel::setData(index, value, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QVariant ShotBrowserListModel::data(const QModelIndex &index, int role) const {
    auto result                        = QVariant();
    const static auto sg_status_list   = json::json_pointer("/attributes/sg_status_list");
    const static auto sg_description   = json::json_pointer("/attributes/sg_description");
    const static auto sg_shot_type     = json::json_pointer("/attributes/sg_shot_type");
    const static auto sg_sequence_type = json::json_pointer("/attributes/sg_sequence_type");
    const static auto sg_unit          = json::json_pointer("/relationships/sg_unit/data/name");

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

        case Roles::hiddenRole:
            if (j.contains("hidden") and j.at("hidden").is_boolean())
                result = j.at("hidden").get<bool>();
            else
                result = false;
            break;

        case Roles::parentHiddenRole: {
            auto pi = index.parent();
            if (pi.isValid()) {
                if (data(pi, hiddenRole).toBool())
                    result = true;
                else if (data(pi, parentHiddenRole).toBool())
                    result = true;
                else
                    result = false;
            } else {
                result = false;
            }
            break;
        }

        case Roles::descriptionRole:
            if (j.contains(sg_description) and j.at(sg_description).is_string())
                result = QString::fromStdString(j.at(sg_description).get<std::string>());
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

        case Roles::statusRole:
            if (j.contains(sg_status_list))
                result = QString::fromStdString(
                    j.at(sg_status_list).is_null() ? ""
                                                   : j.at(sg_status_list).get<std::string>());
            break;

        case Roles::unitRole:
            if (j.contains(sg_unit))
                result = QString::fromStdString(j.at(sg_unit).get<std::string>());
            break;

        case Roles::subtypeRole:
            if (j.count("subtype"))
                result = QString::fromStdString(j.at("subtype").get<std::string>());
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

nlohmann::json ShotBrowserListModel::get_hidden(const QModelIndex &pindex) const {
    auto jsn = R"([])"_json;

    try {
        if (pindex.isValid()) {
            if (data(pindex, hiddenRole).toBool()) {
                auto tmp = R"({"t": null, "i": 0})"_json;
                tmp["t"] = StdFromQString(data(pindex, typeRole).toString());
                tmp["i"] = data(pindex, idRole).toInt();
                jsn.emplace_back(tmp);
            }
        }

        for (auto i = 0; i < rowCount(pindex); i++) {
            auto c = get_hidden(index(i, 0, pindex));
            jsn.insert(jsn.end(), c.begin(), c.end());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return jsn;
}


void ShotBrowserListModel::setHidden(const QVariant &fdata) {
    // iterate over entire tree reseting favourite state.
    // build map..
    auto fmap = std::map<std::string, std::set<int>>();
    auto jsn  = mapFromValue(fdata);

    for (const auto &i : jsn) {
        auto type = i.at("t").get<std::string>();
        auto id   = i.at("i").get<int>();

        if (not fmap.count(type))
            fmap[type] = std::set<int>();

        fmap[type].insert(id);
    }
    set_hidden(fmap);
}

void ShotBrowserListModel::set_hidden(
    const std::map<std::string, std::set<int>> &fmap, const QModelIndex &pindex) {
    try {
        if (pindex.isValid()) {
            auto isfav  = data(pindex, hiddenRole).toBool();
            auto type   = StdFromQString(data(pindex, typeRole).toString());
            auto id     = data(pindex, idRole).toInt();
            auto newfav = false;

            if (auto it = fmap.find(type); it != fmap.end())
                newfav = it->second.count(id);

            if (newfav != isfav)
                setData(pindex, newfav, hiddenRole);
        }

        for (auto i = 0; i < rowCount(pindex); i++)
            set_hidden(fmap, index(i, 0, pindex));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


QVariant ShotBrowserListModel::getHidden() const { return mapFromValue(get_hidden()); }


QVariant ShotBrowserFilterModel::get(const QModelIndex &item, const QString &role) const {
    auto index = mapToSource(item);
    return dynamic_cast<ShotBrowserListModel *>(sourceModel())->get(index, role);
}

bool ShotBrowserFilterModel::set(
    const QModelIndex &item, const QVariant &value, const QString &role) {
    auto index = mapToSource(item);
    return dynamic_cast<ShotBrowserListModel *>(sourceModel())->set(index, value, role);
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

bool ShotBrowserSequenceFilterModel::set(
    const QModelIndex &item, const QVariant &value, const QString &role) {
    auto index = mapToSource(item);
    return dynamic_cast<ShotBrowserListModel *>(sourceModel())->set(index, value, role);
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
    auto result                     = QVariant();
    const static auto sg_asset_name = json::json_pointer("/attributes/sg_asset_name");

    try {
        const auto &j = indexToData(index);

        switch (role) {

        case Roles::assetNameRole:
            if (j.contains(sg_asset_name))
                result = QString::fromStdString(j.at(sg_asset_name).get<std::string>());
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

    // spdlog::warn("{} {} {}", hide_not_favourite_, source_index.isValid(), not
    // source_index.data(ShotBrowserListModel::Roles::favouriteRole).toBool());

    if (not show_hidden_ and source_index.isValid() and
        source_index.data(ShotBrowserListModel::Roles::hiddenRole).toBool())
        return false;

    if (not hide_status_.empty() and source_index.isValid() and
        hide_status_.count(
            source_index.data(ShotBrowserListModel::Roles::statusRole).toString()))
        return false;

    if (not filter_unit_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto value        = index.data(ShotBrowserListModel::Roles::unitRole);
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

    if (not filter_type_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto value        = index.data(ShotBrowserListModel::Roles::subtypeRole);

        for (const auto &i : filter_type_)
            if (i == value)
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