// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/ui/qml/json_tree_model_ui.hpp"
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

JSONTreeModel::JSONTreeModel(QObject *parent) : QAbstractItemModel(parent) {}

nlohmann::json JSONTreeModel::modelData() const {
    // build json from tree..
    return tree_to_json(data_, children_);
}

nlohmann::json JSONTreeModel::indexToFullData(const QModelIndex &index, const int depth) const {
    return tree_to_json(*indexToTree(index), children_, depth);
}

nlohmann::json &JSONTreeModel::indexToData(const QModelIndex &index) {
    return indexToTree(index)->data();
}

const nlohmann::json &JSONTreeModel::indexToData(const QModelIndex &index) const {
    return indexToTree(index)->data();
}

JsonTree *JSONTreeModel::indexToTree(const QModelIndex &index) const {
    if (not index.isValid() or index.internalPointer() == nullptr)
        throw std::runtime_error("indexToTree : Invalid index");

    // validate pointer exists in our tree..
    auto t = (utility::JsonTree *)index.internalPointer();

    // if(not data_.contains(t)) {
    //     // dump_tree(data_);
    //     // spdlog::warn("{}", tree_to_json(data_, "children").dump(2));
    //     throw std::runtime_error(fmt::format("indexToTree : Invalid pointer row {} col {} ptr
    //     {}", index.row(), index.column(), fmt::ptr(t)) );
    // }

    return t;
}

nlohmann::json::json_pointer JSONTreeModel::getIndexPath(const QModelIndex &index) const {
    nlohmann::json::json_pointer result("");

    if (index.isValid()) {
        result = tree_to_pointer(*indexToTree(index), children_);
    }

    return result;
}

QModelIndex JSONTreeModel::getPathIndex(const nlohmann::json::json_pointer &path) {
    auto result = QModelIndex();
    auto ptr    = pointer_to_tree(data_, children_, path);

    if (ptr != nullptr) {
        result = createIndex(ptr->index(), 0, (void *)ptr);
    }

    return result;
}

void JSONTreeModel::setModelData(const nlohmann::json &data) {
    beginResetModel();

    if (role_names_.empty())
        setRoleNames(data);

    data_ = json_to_tree(data, children_);

    endResetModel();
    emit lengthChanged();
    emit jsonChanged();
}

QVariant JSONTreeModel::get(const QModelIndex &item, const QString &role) const {
    return data(item, roleId(role));
}

bool JSONTreeModel::set(const QModelIndex &item, const QVariant &value, const QString &role) {
    return setData(item, value, roleId(role));
}


QModelIndex JSONTreeModel::search(
    const QVariant &value, const QString &role, const QModelIndex &parent, const int start) {
    return search(value, roleId(role), parent, start);
}

QModelIndex JSONTreeModel::search(
    const QVariant &value, const int role, const QModelIndex &parent, const int start) {
    auto result = QModelIndex();

    START_SLOW_WATCHER()
    QModelIndexList indexes = search_list(value, role, parent, start, 1);

    if (not indexes.empty())
        result = indexes[0];

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::search_list(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits) {
    return search_list(value, roleId(role), parent, start, hits);
}

QModelIndex JSONTreeModel::search_recursive(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int depth) {
    return search_recursive(value, roleId(role), parent, start, depth);
}

QModelIndexList JSONTreeModel::search_recursive_list(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {
    return search_recursive_list(value, roleId(role), parent, start, hits, depth);
}

QModelIndexList JSONTreeModel::search_list(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits) {

    return search_recursive_list_base(value, role, parent, start, hits, 0);
}

QModelIndex JSONTreeModel::search_recursive(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int depth) {

    auto result = QModelIndex();

    START_SLOW_WATCHER()

    QModelIndexList indexes = search_recursive_list(value, role, parent, start, 1, depth);
    if (not indexes.empty())
        result = indexes[0];

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::search_recursive_list_base(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int max_depth) {

    START_SLOW_WATCHER()

    QModelIndexList result = QAbstractItemModel::match(
        index(start, 0, parent),
        role,
        value,
        hits,
        Qt::MatchFlags(Qt::MatchWrap | Qt::MatchExactly));

    if (max_depth and (hits == -1 or result.size() < hits)) {
        for (int i = start; i < rowCount(parent); i++) {
            auto chd = index(i, 0, parent);
            if (hasChildren(chd)) {
                auto more_result = search_recursive_list(
                    value, role, chd, 0, hits == -1 ? -1 : hits - result.size(), max_depth - 1);
                result.append(more_result);
                if (hits != -1 and result.size() >= hits)
                    break;
            }
        }
    }

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::search_recursive_list(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {
    return search_recursive_list_base(value, role, parent, start, hits, depth);
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

bool JSONTreeModel::canFetchMore(const QModelIndex &parent) const {
    auto result = false;

    try {
        if (parent.isValid()) {
            const auto &jsn = indexToData(parent);
            if (jsn.count(children_) and jsn.at(children_).is_null())
                result = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool JSONTreeModel::hasChildren(const QModelIndex &parent) const {
    auto result = false;

    try {
        if (not parent.isValid() and not data_.empty()) {
            result = true;
        } else {
            auto node = indexToTree(parent);
            result    = not node->empty();
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
            count = static_cast<int>(data_.size());
        } else {
            auto node = indexToTree(parent);
            count     = static_cast<int>(node->size());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return count;
}

QModelIndex JSONTreeModel::index(int row, int column, const QModelIndex &parent) const {
    auto result = QModelIndex();

    try {

        if (row >= 0 and column >= 0) {

            if (not parent.isValid()) {
                if (row < static_cast<int>(data_.size()))
                    result =
                        createIndex(row, column, (void *)&(*(std::next(data_.begin(), row))));
            } else {
                auto node = indexToTree(parent);

                if (row < static_cast<int>(node->size())) {

                    result =
                        createIndex(row, column, (void *)&(*(std::next(node->begin(), row))));
                }
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

QModelIndex JSONTreeModel::parent(const QModelIndex &child) const {
    auto result = QModelIndex();
    try {
        if (child.isValid()) {
            auto cnode = indexToTree(child);

            // parent of child..
            auto pnode = cnode->parent();

            if (pnode) {
                // if(not data_.contains(pnode))
                //     throw std::runtime_error("Invalid parent pointer");
                // parent of parent, so we can find the row or parent in it's parent
                auto ppnode = pnode->parent();
                if (ppnode) {
                    // if(not data_.contains(ppnode))
                    //     throw std::runtime_error("Invalid parent parent pointer");

                    int row = 0;
                    for (const auto &i : *ppnode) {
                        if (&i == pnode)
                            break;
                        row++;
                    }

                    if (row == static_cast<int>(ppnode->size())) {
                        throw std::runtime_error("item not in parent");
                    }

                    result = createIndex(row, 0, (void *)pnode);
                }
            }
        } else {
            // throw std::runtime_error("Invalid child");
        }
    } catch (const std::exception &err) {
        spdlog::warn("can't find parent {}", err.what());
        throw;
    }

    return result;
}

QVariant JSONTreeModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);
        auto field    = std::string();

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
                field = role_names_.at(id);
            }
        } break;
        }


        if (not field.empty() and j.count(field))
            result = mapFromValue(j.at(field));

    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} role: {}",
            __PRETTY_FUNCTION__,
            err.what(),
            ((role - Roles::LASTROLE) < role_names_.size())
                ? role_names_[role - Roles::LASTROLE]
                : "unknown role");
    }

    return result;
}


bool JSONTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        nlohmann::json &j = indexToData(index);
        switch (role) {
        case Roles::JSONRole: {
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
            auto new_node = json_to_tree(jval, children_);
            auto old_node = indexToTree(index);
            // remove old children

            if (old_node->size()) {
                emit beginRemoveRows(index, 0, old_node->size() - 1);
                old_node->clear();
                emit endRemoveRows();
            }

            // replace data..
            old_node->data() = new_node.data();
            // copy children
            // this doesn't work..
            // need to invalidate/add surplus rows.
            if (new_node.size()) {
                emit beginInsertRows(index, 0, new_node.size() - 1);

                old_node->splice(old_node->end(), new_node.base());

                emit endInsertRows();
            }


            result = true;
            roles.clear();
        } break;

        default: {
            // allow replacement..
            auto id    = role - Roles::LASTROLE;
            auto field = std::string();
            if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                field = role_names_.at(id);
            }
            // if (j.count(field)) {
            j[field] = mapFromValue(value);
            result   = true;
            //}

        } break;
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
        JsonTree *node = nullptr;

        if (not parent.isValid())
            node = &data_;
        else
            node = indexToTree(parent);

        if (node and (row + (count - 1)) < static_cast<int>(node->size())) {
            result     = true;
            auto start = node->begin();
            auto end   = start;
            std::advance(start, row);
            std::advance(end, row + count);

            beginRemoveRows(parent, row, row + (count - 1));

            node->erase(start, end);

            endRemoveRows();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    if (result) {
        if (parent == QModelIndex())
            emit lengthChanged();

        emit jsonChanged();
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

    // spdlog::warn("moveRows");

    // for(const auto &i: persistentIndexList()) {
    //     spdlog::warn("before {} {}", i.row(), getIndexPath(i.internalId()).to_string());
    // }

    if (destinationChild < 0 or count < 1 or sourceRow < 0)
        return false;

    JsonTree *src_children = nullptr;
    JsonTree *dst_children = nullptr;

    try {
        // get src array
        if (sourceParent.isValid()) {
            src_children = indexToTree(sourceParent);
        } else {
            src_children = &data_;
        }

        // get dst array
        if (destinationParent.isValid()) {
            dst_children = indexToTree(destinationParent);
        } else {
            dst_children = &data_;
        }

        // check valid move..
        auto moveFirst = sourceRow;
        auto moveLast  = sourceRow + count - 1;
        auto dest      = destinationChild;

        if (moveLast >= static_cast<int>(src_children->size()))
            return false;

        if (beginMoveRows(sourceParent, moveFirst, moveLast, destinationParent, dest)) {
            // spdlog::warn("{}-{} -> {} {}", moveFirst, moveLast, dest, sourceParent ==
            // destinationParent);

            // clone data
            std::list<JsonTree> tmp;

            auto begin = src_children->begin();
            auto end   = begin;
            std::advance(begin, moveFirst);
            std::advance(end, moveLast + 1);


            tmp.splice(tmp.end(), src_children->base(), begin, end);

            // // remove from source.
            // src_children->erase(begin, end);

            if (moveFirst < dest)
                dest = std::max(0, dest - count);

            auto dst_begin = dst_children->begin();
            std::advance(dst_begin, dest);
            // insert into dst.
            dst_children->splice(dst_begin, tmp);

            // spdlog::warn("{}", dst_children->dump(2));

            endMoveRows();
            // rowsMovePersistent(sourceParent,
            //     moveFirst,
            //     moveLast,
            //     destinationParent,
            //     dest
            // );
        } else {
            spdlog::warn(
                "{} invalid move: f {} l {} d {}",
                __PRETTY_FUNCTION__,
                moveFirst,
                moveLast,
                dest);
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    if (result)
        emit jsonChanged();

    // for(const auto &i: persistentIndexList()) {
    //     spdlog::warn("after {} {}", i.row(), getIndexPath(i.internalId()).to_string());
    // }

    return result;
}

bool JSONTreeModel::insertRows(
    int row, int count, const QModelIndex &parent, const nlohmann::json &data) {
    auto result = false;

    // spdlog::warn("JSONTreeModel::insertRows {}", data.dump(2));

    try {
        utility::JsonTree *node = nullptr;

        if (not parent.isValid())
            node = &data_;
        else
            node = indexToTree(parent);

        if (node and row >= 0) {
            if (row < static_cast<int>(node->size())) {
                result     = true;
                auto begin = node->begin();
                std::advance(begin, row);
                beginInsertRows(parent, row, row + count - 1);
                for (auto i = 0; i < count; i++) {
                    node->insert(begin, data);
                }
                endInsertRows();
            } else {
                result     = true;
                row        = static_cast<int>(node->size());
                auto begin = node->begin();
                std::advance(begin, row);
                beginInsertRows(parent, row, row + count - 1);

                for (auto i = 0; i < count; i++) {
                    node->insert(begin, data);
                }

                endInsertRows();
            }
        } else {
            spdlog::warn("ERM");
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    if (result) {
        if (parent == QModelIndex())
            emit lengthChanged();
        emit jsonChanged();
    }

    return result;
}


bool JSONTreeModel::insertRows(int row, int count, const QModelIndex &parent) {
    return insertRows(row, count, parent, R"({})"_json);
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


int JSONTreeModel::roleId(const QString &role) const {
    int role_id = -1;

    QHashIterator<int, QByteArray> it(roleNames());
    if (it.findNext(role.toUtf8())) {
        role_id = it.key();
    }
    return role_id;
}

QStringList JSONTreeModel::roles() const {
    QStringList tmp;

    for (const auto &i : role_names_)
        tmp.push_back(QStringFromStd(i));

    return tmp;
}

void JSONTreeModel::setRoleNames(const nlohmann::json &data) {
    // no roles but data, scan data for roles..
    if (role_names_.empty()) {
        auto keys = get_all_keys(data);
        role_names_.reserve(keys.size());
        for (const auto &i : keys)
            role_names_.push_back(i);
    }
}

void JSONTreeModel::setRoleNames(
    const std::vector<std::string> roles, const std::string display_role) {
    display_role_ = std::move(display_role);
    role_names_   = std::move(roles);
}

QString JSONTreeModel::roleName(const int id) const {
    QString result;
    auto rolemap = roleNames();
    if (rolemap.count(id))
        result = rolemap.value(id);

    return result;
}

QVariant JSONTreeFilterModel::get(const QModelIndex &item, const QString &role) const {
    QModelIndex sindex = mapToSource(item);
    return sindex.data(roleId(role));
}

bool JSONTreeFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    // check level

    if (not roleFilterMap_.empty() and sourceModel()) {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        for (const auto &[k, v] : roleFilterMap_) {
            if (not v.isNull()) {
                try {
                    auto qv = sourceModel()->data(index, k);
                    if (v.userType() == QMetaType::Bool)
                        if (v.toBool() != qv.toBool())
                            return false;
                        else if (v != qv)
                            return false;
                } catch (...) {
                }
            }
        }
    }
    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

QVariant JSONTreeFilterModel::getRoleFilter(const QString &role) const {
    auto id = roleId(role);
    QVariant result;

    if (roleFilterMap_.count(id))
        result = roleFilterMap_.at(id);

    return result;
}

void JSONTreeFilterModel::setRoleFilter(const QVariant &filter, const QString &role) {
    auto id = roleId(role);
    if (not roleFilterMap_.count(id) or roleFilterMap_.at(id) != filter) {
        roleFilterMap_[id] = filter;
        invalidateFilter();
    }
}

// #include "json_tree_model_ui.moc"
