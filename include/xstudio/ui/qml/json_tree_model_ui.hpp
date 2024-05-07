// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifndef HELPER_QML_EXPORT_H
#define HELPER_QML_EXPORT_H

#ifdef HELPER_QML_STATIC_DEFINE
#define HELPER_QML_EXPORT
#define HELPER_QML_NO_EXPORT
#else
#ifndef HELPER_QML_EXPORT
#ifdef helper_qml_EXPORTS
/* We are building this library */
#define HELPER_QML_EXPORT __declspec(dllexport)
#else
/* We are using this library */
#define HELPER_QML_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef HELPER_QML_NO_EXPORT
#define HELPER_QML_NO_EXPORT
#endif
#endif

#ifndef HELPER_QML_DEPRECATED
#define HELPER_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef HELPER_QML_DEPRECATED_EXPORT
#define HELPER_QML_DEPRECATED_EXPORT HELPER_QML_EXPORT HELPER_QML_DEPRECATED
#endif

#ifndef HELPER_QML_DEPRECATED_NO_EXPORT
#define HELPER_QML_DEPRECATED_NO_EXPORT HELPER_QML_NO_EXPORT HELPER_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef HELPER_QML_NO_DEPRECATED
#define HELPER_QML_NO_DEPRECATED
#endif
#endif

#endif /* HELPER_QML_EXPORT_H */

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

namespace xstudio::ui::qml {

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
    enum Roles { JSONRole = Qt::UserRole + 1, JSONTextRole, LASTROLE };

    inline static const std::map<int, std::string> role_names = {
        {Qt::DisplayRole, "display"}, {JSONRole, "jsonRole"}, {JSONTextRole, "jsonTextRole"}};

    JSONTreeModel(QObject *parent = nullptr);

    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

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

    Q_INVOKABLE QModelIndexList search_list(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndexList search_list(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits);

    Q_INVOKABLE QModelIndex search_recursive(
        const QVariant &value,
        const QString &role       = "display",
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0,
        const int depth           = -1);

    Q_INVOKABLE QModelIndex search_recursive(
        const QVariant &value,
        const int role,
        const QModelIndex &parent = QModelIndex(),
        const int start           = 0,
        const int depth           = -1);

    Q_INVOKABLE QModelIndexList search_recursive_list(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1);

    Q_INVOKABLE QModelIndexList search_recursive_list(
        const QVariant &value,
        const QString &role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1);

    [[nodiscard]] nlohmann::json modelData() const;
    void setModelData(const nlohmann::json &data);


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
    QModelIndex getPathIndex(const nlohmann::json::json_pointer &path);

  protected:
    virtual QModelIndexList search_recursive_list_base(
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
};

class HELPER_QML_EXPORT JSONTreeFilterModel : public QSortFilterProxyModel {
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
