// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "json_tree_model_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;

namespace {
std::set<std::string> get_all_keys(const nlohmann::json &data) {
    std::set<std::string> keys;

    try {
        for (const auto &i : data) {
            if (i.is_array()) {
                auto tmp = get_all_keys(i);
                keys.insert(tmp.begin(), tmp.end());
            } else if (i.is_object()) {
                for (const auto &[k, v] : i.items()) {
                    keys.insert(k);
                    if (v.is_array()) {
                        auto tmp = get_all_keys(v);
                        keys.insert(tmp.begin(), tmp.end());
                    }
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return keys;
}
} // namespace

void JSONTreeModel::setModelData(const nlohmann::json &data) {
    beginResetModel();
    data_ = data;
    build_parent_map();
    endResetModel();
}

QVariant JSONTreeModel::get(const QModelIndex &item, const QString &role) const {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }

    return data(item, role_id);
}


int JSONTreeModel::countExpandedChildren(
    const QModelIndex parent, const QModelIndexList &expanded) {
    int count = 0;

    if (hasChildren(parent)) {
        // root contains all..
        if (parent == QModelIndex()) {
            count = rowCount(parent);
            for (const auto &i : expanded)
                count += rowCount(i);
            // spdlog::warn("I'm groot {}", count);
        } else if (expanded.contains(parent)) {
            count = rowCount(parent);

            // check if any of our children are also expanded.
            // this is recursive..
            for (int i = 0; i < rowCount(parent); i++)
                count += countExpandedChildren(index(i, 0, parent), expanded);

            // spdlog::warn("I'm expanded {}", count);
        }
    }
    return count;
}

bool JSONTreeModel::hasChildren(const QModelIndex &parent) const {
    auto result = false;
    try {
        if (not parent.isValid()) {
            if (data_.is_array())
                result = true;
        } else if (parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());

            if (j.contains(children_) and j.at(children_).is_array())
                result = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

int JSONTreeModel::rowCount(const QModelIndex &parent) const {
    auto count = 0;

    try {
        if (not parent.isValid()) {
            if (data_.is_array())
                count = static_cast<int>(data_.size());
        } else if (parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            if (j.contains(children_) and j.at(children_).is_array())
                count = static_cast<int>(j.at(children_).size());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return count;
}

QModelIndex JSONTreeModel::index(int row, int column, const QModelIndex &parent) const {
    auto result = QModelIndex();

    if (not parent.isValid()) {
        if (data_.is_array() and row < static_cast<int>(data_.size()))
            result = createIndex(row, column, (void *)&(data_[row]));
    } else if (parent.internalPointer()) {
        nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
        if (j.contains(children_) and j.at(children_).is_array() and
            row < static_cast<int>(j.at(children_).size())) {
            result = createIndex(row, column, (void *)&(j[children_][row]));
        }
    }

    return result;
}

void JSONTreeModel::build_parent_map(const nlohmann::json *parent_data, int parent_row) {
    bool root = false;
    if (parent_data == nullptr) {
        root        = true;
        parent_data = &data_;
    }

    const nlohmann::json *children = nullptr;

    // top level, we dont store parents for these.
    if (parent_data->is_array()) {
        parent_map_.clear();
        children = parent_data;
    } else if (
        parent_data->is_object() and parent_data->contains(children_) and
        parent_data->at(children_).is_array()) {
        children = &(parent_data->at(children_));
    }

    if (children) {
        auto row = 0;
        for (const auto &i : *children) {
            if (not root)
                parent_map_[&i] = std::make_pair(parent_row, parent_data);
            build_parent_map(&i, row);
            row++;
        }
    }
}

QModelIndex JSONTreeModel::parent(const QModelIndex &child) const {
    auto result = QModelIndex();

    if (child.isValid() and child.internalPointer()) {
        auto i = parent_map_.find((const nlohmann::json *)child.internalPointer());
        if (i != parent_map_.end())
            result = createIndex(i->second.first, 0, (void *)i->second.second);
    }

    return result;
}

QVariant JSONTreeModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.isValid() and index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
            auto field        = std::string();

            switch (role) {
            case Qt::DisplayRole:
                field = display_role_;
                break;
            case Roles::JSONRole:
                result = QVariantMapFromJson(j);
                break;
            case Roles::JSONTextRole:
                result = QString::fromStdString(j.dump(2));
                break;
            default: {
                auto id = role - Roles::LASTROLE;
                if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                    field = role_names_[id];
                }
            } break;
            }

            if (not field.empty() and j.count(field)) {
                if (j[field].is_boolean())
                    result = QVariant::fromValue(j[field].get<bool>());
                else if (j[field].is_number_integer())
                    result = QVariant::fromValue(j[field].get<int>());
                else if (j[field].is_number_unsigned())
                    result = QVariant::fromValue(j[field].get<int>());
                else if (j[field].is_number_float())
                    result = QVariant::fromValue(j[field].get<float>());
                else if (j[field].is_string())
                    result = QVariant::fromValue(QStringFromStd(j[field].get<std::string>()));
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool JSONTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        if (index.isValid() and index.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)index.internalPointer());
            switch (role) {
            case Roles::JSONRole:
                if (std::string(value.typeName()) == "QJSValue") {
                    j = nlohmann::json::parse(
                        QJsonDocument::fromVariant(value.value<QJSValue>().toVariant())
                            .toJson(QJsonDocument::Compact)
                            .constData());
                } else {
                    j = nlohmann::json::parse(QJsonDocument::fromVariant(value)
                                                  .toJson(QJsonDocument::Compact)
                                                  .constData());
                }
                build_parent_map();
                result = true;
                roles.clear();
                break;

            default: {
                // allow replacement..
                auto id    = role - Roles::LASTROLE;
                auto field = std::string();
                if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                    field = role_names_[id];
                }
                if (j.count(field)) {
                    switch (value.type()) {
                    case QMetaType::Bool:
                        j[field] = value.toBool();
                        result   = true;
                        break;
                    case QMetaType::Double:
                        j[field] = value.toDouble();
                        result   = true;
                        break;
                    case QMetaType::Int:
                        j[field] = value.toInt();
                        result   = true;
                        break;
                    case QMetaType::LongLong:
                        j[field] = value.toLongLong();
                        result   = true;
                        break;
                    case QMetaType::QString:
                        j[field] = StdFromQString(value.toString());
                        result   = true;
                        break;
                    default:
                        break;
                    }
                }

            } break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result) {
        emit dataChanged(index, index, roles);
    }

    return result;
}

bool JSONTreeModel::removeRows(int row, int count, const QModelIndex &parent) {
    auto result = false;

    try {
        nlohmann::json *children = nullptr;

        if (row >= 0 and parent.isValid() and parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            if (j.contains(children_))
                children = &(j[children_]);
        } else if (row >= 0 and not parent.isValid()) {
            children = &data_;
        }

        if (children and children->is_array() and
            row + (count - 1) < static_cast<int>(children->size())) {
            result = true;
            beginRemoveRows(parent, row, row + (count - 1));
            for (auto i = 0; i < count; i++)
                children->erase(children->begin() + row);
            build_parent_map();
            endRemoveRows();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    return result;
}

bool JSONTreeModel::moveRows(
    const QModelIndex &sourceParent,
    int sourceRow,
    int count,
    const QModelIndex &destinationParent,
    int destinationChild) {
    auto result = false;

    if (destinationChild < 0 or count < 1 or sourceRow < 0)
        return false;

    nlohmann::json *src_children = nullptr;
    nlohmann::json *dst_children = nullptr;

    try {
        // get src array
        if (sourceParent.isValid() and sourceParent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)sourceParent.internalPointer());
            if (j.contains(children_))
                src_children = &(j[children_]);
        } else if (not sourceParent.isValid()) {
            src_children = &data_;
        }

        // get dst array
        if (destinationParent.isValid() and destinationParent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)destinationParent.internalPointer());
            if (j.contains(children_))
                dst_children = &(j[children_]);
        } else if (not destinationParent.isValid()) {
            dst_children = &data_;
        }

        // check valid move..
        auto moveFirst = sourceRow;
        auto moveLast  = sourceRow + count - 1;
        auto dest      = destinationChild;

        if (moveLast >= static_cast<int>(src_children->size()))
            return false;


        if (beginMoveRows(sourceParent, moveFirst, moveLast, destinationParent, dest)) {
            // clone data
            auto tmp = R"([])"_json;
            tmp.insert(
                tmp.end(),
                src_children->begin() + moveFirst,
                src_children->begin() + moveLast + 1);

            // remove from source.
            src_children->erase(
                src_children->begin() + moveFirst, src_children->begin() + moveLast + 1);

            // insert into dst.
            dst_children->insert(dst_children->begin() + dest, tmp.begin(), tmp.end());

            // spdlog::warn("{}", dst_children->dump(2));

            build_parent_map();

            endMoveRows();
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }


    return result;
}

bool JSONTreeModel::insertRows(int row, int count, const QModelIndex &parent) {
    auto result = false;

    try {
        nlohmann::json *children = nullptr;

        if (parent.isValid() and parent.internalPointer()) {
            nlohmann::json &j = *((nlohmann::json *)parent.internalPointer());
            if (j.contains(children_))
                children = &(j[children_]);
        } else if (not parent.isValid()) {
            children = &data_;
        }

        if (children and children->is_array() and row >= 0 and
            row <= static_cast<int>(children->size())) {
            result = true;
            beginInsertRows(parent, row, row + count - 1);
            children->insert(children->begin() + row, count, R"({})"_json);
            build_parent_map();
            endInsertRows();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    return result;
}


QHash<int, QByteArray> JSONTreeModel::roleNames() const {
    QHash<int, QByteArray> roles;

    for (const auto &[k, v] : role_names) {
        roles[k] = v.c_str();
    }

    int id = Roles::LASTROLE;

    for (const auto &p : role_names_) {
        roles[id++] = p.c_str();
    }

    return roles;
}


void JSONTreeModel::setRoleNames(
    const std::vector<std::string> roles, const std::string display_role) {
    display_role_ = std::move(display_role);
    role_names_   = std::move(roles);

    // no roles but data, scan data for roles..
    if (role_names_.empty() and data_.is_array()) {
        auto keys = get_all_keys(data_);
        role_names_.reserve(keys.size());
        for (const auto &i : keys)
            role_names_.push_back(i);
    }
}

#include "json_tree_model_ui.moc"
