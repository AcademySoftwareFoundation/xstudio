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
            "completionLocationRole",
            "descriptionRole",
            "divisionRole",
            "hiddenRole",
            "nameRole",
            "parentHiddenRole",
            "projectStatusRole",
            "sgTypeRole",
            "statusRole",
            "subtypeRole",
            "typeRole",
            "unitRole"};

        enum Roles {
            createdRole = JSONTreeModel::Roles::LASTROLE,
            completionLocationRole,
            descriptionRole,
            divisionRole,
            hiddenRole,
            nameRole,
            parentHiddenRole,
            projectStatusRole,
            sgTypeRole,
            statusRole,
            subtypeRole,
            typeRole,
            unitRole,
            LASTROLE
        };

        ShotBrowserListModel(QObject *parent = nullptr) : JSONTreeModel(parent) {
            setRoleNames(RoleNames);
        }

        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        Q_INVOKABLE [[nodiscard]] QVariant getHidden() const;

        Q_INVOKABLE void setHidden(const QVariant &fdata);

        bool setData(
            const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

      private:
        nlohmann::json get_hidden(const QModelIndex &index = QModelIndex()) const;
        void set_hidden(
            const std::map<std::string, std::set<int>> &fmap,
            const QModelIndex &pindex = QModelIndex());
    };

    class ShotBrowserSequenceModel : public ShotBrowserListModel {
        Q_OBJECT

        Q_PROPERTY(QStringList types READ types WRITE setTypes NOTIFY typesChanged)

      public:
        const static inline std::vector<std::string> RoleNames = {"assetNameRole"};

        enum Roles {
            assetNameRole = ShotBrowserListModel::Roles::LASTROLE,
        };

        ShotBrowserSequenceModel(QObject *parent = nullptr) : ShotBrowserListModel(parent) {
            auto roles = ShotBrowserListModel::RoleNames;
            roles.insert(roles.end(), RoleNames.begin(), RoleNames.end());
            setRoleNames(roles);
        }


        void setTypes(const QStringList &value) {
            if (types_ != value) {
                types_ = value;
                emit typesChanged();
            }
        }

        static nlohmann::json flatToTree(const nlohmann::json &src, QStringList &_types);
        static nlohmann::json flatToAssetTree(const nlohmann::json &src, QStringList &_types);

        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        [[nodiscard]] QStringList types() const { return types_; }

      signals:
        void typesChanged();

      private:
        static nlohmann::json sortByNameAndType(const nlohmann::json &json);
        QStringList types_{};
    };

    class ShotBrowserSequenceFilterModel : public QSortFilterProxyModel {
        Q_OBJECT

        Q_PROPERTY(
            QStringList hideStatus READ hideStatus WRITE setHideStatus NOTIFY hideStatusChanged)

        Q_PROPERTY(bool hideEmpty READ hideEmpty WRITE setHideEmpty NOTIFY hideEmptyChanged)
        Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

        Q_PROPERTY(QVariantList unitFilter READ unitFilter WRITE setUnitFilter NOTIFY
                       unitFilterChanged)

        Q_PROPERTY(QVariantList typeFilter READ typeFilter WRITE setTypeFilter NOTIFY
                       typeFilterChanged)

        Q_PROPERTY(QVariantList locationFilter READ locationFilter WRITE setLocationFilter
                       NOTIFY locationFilterChanged)

        QML_NAMED_ELEMENT("ShotBrowserSequenceFilterModel")

      public:
        ShotBrowserSequenceFilterModel(QObject *parent = nullptr)
            : QSortFilterProxyModel(parent) {}

        Q_INVOKABLE [[nodiscard]] QVariant
        get(const QModelIndex &item, const QString &role = "display") const;

        Q_INVOKABLE [[nodiscard]] bool
        set(const QModelIndex &item, const QVariant &value, const QString &role = "display");

        Q_INVOKABLE QModelIndex searchRecursive(
            const QVariant &value,
            const QString &role       = "display",
            const QModelIndex &parent = QModelIndex(),
            const int start           = 0,
            const int depth           = -1);

        [[nodiscard]] bool hideEmpty() const { return hide_empty_; }
        [[nodiscard]] bool showHidden() const { return show_hidden_; }

        [[nodiscard]] QStringList hideStatus() const;
        [[nodiscard]] QVariantList unitFilter() const { return filter_unit_; }
        [[nodiscard]] QVariantList typeFilter() const { return filter_type_; }
        [[nodiscard]] QVariantList locationFilter() const { return filter_location_; }

        void setHideStatus(const QStringList &value);

        void setHideEmpty(const bool value) {
            if (hide_empty_ != value) {
                hide_empty_ = value;
                emit hideEmptyChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }

        void setShowHidden(const bool value) {
            if (show_hidden_ != value) {
                show_hidden_ = value;
                emit showHiddenChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }

        void setUnitFilter(const QVariantList &filter) {
            if (filter_unit_ != filter) {
                filter_unit_ = filter;
                filter_unit_set_.clear();
                for (const auto &i : filter_unit_)
                    filter_unit_set_.insert(i.toString());

                emit unitFilterChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }

        void setTypeFilter(const QVariantList &filter) {
            if (filter_type_ != filter) {
                filter_type_ = filter;

                filter_type_set_.clear();
                for (const auto &i : filter_type_)
                    filter_type_set_.insert(i.toString());

                emit typeFilterChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }

        void setLocationFilter(const QVariantList &filter) {
            if (filter_location_ != filter) {
                filter_location_ = filter;

                filter_location_set_.clear();
                for (const auto &i : filter_location_)
                    filter_location_set_.insert(i.toString());

                emit locationFilterChanged();
                invalidateFilter();
                emit filterChanged();
            }
        }


      signals:
        void hideStatusChanged();
        void showHiddenChanged();
        void unitFilterChanged();
        void typeFilterChanged();
        void locationFilterChanged();
        void hideEmptyChanged();
        void filterChanged();

      protected:
        [[nodiscard]] bool
        filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;


      private:
        std::set<QString> hide_status_;

        std::set<QString> filter_type_set_;
        std::set<QString> filter_unit_set_;
        std::set<QString> filter_location_set_;

        QVariantList filter_unit_;
        QVariantList filter_type_;
        QVariantList filter_location_;


        bool hide_empty_{false};
        bool show_hidden_{true};
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

        Q_INVOKABLE [[nodiscard]] bool
        set(const QModelIndex &item, const QVariant &value, const QString &role = "display");

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