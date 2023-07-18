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
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class PlaylistSelectionUI;

        class ContactSheetUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QUuid uuid READ quuid NOTIFY uuidChanged)
            Q_PROPERTY(QString type READ type NOTIFY typeChanged)
            Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
            Q_PROPERTY(QUuid cuuid READ qcuuid NOTIFY cuuidChanged)
            Q_PROPERTY(QString flag READ flag WRITE setFlag NOTIFY flagChanged)

            Q_PROPERTY(QVariant mediaModel READ mediaModel NOTIFY mediaModelChanged)
            Q_PROPERTY(QList<QObject *> mediaList READ mediaList NOTIFY mediaListChanged)
            Q_PROPERTY(QVariantMap mediaOrder READ mediaOrder NOTIFY mediaOrderChanged)

            Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
            Q_PROPERTY(QObject *selectionFilter READ selectionFilter NOTIFY
                           playlistSelectionThingChanged)
            Q_PROPERTY(QString fullName READ fullName NOTIFY nameChanged)
            Q_PROPERTY(
                QObject *parent_playlist READ parent_playlist NOTIFY parent_playlistChanged)

            Q_PROPERTY(bool hasBackend READ hasBackend NOTIFY backendChanged)

          public:
            explicit ContactSheetUI(
                const utility::Uuid cuuid = utility::Uuid(), QObject *parent = nullptr);
            ~ContactSheetUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);
            [[nodiscard]] caf::actor backend() const { return backend_; }

            [[nodiscard]] bool selected() const { return selected_; }
            [[nodiscard]] QString name() const { return QStringFromStd(name_); }
            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
            [[nodiscard]] QString type() const { return "ContactSheet"; }
            [[nodiscard]] bool expanded() const { return expanded_; }
            [[nodiscard]] QUuid qcuuid() const { return QUuidFromUuid(cuuid_); }
            [[nodiscard]] utility::Uuid cuuid() const { return cuuid_; }
            [[nodiscard]] bool hasBackend() const {
                return backend_ ? true : false;
                ;
            }
            [[nodiscard]] QString flag() const { return QStringFromStd(flag_); }

            QVariant mediaModel() { return QVariant::fromValue(&media_model_); }
            QList<QObject *> mediaList() { return media_; }
            [[nodiscard]] QVariantMap mediaOrder() const { return media_order_; }

            QObject *selectionFilter();
            QObject *parent_playlist() {
                return static_cast<QObject *>(dynamic_cast<PlaylistUI *>(parent()));
            }

            QString fullName();

          signals:
            void uuidChanged();
            void nameChanged();
            void typeChanged();
            void backendChanged();
            void systemInit(QObject *object);
            void cuuidChanged();
            void expandedChanged();
            void flagChanged();
            void selectedChanged();
            void compareModeChanged();
            void playlistSelectionThingChanged();
            void mediaAdded(const QUuid &uuid);
            void parent_playlistChanged();
            void mediaModelChanged();
            void mediaListChanged();
            void mediaOrderChanged();

          public slots:
            void setName(const QString &name);
            void setExpanded(const bool value = true) {
                expanded_ = value;
                emit expandedChanged();
            }
            void setFlag(const QString &_flag) {
                if (_flag != flag()) {
                    flag_ = StdFromQString(_flag);
                    emit flagChanged();
                }
            }
            QList<QUuid> loadMedia(const QUrl &path) { return loadMediaFuture(path).result(); }
            QFuture<QList<QUuid>> loadMediaFuture(const QUrl &path);

            QList<QUuid> handleDrop(const QVariantMap &drop) {
                return handleDropFuture(drop).result();
            }
            QFuture<QList<QUuid>> handleDropFuture(const QVariantMap &drop);

            // bool addMedia(const QUuid &uuid, const QUuid &before_uuid = QUuid());
            void setSelected(const bool value = true) {
                if (selected_ != value) {
                    selected_ = value;
                    emit selectedChanged();
                }
            }

            void dragDropReorder(const QVariantList drop_uuids, const QString before_uuid);

            void sortAlphabetically();
            QUuid getNextItemUuid(const QUuid &quuid);
            bool contains_media(const QUuid &key) const;

          private:
            MediaUI *getNextItem(const utility::Uuid &uuid);
            void update_media();

            utility::Uuid cuuid_;
            caf::actor backend_;
            caf::actor backend_events_;
            std::string name_;
            std::string flag_;
            utility::Uuid uuid_;
            bool expanded_{true};
            bool selected_{false};
            QList<QObject *> media_;
            std::map<utility::Uuid, MediaUI *> uuid_media_;
            QVariantMap media_order_;
            PlaylistSelectionUI *playlist_selection_ = {nullptr};
            MediaModel media_model_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio
