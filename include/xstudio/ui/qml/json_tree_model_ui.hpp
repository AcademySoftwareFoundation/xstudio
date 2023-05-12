// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <caf/all.hpp>
#include <map>
#include <vector>

CAF_PUSH_WARNINGS
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
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
    enum Roles { JSONRole = Qt::UserRole + 1, JSONTextRole, LASTROLE };

    inline static const std::map<int, std::string> role_names = {
        {Qt::DisplayRole, "display"}, {JSONRole, "jsonRole"}, {JSONTextRole, "jsonTextRole"}};

    JSONTreeModel(QObject *parent = nullptr) : QAbstractItemModel(parent) {}

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

  protected:
    void build_parent_map(const nlohmann::json *parent_data = nullptr, int parent_row = 0);

  protected:
    nlohmann::json data_;
    nlohmann::json::json_pointer children_{"/children"};
    std::string display_role_;
    std::vector<std::string> role_names_;

    std::map<const nlohmann::json *, std::pair<int, const nlohmann::json *>> parent_map_;
};

} // namespace xstudio::ui::qml
