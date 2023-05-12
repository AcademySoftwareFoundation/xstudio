// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QUrl>
#include <QFuture>
#include <QtConcurrent>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/ui/qml/session_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class PlaylistSelectionUI;

        class PlaylistUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QUuid uuid READ quuid NOTIFY uuidChanged)
            Q_PROPERTY(QObject *selectionFilter READ selectionFilter NOTIFY
                           playlistSelectionThingChanged)
            Q_PROPERTY(QString type READ type NOTIFY typeChanged)
            Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
            Q_PROPERTY(QVariant itemModel READ itemModel NOTIFY itemModelChanged)
            Q_PROPERTY(QVariant mediaModel READ mediaModel NOTIFY mediaModelChanged)
            Q_PROPERTY(QList<QObject *> mediaList READ mediaList NOTIFY mediaListChanged)
            Q_PROPERTY(QVariantMap mediaOrder READ mediaOrder NOTIFY mediaOrderChanged)
            Q_PROPERTY(QUuid cuuid READ qcuuid NOTIFY cuuidChanged)
            Q_PROPERTY(QString flag READ flag WRITE setFlag NOTIFY flagChanged)
            Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
            Q_PROPERTY(bool loadingMedia READ loadingMedia NOTIFY loadingMediaChanged)
            Q_PROPERTY(bool isBusy READ isBusy WRITE setIsBusy NOTIFY isBusyChanged)
            Q_PROPERTY(
                QObject *selectedSubitem READ selectedSubitem NOTIFY selectedSubitemChanged)
            Q_PROPERTY(QString fullName READ name NOTIFY nameChanged)
            Q_PROPERTY(bool hasBackend READ hasBackend NOTIFY backendChanged)
            Q_PROPERTY(
                int offlineMediaCount READ offlineMediaCount NOTIFY offlineMediaCountChanged)

          public:
            explicit PlaylistUI(
                const utility::Uuid cuuid = utility::Uuid(),
                const utility::Uuid uuid  = utility::Uuid(),
                QObject *parent           = nullptr);
            ~PlaylistUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend, const bool partial = false);

            caf::actor backend() { return backend_; }
            [[nodiscard]] bool hasBackend() const {
                return backend_ ? true : false;
                ;
            }

            [[nodiscard]] QString name() const { return QStringFromStd(name_); }

            [[nodiscard]] QVariantMap mediaOrder() const { return media_order_; }

            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] QUuid qcuuid() const { return QUuidFromUuid(cuuid_); }
            [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
            [[nodiscard]] QString type() const { return "Playlist"; }

            QVariant itemModel() { return QVariant::fromValue(&model_); }
            QVariant mediaModel() { return QVariant::fromValue(&media_model_); }
            QList<QObject *> mediaList() { return media_; }

            [[nodiscard]] bool expanded() const { return expanded_; }
            [[nodiscard]] bool selected() const { return selected_; }
            [[nodiscard]] bool loadingMedia() const { return loadingMedia_; }
            [[nodiscard]] bool isBusy() const { return isBusy_; }

            [[nodiscard]] utility::Uuid cuuid() const { return cuuid_; }
            [[nodiscard]] QString flag() const { return QStringFromStd(flag_); }
            [[nodiscard]] int offlineMediaCount() const;

            void setIsBusy(const bool busy) {
                isBusy_ = busy;
                emit isBusyChanged();
            }

            QObject *selectionFilter();

            utility::Uuid cuuid(const QObject *obj) const;

            QObject *selectedSubitem() { return selected_subitem_; }

            bool contains(const QUuid &uuid_or_cuuid, QUuid &cuuid) const;


          signals:

            void uuidChanged();
            void nameChanged();
            void typeChanged();
            void systemInit(QObject *);
            void playlistSelectionThingChanged();
            void backendChanged();
            void compareModeChanged();
            void expandedChanged();
            void selectedChanged();
            void itemModelChanged();
            void mediaModelChanged();
            void cuuidChanged();
            void mediaListChanged();
            void flagChanged();
            // QUuid is the container uuid
            void newItem(const QVariant &cuuid);
            void newSelection(const QVariantList cuuids, const bool clear);
            void selectedSubitemChanged();
            void mediaAdded(const QUuid &uuid);
            void loadingMediaChanged();
            void isBusyChanged();
            void mediaOrderChanged();
            void offlineMediaCountChanged();

          public slots:
            void setName(const QString &name);
            void initSystem(QObject *system_qobject);

            QList<QUuid> loadMedia(const QUrl &path) { return loadMediaFuture(path).result(); }
            QFuture<QList<QUuid>> loadMediaFuture(const QUrl &path);

            QList<QUuid> handleDrop(const QVariantMap &drop) {
                return handleDropFuture(drop).result();
            }
            QFuture<QList<QUuid>> handleDropFuture(const QVariantMap &drop);

            // QUuid addMedia(
            //     const QUrl &path,
            //     const QString &name = "New Media",
            //     const QUuid &before_uuid  = QUuid()) {return addMediaFuture(path, name,
            //     before_uuid).result(); }
            // QFuture<QUuid> addMediaFuture(
            //     const QUrl &path,
            //     const QString &name = "New Media",
            //     const QUuid &before_uuid  = QUuid());

            Q_INVOKABLE QObject *findMediaObject(const QUuid &uuid);

            void setExpanded(const bool value = true) {
                expanded_ = value;
                emit expandedChanged();
            }
            void setSelected(const bool value = true, const bool recurse = true);

            QUuid createTimeline(
                const QString &name = "Untitled Timeline",
                const QUuid &before = QUuid(),
                const bool into     = false);
            QUuid createSubset(
                const QString &name = "Untitled Subset",
                const QUuid &before = QUuid(),
                const bool into     = false);
            QUuid createContactSheet(
                const QString &name = "Untitled ContactSheet",
                const QUuid &before = QUuid(),
                const bool into     = false);
            QUuid createDivider(
                const QString &name = "Untitled Divider",
                const QUuid &before = QUuid(),
                const bool into     = false);

            QUuid convertToContactSheet(
                const QUuid &uuid_or_cuuid,
                const QString &name = "Converted",
                const QUuid &before = QUuid(),
                const bool into     = false);
            QUuid convertToSubset(
                const QUuid &uuid_or_cuuid,
                const QString &name = "Converted",
                const QUuid &before = QUuid(),
                const bool into     = false);
            QUuid
            createGroup(const QString &name = "Untitled Group", const QUuid &before = QUuid());
            bool removeMedia(const QUuid &uuid);

            bool renameContainer(const QString &name, const QUuid &uuid);
            bool reflagContainer(const QString &flag, const QUuid &uuid);
            bool removeContainer(const QUuid &uuid, const bool remove_media = false);
            bool
            moveContainer(const QUuid &uuid, const QUuid &before_uuid, const bool into = false);
            QUuid duplicateContainer(
                const QUuid &cuuid,
                const QUuid &before_cuuid = QUuid(),
                const bool into           = false);
            void setFlag(const QString &_flag) {
                if (_flag != flag()) {
                    flag_ = StdFromQString(_flag);
                    emit flagChanged();
                }
            }
            void setSelection(const QVariantList &cuuids, const bool clear = true);
            void setSelection(QObject *obj, const bool selected = true);
            void dragDropReorder(const QVariantList drop_uuids, const QString before_uuid);
            void sortAlphabetically();

            bool setJSON(const QString &json, const QString &path);
            QString getJSON(const QString &path);
            QFuture<bool> setJSONFuture(const QString &json, const QString &path);
            QFuture<QString> getJSONFuture(const QString &path);

            QUuid getNextItemUuid(const QUuid &quuid);

            bool contains_media(const QUuid &key) const;

          private:
            QObject *getNextItem(const utility::Uuid &cuuid);

            void build_media_order();
            void update_on_change(const bool select_new_items = false);

            void updateItemModel(const bool select_new_items = false);
            QObject *findItem(const utility::Uuid &uuid);
            utility::Uuid uuid(const QObject *obj) const;
            [[nodiscard]] QUuid findCUuidFromUuid(const utility::Uuid &uuid) const;

          private:
            utility::Uuid cuuid_;
            utility::Uuid uuid_;
            caf::actor backend_;
            caf::actor backend_events_;
            std::string name_;
            std::string flag_;
            PlaylistSelectionUI *playlist_selection_ = {nullptr};

            std::map<utility::Uuid, MediaUI *> uuid_media_;
            QList<QObject *> media_;
            QVariantMap media_order_;
            // holds map to all out children
            QMap<QUuid, QObject *> items_;

            // children mapped to model
            GroupModel model_;
            MediaModel media_model_;

            bool expanded_{false};
            bool selected_{false};
            bool loadingMedia_{false};
            bool isBusy_{false};

            QObject *selected_subitem_{nullptr};

            // dirty hack, we get spurious updates so we need to make sure something has
            // really changed.
            utility::PlaylistTree last_changed_tree_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio