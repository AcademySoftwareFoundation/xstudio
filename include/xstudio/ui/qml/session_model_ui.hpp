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

class SessionModel : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(QString sessionActorAddr READ sessionActorAddr WRITE setSessionActorAddr NOTIFY
                   sessionActorAddrChanged)

    Q_PROPERTY(QQmlPropertyMap *tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(bool modified READ modified WRITE setModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString bookmarkActorAddr READ bookmarkActorAddr NOTIFY bookmarkActorAddrChanged)

    Q_PROPERTY(QVariant playlists READ playlists NOTIFY playlistsChanged)

  public:
    enum Roles {
        actorRole = JSONTreeModel::Roles::LASTROLE,
        actorUuidRole,
        audioActorUuidRole,
        bitDepthRole,
        busyRole,
        childrenRole,
        containerUuidRole,
        flagRole,
        formatRole,
        groupActorRole,
        idRole,
        imageActorUuidRole,
        mediaCountRole,
        mediaStatusRole,
        mtimeRole,
        nameRole,
        pathRole,
        pixelAspectRole,
        placeHolderRole,
        rateFPSRole,
        resolutionRole,
        thumbnailURLRole,
        typeRole,
        uuidRole,
    };

    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit SessionModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    QQmlPropertyMap *tags() { return tag_manager_->attrs_map_; }

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE void dump() const { spdlog::warn("{}", modelData().dump(2)); }

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

    Q_INVOKABLE QFuture<QList<QUuid>> handleDropFuture(
        const int proposedAction,
        const QVariantMap &drop,
        const QModelIndex &index = QModelIndex());

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

    static nlohmann::json sessionTreeToJson(
        const utility::PlaylistTree &tree,
        caf::actor_system &sys,
        const utility::UuidActorMap &uuid_actors);

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
            saved_time_ = utility::clock::now() + std::chrono::seconds(5);
        else
            saved_time_ = last_changed_;
        emit modifiedChanged();
    }
    [[nodiscard]] QString bookmarkActorAddr() const { return bookmark_actor_addr_; };

    [[nodiscard]] QVariant playlists() const;


    Q_INVOKABLE void moveSelectionByIndex(const QModelIndex &index, const int offset);
    Q_INVOKABLE void
    updateSelection(const QModelIndex &index, const QModelIndexList &selection);
    Q_INVOKABLE void gatherMediaFor(const QModelIndex &index, const QModelIndexList &selection);
    Q_INVOKABLE QString getJSON(const QModelIndex &index, const QString &path) {
        return getJSONFuture(index, path).result();
    }
    Q_INVOKABLE QFuture<QString> getJSONFuture(const QModelIndex &index, const QString &path);

    Q_INVOKABLE bool
    setJSON(const QModelIndex &index, const QString &json, const QString &path = "") {
        return setJSONFuture(index, json, path).result();
    }
    Q_INVOKABLE QFuture<bool>
    setJSONFuture(const QModelIndex &index, const QString &json, const QString &path = "");

    Q_INVOKABLE void sortAlphabetically(const QModelIndex &index);

    Q_INVOKABLE void setPlayheadTo(const QModelIndex &index);
    Q_INVOKABLE void setCurrentPlaylist(const QModelIndex &index);

    Q_INVOKABLE void relinkMedia(const QModelIndexList &indexes, const QUrl &path);
    Q_INVOKABLE void decomposeMedia(const QModelIndexList &indexes);
    Q_INVOKABLE void rescanMedia(const QModelIndexList &indexes);
    Q_INVOKABLE QModelIndex getPlaylistIndex(const QModelIndex &index) const;

  signals:
    void bookmarkActorAddrChanged();
    void sessionActorAddrChanged();
    void mediaAdded(const QModelIndex &index);
    void mediaStatusChanged(const QModelIndex &playlist_index);
    void tagsChanged();
    void modifiedChanged();
    void playlistsChanged();
    void
    mediaSourceChanged(const QModelIndex &media, const QModelIndex &source, const int mode);

  public:
    caf::actor_system &system() { return self()->home_system(); }
    static nlohmann::json createEntry(const nlohmann::json &update = R"({})"_json);

  protected:
    QModelIndexList search_recursive_list_base(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1) override;
    // QModelIndexList search_recursive_fast(
    //     const nlohmann::json &searchValue,
    //     const std::string &searchKey,
    //     const nlohmann::json::json_pointer &path,
    //     const nlohmann::json &root,
    //     const int hits) const;

    // QModelIndexList search_recursive_media(
    //     const nlohmann::json &searchValue,
    //     const std::string &searchKey,
    //     const nlohmann::json::json_pointer &path,
    //     const nlohmann::json &root,
    //     const int hits) const;

  private:
    bool isChildOf(const QModelIndex &parent, const QModelIndex &child) const;
    int depthOfChild(const QModelIndex &parent, const QModelIndex &child) const;

    QModelIndexList insertRows(
        int row,
        int count,
        const QString &type,
        const QString &name,
        const QModelIndex &parent = QModelIndex(),
        const bool sync           = true);

    void receivedDataSlot(
        const QVariant &search_value,
        const int search_role,
        const QPersistentModelIndex &search_hint,
        const int role,
        const QString &result);

    void receivedData(
        const nlohmann::json &search_value,
        const int search_role,
        const QPersistentModelIndex &search_hint,
        const int role,
        const nlohmann::json &result);

    void requestData(
        const QVariant &search_value,
        const int search_role,
        const QPersistentModelIndex &search_hint,
        const QModelIndex &index,
        const int role) const;
    void requestData(
        const QVariant &search_value,
        const int search_role,
        const QPersistentModelIndex &search_hint,
        const nlohmann::json &data,
        const int role) const;

    caf::actor actorFromIndex(const QModelIndex &index, const bool try_parent = false);
    utility::Uuid actorUuidFromIndex(const QModelIndex &index, const bool try_parent = false);

    void processChildren(const nlohmann::json &result_json, const QModelIndex &index);

    void forcePopulate(
        const utility::JsonTree &tree,
        const QPersistentModelIndex &search_hint = QModelIndex());
    utility::Uuid refreshId(nlohmann::json &ij);

    QFuture<QList<QUuid>> handleMediaIdDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleContainerIdDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleUriListDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleOtherDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);

    void add_uuid_lookup(const utility::Uuid &uuid, const QModelIndex &index);
    void add_string_lookup(const std::string &str, const QModelIndex &index);
    void add_lookup(const utility::JsonTree &tree, const QModelIndex &index);

  private:
    QString session_actor_addr_;
    QString bookmark_actor_addr_;

    caf::actor session_actor_;
    TagManagerUI *tag_manager_{nullptr};

    utility::time_point saved_time_;
    utility::time_point last_changed_;

    mutable std::set<std::tuple<QVariant, int, int>> in_flight_requests_;
    QThreadPool *request_handler_;


    std::map<utility::Uuid, std::set<QPersistentModelIndex>> uuid_lookup_;
    std::map<std::string, std::set<QPersistentModelIndex>> string_lookup_;
};

} // namespace xstudio::ui::qml
