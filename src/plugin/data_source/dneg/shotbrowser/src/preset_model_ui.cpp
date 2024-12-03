// SPDX-License-Identifier: Apache-2.0
#include "preset_model_ui.hpp"
#include "query_engine.hpp"
#include "model_ui.hpp"

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

ShotBrowserPresetModel::ShotBrowserPresetModel(QueryEngine &query_engine, QObject *parent)
    : query_engine_(query_engine), JSONTreeModel(parent) {
    setRoleNames(std::vector<std::string>(
        {"enabledRole",
         "entityRole",
         "favouriteRole",
         "groupFavouriteRole",
         "groupIdRole",
         "groupUserDataRole",
         "hiddenRole",
         "livelinkRole",
         "nameRole",
         "negatedRole",
         "parentEnabledRole",
         "termRole",
         "typeRole",
         "updateRole",
         "userDataRole",
         "valueRole"}));

    term_lists_ = new QQmlPropertyMap(this);

    for (const auto &i : ValidTerms.items()) {
        auto terms = QStringList();
        for (const auto &j : i.value()) {
            terms.push_back(QStringFromStd(j));
        }
        term_lists_->insert(QStringFromStd(i.key()), QVariant::fromValue(terms));
    }
}


QStringList ShotBrowserPresetModel::entities() const {
    auto result = QStringList();

    for (const auto &i : ValidEntities) {
        result.push_back(QStringFromStd(i));
    }

    return result;
}

QVariant ShotBrowserPresetModel::copy(const QModelIndexList &indexes) const {
    auto result = QVariant();

    if (indexes.size() == 1) {
        auto data = mapFromValue(indexes.at(0).data(JSONRole));
        QueryEngine::regenerate_ids(data);

        result = mapFromValue(data);
    } else if (indexes.size() > 1) {
        auto data = R"([])"_json;
        for (const auto &i : indexes) {
            auto d = mapFromValue(i.data(JSONRole));
            QueryEngine::regenerate_ids(d);
            data.push_back(d);
        }
        result = mapFromValue(data);
    }
    return result;
}

bool ShotBrowserPresetModel::paste(
    const QVariant &qdata, const int row, const QModelIndex &parent) {
    auto result = true;

    try {
        auto items           = R"([])"_json;
        auto data            = mapFromValue(qdata);
        auto parent_type     = StdFromQString(parent.data(typeRole).toString());
        auto current_row     = row;
        auto adjusted_parent = parent;

        if (not data.is_array()) {
            items.push_back(data);
        } else {
            items = data;
        }

        for (auto &i : items) {
            // pasting preset into group, adjust parent..
            if (parent_type == "" and
                (not i.count("type") or i.at("type") == "system" or i.at("type") == "preset")) {
                adjusted_parent = index(1, 0, parent);
                parent_type     = StdFromQString(adjusted_parent.data(typeRole).toString());
                current_row     = rowCount(adjusted_parent);
            }

            if (parent_type == "presets" and
                (not i.count("type") or i.at("type") == "system")) {
                // old style preset.
                try {
                    auto tmppreset    = PresetTemplate;
                    tmppreset["name"] = i.at("name");
                    for (const auto &j : i.at("queries")) {
                        auto term     = TermTemplate;
                        term["value"] = j.at("value");
                        term["term"]  = j.at("term");

                        if (TermProperties.count(j["term"]))
                            term.update(TermProperties[j["term"]]);

                        if (j.count("enabled"))
                            term["enabled"] = j.at("enabled");

                        if (j.count("livelink"))
                            term["livelink"] = j.at("livelink");

                        if (j.count("negated"))
                            term["negated"] = j.at("negated");
                        tmppreset["children"].push_back(term);
                    }
                    QueryEngine::regenerate_ids(tmppreset);
                    insertRows(current_row, 1, adjusted_parent, tmppreset);
                    current_row++;
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            } else if (parent_type == "presets" and i.at("type") == "preset") {
                QueryEngine::regenerate_ids(i);
                insertRows(current_row, 1, adjusted_parent, i);
                current_row++;
            } else if (parent_type == "" and i.at("type") == "group") {
                QueryEngine::regenerate_ids(i);
                insertRows(current_row, 1, parent, i);
                current_row++;
            } else {
                result = false;
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    return result;
}


void ShotBrowserPresetModel::resetPresets(const QModelIndexList &indexes) {
    for (const auto &i : indexes) {
        resetPreset(i);
    }
}

void ShotBrowserPresetModel::resetPreset(const QModelIndex &index) {
    auto updated = index.data(Roles::updateRole);
    if (!updated.isNull() and updated.toBool() == true) {
        auto sp = query_engine_.find_by_id(
            UuidFromQUuid(index.data(JSONTreeModel::Roles::idRole).toUuid()),
            query_engine_.system_presets().as_json());
        if (sp) {
            // here be dragons..
            auto model = const_cast<QAbstractItemModel *>(index.model());
            model->setData(index, mapFromValue(*sp), JSONTreeModel::Roles::JSONRole);
        }
    }
}

QModelIndex ShotBrowserPresetModel::duplicate(const QModelIndex &index) {
    auto result = QModelIndex();

    auto data = mapFromValue(index.data(JSONRole));
    QueryEngine::regenerate_ids(data);

    if (insertRows(index.row() + 1, 1, index.parent(), data)) {
        result = ShotBrowserPresetModel::index(index.row() + 1, 0, index.parent());
    }

    if (index.data(typeRole) != QVariant::fromValue(QString("term")))
        setData(result, result.data(nameRole).toString() + " - Copy", nameRole);

    return result;
}

QModelIndex ShotBrowserPresetModel::getPresetGroup(const QModelIndex &index) const {
    const auto group = QVariant::fromValue(QString("group"));
    auto result      = QModelIndex();
    auto i           = index;

    while (i.isValid()) {
        if (i.data(typeRole) == group) {
            result = i;
            break;
        }
        i = i.parent();
    }

    return result;
}


QVariant ShotBrowserPresetModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    if (not index.isValid())
        return result;

    try {
        const auto &j = indexToData(index);

        try {

            switch (role) {
            case JSONTreeModel::Roles::JSONRole:
                result = QVariantMapFromJson(indexToFullData(index));
                break;

            case JSONTreeModel::Roles::JSONTextRole:
                result = QString::fromStdString(indexToFullData(index).dump(2));
                break;

            case JSONTreeModel::Roles::JSONPathRole:
                result = QString::fromStdString(getIndexPath(index).to_string());
                break;

            case Roles::enabledRole:
                result = j.at("enabled").get<bool>();
                break;

            case Roles::parentEnabledRole: {
                auto p = index.parent();
                result = true;
                while (p.isValid() and p.data(typeRole) == QVariant("term") and
                       p.data(termRole) == QVariant("Operator")) {
                    if (not p.data(enabledRole).toBool()) {
                        result = false;
                        break;
                    }
                    p = p.parent();
                }
            } break;

            case Roles::favouriteRole:
                if (j.count("favourite") and not j.at("favourite").is_null())
                    result = j.at("favourite").get<bool>();
                break;

            case Roles::groupFavouriteRole:
                if (j.value("type", "") == "preset")
                    result = index.parent().parent().data(Roles::favouriteRole).toBool();
                break;

            case Roles::hiddenRole:
                // check parent hidden
                if (j.value("type", "") == "preset" and
                    index.parent().parent().data(Roles::hiddenRole).toBool())
                    result = true;
                else if (j.count("hidden") and not j.at("hidden").is_null())
                    result = j.at("hidden").get<bool>();
                else
                    result = false;
                break;

            case Roles::updateRole:
                if (j.count("update") and not j.at("update").is_null())
                    result = j.at("update").get<bool>();
                break;

            case Roles::userDataRole:
                result = QString::fromStdString(j.value("userdata", ""));
                break;

            case Roles::groupIdRole:
                if (j.at("type") == "group")
                    result = JSONTreeModel::data(index, JSONTreeModel::Roles::idRole);
                else
                    result = getPresetGroup(index).data(JSONTreeModel::Roles::idRole);
                break;

            case Roles::groupUserDataRole:
                if (j.at("type") == "group")
                    result = QString::fromStdString(j.value("userdata", ""));
                else
                    result = getPresetGroup(index).data(role);
                break;

            case Roles::termRole:
                result = QString::fromStdString(j.at("term"));
                break;

            case Roles::valueRole:
                result = QString::fromStdString(j.at("value"));
                break;

            case Roles::entityRole:
                if (j.count("entity"))
                    result = QString::fromStdString(j.at("entity"));
                else {
                    // find group entity..
                    auto p = index.parent();
                    while (p.isValid() and result.isNull()) {
                        result = p.data(Roles::entityRole);
                        p      = p.parent();
                    }
                }

                break;

            case Roles::livelinkRole:
                if (j.count("livelink") and not j.at("livelink").is_null())
                    result = j.at("livelink").get<bool>();
                break;

            case Roles::negatedRole:
                if (j.count("negated") and not j.at("negated").is_null())
                    result = j.at("negated").get<bool>();
                break;

            case JSONTreeModel::Roles::idRole:
                result = QString::fromStdString(to_string(j.at("id").get<Uuid>()));
                break;

            case Roles::nameRole:
            case Qt::DisplayRole:
                result = QString::fromStdString(j.at("name"));
                break;

            case Roles::typeRole:
                result = QString::fromStdString(j.at("type"));
                break;

            default:
                result = JSONTreeModel::data(index, role);
                break;
            }

        } catch (const std::exception &err) {

            spdlog::warn(
                "{} {} {} {} {} {}",
                __PRETTY_FUNCTION__,
                err.what(),
                role,
                index.row(),
                index.internalId(),
                j.dump(2));
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

void ShotBrowserPresetModel::setModelPathData(const std::string &path, const JsonStore &data) {
    // don't propagate
    if (path == "")
        setModelDataBase(data, false);
    else {
    }
}

bool ShotBrowserPresetModel::setData(
    const QModelIndex &index, const QVariant &value, int role) {
    bool result         = false;
    bool preset_changed = false;

    QVector<int> roles({role});

    try {
        // nlohmann::json &j = indexToData(index);
        switch (role) {

        case Roles::enabledRole:
            result = baseSetData(index, value, "enabled", QVector<int>({role}), true);
            if (index.data(typeRole) == QVariant("term") and
                index.data(termRole) == QVariant("Operator")) {

                std::function<void(const QModelIndex &)> changedChild =
                    [&](const QModelIndex &parent) {
                        for (auto i = 0; i < rowCount(parent); i++) {
                            auto child = ShotBrowserPresetModel::index(i, 0, parent);
                            if (child.isValid() and child.data(typeRole) == QVariant("term") and
                                child.data(termRole) == QVariant("Operator"))
                                changedChild(child);
                        }

                        emit dataChanged(
                            ShotBrowserPresetModel::index(0, 0, parent),
                            ShotBrowserPresetModel::index(rowCount(parent) - 1, 0, parent),
                            QVector<int>({parentEnabledRole}));
                    };

                changedChild(index);
            }
            preset_changed = true;
            // if changed mark update field.
            if (result)
                markedAsUpdated(index.parent());
            break;

        case Roles::favouriteRole:
            result = baseSetData(index, value, "favourite", QVector<int>({role}), true);
            if (result) {
                if (index.data(typeRole).toString() == "group") {
                    auto presets = ShotBrowserPresetModel::index(1, 0, index);
                    emit dataChanged(
                        ShotBrowserPresetModel::index(0, 0, presets),
                        ShotBrowserPresetModel::index(rowCount(presets) - 1, 0, presets),
                        QVector<int>({groupFavouriteRole}));
                }
            }
            break;

        case Roles::hiddenRole:
            result = baseSetData(index, value, "hidden", QVector<int>({role}), true);

            // if changed mark update field.
            if (result) {
                emit presetHidden(index, value.toBool());
                markedAsUpdated(index.parent());

                if (index.data(typeRole).toString() == "group") {
                    auto presets = ShotBrowserPresetModel::index(1, 0, index);
                    emit dataChanged(
                        ShotBrowserPresetModel::index(0, 0, presets),
                        ShotBrowserPresetModel::index(rowCount(presets) - 1, 0, presets),
                        roles);
                }
            }
            break;

        case Roles::negatedRole:
            result         = baseSetData(index, value, "negated", QVector<int>({role}), true);
            preset_changed = true;
            // if changed mark update field.
            if (result)
                markedAsUpdated(index.parent());
            break;

        case Roles::livelinkRole:
            result = baseSetData(index, value, "livelink", QVector<int>({role}), true);
            // if changed mark update field.
            preset_changed = true;
            if (result)
                markedAsUpdated(index.parent());
            break;

        case Roles::termRole:
            result = baseSetData(index, value, "term", QVector<int>({role}), true);
            // if changed mark update field.
            preset_changed = true;
            if (result)
                markedAsUpdated(index.parent());
            break;

        case Roles::valueRole:
            // spdlog::warn("Roles::valueRole {}", mapFromValue(value).dump(2));
            result         = baseSetData(index, value, "value", QVector<int>({role}), true);
            preset_changed = true;

            // if changed mark update field.
            if (result)
                markedAsUpdated(index.parent());
            break;

        case Roles::userDataRole:
            result = baseSetData(index, value, "userdata", QVector<int>({role}), true);
            break;

        case Roles::nameRole:
            result = baseSetData(index, value, "name", QVector<int>({role}), true);
            // if changed mark update field.
            if (result)
                markedAsUpdated(index.parent());
            break;

        default:
            result = JSONTreeModel::setData(index, value, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result and preset_changed) {
        emit presetChanged(index.parent());
    }

    return result;
}

bool ShotBrowserPresetModel::removeRows(int row, int count, const QModelIndex &parent) {
    if (parent.isValid() and parent.data(typeRole).toString() == QString("preset"))
        emit presetChanged(parent);

    return JSONTreeModel::removeRows(row, count, parent);
}

bool ShotBrowserPresetModel::receiveEvent(const utility::JsonStore &event_pair) {
    // is it an event for me ?
    // spdlog::warn("{}", event_pair.dump(2));

    auto result = false;
    try {
        const auto &event = event_pair.at("redo");

        auto type = event.value("type", "");
        auto id   = event.value("id", Uuid());

        // ignore reflected events
        if (id != model_id_) {
            if (type == "set") {
                // find index..
                auto path = nlohmann::json::json_pointer(event.value("parent", ""));
                path /= "children";
                path /= std::to_string(event.value("row", 0));
                auto index = getPathIndex(path);
                if (index.isValid()) {
                    auto data = event.value("data", nlohmann::json::object());

                    // we can't know the role..
                    for (const auto &i : data.items()) {
                        auto roles = QVector<int>();

                        try {
                            roles.push_back(role_map_.at(i.key()));
                        } catch (...) {
                        }

                        result |=
                            baseSetData(index, mapFromValue(i.value()), i.key(), roles, false);
                    }
                }
            } else {
                result = JSONTreeModel::receiveEvent(event_pair);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

QModelIndex ShotBrowserPresetModel::insertGroup(const QString &entity, const int row) {
    auto result   = QModelIndex();
    auto parent   = getPathIndex(nlohmann::json::json_pointer(""));
    auto real_row = (row == -1 ? rowCount(parent) : row);

    auto data = GroupTemplate;
    // data["id"]                = Uuid::generate();
    data["entity"] = StdFromQString(entity);
    // data["children"][0]["id"] = Uuid::generate();
    // data["children"][1]["id"] = Uuid::generate();
    data["name"] = "New Group";

    QueryEngine::regenerate_ids(data);

    if (insertRows(real_row, 1, parent, data)) {
        result = index(real_row, 0, parent);
    }

    return result;
}

QModelIndex ShotBrowserPresetModel::insertPreset(const int row, const QModelIndex &parent) {
    auto result = QModelIndex();

    auto data    = PresetTemplate;
    data["name"] = "New Preset";
    QueryEngine::regenerate_ids(data);

    if (insertRows(row, 1, parent, data)) {
        result = index(row, 0, parent);
    }

    return result;
}

QModelIndex ShotBrowserPresetModel::insertTerm(
    const QString &term, const int row, const QModelIndex &parent) {
    auto result = QModelIndex();

    auto data    = TermTemplate;
    data["term"] = StdFromQString(term);

    if (TermProperties.count(data["term"])) {
        data.update(TermProperties[data["term"]]);
    }

    QueryEngine::regenerate_ids(data);

    if (insertRows(row, 1, parent, data)) {
        result = index(row, 0, parent);
        markedAsUpdated(parent);
    }

    emit presetChanged(parent);

    return result;
}

QModelIndex ShotBrowserPresetModel::insertOperatorTerm(
    const bool anding, const int row, const QModelIndex &parent) {
    auto result = QModelIndex();

    auto data     = OperatorTermTemplate;
    data["value"] = anding ? "And" : "Or";
    QueryEngine::regenerate_ids(data);

    if (insertRows(row, 1, parent, data)) {
        result = index(row, 0, parent);
        markedAsUpdated(parent);
    }

    return result;
}

void ShotBrowserPresetModel::markedAsUpdated(const QModelIndex &parent) {
    auto preset_index = parent;
    while (preset_index.isValid() and preset_index.data(Roles::typeRole).toString() != "preset")
        preset_index = preset_index.parent();

    if (preset_index.isValid() and not preset_index.data(Roles::updateRole).isNull())
        baseSetData(
            preset_index,
            QVariant::fromValue(true),
            "update",
            QVector<int>({Roles::updateRole}),
            true);
}

QObject *ShotBrowserPresetModel::termModel(
    const QString &qterm, const QString &entity, const int project_id) {
    QObject *result = nullptr;

    auto term = StdFromQString(qterm);

    if ((term == "Author" or term == "Recipient"))
        term = "User";
    else if (term == "Disable Global")
        term += "-" + StdFromQString(entity);

    const auto key  = QueryEngine::cache_name_auto(term, project_id);
    const auto qkey = QStringFromStd(key);

    if (TermHasNoModel.count(key)) {
        // create empty model.
        if (not term_models_.contains(qkey)) {
            auto model = new ShotBrowserListModel(this);
            model->setModelData(R"([{"name": ""}])"_json);
            term_models_[qkey] = model;
        }
    } else {
        auto cache = query_engine_.get_cache(key);
        if (not cache) {
            cache = query_engine_.get_lookup(key);
        }

        if (cache) {
            if (not term_models_.contains(qkey)) {

                // if(key == "Pipeline Step")
                //     spdlog::warn("{}", cache->dump(2));

                auto model = new ShotBrowserListModel(this);
                if (cache->is_object()) {
                    auto data = R"([])"_json;

                    for (const auto i : cache->items()) {
                        auto item = R"({"name": null, "id":null})"_json;
                        // don't expose ids if they are numbers
                        if (not i.value().at("id").is_string() and
                            std::to_string(i.value().at("id").get<long>()) == i.key())
                            continue;
                        item["name"] = i.key();
                        item["id"]   = i.value().at("id");
                        data.push_back(item);
                    }
                    model->setModelData(data);
                } else if (cache->is_array()) {
                    model->setModelData(*cache);
                }

                term_models_[qkey] = model;
            } else {
                // check row count match.. ?
                auto model = dynamic_cast<ShotBrowserListModel *>(term_models_[qkey]);
                if (cache->is_array() and
                    static_cast<int>(cache->size()) != model->rowCount()) {
                    model->setModelData(*cache);
                } else if (
                    cache->is_object() and
                    static_cast<int>(cache->size()) != model->rowCount()) {
                    auto data = R"([])"_json;

                    for (const auto i : cache->items()) {
                        auto item = R"({"name": null, "id":null})"_json;
                        // don't expose ids if they are numbers
                        if (not i.value().at("id").is_string() and
                            std::to_string(i.value().at("id").get<long>()) == i.key())
                            continue;
                        item["name"] = i.key();
                        item["id"]   = i.value().at("id");
                        data.push_back(item);
                    }
                    model->setModelData(data);
                }
            }
        } else {
            // return empty model... ?
            // spdlog::warn("missing cache {}", key);
            auto model = new ShotBrowserListModel(this);
            model->setModelData(R"([])"_json);
            term_models_[qkey] = model;
        }
    }

    if (term_models_.contains(qkey)) {
        result = *(term_models_.find(qkey));
    }

    return result;
}

void ShotBrowserPresetModel::updateTermModel(const std::string &key, const bool cache) {
    const auto qkey = QStringFromStd(key);
    // if in model, refresh data.
    // spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, key, cache);

    if (term_models_.contains(qkey)) {
        auto data = query_engine_.get_cache(key);
        if (not data)
            data = query_engine_.get_lookup(key);

        if (data) {
            auto model = dynamic_cast<ShotBrowserListModel *>(term_models_[qkey]);
            if (data->is_array() and static_cast<int>(data->size()) != model->rowCount()) {
                model->setModelData(*data);
            } else if (
                data->is_object() and static_cast<int>(data->size()) != model->rowCount()) {
                auto tmp = R"([])"_json;

                for (const auto i : data->items()) {
                    auto item    = R"({"name": null, "id":null})"_json;
                    item["name"] = i.key();
                    item["id"]   = i.value().at("id");
                    tmp.push_back(item);
                }
                model->setModelData(tmp);
            }
        }
    }
}

QFuture<QString> ShotBrowserPresetModel::exportAsSystemPresetsFuture(
    const QUrl &qpath, const QModelIndex &index) const {
    return QtConcurrent::run([=]() {
        auto path = uri_to_posix_path(UriFromQUrl(qpath));
        if (fs::path(path).extension() != ".json")
            path += ".json";

        try {
            // get json from index or the lot
            auto data = R"({"type": "root", "children": []})"_json;

            if (not index.isValid()) {
                data = modelData();
            } else {
                data["children"].push_back(indexToFullData(index));
            }

            data = QueryEngine::validate_presets(data, false, JsonStore(), 0, true);

            std::ofstream myfile;
            myfile.open(path);
            myfile << data.at("children").dump(2) << std::endl;
            myfile.close();
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw;
        }

        return QStringFromStd(std::string("Presets exported to ") + path);
    });
}

QFuture<QString>
ShotBrowserPresetModel::backupPresetsFuture(const QUrl &qpath, const QModelIndex &index) const {
    return QtConcurrent::run([=]() {
        auto path = uri_to_posix_path(UriFromQUrl(qpath));
        if (fs::path(path).extension() != ".json")
            path += ".json";

        try {
            // get json from index or the lot
            auto data = R"({"type": "root", "children": []})"_json;

            if (not index.isValid()) {
                data = modelData();
            } else {
                data["children"].push_back(indexToFullData(index));
            }

            std::ofstream myfile;
            myfile.open(path);
            myfile << data.at("children").dump(2) << std::endl;
            myfile.close();
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw;
        }

        return QStringFromStd(std::string("Presets Backed up to ") + path);
    });
}

QFuture<QString> ShotBrowserPresetModel::restorePresetsFuture(const QUrl &qpath) {
    return QtConcurrent::run([=]() {
        auto path = uri_to_posix_path(UriFromQUrl(qpath));
        if (fs::path(path).extension() != ".json")
            path += ".json";

        try {
            // // get json from index or the lot
            // auto data = R"({"type": "root", "children": []})"_json;

            // if (not index.isValid()) {
            //     data = modelData();
            // } else {
            //     data["children"].push_back(indexToFullData(index));
            // }

            std::ifstream myfile;
            std::stringstream filedata;

            myfile.open(path);

            myfile >> filedata.rdbuf();

            auto restored        = RootTemplate;
            restored["id"]       = Uuid::generate();
            restored["children"] = nlohmann::json::parse(filedata.str());

            setModelDataBase(restored, true);
            myfile.close();
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw;
        }

        return QStringFromStd(std::string("Presets restored from ") + path);
    });
}

nlohmann::json JSONTreeModel::modelData() const {
    // build json from tree..
    return tree_to_json(data_, children_);
}

nlohmann::json JSONTreeModel::indexToFullData(const QModelIndex &index, const int depth) const {
    return tree_to_json(*indexToTree(index), children_, depth);
}


void ShotBrowserPresetFilterModel::setShowHidden(const bool value) {
    if (value != show_hidden_) {
        show_hidden_ = value;
        emit showHiddenChanged();
        invalidateFilter();
    }
}

void ShotBrowserPresetFilterModel::setOnlyShowFavourite(const bool value) {
    if (value != only_show_favourite_) {
        only_show_favourite_ = value;
        emit onlyShowFavouriteChanged();
        invalidateFilter();
    }
}

void ShotBrowserPresetFilterModel::setOnlyShowPresets(const bool value) {
    if (value != only_show_presets_) {
        only_show_presets_ = value;
        emit onlyShowPresetsChanged();
        invalidateFilter();
    }
}

void ShotBrowserPresetFilterModel::setIgnoreSpecialGroups(const bool value) {
    if (value != ignore_special_groups_) {
        ignore_special_groups_ = value;
        emit ignoreSpecialGroupsChanged();
        invalidateFilter();
    }
}

void ShotBrowserPresetFilterModel::setFilterGroupUserData(const QVariant &filter) {
    if (filter_group_user_data_ != filter) {
        filter_group_user_data_ = filter;
        emit filterGroupUserDataChanged();
        invalidateFilter();
    }
}

void ShotBrowserPresetFilterModel::setFilterUserData(const QVariant &filter) {
    if (filter != filter_user_data_) {
        filter_user_data_ = filter;
        emit filterUserDataChanged();
        invalidateFilter();
    }
}

bool ShotBrowserPresetFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    const static auto grp           = QVariant::fromValue(QString("group"));
    const static auto preset        = QVariant::fromValue(QString("preset"));
    const static auto presets       = QVariant::fromValue(QString("presets"));
    const static auto hidden        = QVariant::fromValue(true);
    const static auto not_favourite = QVariant::fromValue(false);

    auto accept = true;

    auto source_index = sourceModel()->index(source_row, 0, source_parent);

    if (source_index.isValid()) {
        auto type           = source_index.data(ShotBrowserPresetModel::Roles::typeRole);
        auto favoured       = QVariant();
        auto group_favoured = QVariant();
        if (only_show_favourite_) {
            favoured = source_index.data(ShotBrowserPresetModel::Roles::favouriteRole);
            group_favoured =
                source_index.data(ShotBrowserPresetModel::Roles::groupFavouriteRole);
        }

        if (only_show_presets_ and type != preset)
            accept = false;
        else if (
            not filter_group_user_data_.isNull() and
            not source_index.data(ShotBrowserPresetModel::Roles::groupUserDataRole).isNull() and
            source_index.data(ShotBrowserPresetModel::Roles::groupUserDataRole) !=
                filter_group_user_data_)
            accept = false;
        else if (
            not show_hidden_ and
            source_index.data(ShotBrowserPresetModel::Roles::hiddenRole) == hidden)
            accept = false;
        else if (
            only_show_favourite_ and type == preset and not favoured.isNull() and
            (favoured == not_favourite or group_favoured == not_favourite))
            accept = false;
        else if (
            not filter_user_data_.isNull() and type == preset and
            source_index.data(ShotBrowserPresetModel::Roles::userDataRole).toString() !=
                filter_user_data_.toString())
            accept = false;
        else if (
            ignore_special_groups_ and
            special_groups_.count(UuidFromQUuid(
                source_index.data(ShotBrowserPresetModel::Roles::groupIdRole).toUuid())))
            accept = false;
    }

    return accept;
}

bool ShotBrowserPresetTreeFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

    return true;
}