// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store_sync.hpp"
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

    // this might slow things down..
    // if(not data_.contains(t)) {
    //     throw std::runtime_error(
    //         fmt::format(
    //             "indexToTree : Invalid pointer row {} col {} ptr {}",
    //             index.row(),
    //             index.column(),
    //             fmt::ptr(t))
    //             );
    // }

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

void JSONTreeModel::setModelDataBase(const nlohmann::json &data, const bool local) {
    beginResetModel();

    if (local and event_send_callback_) {
        auto event = SyncEvent;

        event["redo"]         = xstudio::utility::ResetEvent;
        event["redo"]["id"]   = model_id_;
        event["redo"]["data"] = data;

        event["undo"]         = xstudio::utility::ResetEvent;
        event["undo"]["id"]   = model_id_;
        event["undo"]["data"] = tree_to_json(data_);

        event_send_callback_(event);
    }

    if (role_names_.empty())
        setRoleNames(data);

    data_ = json_to_tree(data, children_);

    endResetModel();
    emit lengthChanged();
    emit jsonChanged();
}

void JSONTreeModel::setModelData(const nlohmann::json &data) { setModelDataBase(data, true); }

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
    QModelIndexList indexes = searchList(value, role, parent, start, 1);

    if (not indexes.empty())
        result = indexes[0];

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::searchList(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits) {
    return searchList(value, roleId(role), parent, start, hits);
}

QModelIndex JSONTreeModel::searchRecursive(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int depth) {
    return searchRecursive(value, roleId(role), parent, start, depth);
}

QModelIndexList JSONTreeModel::searchRecursiveList(
    const QVariant &value,
    const QString &role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {
    return searchRecursiveList(value, roleId(role), parent, start, hits, depth);
}

QModelIndexList JSONTreeModel::searchList(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits) {

    return searchRecursiveListBase(value, role, parent, start, hits, 0);
}

QModelIndex JSONTreeModel::searchRecursive(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int depth) {

    auto result = QModelIndex();

    START_SLOW_WATCHER()

    QModelIndexList indexes = searchRecursiveList(value, role, parent, start, 1, depth);
    if (not indexes.empty())
        result = indexes[0];

    CHECK_SLOW_WATCHER()

    return result;
}

QModelIndexList JSONTreeModel::searchRecursiveListBase(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int max_depth) {

    auto tp = utility::clock::now();

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
                auto more_result = searchRecursiveList(
                    value, role, chd, 0, hits == -1 ? -1 : hits - result.size(), max_depth - 1);
                result.append(more_result);
                if (hits != -1 and result.size() >= hits)
                    break;
            }
        }
    }

    CHECK_SLOW_WATCHER()

    if (std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now() - tp)
            .count() > 100) {
        spdlog::warn(
            "searchRecursiveListBase: Slow search parent {} value {} role {} start {} hits {} "
            "max_depth {} duration milliseconds {}\n",
            mapFromValue(parent.data(Roles::JSONRole)).dump(2),
            mapFromValue(value).dump(2),
            StdFromQString(roleName(role)),
            start,
            hits,
            max_depth,
            std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now() - tp)
                .count());
    }

    return result;
}

QModelIndexList JSONTreeModel::searchRecursiveList(
    const QVariant &value,
    const int role,
    const QModelIndex &parent,
    const int start,
    const int hits,
    const int depth) {
    return searchRecursiveListBase(value, role, parent, start, hits, depth);
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

void JSONTreeModel::fetchMoreWait(const QModelIndex &parent) {
    if (canFetchMore(parent)) {
        fetchMore(parent);

        while (canFetchMore(parent)) {
            QCoreApplication::processEvents(
                QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents, 50);
        }
    }
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

    // spdlog::warn("hasChildren");

    try {
        if (not parent.isValid() and not data_.empty()) {
            result = true;
        } else if (parent.isValid()) {
            const auto &jsn = indexToData(parent);
            if (jsn.count(children_) and jsn.at(children_).is_null())
                result = true;
            else {
                auto node = indexToTree(parent);
                result    = not node->empty();
            }
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
        case Roles::idRole:
            result = QVariant::fromValue(QUuidFromUuid(j.value("id", Uuid())));
            break;
        case Roles::JSONRole:
            result = QVariantMapFromJson(j);
            break;
        case Roles::JSONTextRole:
            result = QString::fromStdString(j.dump(2));
            break;
        case Roles::childCountRole:
            result = rowCount(index);
            break;
        case Roles::JSONPathRole:
            result = QString::fromStdString(getIndexPath(index).to_string());
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

        spdlog::debug(
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

            if (not old_node->empty())
                removeRows(0, old_node->size(), index);

            // replace data..
            // global replace of data ?

            baseSetDataAll(index, value);

            // old_node->data() = new_node.data();

            if (jval.contains(children_) and not jval.at(children_).empty())
                insertRowsData(
                    0, jval.at(children_).size(), index, mapFromValue(jval.at(children_)));

            result = true;
            roles.clear();
        } break;
        case Roles::childrenRole: {
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

            auto old_node = indexToTree(index);

            // remove old children
            if (not old_node->empty())
                removeRows(0, old_node->size(), index);

            if (not jval.at(children_).empty())
                insertRowsData(
                    0, jval.at(children_).size(), index, mapFromValue(jval.at(children_)));

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

            if (j.count(field) && j[field].is_number_integer() &&
                value.canConvert(QMetaType::Int)) {
                // preserving int types from being changed to doubles
                j[field] = value.toInt();
            } else {
                j[field] = mapFromValue(value);
            }
            result = true;
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

bool JSONTreeModel::receiveEvent(const utility::JsonStore &event_pair) {
    // is it an event for me ?
    auto result = false;

    try {
        const auto &event = event_pair.at("redo");

        auto type = event.value("type", "");
        auto id   = event.value("id", Uuid());


        // ignore reflected events
        if (id != model_id_) {
            if (type == "insert_rows") {
                auto path  = nlohmann::json::json_pointer(event.value("parent", ""));
                auto index = getPathIndex(path);
                if (index.isValid()) {

                    result = baseInsertRows(
                        event.value("row", 0),
                        event.value("count", 0),
                        index,
                        event.value("data", nlohmann::json::array()),
                        false);
                }
            } else if (type == "insert") {
                auto path  = nlohmann::json::json_pointer(event.value("parent", ""));
                auto index = getPathIndex(path);
                if (index.isValid()) {
                    nlohmann::json &j         = indexToData(index);
                    j[event.value("key", "")] = event.value("data", nlohmann::json());
                    result                    = true;
                    emit dataChanged(index, index, QVector<int>());
                }
            } else if (type == "remove_rows") {
                auto path  = nlohmann::json::json_pointer(event.value("parent", ""));
                auto index = getPathIndex(path);
                result     = baseRemoveRows(
                    event.value("row", 0), event.value("count", 0), index, false);
            } else if (type == "remove") {
                // remove a key from object.
                auto path  = nlohmann::json::json_pointer(event.value("parent", ""));
                auto index = getPathIndex(path);
                if (index.isValid()) {
                    nlohmann::json &j = indexToData(index);
                    j.erase(event.value("key", ""));
                    result = true;
                    emit dataChanged(index, index, QVector<int>());
                }
            } else if (type == "set") {
                // find index..
                auto path = nlohmann::json::json_pointer(event.value("parent", ""));
                path /= children_;
                path /= std::to_string(event.value("row", 0));
                auto index = getPathIndex(path);
                if (index.isValid()) {
                    auto data = event.value("data", nlohmann::json::object());

                    // we can't know the role..
                    for (const auto &i : data.items())
                        result |= baseSetData(
                            index, mapFromValue(i.value()), i.key(), QVector<int>(), false);
                }
            } else if (type == "move") {
                auto src_index =
                    getPathIndex(nlohmann::json::json_pointer(event.value("src_parent", "")));
                auto dst_index =
                    getPathIndex(nlohmann::json::json_pointer(event.value("dst_parent", "")));
                if (src_index.isValid() and dst_index.isValid()) {
                    result = baseMoveRows(
                        src_index,
                        event.value("src_row", 0),
                        event.value("count", 0),
                        dst_index,
                        event.value("dst_row", 0),
                        false);
                }
            } else if (type == "reset") {
                setModelDataBase(event.value("data", nlohmann::json::object()), false);
                result = true;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool JSONTreeModel::baseSetDataAll(
    const QModelIndex &index, const QVariant &value, const bool local) {

    auto result = false;

    try {
        nlohmann::json &j = indexToData(index);
        const auto v      = mapFromValue(value);

        if (j != v) {
            if (local and event_send_callback_) {
                auto event          = xstudio::utility::SyncEvent;
                event["redo"]       = xstudio::utility::SetEvent;
                event["redo"]["id"] = model_id_;
                event["redo"]["parent"] =
                    StdFromQString(index.parent().data(JSONPathRole).toString());
                event["redo"]["row"]  = index.row();
                event["redo"]["data"] = v;

                event["undo"]       = xstudio::utility::SetEvent;
                event["undo"]["id"] = model_id_;
                event["undo"]["parent"] =
                    StdFromQString(index.parent().data(JSONPathRole).toString());
                event["undo"]["row"]  = index.row();
                event["undo"]["data"] = j;

                event_send_callback_(event);
            }

            j      = v;
            result = true;
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    if (result)
        emit dataChanged(index, index, QVector<int>());

    return result;
}

bool JSONTreeModel::baseSetData(
    const QModelIndex &index,
    const QVariant &value,
    const std::string &key,
    QVector<int> roles,
    const bool local) {

    auto result = false;

    try {
        nlohmann::json &j = indexToData(index);
        const auto v      = mapFromValue(value);

        if (j.count(key) and j.at(key) != v) {
            if (local and event_send_callback_) {
                auto event          = xstudio::utility::SyncEvent;
                event["redo"]       = xstudio::utility::SetEvent;
                event["redo"]["id"] = model_id_;
                event["redo"]["parent"] =
                    StdFromQString(index.parent().data(JSONPathRole).toString());
                event["redo"]["row"]       = index.row();
                event["redo"]["data"]      = R"({})"_json;
                event["redo"]["data"][key] = v;

                event["undo"]       = xstudio::utility::SetEvent;
                event["undo"]["id"] = model_id_;
                event["undo"]["parent"] =
                    StdFromQString(index.parent().data(JSONPathRole).toString());
                event["undo"]["row"]       = index.row();
                event["undo"]["data"]      = R"({})"_json;
                event["undo"]["data"][key] = j.at(key);

                event_send_callback_(event);
            }

            j[key] = v;
            result = true;
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    if (result)
        emit dataChanged(index, index, roles);

    return result;
}

bool JSONTreeModel::removeNodes(const int row, const int count, JsonTree *node) {
    auto result = false;
    if (node and (row + (count - 1)) < static_cast<int>(node->size())) {
        result = true;

        auto start = node->begin();
        auto end   = start;
        std::advance(start, row);
        std::advance(end, row + count);
        node->erase(start, end);
    }
    return result;
}


bool JSONTreeModel::baseRemoveRows(
    int row, int count, const QModelIndex &parent, const bool local) {
    auto result = false;

    try {
        JsonTree *node = nullptr;

        if (not parent.isValid())
            node = &data_;
        else
            node = indexToTree(parent);

        if (node and (row + (count - 1)) < static_cast<int>(node->size())) {
            result = true;

            // send event upstream
            if (local and event_send_callback_) {
                auto event              = xstudio::utility::SyncEvent;
                event["redo"]           = xstudio::utility::RemoveRowsEvent;
                event["redo"]["id"]     = model_id_;
                event["redo"]["parent"] = StdFromQString(parent.data(JSONPathRole).toString());
                event["redo"]["row"]    = row;
                event["redo"]["count"]  = count;

                event["undo"]           = xstudio::utility::InsertRowsEvent;
                event["undo"]["id"]     = model_id_;
                event["undo"]["parent"] = event["redo"]["parent"];
                event["undo"]["row"]    = row;
                event["undo"]["count"]  = count;

                auto undo_data = R"([])"_json;
                for (auto i = 0; i < count; i++)
                    undo_data.push_back(tree_to_json(*(node->child(i + row))));
                event["undo"]["data"] = undo_data;

                event_send_callback_(event);
            }

            beginRemoveRows(parent, row, row + (count - 1));
            removeNodes(row, count, node);
            endRemoveRows();

            emit dataChanged(parent, parent, {Roles::childCountRole});
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


bool JSONTreeModel::removeRows(int row, int count, const QModelIndex &parent) {
    return baseRemoveRows(row, count, parent);
}
bool JSONTreeModel::moveRows(
    const QModelIndex &sourceParent,
    int sourceRow,
    int count,
    const QModelIndex &destinationParent,
    int destinationChild) {
    return baseMoveRows(sourceParent, sourceRow, count, destinationParent, destinationChild);
}

bool JSONTreeModel::moveNodes(
    utility::JsonTree *src, int first_row, int last_row, utility::JsonTree *dst, int dst_row) {

    // clone data
    std::list<JsonTree> tmp;

    auto begin = src->begin();
    auto end   = begin;
    std::advance(begin, first_row);
    std::advance(end, last_row + 1);

    tmp.splice(tmp.end(), src->base(), begin, end);

    auto dst_begin = dst->begin();
    std::advance(dst_begin, dst_row);
    // insert into dst.
    dst->splice(dst_begin, tmp);

    return true;
}

bool JSONTreeModel::reorderRows(
    const QModelIndex &parent, const std::vector<int> &new_row_indeces) {

    // if we have rows in the model A,B,C,D,E and we are re-ordering to
    // E,A,D,C,B then new_row_indeces should be [4,0,3,2,1]

    if (!parent.isValid()) {
        spdlog::error("{} invalid index.", __PRETTY_FUNCTION__);
        return false;
    }

    auto tree = indexToTree(parent);

    if (new_row_indeces.size() != rowCount(parent)) {
        spdlog::error(
            "{}. {} indexes in re-ordered layout, expecting {}.",
            __PRETTY_FUNCTION__,
            new_row_indeces.size(),
            rowCount(parent));
        return false;
    }

    // See Qt docs on layout changes QtAbstractItemModel
    emit layoutAboutToBeChanged(
        QList<QPersistentModelIndex>{parent}, QAbstractItemModel::VerticalSortHint);

    // We need to make a list of the indeces that are going to be changing
    // due to our re-ordering.
    QModelIndexList from;
    for (int dest_row = 0; dest_row < new_row_indeces.size(); ++dest_row) {
        int src_row = new_row_indeces[dest_row];
        if (src_row == dest_row)
            continue;
        from.append(QPersistentModelIndex(index(src_row, 0, parent)));
    }

    // we need a mutable copy of new_row_indeces
    std::vector<int> indeces_mapping = new_row_indeces;

    for (int dest_row = 0; dest_row < indeces_mapping.size(); ++dest_row) {

        int src_row = indeces_mapping[dest_row];

        if (src_row == dest_row)
            continue;


        // move our internal data, one row at a time
        moveNodes(tree, src_row, src_row, tree, dest_row);

        // each row move is the equivalent of removing a row from somewhere
        // and inserting it somewhere else (dest_row) ...

        // because we have moved a row and inserted a row, we have changed
        // the position of all the other rows that lie between the src and
        // dest - this means we have to update the src rows in 'indeces_mapping'
        // so it works for the current re-ordered state of the data
        for (int j = dest_row + 1; j < indeces_mapping.size(); ++j) {
            // looping over the remaining destination indeces ...

            if (indeces_mapping[j] >= dest_row) {
                // the row we just moved has been inserted below the src
                // row at destination j, so the src row needs to be incremented
                indeces_mapping[j]++;
            }
            if (indeces_mapping[j] > src_row) {
                // The row that we moved has been taken away from a position
                // that is less than the src at row at j so decrement
                indeces_mapping[j]--;
            }
        }
    }

    // now we make a list of the updated model indexes for those rows that
    // got moved
    QModelIndexList to;
    for (int dest_row = 0; dest_row < new_row_indeces.size(); ++dest_row) {
        int src_row = new_row_indeces[dest_row];
        if (src_row == dest_row)
            continue;
        to.append(QPersistentModelIndex(index(dest_row, 0, parent)));
    }

    // see Qt docs ...
    changePersistentIndexList(from, to);

    emit layoutChanged(
        QList<QPersistentModelIndex>{parent}, QAbstractItemModel::VerticalSortHint);

    return true;
}

bool JSONTreeModel::baseMoveRows(
    const QModelIndex &sourceParent,
    int sourceRow,
    int count,
    const QModelIndex &destinationParent,
    int destinationChild,
    const bool local) {
    auto result = false;

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

            if (moveFirst < dest && sourceParent == destinationParent)
                dest = std::max(0, dest - count);

            // clone data
            moveNodes(src_children, moveFirst, moveLast, dst_children, dest);
            endMoveRows();

            emit dataChanged(sourceParent, sourceParent, {Roles::childCountRole});

            if (sourceParent != destinationParent) {
                emit dataChanged(destinationParent, destinationParent, {Roles::childCountRole});
                if (sourceParent == QModelIndex() or destinationParent == QModelIndex()) {
                    emit lengthChanged();
                }
            }

            // send event upstream
            if (local and event_send_callback_) {
                auto event    = SyncEvent;
                auto src_path = StdFromQString(sourceParent.data(JSONPathRole).toString());
                auto dst_path = StdFromQString(destinationParent.data(JSONPathRole).toString());

                // we handle moves differently...
                // so we need to massage the data..
                event["redo"]               = xstudio::utility::MoveEvent;
                event["redo"]["id"]         = model_id_;
                event["redo"]["src_parent"] = src_path;
                event["redo"]["src_row"]    = sourceRow;
                event["redo"]["count"]      = count;
                event["redo"]["dst_parent"] = dst_path;

                if (src_path == dst_path) {
                    if (destinationChild < sourceRow)
                        event["redo"]["dst_row"] = destinationChild;
                    else {
                        event["redo"]["dst_row"] = destinationChild - count;
                    }

                    // qt does wierd things here..
                } else {
                    // qt adds after index..
                    event["redo"]["dst_row"] = destinationChild;
                }

                event["undo"]               = xstudio::utility::MoveEvent;
                event["undo"]["id"]         = model_id_;
                event["undo"]["src_parent"] = dst_path;
                event["undo"]["src_row"]    = event["redo"]["dst_row"];
                event["undo"]["count"]      = count;
                event["undo"]["dst_parent"] = src_path;
                event["undo"]["dst_row"]    = sourceRow;

                event_send_callback_(event);
            }
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

    return result;
}

bool JSONTreeModel::insertNodes(
    const int row, const int count, utility::JsonTree *node, const nlohmann::json &data) {
    auto result = false;

    if (node and row >= 0) {
        result     = true;
        auto begin = node->begin();
        std::advance(begin, row);

        if (data.is_array()) {
            for (auto i = 0; i < count; i++) {
                node->insert(begin, json_to_tree(data.at(i), children_));
            }
        } else {
            for (auto i = 0; i < count; i++) {
                node->insert(begin, json_to_tree(data, children_));
            }
        }
    }

    return result;
}


bool JSONTreeModel::baseInsertRows(
    int row,
    int count,
    const QModelIndex &parent,
    const nlohmann::json &data,
    const bool local) {
    auto result = false;

    // spdlog::warn("JSONTreeModel::insertRows {}", data.dump(2));

    try {
        utility::JsonTree *node = nullptr;

        if (not parent.isValid())
            node = &data_;
        else
            node = indexToTree(parent);

        if (node and row >= 0) {
            if (row >= static_cast<int>(node->size()))
                row = static_cast<int>(node->size());

            beginInsertRows(parent, row, row + count - 1);
            result = insertNodes(row, count, node, data);
            endInsertRows();

            emit dataChanged(parent, parent, {Roles::childCountRole});

            // send event
            if (local and event_send_callback_) {
                auto parent_path = StdFromQString(parent.data(JSONPathRole).toString());

                auto tweak_data = R"([])"_json;
                if (data.is_array())
                    tweak_data = data;
                else if (not data.empty())
                    tweak_data[0] = data;

                auto event = SyncEvent;

                event["redo"]           = xstudio::utility::InsertRowsEvent;
                event["redo"]["id"]     = model_id_;
                event["redo"]["parent"] = parent_path;
                event["redo"]["row"]    = row;
                event["redo"]["count"]  = count;
                event["redo"]["data"]   = tweak_data;

                event["undo"]           = xstudio::utility::RemoveRowsEvent;
                event["undo"]["id"]     = model_id_;
                event["undo"]["parent"] = parent_path;
                event["undo"]["row"]    = row;
                event["undo"]["count"]  = count;

                event_send_callback_(event);
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

bool JSONTreeModel::insertRows(
    int row, int count, const QModelIndex &parent, const nlohmann::json &data) {
    return baseInsertRows(row, count, parent, data);
}

bool JSONTreeModel::insertRowsData(
    int row, int count, const QModelIndex &parent, const QVariant &data) {
    return insertRows(row, count, parent, mapFromValue(data));
}


bool JSONTreeModel::insertRows(int row, int count, const QModelIndex &parent) {
    return baseInsertRows(row, count, parent, R"({})"_json);
}


void JSONTreeModel::bindEventFunc(JSONTreeSendEventFunc fs) {
    event_send_callback_ = [fs](auto &&PH1) { return fs(std::forward<decltype(PH1)>(PH1)); };

    model_id_ = Uuid::generate();
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
                    if (v.type() == qv.type() && v == qv)
                        return invert_ ? false : true;
                    else if (v.canConvert(QMetaType::QVariantList)) {
                        const auto vl = v.toList();
                        for (const auto &vli : vl) {
                            if (qv.type() == vli.type() && qv == vli)
                                return invert_ ? false : true;
                        }
                    }
                } catch (...) {
                }
            }
        }
        return invert_ ? true : false;
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


// QModelIndex JSONTreeListModel::mapFromSource(const QModelIndex &sourceIndex) const {
//     return index();
// }

// QModelIndex JSONTreeListModel::mapToSource(const QModelIndex &proxyIndex) const {
//     return proxyIndex;
// }
