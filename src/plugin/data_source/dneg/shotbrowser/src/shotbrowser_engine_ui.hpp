// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "xstudio/ui/qml/helper_ui.hpp"

#include "query_engine.hpp"

CAF_PUSH_WARNINGS
#include <QUrl>
#include <qqml.h>
#include <QQmlEngineExtensionPlugin>
#include <QAbstractListModel>
#include <QQmlApplicationEngine>
#include <QFuture>
#include <QtConcurrent>
#include <QQmlPropertyMap>
CAF_POP_WARNINGS

namespace xstudio {
namespace ui {
    namespace qml {
        using namespace std::chrono_literals;
        const auto SHOTGRID_TIMEOUT = 120s;

        class ShotBrowserSequenceFilterModel;
        class ShotBrowserSequenceModel;
        class ShotBrowserListModel;
        class ShotBrowserFilterModel;
        class ShotBrowserPresetModel;

        class ShotBrowserEngine : public QMLActor {

            Q_OBJECT
            // Q_DISABLE_COPY(ShotBrowserEngine)
            Q_PROPERTY(bool connected READ connected WRITE setConnected NOTIFY connectedChanged)
            Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)

            Q_PROPERTY(QObject *presetsModel READ presetsModel NOTIFY presetsModelChanged)

            Q_PROPERTY(
                QString backendId READ backendId WRITE setBackendId NOTIFY backendIdChanged)

            Q_PROPERTY(QString liveLinkMetadata READ liveLinkMetadata WRITE setLiveLinkMetadata
                           NOTIFY liveLinkMetadataChanged)
            Q_PROPERTY(QVariant liveLinkKey READ liveLinkKey WRITE setLiveLinkKey NOTIFY
                           liveLinkKeyChanged)

            QML_NAMED_ELEMENT("ShotBrowserEngine")
            //![0]
          public:
            static ShotBrowserEngine *instance();
            static QObject *qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine);
            ~ShotBrowserEngine() override = default;

            void init(caf::actor_system &system) override;
            QString name() { return name_; }
            QString backend() { return backend_str_; }
            QString backendId() { return QStringFromStd(std::to_string(backend_id_)); }

            void set_backend(caf::actor backend);
            void setBackendId(const QString &id);

            Q_INVOKABLE QObject *presetsModel();
            Q_INVOKABLE QObject *sequenceTreeModel(const int project_id);

            Q_INVOKABLE QString getShotgunUserName();

            [[nodiscard]] bool connected() const { return connected_; }

            void setConnected(const bool connected);

            QString liveLinkMetadata() const { return live_link_metadata_string_; }
            void setLiveLinkMetadata(QString &data);

            Q_INVOKABLE QString getProjectFromMetadata() const;
            Q_INVOKABLE QString getShotSequenceFromMetadata() const;

            QVariant liveLinkKey() const { return live_link_key_; }
            void setLiveLinkKey(const QVariant &key);

            Q_INVOKABLE bool undo() { return undoFuture().result(); };
            Q_INVOKABLE bool redo() { return redoFuture().result(); }

            Q_INVOKABLE QFuture<bool> undoFuture();
            Q_INVOKABLE QFuture<bool> redoFuture();

            bool ready() const { return ready_; }

            QueryEngine &queryEngine() { return query_engine_; }

          private:
            explicit ShotBrowserEngine(QObject *parent = nullptr);
            void updateQueryEngineTermModel(const std::string &key, const bool cache);

          signals:
            void liveLinkMetadataChanged();
            void liveLinkKeyChanged();
            void connectedChanged();
            void nameChanged();
            void requestSecret(const QString &message);
            void backendChanged();
            void backendIdChanged();
            void systemInit(QObject *object);

            void presetModelsChanged();
            void presetsModelChanged();
            void readyChanged();

            void projectChanged(const int project_id, const QString &project_name);

          public slots:
            void authenticate(const bool cancel = false);
            void setName(const QString &name) {
                if (name_ != name) {
                    name_ = name;
                    emit nameChanged();
                }
            }

            // void updateModel(const QString &name);

            QFuture<QString> getDataFuture(const QString &type, const int project_id = -1);

            QString getPlaylistVersions(const int id) {
                return getPlaylistVersionsFuture(id).result();
            }
            QFuture<QString> getPlaylistVersionsFuture(const int id);

            QString getPlaylistNotes(const int id) {
                return getPlaylistNotesFuture(id).result();
            }
            QFuture<QString> getPlaylistNotesFuture(const int id);

            QString addVersionToPlaylist(
                const QString &version, const QUuid &playlist, const QUuid &before = QUuid()) {
                return addVersionToPlaylistFuture(version, playlist, before).result();
            }
            QFuture<QString> addVersionToPlaylistFuture(
                const QString &version, const QUuid &playlist, const QUuid &before = QUuid());

            QFuture<QString> addVersionsToPlaylistFuture(
                const QVariantMap &versions,
                const QUuid &playlist,
                const QUuid &before = QUuid());

            QString getProjects() { return getProjectsFuture().result(); }
            QFuture<QString> getProjectsFuture();

            QFuture<QString> getSchemaFieldsFuture(const QString &type);

            QFuture<QString> getGroupsFuture(const int project_id);

            QString getSequences(const int project_id) {
                return getSequencesFuture(project_id).result();
            }
            QFuture<QString> getSequencesFuture(const int project_id);

            QString getShots(const int project_id) {
                return getShotsFuture(project_id).result();
            }
            QFuture<QString> getShotsFuture(const int project_id);

            QString getUsers(const int project_id = -1) {
                return getUsersFuture(project_id).result();
            }
            QFuture<QString> getUsersFuture(const int project_id = -1);

            QString getPipeStep() { return getPipeStepFuture().result(); }
            QFuture<QString> getPipeStepFuture();

            QString getEntity(const QString &entity, const int id) {
                return getEntityFuture(entity, id).result();
            }
            QFuture<QString> getEntityFuture(const QString &entity, const int id);

            QString getReferenceTags() { return getReferenceTagsFuture().result(); }
            QFuture<QString> getReferenceTagsFuture();

            QString tagEntity(const QString &entity, const int record_id, const int tag_id) {
                return tagEntityFuture(entity, record_id, tag_id).result();
            }
            QFuture<QString>
            tagEntityFuture(const QString &entity, const int record_id, const int tag_id);

            QString untagEntity(const QString &entity, const int record_id, const int tag_id) {
                return untagEntityFuture(entity, record_id, tag_id).result();
            }
            QFuture<QString>
            untagEntityFuture(const QString &entity, const int record_id, const int tag_id);

            QString createTag(const QString &text) { return createTagFuture(text).result(); }
            QFuture<QString> createTagFuture(const QString &text);

            QString renameTag(const int tag_id, const QString &text) {
                return renameTagFuture(tag_id, text).result();
            }
            QFuture<QString> renameTagFuture(const int tag_id, const QString &text);

            QString removeTag(const int tag_id) { return removeTagFuture(tag_id).result(); }
            QFuture<QString> removeTagFuture(const int tag_id);

            QString updateEntity(
                const QString &entity, const int record_id, const QString &update_json) {
                return updateEntityFuture(entity, record_id, update_json).result();
            }
            QFuture<QString> updateEntityFuture(
                const QString &entity, const int record_id, const QString &update_json);

            QString updatePlaylistVersions(const QUuid &playlist) {
                return updatePlaylistVersionsFuture(playlist).result();
            }
            QFuture<QString> updatePlaylistVersionsFuture(const QVariant &playlist) {
                return updatePlaylistVersionsFuture(playlist.toUuid());
            }
            QFuture<QString> updatePlaylistVersionsFuture(const QUuid &playlist);

            QString
            refreshPlaylistVersions(const QUuid &playlist, const bool matchOrder = false) {
                return refreshPlaylistVersionsFuture(playlist, matchOrder).result();
            }
            // QFuture<QString> refreshPlaylistVersionsFuture(
            //     const QVariant &playlist, const bool matchOrder = false) {
            //     return refreshPlaylistVersionsFuture(playlist.toUuid(), matchOrder);
            // }
            QFuture<QString>
            refreshPlaylistVersionsFuture(const QUuid &playlist, const bool matchOrder = false);

            // QString refreshPlaylistNotes(const QUuid &playlist) {
            //     return refreshPlaylistNotesFuture(playlist).result();
            // }
            // QFuture<QString> refreshPlaylistNotesFuture(const QVariant &playlist) {
            //     return refreshPlaylistNotesFuture(playlist.toUuid());
            // }
            // QFuture<QString> refreshPlaylistNotesFuture(const QUuid &playlist);

            QString createPlaylist(
                const QUuid &playlist,
                const int project_id,
                const QString &name,
                const QString &location,
                const QString &playlist_type) {

                return createPlaylistFuture(playlist, project_id, name, location, playlist_type)
                    .result();
            }
            QFuture<QString> createPlaylistFuture(
                const QVariant &playlist,
                const int project_id,
                const QString &name,
                const QString &location,
                const QString &playlist_type) {
                return createPlaylistFuture(
                    playlist.toUuid(), project_id, name, location, playlist_type);
            }
            QFuture<QString> createPlaylistFuture(
                const QUuid &playlist,
                const int project_id,
                const QString &name,
                const QString &location,
                const QString &playlist_type);


            QString getVersions(const int project_id, const QVariant &ids) {
                return getVersionsFuture(project_id, ids).result();
            }
            QFuture<QString> getVersionsFuture(const int project_id, const QVariant &ids);

            QString preparePlaylistNotes(
                const QUuid &playlist,
                const QList<QUuid> &media,
                const bool notify_owner                 = false,
                const std::vector<int> notify_group_ids = {},
                const bool combine                      = false,
                const bool add_time                     = false,
                const bool add_playlist_name            = false,
                const bool add_type                     = false,
                const bool anno_requires_note           = true,
                const bool skip_already_published       = false,
                const QString &defaultType              = "") {
                return preparePlaylistNotesFuture(
                           playlist,
                           media,
                           notify_owner,
                           notify_group_ids,
                           combine,
                           add_time,
                           add_playlist_name,
                           add_type,
                           anno_requires_note,
                           skip_already_published,
                           defaultType)
                    .result();
            }
            QFuture<QString> preparePlaylistNotesFuture(
                const QUuid &playlist,
                const QList<QUuid> &media,
                const bool notify_owner                 = false,
                const std::vector<int> notify_group_ids = {},
                const bool combine                      = false,
                const bool add_time                     = false,
                const bool add_playlist_name            = false,
                const bool add_type                     = false,
                const bool anno_requires_note           = true,
                const bool skip_already_published       = false,
                const QString &defaultType              = "");

            QString pushPlaylistNotes(const QString &notes, const QUuid &playlist) {
                return pushPlaylistNotesFuture(notes, playlist).result();
            }
            QFuture<QString>
            pushPlaylistNotesFuture(const QString &notes, const QUuid &playlist);

            QString getPlaylistValidMediaCount(const QUuid &playlist) {
                return getPlaylistValidMediaCountFuture(playlist).result();
            }
            QFuture<QString> getPlaylistValidMediaCountFuture(const QUuid &playlist);

            QString getPlaylistLinkMedia(const QUuid &playlist) {
                return getPlaylistLinkMediaFuture(playlist).result();
            }
            QFuture<QString> getPlaylistLinkMediaFuture(const QUuid &playlist);

            QString requestFileTransfer(
                const QVariantList &items,
                const QString &project,
                const QString &src_location,
                const QString &dest_location) {
                return requestFileTransferFuture(items, project, src_location, dest_location)
                    .result();
            }
            QFuture<QString> requestFileTransferFuture(
                const QVariantList &items,
                const QString &project,
                const QString &src_location,
                const QString &dest_location);

            void populateCaches();

            QFuture<QString> executeProjectQuery(
                const QStringList &preset_paths,
                const int project_id,
                const QVariantMap &env           = QVariantMap(),
                const QVariantList &custom_terms = QVariantList());

            QFuture<QString> executeQuery(
                const QStringList &preset_paths,
                const QVariantMap &env           = QVariantMap(),
                const QVariantList &custom_terms = QVariantList());

            QVariant mergeQueries(
                const QVariant &dst,
                const QVariant &src,
                const bool ignore_duplicates = true) const;

            // value used by query, may map to id.

            void updateQueryValueCache(
                const std::string &type,
                const utility::JsonStore &data,
                const int project_id = -1);

            QFuture<QString> addDownloadToMediaFuture(const QUuid &uuid);
            QString addDownloadToMedia(const QUuid &uuid) {
                return addDownloadToMediaFuture(uuid).result();
            }

            void cacheProject(const int project_id);

            void requestData(
                const QPersistentModelIndex &index,
                const int role,
                const nlohmann::json &request) const;

          private:
            void JSONTreeSendEventFunc(const utility::JsonStore &event);

            void receivedDataSlot(
                const QPersistentModelIndex &index, const int role, const QString &result);

            QFuture<QString> loadPresetModelFuture();

            QFuture<QString> getCustomEntity24Future(const int project_id);
            void createUnitCache(const int project_id);

            QFuture<QString> getStageFuture(const int project_id);
            void createStageCache(const int project_id);

            QFuture<QString> getPlaylistsFuture(const int project_id);
            void createPlaylistCache(const int project_id);

            void createShotCache(const int project_id);
            void createSequenceModels(const int project_id);
            void createGroupModel(const int project_id);
            void createUserCache(const int project_id);

            QFuture<QString> getDepartmentsFuture();

            void checkReady();

          private:
            bool connected_{false};
            static ShotBrowserEngine *this_;
            caf::actor backend_;
            caf::actor_id backend_id_{0};
            caf::actor backend_events_;

            QString backend_str_;
            QString name_{"test"};

            QMap<int, ShotBrowserSequenceModel *> sequences_tree_map_;

            std::map<std::string, bool> preset_update_pending_;
            int maximum_result_count_{2500};

            QVariant live_link_key_;
            QString live_link_metadata_string_;
            utility::JsonStore live_link_metadata_;

            // map UI value to query value
            QueryEngine query_engine_;

            ShotBrowserPresetModel *presets_model_{nullptr};

            bool disable_flush_{true};

            utility::Uuid user_preset_event_id_;
            utility::Uuid system_preset_event_id_;

            bool ready_{false};
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio
