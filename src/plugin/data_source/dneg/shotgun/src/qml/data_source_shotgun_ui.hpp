// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

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

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/history/history.hpp"

namespace xstudio {
using namespace shotgun_client;
namespace ui {
    namespace qml {
        using namespace std::chrono_literals;
        const auto SHOTGUN_TIMEOUT = 120s;

        class ShotModel;
        class ShotgunListModel;
        class PlaylistModel;
        class EditModel;
        class ReferenceModel;
        class NoteModel;
        class MediaActionModel;
        class ShotgunFilterModel;
        class ShotgunTreeModel;
        class ShotgunSequenceModel;
        class JSONTreeModel;

        class ShotgunDataSourceUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(bool connected READ connected WRITE setConnected NOTIFY connectedChanged)
            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
            Q_PROPERTY(
                QString backendId READ backendId WRITE setBackendId NOTIFY backendIdChanged)

            Q_PROPERTY(QQmlPropertyMap *termModels READ termModels NOTIFY termModelsChanged)
            Q_PROPERTY(
                QQmlPropertyMap *resultModels READ resultModels NOTIFY resultModelsChanged)
            Q_PROPERTY(
                QQmlPropertyMap *presetModels READ presetModels NOTIFY presetModelsChanged)

            Q_PROPERTY(QString liveLinkMetadata READ liveLinkMetadata WRITE setLiveLinkMetadata
                           NOTIFY liveLinkMetadataChanged)

            QML_NAMED_ELEMENT("ShotgunDataSource")
            //![0]
          public:
            explicit ShotgunDataSourceUI(QObject *parent = nullptr);
            ~ShotgunDataSourceUI() override = default;

            void init(caf::actor_system &system) override;
            QString name() { return name_; }
            QString backend() { return backend_str_; }
            QString backendId() { return QStringFromStd(std::to_string(backend_id_)); }

            void set_backend(caf::actor backend);
            void setBackendId(const QString &id);

            QQmlPropertyMap *termModels() const { return term_models_; }
            QQmlPropertyMap *resultModels() const { return result_models_; }
            QQmlPropertyMap *presetModels() const { return preset_models_; }

            Q_INVOKABLE QObject *groupModel(const int project_id);
            Q_INVOKABLE QObject *sequenceModel(const int project_id);
            Q_INVOKABLE QObject *sequenceTreeModel(const int project_id);
            Q_INVOKABLE QObject *shotModel(const int project_id);
            Q_INVOKABLE QObject *customEntity24Model(const int project_id);
            Q_INVOKABLE QObject *shotSearchFilterModel(const int project_id);
            Q_INVOKABLE QObject *playlistModel(const int project_id);

            Q_INVOKABLE QString getShotgunUserName();

            //  unused ?
            Q_INVOKABLE QString getShotSequence(const int project_id, const QString &shot);

            [[nodiscard]] bool connected() const { return connected_; }

            void setConnected(const bool connected);

            QString liveLinkMetadata() const { return live_link_metadata_string_; }
            void setLiveLinkMetadata(QString &data);

            Q_INVOKABLE void undo();
            Q_INVOKABLE void redo();
            Q_INVOKABLE void snapshot(const QString &preset);

            Q_INVOKABLE void resetPreset(const QString &preset, const int index = -1);

          signals:
            void liveLinkMetadataChanged();
            void connectedChanged();
            void nameChanged();
            void requestSecret(const QString &message);
            void backendChanged();
            void backendIdChanged();
            void systemInit(QObject *object);

            void termModelsChanged();
            void resultModelsChanged();
            void presetModelsChanged();

            void projectChanged(const int project_id, const QString &project_name);

          public slots:
            void authenticate(const bool cancel = false);
            void setName(const QString &name) {
                if (name_ != name) {
                    name_ = name;
                    emit nameChanged();
                }
            }

            void updateModel(const QString &name);

            QString getPlaylists(const int project_id) {
                return getPlaylistsFuture(project_id).result();
            }
            QFuture<QString> getPlaylistsFuture(const int project_id);

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

            QString getProjects() { return getProjectsFuture().result(); }
            QFuture<QString> getProjectsFuture();


            QString getSchemaFields(
                const QString &entity, const QString &field, const QString &update_name = "") {
                return getSchemaFieldsFuture(entity, field, update_name).result();
            }
            QFuture<QString> getSchemaFieldsFuture(
                const QString &entity, const QString &field, const QString &update_name = "");

            QString getGroups(const int project_id) {
                return getGroupsFuture(project_id).result();
            }
            QFuture<QString> getGroupsFuture(const int project_id);

            QString getSequences(const int project_id) {
                return getSequencesFuture(project_id).result();
            }
            QFuture<QString> getSequencesFuture(const int project_id);

            QString getShots(const int project_id) {
                return getShotsFuture(project_id).result();
            }
            QFuture<QString> getShotsFuture(const int project_id);

            QString getUsers() { return getUsersFuture().result(); }
            QFuture<QString> getUsersFuture();

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

            QString getDepartments() { return getDepartmentsFuture().result(); }
            QFuture<QString> getDepartmentsFuture();

            QString getCustomEntity24(const int project_id) {
                return getCustomEntity24Future(project_id).result();
            }
            QFuture<QString> getCustomEntity24Future(const int project_id);

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

            QString refreshPlaylistVersions(const QUuid &playlist) {
                return refreshPlaylistVersionsFuture(playlist).result();
            }
            QFuture<QString> refreshPlaylistVersionsFuture(const QVariant &playlist) {
                return refreshPlaylistVersionsFuture(playlist.toUuid());
            }
            QFuture<QString> refreshPlaylistVersionsFuture(const QUuid &playlist);

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
                const QVariantList &uuids,
                const QString &project,
                const QString &src_location,
                const QString &dest_location) {
                return requestFileTransferFuture(uuids, project, src_location, dest_location)
                    .result();
            }
            QFuture<QString> requestFileTransferFuture(
                const QVariantList &uuids,
                const QString &project,
                const QString &src_location,
                const QString &dest_location);

            void populateCaches();

            void syncModelChanges(const QString &name);

            QFuture<QString> executeQuery(
                const QString &context,
                const int project_id,
                const QVariant &query,
                const bool update_result_model = false);

            QFuture<QString> executeQueryNew(
                const QString &context,
                const int project_id,
                const QVariant &query,
                const bool update_result_model = false);

            QVariant mergeQueries(
                const QVariant &dst,
                const QVariant &src,
                const bool ignore_duplicates = true) const;

            // value used by query, may map to id.
            utility::JsonStore getQueryValue(
                const std::string &type,
                const utility::JsonStore &value,
                const int project_id = -1) const;
            void updateQueryValueCache(
                const std::string &type,
                const utility::JsonStore &data,
                const int project_id = -1);

            QString preferredVisual(const QString &category) const {
                QString result("SG Movie");
                try {
                    result =
                        QStringFromStd(source_selection_.at(StdFromQString(category)).first);
                } catch (...) {
                }
                return result;
            }

            QString preferredAudio(const QString &category) const {
                QString result("SG Movie");
                try {
                    result =
                        QStringFromStd(source_selection_.at(StdFromQString(category)).second);
                } catch (...) {
                }
                return result;
            }

            QString flagText(const QString &category) const {
                QString result("");
                try {
                    result = QStringFromStd(flag_selection_.at(StdFromQString(category)).first);
                } catch (...) {
                }
                return result;
            }

            QString flagColour(const QString &category) const {
                QString result("");
                try {
                    result =
                        QStringFromStd(flag_selection_.at(StdFromQString(category)).second);
                } catch (...) {
                }
                return result;
            }

            QFuture<QString> addDownloadToMediaFuture(const QUuid &uuid);
            QString addDownloadToMedia(const QUuid &uuid) {
                return addDownloadToMediaFuture(uuid).result();
            }

          private:
            utility::JsonStore purgeOldSystem(
                const utility::JsonStore &vprefs, const utility::JsonStore &drefs) const;

            void populatePresetModel(
                const utility::JsonStore &prefs,
                const std::string &path,
                ShotgunTreeModel *model,
                const bool purge_old   = true,
                const bool clear_flags = false);
            shotgun_client::Text addTextValue(
                const std::string &filter,
                const std::string &value,
                const bool negated = false) const;

            void addTerm(
                const int project_id,
                const std::string &context,
                shotgun_client::FilterBy *qry,
                const utility::JsonStore &term);

            std::tuple<
                utility::JsonStore,
                std::vector<std::string>,
                int,
                std::pair<std::string, std::string>,
                std::pair<std::string, std::string>>
            buildQuery(
                const std::string &context,
                const int project_id,
                const utility::JsonStore &query);

            void loadPresets(const bool purge_old = true);
            void flushPreset(const std::string &preset);
            utility::JsonStore buildDataFromField(const utility::JsonStore &data);

            void setPreset(const std::string &preset, const utility::JsonStore &data);
            utility::JsonStore getPresetData(const std::string &preset);

            void createCustomEntity24Models(const int project_id);
            void createShotModels(const int project_id);
            void createSequenceModels(const int project_id);

            void handleResult(
                const utility::JsonStore &request,
                const std::string &model,
                const std::string &name);

            bool connected_{false};
            caf::actor backend_;
            caf::actor_id backend_id_{0};
            QString backend_str_;
            QString name_{"test"};

            QMap<int, ShotgunListModel *> groups_map_;
            QMap<int, ShotgunFilterModel *> groups_filter_map_;
            QMap<int, ShotgunListModel *> sequences_map_;
            QMap<int, ShotgunSequenceModel *> sequences_tree_map_;
            QMap<int, ShotgunListModel *> shots_map_;
            QMap<int, ShotgunListModel *> custom_entity_24_map_;
            QMap<int, ShotgunFilterModel *> shots_filter_map_;
            QMap<int, ShotgunListModel *> playlists_map_;

            std::map<std::string, bool> preset_update_pending_;
            int maximum_result_count_{2500};

            QString live_link_metadata_string_;
            utility::JsonStore live_link_metadata_;

            // map UI value to query value
            std::map<std::string, std::map<std::string, utility::JsonStore>> query_value_cache_;
            // capture default source selection

            std::map<std::string, std::pair<std::string, std::string>> source_selection_;
            std::map<std::string, std::pair<std::string, std::string>> flag_selection_;

            history::History<std::pair<std::string, utility::JsonStore>> history_;

            QQmlPropertyMap *term_models_{nullptr};
            QQmlPropertyMap *result_models_{nullptr};
            QQmlPropertyMap *preset_models_{nullptr};
            bool disable_flush_{true};
            std::map<std::string, uint64_t> epoc_map_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio
