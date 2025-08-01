// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <filesystem>

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QStringList>
#include <QUrl>
#include <QFuture>
#include <QtConcurrent>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class PlaylistUI;
        class BookmarksUI;
        class PlaylistSelectionUI;

        class GroupModel : public QAbstractListModel {
            Q_OBJECT
          public:
            enum PlaylistRoles {
                TypeRole = Qt::UserRole + 1,
                NameRole,
                ObjectRole,
                ExpandedRole,
                SelectedRole
            };

            GroupModel(QObject *parent = nullptr);
            [[nodiscard]] int
            rowCount(const QModelIndex &parent = QModelIndex()) const override;
            // int columnCount(const QModelIndex& parent = QModelIndex()) const;
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            bool populate(QList<QObject *> &items, QObject *controller = nullptr);
            // virtual bool insertRows(int row, int count, const QModelIndex &parent =
            // QModelIndex());
            //                bool moveRows(
            // const QModelIndex &sourceParent,
            // int sourceRow,
            // int count,
            // const QModelIndex &destinationParent,
            // int destinationChild);
          public slots:
            bool move(int sourceRow, int count, int destinationChild);
            QVariant get_object(const int sourceRow);
            QVariant next_object(const QVariant obj);
            [[nodiscard]] bool empty() const;
            [[nodiscard]] int size() const;


          protected:
            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

          private:
            QList<QObject *> data_;
            QObject *controller_{nullptr};
        };

        class ContainerDividerUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(QUuid cuuid READ qcuuid NOTIFY cuuidChanged)

            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QUuid uuid READ quuid NOTIFY uuidChanged)
            Q_PROPERTY(QString type READ type NOTIFY typeChanged)
            Q_PROPERTY(QString flag READ flag WRITE setFlag NOTIFY flagChanged)
            Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)

          public:
            explicit ContainerDividerUI(QObject *parent = nullptr);
            explicit ContainerDividerUI(
                const utility::Uuid cuuid,
                const std::string name,
                const std::string type,
                const std::string flag,
                const utility::Uuid uuid,
                QObject *parent = nullptr);
            ~ContainerDividerUI() override = default;

            [[nodiscard]] bool selected() const { return selected_; }
            [[nodiscard]] QString name() const { return QStringFromStd(name_); }

            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] QUuid qcuuid() const { return QUuidFromUuid(cuuid_); }

            [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
            [[nodiscard]] utility::Uuid cuuid() const { return cuuid_; }

            [[nodiscard]] QString type() const { return QStringFromStd(type_); }
            [[nodiscard]] QString flag() const { return QStringFromStd(flag_); }

            friend class SessionUI;

          signals:
            void uuidChanged();
            void cuuidChanged();
            void nameChanged();
            void typeChanged();
            void flagChanged();
            void selectedChanged();

          public slots:
            void setName(const QString &_name) {
                if (_name != name()) {
                    name_ = StdFromQString(_name);
                    emit nameChanged();
                }
            }
            void setFlag(const QString &_flag) {
                if (_flag != flag()) {
                    flag_ = StdFromQString(_flag);
                    emit flagChanged();
                }
            }
            void setSelected(const bool value = true) {
                if (selected_ != value) {
                    selected_ = value;
                    emit selectedChanged();
                }
            }

          private:
            utility::Uuid cuuid_;
            std::string name_;
            std::string type_;
            std::string flag_;
            utility::Uuid uuid_;
            bool selected_{false};
        };


        class ContainerGroupUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QUuid cuuid READ qcuuid NOTIFY cuuidChanged)
            Q_PROPERTY(QUuid uuid READ quuid NOTIFY uuidChanged)
            Q_PROPERTY(QList<QObject *> items READ items NOTIFY itemsChanged)
            Q_PROPERTY(QVariant itemModel READ itemModel NOTIFY itemModelChanged)
            Q_PROPERTY(QString type READ type NOTIFY typeChanged)
            Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
            Q_PROPERTY(QString flag READ flag WRITE setFlag NOTIFY flagChanged)
            Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)

          public:
            explicit ContainerGroupUI(QObject *parent = nullptr);
            explicit ContainerGroupUI(
                const utility::Uuid cuuid,
                const std::string name,
                const std::string type,
                const std::string flag,
                const utility::Uuid uuid,
                QObject *parent = nullptr);
            ~ContainerGroupUI() override = default;

            [[nodiscard]] bool selected() const { return selected_; }
            [[nodiscard]] QString name() const { return QStringFromStd(name_); }
            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] utility::Uuid uuid() const { return uuid_; }

            [[nodiscard]] QUuid qcuuid() const { return QUuidFromUuid(cuuid_); }
            [[nodiscard]] utility::Uuid cuuid() const { return cuuid_; }

            QList<QObject *> items() { return items_; }

            [[nodiscard]] QObject *findItem(const utility::Uuid &uuid) const;
            [[nodiscard]] QString type() const { return QStringFromStd(type_); }
            QVariant itemModel() { return QVariant::fromValue(&model_); }

            QList<QObject *> items_;
            [[nodiscard]] bool expanded() const { return expanded_; }
            [[nodiscard]] QString flag() const { return QStringFromStd(flag_); }

            friend class SessionUI;
            friend class PlaylistUI;

          public slots:
            void setExpanded(const bool value = true) {
                expanded_ = value;
                emit expandedChanged();
            }
            void setName(const QString &_name) {
                if (_name != name()) {
                    name_ = StdFromQString(_name);
                    emit nameChanged();
                }
            }
            void setFlag(const QString &_flag) {
                if (_flag != flag()) {
                    flag_ = StdFromQString(_flag);
                    emit flagChanged();
                }
            }
            void setSelected(const bool value = true) {
                selected_ = value;
                emit selectedChanged();
            }

          signals:
            void cuuidChanged();
            void uuidChanged();
            void nameChanged();
            void itemsChanged();
            void typeChanged();
            void itemModelChanged();
            void expandedChanged();
            void flagChanged();
            void selectedChanged();

          private:
            // QUuid findCUuidFromUuid(const utility::Uuid &uuid) const;

            void changedItems() {
                spdlog::info("changed group {}", items_.count());
                model_.populate(items_, this);
                emit itemsChanged();
            }

          private:
            utility::Uuid cuuid_;
            std::string name_;
            std::string type_;
            std::string flag_;
            utility::Uuid uuid_;
            GroupModel model_;
            bool expanded_{true};
            bool selected_{false};
        };

        class SessionUI : public QMLActor {

            Q_OBJECT
            // Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QVariant itemModel READ itemModel NOTIFY itemModelChanged)
            Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
            Q_PROPERTY(bool modified READ modified WRITE setModified NOTIFY modifiedChanged)
            Q_PROPERTY(QUrl path READ path WRITE setPath NOTIFY pathChanged)
            Q_PROPERTY(QString pathNative READ pathNative NOTIFY pathChanged)

            Q_PROPERTY(QString sessionActorAddr READ sessionActorAddr WRITE setSessionActorAddr
                           NOTIFY sessionActorAddrChanged)

            Q_PROPERTY(QVariant playlistNames READ playlistNames NOTIFY playlistNamesChanged)
            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(
                QDateTime sessionFileMTime READ sessionFileMTime NOTIFY sessionFileMTimechanged)

            Q_PROPERTY(QObject *playlist READ playlist NOTIFY playlistChanged)
            Q_PROPERTY(QObject *selectedSource READ selectedSource NOTIFY selectedSourceChanged)
            Q_PROPERTY(QObject *onScreenSource READ onScreenSource NOTIFY onScreenSourceChanged)
            Q_PROPERTY(
                double mediaRate READ mediaRate WRITE setMediaRate NOTIFY mediaRateChanged)

          public:
            explicit SessionUI(QObject *parent = nullptr);
            ~SessionUI() override;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            // QList<QObject *> playlists() {return playlists_list_;}
            QVariant itemModel() { return QVariant::fromValue(&model_); }
            [[nodiscard]] bool expanded() const { return expanded_; }

            [[nodiscard]] bool modified() const {
                return xstudio::utility::time_point() != last_changed_ and
                       saved_time_ < last_changed_;
            }
            [[nodiscard]] QUrl path() const { return QUrlFromUri(path_); }
            [[nodiscard]] QString pathNative() const {
                return QStringFromStd(utility::uri_to_posix_path(path_));
            }
            [[nodiscard]] QString name() const { return name_; }
            [[nodiscard]] QDateTime sessionFileMTime() const {
                return QDateTime::fromMSecsSinceEpoch(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        session_file_mtime_.time_since_epoch())
                        .count());
            }

            void setSessionActorAddr(const QString &addr);

            QVariant playlistNames() { return playlistNames_; }

            QObject *playlist();

            QObject *selectedSource();
            QObject *onScreenSource() { return on_screen_source_; }

            // QString name();
            utility::Uuid uuid(const QObject *obj) const;
            utility::Uuid cuuid(const QObject *obj) const;

            double mediaRate() { return media_rate_.to_fps(); }

            [[nodiscard]] QString sessionActorAddr() const { return session_actor_addr_; };

          signals:

            // void nameChanged();
            void backendChanged();

            void sessionActorAddrChanged();

            // void playlistsChanged();
            void modifiedChanged();
            void itemModelChanged();
            void expandedChanged();
            void pathChanged();
            void newItem(QVariant cuuid);
            void playlistRemoved(const QVariant uuid);
            void nameChanged();
            void playlistChanged();
            void playlistNamesChanged();
            void selectedSourceChanged();
            void onScreenSourceChanged();
            void sessionRequest(const QUrl path, const QByteArray jsn);
            void mediaAdded(const QUuid &uuid);
            void mediaRateChanged(const double rate);
            void sessionFileMTimechanged();

          public slots:
            // bool save(const QUrl &path);
            // bool load(const QUrl &path);
            void setModified(const bool modified = true, bool clear = false) {
                if (modified)
                    saved_time_ = utility::time_point();
                else if (clear)
                    saved_time_ = utility::clock::now() + std::chrono::seconds(1);
                else
                    saved_time_ = last_changed_;
                emit modifiedChanged();
            }
            void setPath(const QUrl &path, const bool inform_backend = true);
            QString posixPath() const {
                return QStringFromStd(utility::uri_to_posix_path(path_));
            }
            void initSystem(QObject *system_qobject);

            QUuid createPlaylist(
                const QString &name = "Untitled Playlist",
                const QUuid before  = QUuid(),
                const bool into     = false);

            QUuid
            createGroup(const QString &name = "Untitled Group", const QUuid before = QUuid());
            QUuid createDivider(
                const QString &name = "Untitled Divider",
                const QUuid before  = QUuid(),
                const bool into     = false);
            void setExpanded(const bool value) {
                expanded_ = value;
                emit expandedChanged();
            }
            bool reflagContainer(const QString &flag, const QUuid &uuid);
            bool renameContainer(const QString &name, const QUuid &cuuid);
            bool
            moveContainer(const QUuid &uuid, const QUuid &before_uuid, const bool into = false);
            bool removeContainer(const QUuid &cuuid);
            QUuid duplicateContainer(
                const QUuid &cuuid,
                const QUuid &before_cuuid = QUuid(),
                const bool into           = false);
            QObject *findItem(const QUuid &uuid) const { return findItem(UuidFromQUuid(uuid)); }

            QList<QUuid> handleDrop(const QVariantMap &drop) {
                return handleDropFuture(drop).result();
            }
            QFuture<QList<QUuid>> handleDropFuture(const QVariantMap &drop);

            bool loadUrls(const QList<QUrl> &urls, const bool single_playlist = false);
            bool loadUrl(const QUrl &url, const bool single_playlist = false);
            void
            setSelection(const QVariantList &cuuids = QVariantList(), const bool clear = true);
            void
            setSelection(QObject *obj, const bool selected = true, const bool clear = true);
            bool isSelected(QObject *obj) const;
            QObject *getPlaylist(const QUuid &uuid);
            QList<QObject *> getPlaylists();

            void newSession(const QString &name);
            QString save();
            QString save(const QUrl &path);
            QString save_selected(const QUrl &path);
            bool load(const QUrl &path);
            bool load(const QUrl &path, const QVariant &json);
            bool import(const QUrl &path, const QVariant &json = QVariant());
            void setName(const QString &name);
            void switchOnScreenSource(const QUuid &uuid = QUuid(), const bool broadcast = true);
            void rebuildPlaylistNamesList();
            void setMediaRate(const double rate);
            void setViewerPlayhead();
            QUuid mergePlaylists(
                const QVariantList &cuuids = QVariantList(),
                const QString &name        = "Combined Playlist",
                const QUuid &before_cuuid  = QUuid(),
                const bool into            = false);

            QVariantList copyContainers(
                const QVariantList &cuuids,
                const QUuid &playlist_cuuid,
                const QUuid &before_cuuid = QUuid());

            QVariantList moveContainers(
                const QVariantList &cuuids,
                const QUuid &playlist_cuuid,
                const bool with_media     = false,
                const QUuid &before_cuuid = QUuid());

            QVariantList copyMedia(
                const QUuid &target_cuuid,
                const QVariantList &media_quuids,
                const bool force_duplicate = false,
                const QUuid &before_cuuid  = QUuid());

            QVariantList moveMedia(
                const QUuid &target_cuuid,
                const QUuid &src_cuuid,
                const QVariantList &media_quuids,
                const QUuid &before_cuuid = QUuid());

            QUuid getNextItemUuid(const QUuid &quuid);

          private:
            bool
            loadUris(const std::vector<caf::uri> &uris, const bool single_playlist = false);
            [[nodiscard]] QObject *findItem(const utility::Uuid &uuid) const;
            void updateItemModel(const bool select_new_items = false, const bool reset = false);
            QObject *getNextItem(const utility::Uuid &cuuid);


          private:
            caf::actor backend_;
            caf::actor backend_events_;
            caf::uri path_;
            std::filesystem::file_time_type session_file_mtime_{
                std::filesystem::file_time_type::min()};

            QMap<QUuid, QObject *> items_;
            QList<QObject *> items_list_;
            GroupModel model_;

            QVariant playlistNames_;
            QString name_;

            bool expanded_{true};
            utility::time_point saved_time_;
            utility::time_point last_changed_;

            PlaylistUI *playlist_{nullptr};
            QObject *selected_source_{nullptr};
            QObject *on_screen_source_{nullptr};
            utility::PlaylistTree last_changed_tree_;
            utility::FrameRate media_rate_;

            caf::actor dummy_playlist_;

            QString session_actor_addr_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio