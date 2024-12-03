// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QUrl>
#include <qqml.h>
#include <QQmlEngineExtensionPlugin>
#include <QAbstractListModel>
#include <QAbstractItemModel>
#include <QQmlApplicationEngine>
#include <QFuture>
#include <QtConcurrent>
#include <QAbstractProxyModel>
#include <QSortFilterProxyModel>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/json_store.hpp"

#include "query_engine.hpp"

namespace xstudio {
using namespace shotgun_client;

namespace ui::qml {


    class ShotBrowserPresetModel : public JSONTreeModel {
        Q_PROPERTY(QStringList entities READ entities NOTIFY entitiesChanged)
        Q_PROPERTY(QQmlPropertyMap *termLists READ termLists NOTIFY termListsChanged)

        Q_OBJECT
      public:
        enum Roles {
            enabledRole = JSONTreeModel::Roles::LASTROLE,
            entityRole,
            favouriteRole,
            groupFavouriteRole,
            groupIdRole,
            groupUserDataRole,
            hiddenRole,
            livelinkRole,
            nameRole,
            negatedRole,
            parentEnabledRole,
            termRole,
            typeRole,
            updateRole,
            userDataRole,
            valueRole
        };

        ShotBrowserPresetModel(QueryEngine &query_engine, QObject *parent = nullptr);

        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        bool setData(
            const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

        void setModelPathData(const std::string &path, const utility::JsonStore &data);

        bool receiveEvent(const utility::JsonStore &event) override;

        Q_INVOKABLE QModelIndex insertGroup(const QString &entity, const int row);
        Q_INVOKABLE QModelIndex insertPreset(const int row, const QModelIndex &parent);
        Q_INVOKABLE QModelIndex
        insertTerm(const QString &term, const int row, const QModelIndex &parent);

        Q_INVOKABLE QModelIndex
        insertOperatorTerm(const bool anding, const int row, const QModelIndex &parent);

        [[nodiscard]] QStringList entities() const;

        QQmlPropertyMap *termLists() const { return term_lists_; }

        Q_INVOKABLE QObject *
        termModel(const QString &term, const QString &entity = "", const int project_id = -1);

        Q_INVOKABLE bool
        removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

        Q_INVOKABLE void resetPresets(const QModelIndexList &indexes);
        Q_INVOKABLE void resetPreset(const QModelIndex &index);
        Q_INVOKABLE QModelIndex duplicate(const QModelIndex &index);
        Q_INVOKABLE QVariant copy(const QModelIndexList &indexes) const;
        Q_INVOKABLE bool paste(const QVariant &data, const int row, const QModelIndex &parent);

        Q_INVOKABLE QModelIndex getPresetGroup(const QModelIndex &index) const;

        Q_INVOKABLE QFuture<QString> exportAsSystemPresetsFuture(
            const QUrl &path, const QModelIndex &index = QModelIndex()) const;
        Q_INVOKABLE QString exportAsSystemPresets(
            const QUrl &path, const QModelIndex &index = QModelIndex()) const {
            return exportAsSystemPresetsFuture(path, index).result();
        }

        Q_INVOKABLE QFuture<QString>
        backupPresetsFuture(const QUrl &path, const QModelIndex &index = QModelIndex()) const;
        Q_INVOKABLE QString
        backupPresets(const QUrl &path, const QModelIndex &index = QModelIndex()) const {
            return backupPresetsFuture(path, index).result();
        }

        Q_INVOKABLE QFuture<QString> restorePresetsFuture(const QUrl &path);
        Q_INVOKABLE QString restorePresets(const QUrl &path) {
            return restorePresetsFuture(path).result();
        }

        void updateTermModel(const std::string &key, const bool cache);

      signals:
        void entitiesChanged();
        void termListsChanged();
        void presetChanged(QModelIndex);
        void presetHidden(QModelIndex, bool);

      private:
        void markedAsUpdated(const QModelIndex &parent);

        const std::map<std::string, int> role_map_ = {
            {"update", Roles::updateRole},
            {"hidden", Roles::hiddenRole},
            {"enabled", Roles::enabledRole},
            {"favourite", Roles::favouriteRole},
            {"term", Roles::termRole},
            {"value", Roles::valueRole},
            {"userdata", Roles::userDataRole},
            {"userdata", Roles::groupUserDataRole},
            {"name", Roles::nameRole},
            {"negated", Roles::negatedRole}};
        QQmlPropertyMap *term_lists_{nullptr};

        QMap<QString, QObject *> term_models_;
        QueryEngine &query_engine_;
    };

    class ShotBrowserPresetTreeFilterModel : public QSortFilterProxyModel {
        Q_OBJECT
        QML_NAMED_ELEMENT("ShotBrowserSequenceFilterModel")

      public:
        ShotBrowserPresetTreeFilterModel(QObject *parent = nullptr)
            : QSortFilterProxyModel(parent) {
            setDynamicSortFilter(true);
        }

      protected:
        [[nodiscard]] bool
        filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    };

    class ShotBrowserPresetFilterModel : public QSortFilterProxyModel {
        Q_OBJECT

        Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
        Q_PROPERTY(bool onlyShowFavourite READ onlyShowFavourite WRITE setOnlyShowFavourite
                       NOTIFY onlyShowFavouriteChanged)

        Q_PROPERTY(bool onlyShowPresets READ onlyShowPresets WRITE setOnlyShowPresets NOTIFY
                       onlyShowPresetsChanged)

        Q_PROPERTY(bool ignoreSpecialGroups READ ignoreSpecialGroups WRITE
                       setIgnoreSpecialGroups NOTIFY ignoreSpecialGroupsChanged)

        Q_PROPERTY(QVariant filterUserData READ filterUserData WRITE setFilterUserData NOTIFY
                       filterUserDataChanged)

        Q_PROPERTY(QVariant filterGroupUserData READ filterGroupUserData WRITE
                       setFilterGroupUserData NOTIFY filterGroupUserDataChanged)


      public:
        ShotBrowserPresetFilterModel(QObject *parent = nullptr)
            : QSortFilterProxyModel(parent) {
            setDynamicSortFilter(true);
        }

        [[nodiscard]] bool showHidden() const { return show_hidden_; }
        [[nodiscard]] bool onlyShowFavourite() const { return only_show_favourite_; }
        [[nodiscard]] bool onlyShowPresets() const { return only_show_presets_; }
        [[nodiscard]] bool ignoreSpecialGroups() const { return ignore_special_groups_; }
        [[nodiscard]] QVariant filterUserData() const { return filter_user_data_; }
        [[nodiscard]] QVariant filterGroupUserData() const { return filter_group_user_data_; }

        Q_INVOKABLE void setFilterGroupUserData(const QVariant &filter);
        Q_INVOKABLE void setFilterUserData(const QVariant &filter);
        Q_INVOKABLE void setShowHidden(const bool value);
        Q_INVOKABLE void setOnlyShowFavourite(const bool value);
        Q_INVOKABLE void setOnlyShowPresets(const bool value);
        Q_INVOKABLE void setIgnoreSpecialGroups(const bool value);

      signals:
        void ignoreSpecialGroupsChanged();
        void showHiddenChanged();
        void onlyShowPresetsChanged();
        void onlyShowFavouriteChanged();
        void filterUserDataChanged();
        void filterGroupUserDataChanged();

      protected:
        [[nodiscard]] bool
        filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

      private:
        QVariant filter_group_user_data_;
        bool show_hidden_{false};
        bool only_show_favourite_{false};
        bool only_show_presets_{false};
        bool ignore_special_groups_{false};
        QVariant filter_user_data_;
        std::set<utility::Uuid> special_groups_{
            utility::Uuid("ef787e88-1b8f-4d89-bbc7-3ecf85987792"), // quick menu
            utility::Uuid("28612cf7-a814-4714-a4eb-443126cf0cd4"), // note history
            utility::Uuid("4c512dae-e1e3-43b7-a02a-4fb7d93fde62"), // shot history
            utility::Uuid("b6e0ca0e-2d23-4b1d-a33a-761596183d5f"), // replace/conform
            utility::Uuid("86439af3-16be-4a2f-89f3-ee1e5810ae47")  // view in cut
        };
    };
} // namespace ui::qml
} // namespace xstudio
