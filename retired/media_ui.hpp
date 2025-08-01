// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QUrl>
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QFuture>
#include <QtConcurrent>
#include <QQuickPaintedItem>
#include <QImage>
CAF_POP_WARNINGS

#include "xstudio/media/media.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/media_reference.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        /* This class allows us to expose a dynamic list of MediaUI objects to QML, it's
        used by PlaylistUI, SubsetUI, ContactSheetUI etc so the UI can expose their
        media item contents */

        // QModelIndexList QAbstractItemModel::match(const QModelIndex &start, int role,
        // const QVariant &value, int hits = 1, Qt::MatchFlags flags =
        // Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap)) const
        class MediaModel : public QAbstractListModel {
            Q_OBJECT

            Q_PROPERTY(int length READ length NOTIFY lengthChanged)

          public:
            enum MediaRoles { UuidRole, ObjectRole };

            MediaModel(QObject *parent = nullptr);
            [[nodiscard]] int
            rowCount(const QModelIndex &parent = QModelIndex()) const override;
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            [[nodiscard]] int length() const { return rowCount(); }

            Q_INVOKABLE [[nodiscard]] QVariant
            get(const int row, const QString &rolename) const;
            // Q_INVOKABLE [[nodiscard]] int match(const QVariant &value, const QString
            // &rolename,const int start_row=0) const;

            bool populate(QList<QObject *> &items);

          signals:

            void lengthChanged();

          protected:
            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

          private:
            [[nodiscard]] int get_role(const QString &rolename) const;

          private:
            QList<QObject *> data_;
            QObject *controller_{nullptr};
        };

        class MediaStreamUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)

          public:
            explicit MediaStreamUI(QObject *parent = nullptr);
            ~MediaStreamUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            caf::actor backend() { return backend_; }

            [[nodiscard]] QString name() const { return name_; }
            [[nodiscard]] QUuid uuid() const { return QUuidFromUuid(uuid_); }

          signals:
            void nameChanged();
            void uuidChanged();
            void backendChanged();

          public slots:
            void initSystem(QObject *system_qobject);

          private:
            void update_on_change();

          private:
            caf::actor backend_;
            caf::actor backend_events_;
            QString name_;
            utility::Uuid uuid_;
        };

        class MediaSourceUI : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QString fpsString READ fpsString WRITE setFpsString NOTIFY fpsChanged)
            Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
            Q_PROPERTY(QString path READ path NOTIFY pathChanged)
            Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)
            Q_PROPERTY(QObject *current READ current NOTIFY currentChanged)
            Q_PROPERTY(QList<QObject *> streams READ streams NOTIFY streamsChanged)
            Q_PROPERTY(QString metadata READ metadata NOTIFY metadataChanged)
            Q_PROPERTY(QString bitDepth READ bitDepth NOTIFY bitDepthChanged)
            Q_PROPERTY(QString format READ format NOTIFY formatChanged)
            Q_PROPERTY(QString resolution READ resolution NOTIFY resolutionChanged)
            Q_PROPERTY(
                int durationFramesNumeric READ durationFramesNumeric NOTIFY durationChanged)
            Q_PROPERTY(QString durationFrames READ durationFrames NOTIFY durationChanged)
            Q_PROPERTY(QString durationSeconds READ durationSeconds NOTIFY durationChanged)
            Q_PROPERTY(QString durationTimecode READ durationTimecode NOTIFY durationChanged)
            Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)
            Q_PROPERTY(QString thumbnailURL READ getThumbnailURL NOTIFY thumbnailURLChanged)
            Q_PROPERTY(QString mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
            Q_PROPERTY(bool mediaOnline READ mediaOnline NOTIFY mediaStatusChanged)
            Q_PROPERTY(QImage thumbnail READ thumbnail NOTIFY thumbnailChanged)
            Q_PROPERTY(float pixelAspect READ pixelAspect NOTIFY pixelAspectChanged)

          public:
            explicit MediaSourceUI(QObject *parent = nullptr);
            ~MediaSourceUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            caf::actor backend() { return backend_; }

            QString name() { return name_; }
            [[nodiscard]] QString fpsString() const { return fps_string_; }
            QString fileName() { return file_name_; }
            QString path() { return file_path_; }
            QUuid uuid() { return QUuidFromUuid(uuid_); }
            QObject *current() { return current_; }
            QList<QObject *> streams() {
                QList<QObject *> streams;
                for (const auto &i : streams_)
                    streams.append(i);
                return streams;
            }
            QString metadata();

            QString bitDepth() { return bit_depth_; }
            QString format() { return format_; }
            QString resolution() { return resolution_; }
            [[nodiscard]] bool colourManaged() const { return colourManaged_; }

            [[nodiscard]] QString durationFrames() const { return duration_frames_; }
            [[nodiscard]] int durationFramesNumeric() const { return duration_frames_int_; }
            [[nodiscard]] QString durationSeconds() const { return duration_seconds_; }
            [[nodiscard]] QString durationTimecode() const { return duration_timecode_; }
            [[nodiscard]] double fps() const { return fps_; }

            [[nodiscard]] bool mediaOnline() const {
                return media_status_ == media::MediaStatus::MS_ONLINE;
            }
            [[nodiscard]] QString mediaStatus() const {
                return QStringFromStd(to_string(media_status_));
            }
            [[nodiscard]] const QImage &thumbnail() const { return thumbnail_; }
            [[nodiscard]] float thumbnail_position_in_clip_duration() const {
                return thumbnail_position_in_clip_duration_;
            }
            [[nodiscard]] float pixelAspect() const { return pixel_aspect_; }

          signals:
            void fileNameChanged();
            void pathChanged();
            void fpsChanged();
            void nameChanged();
            void uuidChanged();
            void backendChanged();
            void streamsChanged();
            void currentChanged();
            void metadataChanged();
            void bitDepthChanged();
            void formatChanged();
            void resolutionChanged();
            void durationChanged();
            void thumbnailURLChanged();
            void mediaStatusChanged();
            void thumbnailChanged(const QImage &, const float);
            void pixelAspectChanged();

          public slots:
            void initSystem(QObject *system_qobject);
            QString getThumbnailURL(int frame = -1) {
                return getThumbnailURLFuture(frame).result();
            }
            QFuture<QString> getThumbnailURLFuture(int frame = -1);
            void requestThumbnail(float position_in_clip_duration);
            void cancelThumbnailRequest();
            void setFpsString(const QString &fps);
            void switchToStream(const QUuid stream_uuid);
            void switchToStream(const QString stream_id);

          private:
            void update_from_detail(const utility::ContainerDetail &detail);
            void update_on_change();
            void set_sources_count(const size_t count);
            void inspect_metadata(const nlohmann::json &);
            void update_from_backend(
                const utility::JsonStore &meta_data,
                const utility::JsonStore &colour_params,
                const utility::ContainerDetail &detail);
            void update_streams_from_backend(
                const std::string filename,
                const std::string fps_string,
                const double fps,
                const media::StreamDetail stream_detail,
                std::vector<utility::UuidActor> streams,
                utility::Uuid current_stream);

          private:
            caf::actor backend_;
            caf::actor backend_events_;
            QString name_;
            QString file_name_;
            QString file_path_;
            utility::Uuid uuid_;
            MediaStreamUI *current_{nullptr};
            std::vector<MediaStreamUI *> streams_;
            QString bit_depth_;
            QString format_;
            QString resolution_;
            float pixel_aspect_;
            QString fps_string_;
            double fps_;
            QString duration_seconds_;
            QString duration_frames_;
            QString duration_timecode_;
            int duration_frames_int_{0};
            bool colourManaged_ = {false};
            media::MediaStatus media_status_{media::MediaStatus::MS_ONLINE};
            QImage thumbnail_;
            float thumbnail_position_in_clip_duration_;
            utility::Uuid latest_thumb_request_job_id_;
        };

        class MediaUI : public QMLActor {
            Q_OBJECT

            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QObject *mediaSource READ mediaSource NOTIFY mediaSourceChanged)
            Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)
            Q_PROPERTY(QString flag READ flag WRITE setFlag NOTIFY flagChanged)
            Q_PROPERTY(QString flagText READ flagText WRITE setFlagText NOTIFY flagTextChanged)
            Q_PROPERTY(QString mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
            Q_PROPERTY(bool mediaOnline READ mediaOnline NOTIFY mediaStatusChanged)

          public:
            explicit MediaUI(QObject *parent = nullptr);
            ~MediaUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            caf::actor backend() { return backend_; }
            QUuid uuid() const { return QUuidFromUuid(uuid_); }

            QString name() { return name_; }
            QObject *mediaSource() { return media_source_; }

            utility::UuidActor backend_ua() const {
                return utility::UuidActor(uuid_, backend_);
            }
            [[nodiscard]] QString flag() const { return QStringFromStd(flag_); }
            [[nodiscard]] QString flagText() const { return QStringFromStd(flag_text_); }

            [[nodiscard]] bool mediaOnline() const;
            [[nodiscard]] QString mediaStatus() const;

            Q_INVOKABLE QString getMetadata() { return getMetadataFuture().result(); }
            Q_INVOKABLE QFuture<QString> getMetadataFuture();

          signals:

            void mediaSourceChanged();
            void nameChanged();
            void uuidChanged();
            void backendChanged();
            void flagChanged();
            void flagTextChanged();
            void mediaStatusChanged();

          public slots:

            void initSystem(QObject *system_qobject);
            void setCurrentSource(const QUuid &uuid);
            void setFlag(const QString &_flag);
            void setFlagText(const QString &_flag_text);

          private:
            void update_on_change();

          private:
            caf::actor backend_;
            caf::actor backend_events_;
            QString name_;
            utility::Uuid uuid_;
            MediaSourceUI *media_source_{nullptr};
            std::vector<MediaSourceUI *> sources_;
            std::string flag_;
            std::string flag_text_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio