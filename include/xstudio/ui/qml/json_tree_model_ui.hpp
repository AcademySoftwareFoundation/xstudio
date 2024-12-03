// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <caf/all.hpp>
#include <map>
#include <vector>

CAF_PUSH_WARNINGS
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
#include <QSortFilterProxyModel>
CAF_POP_WARNINGS

#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/tree.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/ui/qml/actor_object.hpp"

#include "helper_qml_export.h"

namespace xstudio::ui::qml {

typedef std::function<void(const utility::JsonStore &event)> JSONTreeSendEventFunc;

class HELPER_QML_EXPORT JSONTreeModel : public QAbstractItemModel {
    Q_OBJECT

    Q_PROPERTY(int count READ length NOTIFY lengthChanged)
    Q_PROPERTY(int length READ length NOTIFY lengthChanged)

  public:
    [[nodiscard]] int length() const { return rowCount(); }

  signals:
    void jsonChanged();
    void lengthChanged();

  public:
    enum Roles {
        idRole = Qt::UserRole + 1,
        childCountRole,
        childrenRole,
        JSONRole,
        JSONTextRole,
        JSONPathRole,
        LASTROLE
    };

    inline static const std::map<int, std::string> role_names = {
        {Qt::DisplayRole, "display"},
        {idRole, "idRole"},
        {childCountRole, "childCountRole"},
        {childrenRole, "childrenRole"},
        {JSONRole, "jsonRole"},
        {JSONTextRole, "jsonTextRole"},
        {JSONPathRole, "jsonPathRole"}};

    JSONTreeModel(QObject *parent = nullptr);

    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

    Q_INVOKABLE void fetchMoreWait(const QModelIndex &parent);


    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return 1;
    }
    [[nodiscard]] bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE bool
    set(const QModelIndex &item, const QVariant &value, const QString &role = "display");

    Q_INVOKABLE [[nodiscard]] QVariant
    get(const QModelIndex &item, const QString &role = "display") const;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE [[nodiscard]] QStringList roles() const;
    Q_INVOKABLE [[nodiscard]] int roleId(const QString &role) const;
    Q_INVOKABLE [[nodiscard]] QString roleName(const int id) const;

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool moveRows(
        const QModelIndex &sourceParent,
        int sourceRow,
        int count,
        const QModelIndex &destinationParent,
        int destinationChild) override;
    Q_INVOKABLE bool
    insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool
    insertRowsData(int row, int count, const QModelIndex &parent, const QVariant &data);

    bool insertRows(int row, int count, const QModelIndex &parent, const nlohmann::json &data);

    Q_INVOKABLE QModelIndex invalidIndex() const { return QModelIndex(); }

    Q_INVOKABLE int
    countExpandedChildren(const QModelIndex parent, const QModelIndexList &expanded);

    Q_INVOKABLE QModelIndex search(
        const QVariant &value,
        const QString &role       = "display",
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0);

    Q_INVOKABLE QModelIndex search(
        const QVariant &value,
        const int role,
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0);

    Q_INVOKABLE QModelIndexList searchList(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndexList searchList(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndex searchRecursive(
        const QVariant &value,
        const QString &role       = "display",
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0,
        const int depth           = -1);

    Q_INVOKABLE QModelIndex searchRecursive(
        const QVariant &value,
        const int role,
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0,
        const int depth           = -1);

    Q_INVOKABLE QModelIndexList searchRecursiveList(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1);

    Q_INVOKABLE QModelIndexList searchRecursiveList(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1);

    [[nodiscard]] nlohmann::json modelData() const;

    virtual void setModelData(const nlohmann::json &data);

    const std::string &children() const { return children_; }
    void setChildren(const std::string &value) { children_ = value; }

    void setRoleNames(const nlohmann::json &data);

    void setRoleNames(
        const std::vector<std::string> roles = {}, const std::string display_role = "display");

    nlohmann::json &indexToData(const QModelIndex &index);
    const nlohmann::json &indexToData(const QModelIndex &index) const;
    nlohmann::json indexToFullData(const QModelIndex &index, const int depth = -1) const;

    utility::JsonTree *indexToTree(const QModelIndex &index) const;
    nlohmann::json::json_pointer getIndexPath(const QModelIndex &index = QModelIndex()) const;
    virtual QModelIndex getPathIndex(const nlohmann::json::json_pointer &path);

    void bindEventFunc(JSONTreeSendEventFunc fs);
    virtual bool receiveEvent(const utility::JsonStore &event);

    /* For row reordering at a given parent index only. The full list of
    re-ordered indeces must be provbided.

    e.g. if we have rows in the model A,B,C,D,E and we are re-ordering to
    E,A,D,C,B then new_row_indeces should be [4,0,3,2,1] */
    bool reorderRows(const QModelIndex &parent, const std::vector<int> &new_row_indeces);


  protected:
    void setModelDataBase(const nlohmann::json &data, const bool local = true);

    bool insertNodes(
        const int row,
        const int count,
        utility::JsonTree *node,
        const nlohmann::json &data = R"({})"_json);

    bool baseInsertRows(
        int row,
        int count,
        const QModelIndex &parent  = QModelIndex(),
        const nlohmann::json &data = R"({})"_json,
        const bool local           = true);

    bool moveNodes(
        utility::JsonTree *src,
        int first_row,
        int last_row,
        utility::JsonTree *dst,
        int dst_row);

    bool baseMoveRows(
        const QModelIndex &sourceParent,
        int sourceRow,
        int count,
        const QModelIndex &destinationParent,
        int destinationChild,
        const bool local = true);

    bool removeNodes(const int row, const int count, utility::JsonTree *node);

    bool baseRemoveRows(
        int row, int count, const QModelIndex &parent = QModelIndex(), const bool local = true);

    bool baseSetData(
        const QModelIndex &index,
        const QVariant &value,
        const std::string &key,
        QVector<int> roles,
        const bool local = true);

    bool
    baseSetDataAll(const QModelIndex &index, const QVariant &value, const bool local = true);

    virtual QModelIndexList searchRecursiveListBase(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1);

    std::string children_{"children"};
    std::string display_role_;
    std::vector<std::string> role_names_;
    utility::JsonTree data_;
    utility::Uuid model_id_;

  private:
    JSONTreeSendEventFunc event_send_callback_{nullptr};
};

// class JSONTreeListModel : public QAbstractProxyModel {
//     Q_OBJECT

//   public:
//     JSONTreeListModel(QObject *parent = nullptr) : QAbstractProxyModel(parent) {
//     }

//     QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
//     QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
// };

class HELPER_QML_EXPORT JSONTreeFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(int length READ length NOTIFY lengthChanged)
    Q_PROPERTY(int count READ length NOTIFY lengthChanged)
    Q_PROPERTY(bool invert READ invert WRITE setInvert NOTIFY invertChanged)

    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY
                   sortAscendingChanged)
    Q_PROPERTY(
        QString sortRoleName READ sortRoleName WRITE setSortRoleName NOTIFY sortRoleNameChanged)

  public:
    JSONTreeFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setDynamicSortFilter(true);
        setSortRole(Qt::DisplayRole);
        sort(0, Qt::DescendingOrder);

        connect(
            this, &QAbstractListModel::rowsInserted, this, &JSONTreeFilterModel::lengthChanged);
        connect(
            this, &QAbstractListModel::modelReset, this, &JSONTreeFilterModel::lengthChanged);
        connect(
            this, &QAbstractListModel::rowsRemoved, this, &JSONTreeFilterModel::lengthChanged);
    }

    Q_INVOKABLE [[nodiscard]] QVariant getRoleFilter(const QString &role = "display") const;
    Q_INVOKABLE void setRoleFilter(const QVariant &filter, const QString &role = "display");
    Q_INVOKABLE void setInvert(const bool invert) {
        if (invert != invert_) {
            invert_ = invert;
            emit invertChanged();
        }
    }

    Q_INVOKABLE [[nodiscard]] QVariant
    get(const QModelIndex &item, const QString &role = "display") const;

    [[nodiscard]] int length() const { return rowCount(); }

    [[nodiscard]] bool sortAscending() const { return sortOrder() == Qt::AscendingOrder; }

    [[nodiscard]] QString sortRoleName() const {
        try {
            return roleNames().value(sortRole());
        } catch (...) {
        }

        return QString();
    }

    [[nodiscard]] bool invert() const { return invert_; }

    void setSortAscending(const bool ascending = true) {
        if (ascending != (sortOrder() == Qt::AscendingOrder ? true : false)) {
            sort(0, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
            emit sortAscendingChanged();
        }
    }

    [[nodiscard]] int roleId(const QString &role) const {
        auto role_id = -1;

        QHashIterator<int, QByteArray> it(roleNames());
        if (it.findNext(role.toUtf8())) {
            role_id = it.key();
        }
        return role_id;
    }

    void setSortRoleName(const QString &role) {
        auto role_id = roleId(role);
        if (role_id != sortRole()) {
            setSortRole(role_id);
            emit sortRoleNameChanged();
        }
    }

  signals:
    void lengthChanged();
    void sortAscendingChanged();
    void sortRoleNameChanged();
    void invertChanged();

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  private:
    std::map<int, QVariant> roleFilterMap_;
    bool invert_ = {false};
};
} // namespace xstudio::ui::qml