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

namespace xstudio::ui::qml {

class JSONTreeModel : public QAbstractItemModel {
    Q_OBJECT

    Q_PROPERTY(int count READ length NOTIFY lengthChanged)
    Q_PROPERTY(int length READ length NOTIFY lengthChanged)

  public:
    [[nodiscard]] int length() const { return rowCount(); }

  signals:
    void jsonChanged();
    void lengthChanged();

  public:
    enum Roles { JSONRole = Qt::UserRole + 1, JSONTextRole, JSONPointerRole, LASTROLE };

    inline static const std::map<int, std::string> role_names = {
        {Qt::DisplayRole, "display"},
        {JSONRole, "jsonRole"},
        {JSONTextRole, "jsonTextRole"},
        {JSONPointerRole, "jsonPointerRole"}};

    JSONTreeModel(QObject *parent = nullptr);

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

    Q_INVOKABLE [[nodiscard]] bool
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

    bool insertRows(int row, int count, const QModelIndex &parent, const nlohmann::json &data);

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

    Q_INVOKABLE QModelIndexList search(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndexList search(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndex search_recursive(
        const QVariant &value,
        const QString &role       = "display",
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0);

    Q_INVOKABLE QModelIndex search_recursive(
        const QVariant &value,
        const int role,
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0);

    Q_INVOKABLE QModelIndexList search_recursive(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndexList search_recursive(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndexList match(
        const QModelIndex &start,
        const int role,
        const QVariant &value,
        const int hits = 1,
        Qt::MatchFlags flags =
            Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const override;

    Q_INVOKABLE QModelIndexList match(
        const QModelIndex &start,
        const QString &role,
        const QVariant &value,
        int hits             = 1,
        Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const;


    const nlohmann::json &modelData() const { return data_; }
    void setModelData(const nlohmann::json &data);

    const nlohmann::json::json_pointer &childrenJsonPointer() const { return children_; }
    void setChildrenJsonPointer(const std::string &value) {
        children_ = nlohmann::json::json_pointer(value);
    }

    void setRoleNames(
        const std::vector<std::string> roles = {}, const std::string display_role = "display");


    nlohmann::json &indexToData(const QModelIndex &index);
    const nlohmann::json &indexToData(const QModelIndex &index) const;

  protected:
    void build_lookup();
    void add_pointers(
        const nlohmann::json &jsn,
        const std::string &parent = "");
    quintptr getIndexId(const nlohmann::json::json_pointer &ptr) const;
    nlohmann::json::json_pointer getIndexPath(const quintptr id) const;

    QModelIndex pointerToIndex(const nlohmann::json::json_pointer &ptr) const;

    void rowsMovePersistent(const QModelIndex &parent, int start, int end,
        const QModelIndex &dest, int row
    );
    void rowsInsertedPersistent(const QModelIndex &parent, int first, int last);
    void rowsRemovedPersistent(const QModelIndex &parent, int first, int last);

  protected:
    nlohmann::json data_;
    nlohmann::json::json_pointer children_{"/children"};
    std::string display_role_;
    std::vector<std::string> role_names_;

    mutable std::map<size_t, nlohmann::json::json_pointer> pointer_data_;
};

class JSONTreeFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(int length READ length NOTIFY lengthChanged)
    Q_PROPERTY(int count READ length NOTIFY lengthChanged)

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

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  private:
    std::map<int, QVariant> roleFilterMap_;
};


} // namespace xstudio::ui::qml
