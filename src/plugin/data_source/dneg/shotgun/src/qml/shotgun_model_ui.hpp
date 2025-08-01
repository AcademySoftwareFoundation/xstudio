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
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
using namespace shotgun_client;
namespace ui {
    namespace qml {

        class ShotgunListModel;

        class ShotgunSequenceModel : public JSONTreeModel {
          public:
            enum Roles { idRole = JSONTreeModel::Roles::LASTROLE, nameRole, typeRole };

            ShotgunSequenceModel(QObject *parent = nullptr) : JSONTreeModel(parent) {
                setRoleNames(std::vector<std::string>({"idRole", "nameRole", "typeRole"}));
            }
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

            static nlohmann::json flatToTree(const nlohmann::json &src);

          private:
            static nlohmann::json sortByName(const nlohmann::json &json);
        };

        class ShotgunTreeModel : public JSONTreeModel {
            Q_OBJECT

            Q_PROPERTY(bool hasActiveFilter READ hasActiveFilter NOTIFY hasActiveFilterChanged)
            Q_PROPERTY(
                bool hasActiveLiveLink READ hasActiveLiveLink NOTIFY hasActiveLiveLinkChanged)
            Q_PROPERTY(int length READ length NOTIFY lengthChanged)
            Q_PROPERTY(int activePreset READ activePreset WRITE setActivePreset NOTIFY
                           activePresetChanged)

            Q_PROPERTY(QString activeShot READ activeShot NOTIFY activeShotChanged)
            Q_PROPERTY(QString activeSeq READ activeSeq NOTIFY activeSeqChanged)

          public:
            enum Roles {
                argRole = JSONTreeModel::Roles::LASTROLE,
                detailRole,
                enabledRole,
                expandedRole,
                idRole,
                nameRole,
                negationRole,
                queriesRole,
                liveLinkRole,
                termRole,
                typeRole
            };

            ShotgunTreeModel(QObject *parent = nullptr) : JSONTreeModel(parent) {
                setChildren("queries");
                setRoleNames(std::vector<std::string>(
                    {"argRole",
                     "detailRole",
                     "enabledRole",
                     "expandedRole",
                     "idRole",
                     "nameRole",
                     "negationRole",
                     "queriesRole",
                     "liveLinkRole",
                     "termRole",
                     "typeRole"}));
                connect(
                    this,
                    &QAbstractListModel::rowsInserted,
                    this,
                    &ShotgunTreeModel::modelChanged);
                connect(
                    this,
                    &QAbstractListModel::rowsRemoved,
                    this,
                    &ShotgunTreeModel::modelChanged);
                connect(
                    this,
                    &QAbstractListModel::rowsMoved,
                    this,
                    &ShotgunTreeModel::modelChanged);
                connect(
                    this,
                    &ShotgunTreeModel::modelChanged,
                    this,
                    &ShotgunTreeModel::lengthChanged);
            }

            [[nodiscard]] int length() const { return rowCount(); }
            [[nodiscard]] int activePreset() const { return active_preset_; }
            [[nodiscard]] QString activeSeq() const { return active_seq_; }
            [[nodiscard]] QString activeShot() const { return active_shot_; }

            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            bool setData(
                const QModelIndex &index,
                const QVariant &value,
                int role = Qt::EditRole) override;
            Q_INVOKABLE bool
            removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

            Q_INVOKABLE bool insert(
                int row,
                const QModelIndex &parent = QModelIndex(),
                const QVariant &data      = QVariant());

            Q_INVOKABLE [[nodiscard]] QVariant
            get(int row,
                const QModelIndex &parent = QModelIndex(),
                const QString &role       = "display") const;

            Q_INVOKABLE [[nodiscard]] QVariant
            get(const QModelIndex &index, const QString &role = "display") const;

            Q_INVOKABLE QVariant get(int row, const QString &role) const {
                return get(row, QModelIndex(), role);
            }
            Q_INVOKABLE bool
            set(int row,
                const QVariant &value,
                const QString &role       = "display",
                const QModelIndex &parent = QModelIndex());

            Q_INVOKABLE int search(
                const QVariant &value,
                const QString &role       = "display",
                const QModelIndex &parent = QModelIndex(),
                const int start           = 0);
            Q_INVOKABLE void setType(const int row, const QString &type = "user");

            Q_INVOKABLE void clearLoaded();
            Q_INVOKABLE void clearExpanded();
            Q_INVOKABLE void clearLiveLinks();

            Q_INVOKABLE void setActivePreset(const int row = -1);

            Q_INVOKABLE QVariant
            applyLiveLink(const QVariant &preset, const QVariant &livelink);
            Q_INVOKABLE [[nodiscard]] int getProjectId(const QVariant &livelink) const;

            void populate(const utility::JsonStore &);

            [[nodiscard]] bool hasActiveFilter() const { return has_active_filter_; }
            [[nodiscard]] bool hasActiveLiveLink() const { return has_active_live_link_; }

            void updateLiveLinks(const utility::JsonStore &data);
            void refreshLiveLinks();

            utility::JsonStore
            applyLiveLink(const utility::JsonStore &preset, const utility::JsonStore &livelink);
            utility::JsonStore applyLiveLinkValue(
                const utility::JsonStore &query, const utility::JsonStore &livelink);

            virtual void updateLiveLink(const QModelIndex &index);

            void setSequenceMap(const QMap<int, ShotgunListModel *> *map) {
                sequence_map_ = map;
            }

          protected:
            [[nodiscard]] utility::JsonStore
            getSequence(const int project_id, const int shot_id) const;

          signals:
            void modelChanged();
            void lengthChanged();
            void hasActiveFilterChanged();
            void hasActiveLiveLinkChanged();
            void activePresetChanged();
            void activeShotChanged();
            void activeSeqChanged();

          private:
            void checkForActiveFilter();
            void checkForActiveLiveLink();

          protected:
            utility::JsonStore live_link_data_;
            bool has_active_filter_{true};
            bool has_active_live_link_{false};
            const QMap<int, ShotgunListModel *> *sequence_map_{nullptr};
            int active_preset_{-1};
            QString active_seq_{};
            QString active_shot_{};
        };


        class ShotgunListModel : public QAbstractListModel {
            Q_OBJECT

            Q_PROPERTY(int length READ length NOTIFY lengthChanged)
            Q_PROPERTY(int count READ length NOTIFY lengthChanged)
            Q_PROPERTY(bool truncated READ truncated NOTIFY truncatedChanged)

          public:
            enum Roles {
                JSONRole = Qt::UserRole + 1,
                addressingRole,
                artistRole,
                assetRole,
                authorRole,
                clientNoteRole,
                contentRole,
                createdByRole,
                createdDateRole,
                dateSubmittedToClientRole,
                departmentRole,
                detailRole,
                frameRangeRole,
                frameSequenceRole,
                idRole,
                loginRole,
                itemCountRole,
                JSONTextRole,
                linkedVersionsRole,
                movieRole,
                nameRole,
                noteCountRole,
                noteTypeRole,
                onSiteChn,
                onSiteLon,
                onSiteMtl,
                onSiteMum,
                onSiteSyd,
                onSiteVan,
                pipelineStatusRole,
                pipelineStatusFullRole,
                pipelineStepRole,
                playlistNameRole,
                productionStatusRole,
                projectRole,
                projectIdRole,
                resultTypeRole,
                sequenceRole,
                shotNameRole,
                shotRole,
                siteRole,
                stalkUuidRole,
                subjectRole,
                submittedToDailiesRole,
                tagRole,
                thumbRole,
                twigNameRole,
                twigTypeRole,
                typeRole,
                URLRole,
                versionNameRole,
                versionRole
            };

            inline static const std::map<int, std::string> role_names = {
                {addressingRole, "addressingRole"},
                {artistRole, "artistRole"},
                {assetRole, "assetRole"},
                {authorRole, "authorRole"},
                {clientNoteRole, "clientNoteRole"},
                {contentRole, "contentRole"},
                {createdByRole, "createdByRole"},
                {createdDateRole, "createdDateRole"},
                {dateSubmittedToClientRole, "dateSubmittedToClientRole"},
                {departmentRole, "departmentRole"},
                {detailRole, "detailRole"},
                {frameRangeRole, "frameRangeRole"},
                {frameSequenceRole, "frameSequenceRole"},
                {idRole, "idRole"},
                {itemCountRole, "itemCountRole"},
                {JSONRole, "jsonRole"},
                {loginRole, "loginRole"},
                {JSONTextRole, "jsonTextRole"},
                {linkedVersionsRole, "linkedVersionsRole"},
                {movieRole, "movieRole"},
                {nameRole, "nameRole"},
                {noteCountRole, "noteCountRole"},
                {noteTypeRole, "noteTypeRole"},
                {onSiteChn, "onSiteChn"},
                {onSiteLon, "onSiteLon"},
                {onSiteMtl, "onSiteMtl"},
                {onSiteMum, "onSiteMum"},
                {onSiteSyd, "onSiteSyd"},
                {onSiteVan, "onSiteVan"},
                {pipelineStatusRole, "pipelineStatusRole"},
                {pipelineStatusFullRole, "pipelineStatusFullRole"},
                {pipelineStepRole, "pipelineStepRole"},
                {playlistNameRole, "playlistNameRole"},
                {productionStatusRole, "productionStatusRole"},
                {projectRole, "projectRole"},
                {projectIdRole, "projectIdRole"},
                {Qt::DisplayRole, "display"},
                {resultTypeRole, "resultTypeRole"},
                {sequenceRole, "sequenceRole"},
                {shotNameRole, "shotNameRole"},
                {shotRole, "shotRole"},
                {siteRole, "siteRole"},
                {stalkUuidRole, "stalkUuidRole"},
                {subjectRole, "subjectRole"},
                {submittedToDailiesRole, "submittedToDailiesRole"},
                {tagRole, "tagRole"},
                {thumbRole, "thumbRole"},
                {twigNameRole, "twigNameRole"},
                {twigTypeRole, "twigTypeRole"},
                {typeRole, "typeRole"},
                {URLRole, "URLRole"},
                {versionNameRole, "versionNameRole"},
                {versionRole, "versionRole"}};

            ShotgunListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {
                connect(
                    this,
                    &QAbstractListModel::rowsInserted,
                    this,
                    &ShotgunListModel::lengthChanged);
                connect(
                    this,
                    &QAbstractListModel::rowsRemoved,
                    this,
                    &ShotgunListModel::lengthChanged);
            }

            void populate(const utility::JsonStore &);
            void setTruncated(const bool truncated = true) {
                if (truncated_ != truncated) {
                    truncated_ = truncated;
                    emit truncatedChanged();
                }
            }

            [[nodiscard]] int
            rowCount(const QModelIndex &parent = QModelIndex()) const override {
                Q_UNUSED(parent);
                return (data_.is_null() ? 0 : data_.size());
            }
            [[nodiscard]] int length() const { return rowCount(); }
            [[nodiscard]] bool truncated() const { return truncated_; }

            Q_INVOKABLE int
            search(const QVariant &value, const QString &role = "display", const int start = 0);
            Q_INVOKABLE QVariant get(int row, const QString &role = "display");
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            Q_INVOKABLE void clear() { populate(utility::JsonStore(R"([])"_json)); }

            Q_INVOKABLE void append(const QVariant &data);

            [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
                QHash<int, QByteArray> roles;
                for (const auto &p : role_names) {
                    roles[p.first] = p.second.c_str();
                }
                return roles;
            }

            utility::JsonStore &modelData() { return data_; }

            void setQueryValueCache(
                const std::map<std::string, std::map<std::string, utility::JsonStore>> *qvc) {
                query_value_cache_ = qvc;
            }
            utility::JsonStore getQueryValue(
                const std::string &type,
                const utility::JsonStore &value,
                const int project_id = -1) const;


          signals:
            void lengthChanged();
            void truncatedChanged();

          protected:
            utility::JsonStore data_;
            bool truncated_{false};
            const std::map<std::string, std::map<std::string, utility::JsonStore>>
                *query_value_cache_{nullptr};
        };

        class ShotgunFilterModel : public QSortFilterProxyModel {
            Q_OBJECT

            Q_PROPERTY(int length READ length NOTIFY lengthChanged)
            Q_PROPERTY(int count READ length NOTIFY lengthChanged)
            Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY
                           sortAscendingChanged)
            Q_PROPERTY(QString sortRoleName READ sortRoleName WRITE setSortRoleName NOTIFY
                           sortRoleNameChanged)

            Q_PROPERTY(QItemSelection selection READ selection WRITE setSelection NOTIFY
                           selectionChanged)

            Q_PROPERTY(QItemSelection selectionFilter READ selectionFilter WRITE
                           setSelectionFilter NOTIFY selectionFilterChanged)

          public:
            ShotgunFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
                setFilterRole(ShotgunListModel::Roles::nameRole);
                setFilterCaseSensitivity(Qt::CaseInsensitive);
                setDynamicSortFilter(true);
                setSortRole(ShotgunListModel::Roles::createdDateRole);
                sort(0, Qt::DescendingOrder);

                connect(
                    this,
                    &QAbstractListModel::rowsInserted,
                    this,
                    &ShotgunFilterModel::lengthChanged);
                connect(
                    this,
                    &QAbstractListModel::modelReset,
                    this,
                    &ShotgunFilterModel::lengthChanged);
                connect(
                    this,
                    &QAbstractListModel::rowsRemoved,
                    this,
                    &ShotgunFilterModel::lengthChanged);
            }

            [[nodiscard]] QItemSelection selection() const { return selection_sort_; }
            [[nodiscard]] QItemSelection selectionFilter() const { return selection_filter_; }

            [[nodiscard]] int length() const { return rowCount(); }
            // [[nodiscard]] QObject *srcModel() const { return }

            Q_INVOKABLE int
            search(const QVariant &value, const QString &role = "display", const int start = 0);
            Q_INVOKABLE QVariant get(int row, const QString &role = "display");

            Q_INVOKABLE [[nodiscard]] QString
            getRoleFilter(const QString &role = "display") const;
            Q_INVOKABLE void
            setRoleFilter(const QString &filter, const QString &role = "display");

            [[nodiscard]] bool sortAscending() const {
                return sortOrder() == Qt::AscendingOrder;
            }

            [[nodiscard]] QString sortRoleName() const {
                try {
                    return roleNames().value(sortRole());
                } catch (...) {
                }

                return QString();
            }

            void setSelection(const QItemSelection &selection) {
                if (selection_sort_ != selection) {
                    selection_sort_ = selection;
                    emit selectionChanged();
                    setDynamicSortFilter(false);
                    sort(0, sortOrder());
                    setDynamicSortFilter(true);
                }
            }

            void setSelectionFilter(const QItemSelection &selection) {
                if (selection_filter_ != selection) {
                    selection_filter_ = selection;
                    emit selectionFilterChanged();
                    invalidateFilter();
                    setDynamicSortFilter(false);
                    sort(0, sortOrder());
                    setDynamicSortFilter(true);
                }
            }

            void setSortAscending(const bool ascending = true) {
                if (ascending != (sortOrder() == Qt::AscendingOrder ? true : false)) {
                    sort(0, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
                    emit sortAscendingChanged();
                }
            }
            [[nodiscard]] int getRoleId(const QString &role) const {
                auto role_id = -1;

                QHashIterator<int, QByteArray> it(roleNames());
                if (it.findNext(role.toUtf8())) {
                    role_id = it.key();
                }
                return role_id;
            }

            void setSortRoleName(const QString &role) {
                auto role_id = getRoleId(role);
                if (role_id != sortRole()) {
                    setSortRole(role_id);
                    emit sortRoleNameChanged();
                }
            }

          signals:
            void lengthChanged();
            void sortAscendingChanged();
            void sortRoleNameChanged();
            void selectionChanged();
            void selectionFilterChanged();

          protected:
            [[nodiscard]] bool
            filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
            [[nodiscard]] bool
            lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;

          private:
            std::map<int, QString> roleFilterMap_;

            QItemSelection selection_sort_;
            QItemSelection selection_filter_;
        };

        class ShotModel : public ShotgunListModel {
            Q_OBJECT

          public:
            ShotModel(QObject *parent = nullptr) : ShotgunListModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

            void setSequenceMap(const QMap<int, ShotgunListModel *> *map) {
                sequence_map_ = map;
            }

          protected:
            [[nodiscard]] utility::JsonStore
            getSequence(const int project_id, const int shot_id) const;

          private:
            const QMap<int, ShotgunListModel *> *sequence_map_{nullptr};
        };

        class PlaylistModel : public ShotgunListModel {
            Q_OBJECT

          public:
            PlaylistModel(QObject *parent = nullptr) : ShotgunListModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        };

        class EditModel : public ShotgunListModel {
            Q_OBJECT

          public:
            EditModel(QObject *parent = nullptr) : ShotgunListModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        };

        class ReferenceModel : public ShotModel {
            Q_OBJECT

          public:
            ReferenceModel(QObject *parent = nullptr) : ShotModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        };

        class MediaActionModel : public ShotModel {
            Q_OBJECT

          public:
            MediaActionModel(QObject *parent = nullptr) : ShotModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        };

        class NoteModel : public ShotgunListModel {
            Q_OBJECT

          public:
            NoteModel(QObject *parent = nullptr) : ShotgunListModel(parent) {}
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio
