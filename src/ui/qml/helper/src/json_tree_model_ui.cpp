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

void JSONTreeModel::rowsInsertedPersistent(const QModelIndex &parent, int first, int last) {

    QModelIndexList mFrom, mTo;

    // all children change ..
    // create before after..
    // find matching persistent..
    auto parent_path_ptr = getIndexPath(parent.internalId());
    auto parent_path = parent_path_ptr.to_string();

    for(const auto &i: persistentIndexList()) {
        auto child_path_ptr = getIndexPath(i.internalId());
        auto child_path = child_path_ptr.to_string();

        if(starts_with(child_path, parent_path)) {
            auto tmp = child_path.substr(parent_path.size());
            std::cmatch m;

            if (std::regex_match(tmp.c_str(), m, std::regex("^/children/(\\d+)(.*)"))) {
                auto child_row = std::stoi(m[1].str());
                auto new_child_row = child_row;

                // have we changed.. ?
                if(child_row > first) {
                    new_child_row = child_row + ((last - first) + 1);
                }

                if(new_child_row != child_row) {
                    auto new_child_path = parent_path + "/children/" + std::to_string(new_child_row) + m[2].str();
                    mFrom.append(i);
                    mTo.append(createIndex(new_child_row, 0, getIndexId(nlohmann::json::json_pointer(new_child_path))));
                } else {
                    // spdlog::warn("{} {} {}", child_path, child_row, new_child_row);
                }
            }
        }
    }

    if (not mFrom.empty())
         changePersistentIndexList(mFrom, mTo);
}

void JSONTreeModel::rowsRemovedPersistent(const QModelIndex &parent, int first, int last) {
    QModelIndexList mFrom, mTo;

    // all children change ..
    // create before after..
    // find matching persistent..
    auto parent_path_ptr = getIndexPath(parent.internalId());
    auto parent_path = parent_path_ptr.to_string();

    for(const auto &i: persistentIndexList()) {
        auto child_path_ptr = getIndexPath(i.internalId());
        auto child_path = child_path_ptr.to_string();

        if(starts_with(child_path, parent_path)) {
            auto tmp = child_path.substr(parent_path.size());
            std::cmatch m;

            if (std::regex_match(tmp.c_str(), m, std::regex("^/children/(\\d+)(.*)"))) {
                auto child_row = std::stoi(m[1].str());
                auto new_child_row = child_row;

                // have we changed.. ?
                if(child_row > last) {
                    new_child_row = child_row - ((last - first) + 1);
                }

                if(new_child_row != child_row) {
                    auto new_child_path = parent_path + "/children/" + std::to_string(new_child_row) + m[2].str();
                    mFrom.append(i);
                    mTo.append(createIndex(new_child_row, 0, getIndexId(nlohmann::json::json_pointer(new_child_path))));
                } else {
                    // spdlog::warn("{} {} {}", child_path, child_row, new_child_row);
                }
            }
        }
    }

    if (not mFrom.empty())
         changePersistentIndexList(mFrom, mTo);
}

void JSONTreeModel::rowsMovePersistent(const QModelIndex &parent,
    int start,
    int end,
    const QModelIndex &dest,
    int row
) {
    // spdlog::warn("rowsMovePersistent {} {} {}", start, end, row);
    // get my path .. then check persistent indexes for matches...
    // crap this applies to all children.. of parent and dest.
    // argh...

    if(parent == dest) {
        // force rebuild..
        QModelIndexList mFrom, mTo;

        // all children change ..
        // create before after..
        // find matching persistent..
        auto parent_path_ptr = getIndexPath(dest.internalId());
        auto parent_path = parent_path_ptr.to_string();

        for(const auto &i: persistentIndexList()) {
            auto child_path_ptr = getIndexPath(i.internalId());
            auto child_path = child_path_ptr.to_string();

            if(starts_with(child_path, parent_path)) {
                auto tmp = child_path.substr(parent_path.size());
                std::cmatch m;

                if (std::regex_match(tmp.c_str(), m, std::regex("^/children/(\\d+)(.*)"))) {
                    auto child_row = std::stoi(m[1].str());
                    auto new_child_row = child_row;


                    if(child_row < start and child_row >= row) {
                        new_child_row += (end - start) + 1;
                    } else if(child_row >= start and child_row <= end)
                        new_child_row = row + (child_row - start);
                    else {

                    }


                    // // have we changed.. ?
                    // if(child_row >= start and child_row <= end) {
                    //     if(row == rowCount(dest))
                    //         new_child_row = (row - 1) + (child_row - start);
                    //     else
                    //         new_child_row = row + (child_row - start);
                    // } else if(child_row >= row and start > child_row) {
                    //     new_child_row += (end - start) + 1;
                    // }

                    if(new_child_row != child_row) {
                        auto new_child_path = parent_path + "/children/" + std::to_string(new_child_row) + m[2].str();
                        mFrom.append(i);
                        mTo.append(createIndex(m[2].str().empty() ? new_child_row : i.row(), 0, getIndexId(nlohmann::json::json_pointer(new_child_path))));
                        // spdlog::warn("CHANGED {} -> {} / {} -> {} ", child_row, new_child_row, child_path, new_child_path );
                    } else {
                        // spdlog::warn("{} {}", child_path, child_row);
                    }
                }
            }
        }

        if (not mFrom.empty())
             changePersistentIndexList(mFrom, mTo);


        // for(const auto &i: persistentIndexList()) {
        //     spdlog::warn("after fix {} {}", i.row(), getIndexPath(i.internalId()).to_string());
        // }

    }
}


void JSONTreeModel::add_pointers(
    const nlohmann::json &jsn,
    const std::string &parent) {

    if (not jsn.is_primitive()) {
        if (jsn.is_array()) {
            for (size_t i = 0; i < jsn.size(); i++) {
                auto path = parent + "/" + std::to_string(i);
                pointer_data_.emplace(
                    std::make_pair(
                        std::hash<std::string>{}(path),
                        nlohmann::json::json_pointer(path)
                    )
                );

                add_pointers(jsn.at(i), path);
            }
        } else if (jsn.is_object()) {
            if (jsn.contains(children_) and jsn.at(children_).is_array())
                add_pointers(jsn.at(children_), parent + children_.to_string());
        }
    }
}

void JSONTreeModel::build_lookup() {
    // build list of pointers.
    pointer_data_.clear();
    add_pointers(data_);
}

nlohmann::json &JSONTreeModel::indexToData(const QModelIndex &index) {
    auto id = index.internalId();
    if (not index.isValid() or id == std::numeric_limits<quintptr>::max()) {
        throw std::runtime_error("Invalid index");
    }

    auto ptr = getIndexPath(id);
    if (not data_.contains(ptr))
        throw std::runtime_error(
            fmt::format("Invalid index path id - row {}, path id {}, path {}", index.row(), id, ptr.to_string()));

    return data_[ptr];
}

const nlohmann::json &JSONTreeModel::indexToData(const QModelIndex &index) const {
    auto id = index.internalId();
    if (not index.isValid() or id == std::numeric_limits<quintptr>::max()) {
        throw std::runtime_error("Invalid index");
    }

    auto ptr = getIndexPath(id);
    if (not data_.contains(ptr))
        throw std::runtime_error(
            fmt::format("Invalid index path id - row {}, path id {}, path {}", index.row(), id, ptr.to_string()));

    return data_.at(ptr);
}

void JSONTreeModel::setModelData(const nlohmann::json &data) {
    beginResetModel();
    data_ = data;

    build_lookup();

    if (role_names_.empty())
        setRoleNames();

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

    auto indexes = QAbstractItemModel::match(
        index(start, 0, parent),
        role,
        value,
        1,
        Qt::MatchFlags(Qt::MatchWrap | Qt::MatchExactly));

    if (not indexes.empty())
        result = indexes[0];

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::search(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits) {
    return search(value, roleId(role), parent, start, hits);
}

QModelIndexList JSONTreeModel::search(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits) {
    return QAbstractItemModel::match(
        index(start, 0, parent),
        role,
        value,
        hits,
        Qt::MatchFlags(Qt::MatchWrap | Qt::MatchExactly));
}


QModelIndex JSONTreeModel::search_recursive(
    const QVariant &value, const int role, const QModelIndex &parent, const int start) {

    START_SLOW_WATCHER()

    auto result = search(value, role, parent, start);

    if (result == QModelIndex()) {
        for (int i = start; i < rowCount(parent); i++) {
            auto chd = index(i, 0, parent);

            if (hasChildren(chd)) {
                result = search_recursive(value, role, chd, 0);
                if (result != QModelIndex())
                    break;
            }
        }
    }

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndex JSONTreeModel::search_recursive(
    const QVariant &value, const QString &role, const QModelIndex &parent, const int start) {
    return search_recursive(value, roleId(role), parent, start);
}

QModelIndexList JSONTreeModel::search_recursive(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits) {

    START_SLOW_WATCHER()

    auto result = QAbstractItemModel::match(
        index(start, 0, parent),
        role,
        value,
        hits,
        Qt::MatchFlags(Qt::MatchWrap | Qt::MatchExactly));

    if (hits == -1 or result.size() < hits) {
        for (int i = start; i < rowCount(parent); i++) {
            auto chd = index(i, 0, parent);
            if (hasChildren(chd)) {
                auto more_result = search_recursive(
                    value, role, chd, 0, hits == -1 ? -1 : hits - result.size());
                result.append(more_result);
                if (hits != -1 and result.size() >= hits)
                    break;
            }
        }
    }

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::search_recursive(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits) {
    return search_recursive(value, roleId(role), parent, start, hits);
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
        } else {
            const auto &j = indexToData(parent);
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
        } else {
            const auto &j = indexToData(parent);
            if (j.contains(children_) and j.at(children_).is_array())
                count = static_cast<int>(j.at(children_).size());
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return count;
}

quintptr JSONTreeModel::getIndexId(const nlohmann::json::json_pointer &ptr) const {
    size_t result = std::hash<std::string>{}(ptr.to_string());

    if(not pointer_data_.count(result)) {
        pointer_data_.emplace(
            std::make_pair(
                result,
                ptr
            )
        );
    }

    return static_cast<quintptr>(result);
}

nlohmann::json::json_pointer JSONTreeModel::getIndexPath(const quintptr id) const {
    nlohmann::json::json_pointer result;

    try{
        result = pointer_data_.at(id);
    } catch(...){}

    return result;
}

QModelIndex JSONTreeModel::pointerToIndex(const nlohmann::json::json_pointer &ptr) const {
    QModelIndex result;

    try {
        // make sure it's valid..
        auto i = data_.at(ptr);

        if (ptr == nlohmann::json::json_pointer("/")) {

            // get row
        } else {
            auto row = stoi(ptr.back());
            result   = createIndex(row, 0, getIndexId(ptr));
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QModelIndex JSONTreeModel::index(int row, int column, const QModelIndex &parent) const {
    auto result = QModelIndex();

    try {

        if (row >= 0 and column >= 0) {

            if (not parent.isValid()) {
                if (data_.is_array() and row < static_cast<int>(data_.size()))
                    result = createIndex(
                        row,
                        column,
                        getIndexId(nlohmann::json::json_pointer("/" + std::to_string(row))));
            } else {
                const auto &j = indexToData(parent);

                // get parent path.
                auto parent_path = getIndexPath(parent.internalId());

                if (j.contains(children_) and j.at(children_).is_array() and
                    row < static_cast<int>(j.at(children_).size())) {

                    result = createIndex(
                        row, column, getIndexId(parent_path / children_ / std::to_string(row)));
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
        auto id = child.internalId();
        if (child.isValid() and id != std::numeric_limits<quintptr>::max()) {
            auto parent_path = getIndexPath(id).parent_pointer().parent_pointer();

            result = createIndex(std::stoi(parent_path.back()), 0, getIndexId(parent_path));
        }
    } catch (...) {
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
        case Roles::JSONPointerRole:
            result = QString::fromStdString(getIndexPath(index.internalId()).to_string());
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
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


bool JSONTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        nlohmann::json &j = indexToData(index);
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

            result = true;
            roles.clear();
            break;

        default: {
            // allow replacement..
            auto id    = role - Roles::LASTROLE;
            auto field = std::string();
            if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                field = role_names_.at(id);
            }
            if (j.count(field)) {
                j[field] = mapFromValue(value);
                result   = true;
            }

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
        nlohmann::json *children = nullptr;

        if (row >= 0 and parent.isValid()) {
            nlohmann::json &j = indexToData(parent);
            if (j.contains(children_))
                children = &(j.at(children_));
        } else if (row >= 0 and not parent.isValid()) {
            children = &data_;
        }

        if (children and children->is_array() and
            row + (count - 1) < static_cast<int>(children->size())) {
            result = true;
            rowsRemovedPersistent(parent, row, row + (count-1));
            beginRemoveRows(parent, row, row + (count - 1));
            for (auto i = 0; i < count; i++)
                children->erase(children->begin() + row);
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

    // for(const auto &i: persistentIndexList()) {
    //     spdlog::warn("before {} {}", i.row(), getIndexPath(i.internalId()).to_string());
    // }

    if (destinationChild < 0 or count < 1 or sourceRow < 0)
        return false;

    nlohmann::json *src_children = nullptr;
    nlohmann::json *dst_children = nullptr;

    try {
        // get src array
        if (sourceParent.isValid()) {
            nlohmann::json &j = indexToData(sourceParent);
            if (j.contains(children_))
                src_children = &(j.at(children_));
        } else if (not sourceParent.isValid()) {
            src_children = &data_;
        }

        // get dst array
        if (destinationParent.isValid()) {
            nlohmann::json &j = indexToData(destinationParent);
            if (j.contains(children_))
                dst_children = &(j.at(children_));
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
            // spdlog::warn("{}-{} -> {} {}", moveFirst, moveLast, dest, sourceParent ==
            // destinationParent);

            // clone data
            auto tmp = R"([])"_json;
            tmp.insert(
                tmp.end(),
                src_children->begin() + moveFirst,
                src_children->begin() + moveLast + 1);

            // remove from source.
            src_children->erase(
                src_children->begin() + moveFirst, src_children->begin() + moveLast + 1);

            if (moveFirst < dest)
                dest = std::max(0, dest - count);

            // insert into dst.
            dst_children->insert(dst_children->begin() + dest, tmp.begin(), tmp.end());

            // spdlog::warn("{}", dst_children->dump(2));

            endMoveRows();
            // rowsMovePersistent(sourceParent,
            //     moveFirst,
            //     moveLast,
            //     destinationParent,
            //     dest
            // );
        } else {
            spdlog::warn("{} invalid move", __PRETTY_FUNCTION__);
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

    try {
        nlohmann::json *children = &data_;

        if (parent.isValid()) {
            nlohmann::json &j = indexToData(parent);
            // spdlog::warn("{}", j.dump(2));

            if (j.contains(children_))
                children = &(j.at(children_));
        }

        if (children and children->is_array() and row >= 0 and
            row < static_cast<int>(children->size())) {
            result = true;
            rowsInsertedPersistent(parent, row, row+count-1);
            beginInsertRows(parent, row, row + count - 1);
            children->insert(children->begin() + row, count, data);
            endInsertRows();
        } else if (children and children->is_array() and row >= 0) {
            result = true;
            row    = static_cast<int>(children->size());
            rowsInsertedPersistent(parent, row, row + count - 1);
            beginInsertRows(parent, row, row + count - 1);
            children->insert(children->begin() + row, count, data);
            endInsertRows();
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

QModelIndexList JSONTreeModel::match(
    const QModelIndex &start,
    const QString &role,
    const QVariant &value,
    int hits,
    Qt::MatchFlags flags) const {
    return match(start, roleId(role), value, hits, flags);
}

QModelIndexList JSONTreeModel::match(
    const QModelIndex &start,
    const int role,
    const QVariant &value,
    int hits,
    Qt::MatchFlags flags) const {
    return QAbstractItemModel::match(start, role, value, hits, flags);
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
                    if (v != qv)
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
