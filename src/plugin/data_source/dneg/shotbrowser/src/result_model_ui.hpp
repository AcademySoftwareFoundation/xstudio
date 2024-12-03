// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/ui/qml/json_tree_model_ui.hpp"


CAF_PUSH_WARNINGS
#include <QUrl>
#include <qqml.h>
#include <QQmlEngineExtensionPlugin>
#include <QAbstractListModel>
#include <QQmlApplicationEngine>
#include <QFuture>
#include <QtConcurrent>
#include <QQmlPropertyMap>
#include <QSortFilterProxyModel>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace std::chrono_literals;

class ShotBrowserResultModel : public JSONTreeModel {

    Q_OBJECT
    Q_PROPERTY(int length READ length NOTIFY lengthChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY stateChanged)

    Q_PROPERTY(int page READ page NOTIFY stateChanged)
    Q_PROPERTY(int executionMilliseconds READ executionMilliseconds NOTIFY stateChanged)
    Q_PROPERTY(int maxResult READ maxResult NOTIFY stateChanged)

    Q_PROPERTY(QUuid presetId READ presetId NOTIFY stateChanged)
    Q_PROPERTY(QUuid groupId READ groupId NOTIFY stateChanged)

    Q_PROPERTY(QString audioSource READ audioSource NOTIFY stateChanged)
    Q_PROPERTY(QString visualSource READ visualSource NOTIFY stateChanged)
    Q_PROPERTY(QString flagColour READ flagColour NOTIFY stateChanged)
    Q_PROPERTY(QString flagText READ flagText NOTIFY stateChanged)
    Q_PROPERTY(QString entity READ entity NOTIFY stateChanged)
    Q_PROPERTY(QDateTime requestedAt READ requestedAt NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap env READ env NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap context READ context NOTIFY stateChanged)

    Q_PROPERTY(bool isGrouped READ isGrouped WRITE setIsGrouped NOTIFY isGroupedChanged)
    Q_PROPERTY(bool canBeGrouped READ canBeGrouped NOTIFY canBeGroupedChanged)


    QML_NAMED_ELEMENT("ShotBrowserResultModel")
    //![0]
  public:
    enum Roles {
        addressingRole = JSONTreeModel::Roles::LASTROLE,
        artistRole,
        assetRole,
        attachmentsRole,
        authorRole,
        clientFilenameRole,
        clientNoteRole,
        contentRole,
        createdByRole,
        createdDateRole,
        dateSubmittedToClientRole,
        departmentRole,
        detailRole,
        frameRangeRole,
        frameSequenceRole,
        linkedVersionsRole,
        loginRole,
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
        otioRole,
        pipelineStatusFullRole,
        pipelineStatusRole,
        pipelineStepRole,
        playlistNameRole,
        playlistTypeRole,
        productionStatusRole,
        projectIdRole,
        projectRole,
        resultRowRole,
        sequenceRole,
        shotRole,
        siteRole,
        stageRole,
        stalkUuidRole,
        subjectRole,
        submittedToDailiesRole,
        tagRole,
        thumbRole,
        twigNameRole,
        twigTypeRole,
        typeRole,
        URLRole,
        versionCountRole,
        versionNameRole,
        versionRole
    };

    explicit ShotBrowserResultModel(QObject *parent = nullptr);
    ~ShotBrowserResultModel() override = default;

    [[nodiscard]] int length() const { return rowCount(); }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE bool setResultData(const QStringList &data) {
        return setResultDataFuture(data).result();
    }
    Q_INVOKABLE QFuture<bool> setResultDataFuture(const QStringList &data);

    void fetchMore(const QModelIndex &parent) override;

    [[nodiscard]] bool truncated() const;
    [[nodiscard]] int page() const;
    [[nodiscard]] int maxResult() const;
    [[nodiscard]] int executionMilliseconds() const;

    [[nodiscard]] QUuid presetId() const;
    [[nodiscard]] QUuid groupId() const;

    [[nodiscard]] QString audioSource() const;
    [[nodiscard]] QString visualSource() const;
    [[nodiscard]] QString flagColour() const;
    [[nodiscard]] QString flagText() const;
    [[nodiscard]] QString entity() const;
    [[nodiscard]] QDateTime requestedAt() const;
    [[nodiscard]] QVariantMap env() const;
    [[nodiscard]] QVariantMap context() const;

    [[nodiscard]] bool isGrouped() const { return is_grouped_; }
    [[nodiscard]] bool canBeGrouped() const { return can_be_grouped_; }

    void setIsGrouped(const bool value);
    void setCanBeGrouped(const bool value);

  signals:
    void lengthChanged();
    void truncatedChanged();
    void stateChanged();
    void isGroupedChanged();
    void canBeGroupedChanged();

  private:
    template <typename T> T getResultValue(const std::string &path, const T value) const {
        const auto jp = nlohmann::json::json_pointer(path);
        T result      = value;
        if (result_data_.contains(jp) and not result_data_.at(jp).is_null())
            result = result_data_.at(jp);

        return result;
    }

    void groupBy();
    void unGroupBy();

    void setEmpty();

    nlohmann::json result_data_ = R"({})"_json;
    bool is_grouped_{false};
    bool can_be_grouped_{true};
    bool was_grouped_{false};
};

class ShotBrowserResultFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(int length READ length NOTIFY lengthChanged)
    Q_PROPERTY(int count READ length NOTIFY lengthChanged)


    Q_PROPERTY(bool sortByNaturalOrder READ sortByNaturalOrder WRITE setSortByNaturalOrder
                   NOTIFY sortByChanged)
    Q_PROPERTY(bool sortByCreationDate READ sortByCreationDate WRITE setSortByCreationDate
                   NOTIFY sortByChanged)
    Q_PROPERTY(
        bool sortByShotName READ sortByShotName WRITE setSortByShotName NOTIFY sortByChanged)
    Q_PROPERTY(bool sortInAscending READ sortInAscending WRITE setSortInAscending NOTIFY
                   sortInAscendingChanged)

    Q_PROPERTY(bool filterChn READ filterChn WRITE setFilterChn NOTIFY filterChnChanged)
    Q_PROPERTY(bool filterLon READ filterLon WRITE setFilterLon NOTIFY filterLonChanged)
    Q_PROPERTY(bool filterMtl READ filterMtl WRITE setFilterMtl NOTIFY filterMtlChanged)
    Q_PROPERTY(bool filterMum READ filterMum WRITE setFilterMum NOTIFY filterMumChanged)
    Q_PROPERTY(bool filterVan READ filterVan WRITE setFilterVan NOTIFY filterVanChanged)
    Q_PROPERTY(bool filterSyd READ filterSyd WRITE setFilterSyd NOTIFY filterSydChanged)

    Q_PROPERTY(QString filterPipeStep READ filterPipeStep WRITE setFilterPipeStep NOTIFY
                   filterPipeStepChanged)
    Q_PROPERTY(QString filterName READ filterName WRITE setFilterName NOTIFY filterNameChanged)

    QML_NAMED_ELEMENT("ShotBrowserResultFilterModel")

  public:
    typedef enum { OM_NATURAL = 0, OM_DATE, OM_SHOT } OrderMode;


    ShotBrowserResultFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        sort(0);

        // not sure this stuff is working..
        connect(
            this,
            &QSortFilterProxyModel::rowsInserted,
            this,
            &ShotBrowserResultFilterModel::lengthChanged);
        connect(
            this,
            &QSortFilterProxyModel::modelReset,
            this,
            &ShotBrowserResultFilterModel::lengthChanged);
        connect(
            this,
            &QSortFilterProxyModel::rowsRemoved,
            this,
            &ShotBrowserResultFilterModel::lengthChanged);
        connect(
            this,
            &QSortFilterProxyModel::rowsMoved,
            this,
            &ShotBrowserResultFilterModel::lengthChanged);
    }

    [[nodiscard]] bool sortByNaturalOrder() const { return ordering_ == OM_NATURAL; }
    [[nodiscard]] bool sortByCreationDate() const { return ordering_ == OM_DATE; }
    [[nodiscard]] bool sortByShotName() const { return ordering_ == OM_SHOT; }
    [[nodiscard]] bool sortInAscending() const { return sortInAscending_; }

    [[nodiscard]] bool filterChn() const { return filterChn_; }
    [[nodiscard]] bool filterLon() const { return filterLon_; }
    [[nodiscard]] bool filterMtl() const { return filterMtl_; }
    [[nodiscard]] bool filterMum() const { return filterMum_; }
    [[nodiscard]] bool filterVan() const { return filterVan_; }
    [[nodiscard]] bool filterSyd() const { return filterSyd_; }

    [[nodiscard]] QString filterPipeStep() const { return filterPipeStep_; }
    [[nodiscard]] QString filterName() const { return filterName_; }

    [[nodiscard]] int length() const { return rowCount(); }

    void setSortByNaturalOrder(const bool value);
    void setSortByCreationDate(const bool value);
    void setSortByShotName(const bool value);
    void setSortInAscending(const bool value);

    void setFilterChn(const bool value);
    void setFilterLon(const bool value);
    void setFilterMtl(const bool value);
    void setFilterMum(const bool value);
    void setFilterVan(const bool value);
    void setFilterSyd(const bool value);

    void setFilterPipeStep(const QString &value);
    void setFilterName(const QString &value);

  signals:
    void sortByChanged();
    void sortInAscendingChanged();

    void filterChnChanged();
    void filterLonChanged();
    void filterMtlChanged();
    void filterMumChanged();
    void filterVanChanged();
    void filterSydChanged();

    void filterPipeStepChanged();
    void filterNameChanged();

    void lengthChanged();

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    [[nodiscard]] bool
    lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

  private:
    bool sortInAscending_{false};

    bool filterChn_{false};
    bool filterLon_{false};
    bool filterMtl_{false};
    bool filterMum_{false};
    bool filterVan_{false};
    bool filterSyd_{false};

    QString filterPipeStep_{};
    QString filterName_{};

    OrderMode ordering_{OM_NATURAL};
};

} // namespace xstudio::ui::qml
