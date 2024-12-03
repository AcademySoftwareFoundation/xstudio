// SPDX-License-Identifier: Apache-2.0
#pragma once

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/session_qml_export.h"

#include <caf/all.hpp>
#include <queue>

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/timeline/item.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

// namespace xstudio::timeline {
//     class Item;
// }

namespace xstudio::ui::qml {
using namespace caf;

class SESSION_QML_EXPORT SessionModel : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(QString sessionActorAddr READ sessionActorAddr WRITE setSessionActorAddr NOTIFY
                   sessionActorAddrChanged)

    Q_PROPERTY(bool modified READ modified WRITE setModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString bookmarkActorAddr READ bookmarkActorAddr NOTIFY bookmarkActorAddrChanged)
    Q_PROPERTY(
        QUuid onScreenPlayheadUuid READ onScreenPlayheadUuid NOTIFY onScreenPlayheadUuidChanged)
    Q_PROPERTY(QVariant playlists READ playlists NOTIFY playlistsChanged)

    // currentMediaContainerIndex is the index of the Playlist/Subset/Timeline that is being
    // viewed in the MediaList panels etc.
    Q_PROPERTY(QPersistentModelIndex currentMediaContainerIndex READ currentMediaContainerIndex
                   WRITE setCurrentMediaContainer NOTIFY currentMediaContainerChanged)

    // viewportCurrentMediaContainerIndex is the index of the Playlist/Subset/Timeline that owns
    // the Plyhead that is currently driving the viewport This can be different to
    // currentMediaContainerIndex, as sometimes we need to look at media in a different playlist
    // to what is on-screen
    Q_PROPERTY(
        QPersistentModelIndex viewportCurrentMediaContainerIndex READ
            viewportCurrentMediaContainerIndex WRITE setViewportCurrentMediaContainerIndex
                NOTIFY viewportCurrentMediaContainerIndexChanged)


  public:
    enum Roles {
        activeDurationRole = JSONTreeModel::Roles::LASTROLE,
        activeRangeValidRole,
        activeStartRole,
        actorRole,
        actorUuidRole,
        audioActorUuidRole,
        availableDurationRole,
        availableStartRole,
        bitDepthRole,
        bookmarkUuidsRole,
        busyRole,
        clipMediaUuidRole,
        containerUuidRole,
        enabledRole,
        errorRole,
        expandedRole,
        flagColourRole,
        flagTextRole,
        formatRole,
        groupActorRole,
        imageActorUuidRole,
        lockedRole,
        markersRole,
        mediaCountRole,
        mediaDisplayInfoRole,
        mediaStatusRole,
        metadataChangedRole,
        mtimeRole,
        nameRole,
        notificationRole,
        pathRole,
        pathShakeRole,
        pixelAspectRole,
        placeHolderRole,
        propertyRole,
        rateFPSRole,
        resolutionRole,
        selectionRole,
        thumbnailImageRole,
        thumbnailURLRole,
        timecodeAsFramesRole,
        trackIndexRole,
        trimmedDurationRole,
        trimmedStartRole,
        typeRole,
        uuidRole,
        userDataRole,
    };

    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit SessionModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE void dump() const { spdlog::warn("{}", modelData().dump(2)); }

    Q_INVOKABLE void dump(const QModelIndex &index) const {
        qDebug() << "Dumping index " << index;
        const nlohmann::json &j = indexToData(index);
        std::cerr << j.dump(2) << "\n";
    }

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void fetchMore(const QModelIndex &parent) override;

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

    Q_INVOKABLE QModelIndexList moveRows(
        const QModelIndexList &indexes,
        const int row,
        const QModelIndex &parent,
        const bool doCopy = false);

    Q_INVOKABLE void
    mergeRows(const QModelIndexList &indexes, const QString &name = "Combined Playlist");

    // begin timeline operations

    Q_INVOKABLE bool
    removeTimelineItems(const QModelIndexList &indexes, const bool isRipple = false);
    Q_INVOKABLE bool
    removeTimelineItems(const QModelIndex &track_index, const int frame, const int duration);

    Q_INVOKABLE QModelIndex
    bakeTimelineItems(const QModelIndexList &indexes, const QString &trackName = "");

    Q_INVOKABLE QModelIndexList duplicateTimelineClips(
        const QModelIndexList &indexes,
        const QString &trackName   = "",
        const QString &trackSuffix = "Duplicate",
        const bool append          = true);

    Q_INVOKABLE QModelIndexList
    duplicateTimelineClipsTo(const QModelIndexList &indexes, const QModelIndex &trackIndex);

    Q_INVOKABLE bool replaceTimelineTrack(const QModelIndex &src, const QModelIndex &dst);

    Q_INVOKABLE QRect timelineRect(const QModelIndexList &indexes) const;

    Q_INVOKABLE QModelIndex getTimelineIndex(const QModelIndex &index) const;
    Q_INVOKABLE QModelIndex getTimelineTrackIndex(const QModelIndex &index) const;

    Q_INVOKABLE QModelIndex
    getTimelineClipIndex(const QModelIndex &timelineIndex, const int frame);

    Q_INVOKABLE void resetTimelineItemDragFlag(const QModelIndexList &items);
    Q_INVOKABLE void updateTimelineItemDragFlag(
        const QModelIndexList &items,
        const bool isRolling,
        const bool isRipple,
        const bool isOverwrite);

    Q_INVOKABLE void beginTimelineItemDrag(
        const QModelIndexList &items,
        const QString &mode,
        const bool isRipple    = false,
        const bool isOverwrite = false);
    Q_INVOKABLE void updateTimelineItemDrag(
        const QModelIndexList &items,
        const QString &mode,
        int frameChange,
        int trackChange,
        const bool isRipple    = false,
        const bool isOverwrite = false);
    Q_INVOKABLE void endTimelineItemDrag(
        const QModelIndexList &items, const QString &mode, const bool isOverwrite = false);

    Q_INVOKABLE void draggingAdjust(const QModelIndex &item, const int frameChange);
    Q_INVOKABLE int checkAdjust(
        const QModelIndex &item,
        const int frameChange,
        const bool lockDuration = false,
        const bool lockEnd      = false);

    Q_INVOKABLE QModelIndexList
    getTimelineClipIndexes(const QModelIndex &timelineIndex, const QModelIndex &mediaIndex);

    Q_INVOKABLE int startFrameInParent(const QModelIndex &timelineItemIndex);

    Q_INVOKABLE QVariantList snapTo(
        const QModelIndex &ignore,
        const int cursor,
        const int clipStart,
        const int clipDuration,
        const int currentOffset,
        const int window,
        const QUuid &key = QUuid());

    // return all gap/clip items boundaries in timeline frames.
    Q_INVOKABLE QVariantList boundaryFramesInTimeline(const QModelIndexList &indexes);


    Q_INVOKABLE QModelIndexList getIndexesByName(
        const QModelIndex &idx, const QString &name, const QString &type = "") const;

    Q_INVOKABLE QModelIndexList modifyItemSelectionHorizontal(
        const QModelIndexList &clips, const int left, const int right);
    Q_INVOKABLE QModelIndexList
    modifyItemSelectionVertical(const QModelIndexList &clips, const int up, const int down);

    Q_INVOKABLE QVariantList mediaFrameToTimelineFrames(
        const QModelIndex &timelineIndex,
        const QModelIndex &mediaIndex,
        const int logicalMediaFrame,
        const bool skipDisabled = false);

    Q_INVOKABLE QModelIndexList getTimelineVideoClipIndexesFromRect(
        const QModelIndex &timelineIndex,
        const int left,
        const int top,
        const int right,
        const int bottom,
        const double frameScale,
        const double trackScale,
        const bool skipLocked = false);

    Q_INVOKABLE QModelIndexList getTimelineAudioClipIndexesFromRect(
        const QModelIndex &timelineIndex,
        const int left,
        const int top,
        const int right,
        const int bottom,
        const double frameScale,
        const double trackScale,
        const bool skipLocked = false);

    Q_INVOKABLE int
    getTimelineFrameFromClip(const QModelIndex &clipIndex, const int logicalMediaFrame);

    Q_INVOKABLE QModelIndex insertTimelineGap(
        const int row,
        const QModelIndex &parent,
        const int frames    = 24,
        const double rate   = 24.0,
        const QString &name = "Gap");
    Q_INVOKABLE QModelIndex insertTimelineClip(
        const int row,
        const QModelIndex &parent,
        const QModelIndex &mediaIndex,
        const QString &name = "Clip");
    Q_INVOKABLE QModelIndex splitTimelineClip(const int frame, const QModelIndex &index);


    Q_INVOKABLE bool moveTimelineItem(const QModelIndex &index, const int distance);
    Q_INVOKABLE bool moveRangeTimelineItems(
        const QModelIndex &track_index,
        const int frame,
        const int duration,
        const int dest,
        const bool insert);
    Q_INVOKABLE bool
    alignTimelineItems(const QModelIndexList &indexes, const bool align_right = true);

    Q_INVOKABLE QFuture<bool>
    exportOTIO(const QModelIndex &timeline, const QUrl &path, const QString &type = "otio");

    // end timeline operations

    // notification methods
    Q_INVOKABLE void removeNotification(const QModelIndex &index, const QUuid &uuid);
    Q_INVOKABLE QUuid infoNotification(
        const QModelIndex &index,
        const QString &text,
        const int seconds        = 10,
        const QUuid &replaceUuid = QUuid());
    Q_INVOKABLE QUuid warnNotification(
        const QModelIndex &index,
        const QString &text,
        const int seconds        = 10,
        const QUuid &replaceUuid = QUuid());
    Q_INVOKABLE QUuid processingNotification(const QModelIndex &index, const QString &text);
    Q_INVOKABLE QUuid
    progressPercentageNotification(const QModelIndex &index, const QString &text);
    Q_INVOKABLE QUuid progressRangeNotification(
        const QModelIndex &index, const QString &text, const float min, const float max);
    Q_INVOKABLE void
    updateProgressNotification(const QModelIndex &index, const QUuid &uuid, const float value);


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

    static nlohmann::json timelineItemToJson(
        const timeline::Item &item, caf::actor_system &sys, const bool recurse = true);


    Q_INVOKABLE QString save(const QUrl &path, const QModelIndexList &selection = {}) {
        return saveFuture(path, selection).result();
    }
    Q_INVOKABLE QFuture<QString>
    saveFuture(const QUrl &path, const QModelIndexList &selection = {});


    Q_INVOKABLE QFuture<bool> importFuture(const QUrl &path, const QVariant &json);
    Q_INVOKABLE bool import(const QUrl &path, const QVariant &json) {
        return importFuture(path, json).result();
    }

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

    [[nodiscard]] QUuid onScreenPlayheadUuid() const { return on_screen_playhead_uuid_; };

    [[nodiscard]] QVariant playlists() const;

    Q_INVOKABLE void moveSelectionByIndex(const QModelIndex &index, const int offset);
    Q_INVOKABLE void updateSelection(
        const QModelIndex &index,
        const QModelIndexList &selection,
        const QItemSelectionModel::SelectionFlags &mode = QItemSelectionModel::ClearAndSelect);
    Q_INVOKABLE void
    updateMediaListFilterString(const QModelIndex &index, const QString &filter_string);
    Q_INVOKABLE void gatherMediaFor(const QModelIndex &index, const QModelIndexList &selection);

    Q_INVOKABLE QVariant getJSONObject(const QModelIndex &index, const QString &path) {
        return getJSONObjectFuture(index, path).result();
    }
    Q_INVOKABLE QFuture<QVariant> getJSONObjectFuture(
        const QModelIndex &index, const QString &path, const bool includeSource = false);

    Q_INVOKABLE QString getJSON(const QModelIndex &index, const QString &path) {
        return getJSONFuture(index, path).result();
    }
    Q_INVOKABLE QFuture<QString> getJSONFuture(
        const QModelIndex &index, const QString &path, const bool includeSource = false);

    Q_INVOKABLE QStringList
    getMediaSourceNames(const QModelIndex &media_index, bool image_sources);
    Q_INVOKABLE QStringList setMediaSource(
        const QModelIndex &media_index, const QString &mediaSourceName, bool image_source);


    Q_INVOKABLE bool
    setJSON(const QModelIndex &index, const QString &json, const QString &path = "") {
        return setJSONFuture(index, json, path).result();
    }
    Q_INVOKABLE QFuture<bool>
    setJSONFuture(const QModelIndex &index, const QString &json, const QString &path = "");

    Q_INVOKABLE void sortByMediaDisplayInfo(
        const QModelIndex &index, const int sort_column_idx, const bool ascending);

    Q_INVOKABLE void setViewportCurrentMediaContainerIndex(const QModelIndex &index);
    Q_INVOKABLE void setCurrentMediaContainer(const QModelIndex &index);

    Q_INVOKABLE void relinkMedia(const QModelIndexList &indexes, const QUrl &path);
    Q_INVOKABLE void decomposeMedia(const QModelIndexList &indexes);
    Q_INVOKABLE void rescanMedia(const QModelIndexList &indexes);
    Q_INVOKABLE QModelIndex getPlaylistIndex(const QModelIndex &index) const;
    Q_INVOKABLE QModelIndex getContainerIndex(const QModelIndex &index) const;

    Q_INVOKABLE QFuture<bool> undoFuture(const QModelIndex &index);
    Q_INVOKABLE QFuture<bool> redoFuture(const QModelIndex &index);

    Q_INVOKABLE void purgePlaylist(const QModelIndex &index);

    Q_INVOKABLE QPersistentModelIndex currentMediaContainerIndex() {
        return current_playlist_index_;
    }

    Q_INVOKABLE QPersistentModelIndex viewportCurrentMediaContainerIndex() {
        return current_playhead_owner_index_;
    }

    Q_INVOKABLE void
    setTimelineFocus(const QModelIndex &timeline, const QModelIndexList &indexes) const;

    Q_INVOKABLE bool undo(const QModelIndex &index) { return undoFuture(index).result(); }
    Q_INVOKABLE bool redo(const QModelIndex &index) { return redoFuture(index).result(); }

    Q_INVOKABLE void updateCurrentMediaContainerIndexFromBackend();
    Q_INVOKABLE void updateViewportCurrentMediaContainerIndexFromBackend();

  public slots:
    void updateMedia();
    void setSelectedMedia(const QModelIndexList &indexes);

  signals:

    void bookmarkActorAddrChanged();
    void onScreenPlayheadUuidChanged();
    void sessionActorAddrChanged();
    void mediaAdded(const QModelIndex &index);
    void mediaStatusChanged(const QModelIndex &playlist_index);
    void modifiedChanged();
    void playlistsChanged();
    void currentMediaContainerChanged();
    void viewportCurrentMediaContainerIndexChanged();
    void
    mediaSourceChanged(const QModelIndex &media, const QModelIndex &source, const int mode);
    void makeTimelineSelection(QModelIndex timeline, QModelIndexList items);

  public:
    caf::actor_system &system() const { return self()->home_system(); }
    static nlohmann::json createEntry(const nlohmann::json &update = R"({})"_json);

  protected:
    QModelIndexList searchRecursiveListBase(
        const QVariant &value,
        const int role,
        const QModelIndex &parent,
        const int start,
        const int hits,
        const int depth = -1) override;
    // QModelIndexList searchRecursive_fast(
    //     const nlohmann::json &searchValue,
    //     const std::string &searchKey,
    //     const nlohmann::json::json_pointer &path,
    //     const nlohmann::json &root,
    //     const int hits) const;

    // QModelIndexList searchRecursive_media(
    //     const nlohmann::json &searchValue,
    //     const std::string &searchKey,
    //     const nlohmann::json::json_pointer &path,
    //     const nlohmann::json &root,
    //     const int hits) const;

  private:
    bool isChildOf(const QModelIndex &parent, const QModelIndex &child) const;
    int depthOfChild(const QModelIndex &parent, const QModelIndex &child) const;

    void triggerMediaStatusChange(const QModelIndex &index);

    void updateErroredCount(const QModelIndex &media_index);

    QModelIndexList getTimelineClipIndexesFromRect(
        const QModelIndex &timelineIndex,
        const int left,
        const int top,
        const int right,
        const int bottom,
        const double frameScale,
        const double trackScale,
        const timeline::ItemType type,
        const bool skipLocked);


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

    void finishedDataSlot(const QVariant &search_value, const int search_role, const int role);
    void startedDataSlot(const QVariant &search_value, const int search_role, const int role);

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

    caf::actor actorFromIndex(const QModelIndex &index, const bool try_parent = false) const;
    utility::Uuid actorUuidFromIndex(const QModelIndex &index, const bool try_parent = false);
    utility::Uuid
    containerUuidFromIndex(const QModelIndex &index, const bool try_parent = false);

    void processChildren(const nlohmann::json &result_json, const QModelIndex &index);

    void forcePopulate(
        const utility::JsonTree &tree,
        const QPersistentModelIndex &search_hint = QModelIndex());
    utility::Uuid refreshId(nlohmann::json &ij);

    QFuture<QList<QUuid>> handleMediaIdDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleTimelineIdDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleContainerIdDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleUriListDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);
    QFuture<QList<QUuid>> handleOtherDropFuture(
        const int proposedAction, const utility::JsonStore &drop, const QModelIndex &index);

    void add_id_uuid_lookup(const utility::Uuid &uuid, const QModelIndex &index);
    void add_uuid_lookup(const utility::Uuid &uuid, const QModelIndex &index);
    void add_string_lookup(const std::string &str, const QModelIndex &index);
    void add_lookup(const utility::JsonTree &tree, const QModelIndex &index);
    void item_event_callback(const utility::JsonStore &event, timeline::Item &item);
    void add_processed_event(const utility::Uuid &uuid);
    bool wait_for_event(
        const utility::Uuid &uuid,
        const std::chrono::milliseconds &timeout = std::chrono::milliseconds(1000));

  private:
    QString session_actor_addr_;
    QString bookmark_actor_addr_;
    QUuid on_screen_playhead_uuid_;

    caf::actor session_actor_;

    utility::time_point saved_time_;
    utility::time_point last_changed_;

    mutable std::set<std::string> in_flight_requests_;
    QThreadPool *request_handler_;

    std::map<utility::Uuid, std::set<QPersistentModelIndex>> id_uuid_lookup_;
    std::map<utility::Uuid, std::set<QPersistentModelIndex>> uuid_lookup_;
    std::map<std::string, std::set<QPersistentModelIndex>> string_lookup_;

    std::map<caf::actor, timeline::Item> timeline_lookup_;

    bool mediaStatusChangePending_{false};
    QPersistentModelIndex mediaStatusIndex_;
    QPersistentModelIndex current_playlist_index_;
    QPersistentModelIndex current_playhead_owner_index_;

    QMap<QString, QImage> media_thumbnails_; // key is actor string
    utility::UuidSet processed_events_;
    std::queue<utility::Uuid> processed_events_queue_;
};

} // namespace xstudio::ui::qml
