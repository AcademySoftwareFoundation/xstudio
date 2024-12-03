// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
// using namespace shotgun_client;
namespace ui::qml {

    // dumping ground for termModels
    class ShotBrowserListModel : public JSONTreeModel {
        Q_OBJECT

      public:
        const static inline std::vector<std::string> RoleNames = {
            "createdRole",
            "descriptionRole",
            "divisionRole",
            "nameRole",
            "projectStatusRole",
            "sgTypeRole",
            "typeRole"};

        enum Roles {
            createdRole = JSONTreeModel::Roles::LASTROLE,
            descriptionRole,
            divisionRole,
            nameRole,
            projectStatusRole,
            sgTypeRole,
            typeRole,
            LASTROLE
        };

        ShotBrowserListModel(QObject *parent = nullptr) : JSONTreeModel(parent) {
            setRoleNames(RoleNames);
        }

        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    };

    class ShotBrowserSequenceModel : public ShotBrowserListModel {

      public:
        const static inline std::vector<std::string> RoleNames = {"statusRole", "unitRole"};

        enum Roles { statusRole = ShotBrowserListModel::Roles::LASTROLE, unitRole };

        ShotBrowserSequenceModel(QObject *parent = nullptr) : ShotBrowserListModel(parent) {
            auto roles = ShotBrowserListModel::RoleNames;
            roles.insert(roles.end(), RoleNames.begin(), RoleNames.end());
            setRoleNames(roles);
        }

        static nlohmann::json flatToTree(const nlohmann::json &src);

        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

      private:
        static nlohmann::json sortByName(const nlohmann::json &json);
    };

    class ShotBrowserSequenceFilterModel : public QSortFilterProxyModel {
        Q_OBJECT

        Q_PROPERTY(
            QStringList hideStatus READ hideStatus WRITE setHideStatus NOTIFY hideStatusChanged)

        Q_PROPERTY(bool hideEmpty READ hideEmpty WRITE setHideEmpty NOTIFY hideEmptyChanged)

        Q_PROPERTY(QVariantList unitFilter READ unitFilter WRITE setUnitFilter NOTIFY
                       unitFilterChanged)

        QML_NAMED_ELEMENT("ShotBrowserSequenceFilterModel")

      public:
        ShotBrowserSequenceFilterModel(QObject *parent = nullptr)
            : QSortFilterProxyModel(parent) {}

        Q_INVOKABLE [[nodiscard]] QVariant
        get(const QModelIndex &item, const QString &role = "display") const;

        Q_INVOKABLE QModelIndex searchRecursive(
            const QVariant &value,
            const QString &role       = "display",
            const QModelIndex &parent = QModelIndex(),
            const int start           = 0,
            const int depth           = -1);

        [[nodiscard]] bool hideEmpty() const { return hide_empty_; }
        [[nodiscard]] QStringList hideStatus() const;
        [[nodiscard]] QVariantList unitFilter() const { return filter_unit_; }

        void setHideStatus(const QStringList &value);

        void setHideEmpty(const bool value) {
            if (hide_empty_ != value) {
                hide_empty_ = value;
                emit hideEmptyChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }

        void setUnitFilter(const QVariantList &filter) {
            if (filter_unit_ != filter) {
                filter_unit_ = filter;
                emit unitFilterChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }


      signals:
        void hideStatusChanged();
        void unitFilterChanged();
        void hideEmptyChanged();
        void filterChanged();

      protected:
        [[nodiscard]] bool
        filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;


      private:
        std::set<QString> hide_status_;
        QVariantList filter_unit_;
        bool hide_empty_{false};
    };


    class ShotBrowserFilterModel : public QSortFilterProxyModel {
        Q_OBJECT

        Q_PROPERTY(int length READ length NOTIFY lengthChanged)
        Q_PROPERTY(int count READ length NOTIFY lengthChanged)

        Q_PROPERTY(QVariantList divisionFilter READ divisionFilter WRITE setDivisionFilter
                       NOTIFY divisionFilterChanged)

        Q_PROPERTY(QVariantList projectStatusFilter READ projectStatusFilter WRITE
                       setProjectStatusFilter NOTIFY projectStatusFilterChanged)

        Q_PROPERTY(QItemSelection selectionFilter READ selectionFilter WRITE setSelectionFilter
                       NOTIFY selectionFilterChanged)

      public:
        ShotBrowserFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
            connect(
                this,
                &QAbstractListModel::rowsInserted,
                this,
                &ShotBrowserFilterModel::lengthChanged);
            connect(
                this,
                &QAbstractListModel::modelReset,
                this,
                &ShotBrowserFilterModel::lengthChanged);
            connect(
                this,
                &QAbstractListModel::rowsRemoved,
                this,
                &ShotBrowserFilterModel::lengthChanged);
        }

        Q_INVOKABLE [[nodiscard]] QVariant
        get(const QModelIndex &item, const QString &role = "display") const;

        [[nodiscard]] int length() const { return rowCount(); }

        Q_INVOKABLE QModelIndex searchRecursive(
            const QVariant &value,
            const QString &role       = "display",
            const QModelIndex &parent = QModelIndex(),
            const int start           = 0,
            const int depth           = -1);


        [[nodiscard]] QItemSelection selectionFilter() const { return selection_filter_; }
        [[nodiscard]] QVariantList divisionFilter() const { return filter_division_; }
        [[nodiscard]] QVariantList projectStatusFilter() const {
            return filter_project_status_;
        }

        void setSelectionFilter(const QItemSelection &selection) {
            if (selection_filter_ != selection) {
                selection_filter_ = selection;
                emit selectionFilterChanged();
                invalidateFilter();
                // setDynamicSortFilter(false);
                // sort(0, sortOrder());
                // setDynamicSortFilter(true);
            }
        }

        void setDivisionFilter(const QVariantList &filter) {
            if (filter_division_ != filter) {
                filter_division_ = filter;
                emit divisionFilterChanged();
                invalidateFilter();
            }
        }

        void setProjectStatusFilter(const QVariantList &filter) {
            if (filter_project_status_ != filter) {
                filter_project_status_ = filter;
                emit projectStatusFilterChanged();
                invalidateFilter();
            }
        }

      signals:
        void lengthChanged();
        void selectionFilterChanged();
        void divisionFilterChanged();
        void projectStatusFilterChanged();

      protected:
        [[nodiscard]] bool
        filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
        // [[nodiscard]] bool
        // lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;

      private:
        QItemSelection selection_filter_;
        QVariantList filter_division_;
        QVariantList filter_project_status_;
    };


} // namespace ui::qml
} // namespace xstudio