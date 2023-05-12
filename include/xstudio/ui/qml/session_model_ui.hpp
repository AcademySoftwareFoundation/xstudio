// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/ui/qml/tag_ui.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;


class CafResponse : public QObject {
    Q_OBJECT

  signals:
    //  Search value, search role, set role, set value
    void received(QVariant, int, int, QString);

  public:
    CafResponse(
        const QVariant &search_value,
        const int search_role,
        const QModelIndex &index,
        int role,
        QThreadPool *pool);
    CafResponse(
        const QVariant &search_value,
        const int search_role,
        const nlohmann::json &data,
        int role,
        QThreadPool *pool);

  private:
    void handleFinished();

    QFutureWatcher<QMap<int, QString>> watcher_;

    const QVariant search_value_;
    const int search_role_;
    const int role_;
};

class SessionModel : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(QString sessionActorAddr READ sessionActorAddr WRITE setSessionActorAddr NOTIFY
                   sessionActorAddrChanged)

    Q_PROPERTY(QQmlPropertyMap *tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(bool modified READ modified WRITE setModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString bookmarkActorAddr READ bookmarkActorAddr NOTIFY bookmarkActorAddrChanged)

  public:
    enum Roles {
        actorRole = JSONTreeModel::Roles::LASTROLE,
        actorUuidRole,
        audioActorUuidRole,
        busyRole,
        childrenRole,
        containerUuidRole,
        flagRole,
        groupActorRole,
        idRole,
        imageActorUuidRole,
        mediaStatusRole,
        mtimeRole,
        nameRole,
        pathRole,
        rateFPSRole,
        thumbnailURLRole,
        typeRole,
    };

    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit SessionModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    QQmlPropertyMap *tags() { return tag_manager_->attrs_map_; }

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool
    removeRows(int row, int count, const bool deep, const QModelIndex &parent = QModelIndex());

    // doesn't directly modify json, this is back populated from backend
    Q_INVOKABLE bool
    duplicateRows(int row, int count, const QModelIndex &parent = QModelIndex());

    // possibly makes above redundant..
    Q_INVOKABLE QModelIndexList
    copyRows(const QModelIndexList &indexes, const int row, const QModelIndex &parent);


    Q_INVOKABLE bool moveRows(
        const QModelIndex &sourceParent,
        int sourceRow,
        int count,
        const QModelIndex &destinationParent,
        int destinationChild) override {
        return JSONTreeModel::moveRows(
            sourceParent, sourceRow, count, destinationParent, destinationChild);
    }

    Q_INVOKABLE QModelIndexList
    moveRows(const QModelIndexList &indexes, const int row, const QModelIndex &parent);

    Q_INVOKABLE void
    mergeRows(const QModelIndexList &indexes, const QString &name = "Combined Playlist");


    [[nodiscard]] QString sessionActorAddr() const { return session_actor_addr_; };
    void setSessionActorAddr(const QString &addr);

    Q_INVOKABLE QFuture<QList<QUuid>>
    handleDropFuture(const QVariantMap &drop, const QModelIndex &index = QModelIndex());

    Q_INVOKABLE QModelIndexList insertRowsAsync(
        int row,
        int count,
        const QString &type,
        const QString &name,
        const QModelIndex &parent = QModelIndex()) {
        return insertRows(row, count, type, name, parent, false);
    }

    Q_INVOKABLE QModelIndexList insertRowsSync(
        int row,
        int count,
        const QString &type,
        const QString &name,
        const QModelIndex &parent = QModelIndex()) {
        return insertRows(row, count, type, name, parent, true);
    }

    static nlohmann::json playlistTreeToJson(
        const utility::PlaylistTree &tree,
        caf::actor_system &sys,
        const utility::UuidActorMap &uuid_actors);

    static nlohmann::json
    containerDetailToJson(const utility::ContainerDetail &detail, caf::actor_system &sys);

    Q_INVOKABLE QString save(const QUrl &path, const QModelIndexList &selection = {}) {
        return saveFuture(path, selection).result();
    }
    Q_INVOKABLE QFuture<QString>
    saveFuture(const QUrl &path, const QModelIndexList &selection = {});


    Q_INVOKABLE QFuture<bool> importFuture(const QUrl &path, const QVariant &json);
    Q_INVOKABLE bool import(const QUrl &path, const QVariant &json) {
        return importFuture(path, json).result();
    }


    Q_INVOKABLE QUuid addTag(
        const QUuid &quuid,
        const QString &type,
        const QString &data,
        const QString &unique = "",
        const bool persistent = false);


    Q_INVOKABLE QFuture<QUrl> getThumbnailURLFuture(const QModelIndex &index, const int frame);
    Q_INVOKABLE QFuture<QUrl>
    getThumbnailURLFuture(const QModelIndex &index, const float frame);
    Q_INVOKABLE QFuture<bool> clearCacheFuture(const QModelIndexList &indexes);

    [[nodiscard]] bool modified() const {
        return xstudio::utility::time_point() != last_changed_ and saved_time_ < last_changed_;
    }

    void setModified(const bool modified = true, bool clear = false) {
        if (modified)
            saved_time_ = utility::time_point();
        else if (clear)
            saved_time_ = utility::clock::now() + std::chrono::seconds(1);
        else
            saved_time_ = last_changed_;
        emit modifiedChanged();
    }
    [[nodiscard]] QString bookmarkActorAddr() const { return bookmark_actor_addr_; };


  signals:
    void bookmarkActorAddrChanged();
    void sessionActorAddrChanged();
    void mediaAdded(const QModelIndex &index);
    void tagsChanged();
    void modifiedChanged();


  public:
    caf::actor_system &system() { return self()->home_system(); }
    static nlohmann::json createEntry(const nlohmann::json &update = R"({})"_json);

  private:
    QModelIndexList insertRows(
        int row,
        int count,
        const QString &type,
        const QString &name,
        const QModelIndex &parent = QModelIndex(),
        const bool sync           = true);

    void receivedData(
        const QVariant &search_value,
        const int search_role,
        const int role,
        const QString &result);

    void requestData(
        const QVariant &search_value,
        const int search_role,
        const QModelIndex &index,
        const int role) const;
    void requestData(
        const QVariant &search_value,
        const int search_role,
        const nlohmann::json &data,
        const int role) const;

    caf::actor actorFromIndex(const QModelIndex &index, const bool try_parent = false);
    utility::Uuid actorUuidFromIndex(const QModelIndex &index, const bool try_parent = false);


    void processChildren(
        const nlohmann::json &result_json,
        nlohmann::json &index_json,
        const QModelIndex &index);


    QString session_actor_addr_;
    QString bookmark_actor_addr_;

    caf::actor session_actor_;
    TagManagerUI *tag_manager_{nullptr};

    utility::time_point saved_time_;
    utility::time_point last_changed_;
};

} // namespace xstudio::ui::qml
