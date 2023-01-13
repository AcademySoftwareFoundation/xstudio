// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QUrl>
#include <QFuture>
#include <QtConcurrent>
#include <QDateTime>
#include <QQmlEngine>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/qml/media_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class CategoryObject : public QObject {
            Q_OBJECT

            Q_PROPERTY(QString text READ text NOTIFY textChanged)
            Q_PROPERTY(QString value READ value NOTIFY valueChanged)
            Q_PROPERTY(QString colour READ colour NOTIFY colourChanged)

          public:
            explicit CategoryObject(
                QString text, QString value, QString colour, QObject *parent = nullptr)
                : QObject(parent),
                  text_(std::move(text)),
                  value_(std::move(value)),
                  colour_(std::move(colour)) {}
            ~CategoryObject() override = default;

            [[nodiscard]] QString text() const { return text_; }
            [[nodiscard]] QString value() const { return value_; }
            [[nodiscard]] QString colour() const { return colour_; }

          signals:
            void textChanged();
            void valueChanged();
            void colourChanged();

          private:
            QString text_{""};
            QString value_{""};
            QString colour_{""};
        };

        class BookmarkDetailUI : public QObject {
            Q_OBJECT

            Q_PROPERTY(QUuid uuid READ quuid WRITE setQuuid NOTIFY uuidChanged)
            Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
            Q_PROPERTY(QString colour READ colour WRITE setColour NOTIFY colourChanged)
            Q_PROPERTY(QString subject READ subject WRITE setSubject NOTIFY subjectChanged)
            Q_PROPERTY(double start READ start WRITE setStart NOTIFY startChanged)
            Q_PROPERTY(double duration READ duration WRITE setDuration NOTIFY durationChanged)
            Q_PROPERTY(double logicalStartFrame READ logicalStartFrame WRITE
                           setLogicalStartFrame NOTIFY logicalStartFrameChanged)

          public:
            explicit BookmarkDetailUI(QObject *parent = nullptr) : QObject(parent) {}
            ~BookmarkDetailUI() override = default;

            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(detail_.uuid_); }
            [[nodiscard]] double start() const;
            [[nodiscard]] double duration() const;
            [[nodiscard]] QString category() const;
            [[nodiscard]] QString colour() const;
            [[nodiscard]] QString subject() const;
            [[nodiscard]] int logicalStartFrame() const {
                return *(detail_.logical_start_frame_);
            }

          signals:
            void uuidChanged();
            void startChanged();
            void durationChanged();
            void logicalStartFrameChanged();
            void categoryChanged();
            void colourChanged();
            void subjectChanged();

          public slots:
            void setQuuid(const QUuid &quuid) { detail_.uuid_ = UuidFromQUuid(quuid); }
            void setStart(const double);
            void setDuration(const double);
            void setCategory(const QString &);
            void setSubject(const QString &);
            void setColour(const QString &);
            double getSecond(const int frame) { return detail_.get_second(frame); }
            void setLogicalStartFrame(const int value) { detail_.logical_start_frame_ = value; }

          public:
            bookmark::BookmarkDetail detail_;
        };

        class BookmarksUI;

        class BookmarkUI : public QObject {
            Q_OBJECT

          public:
            explicit BookmarkUI(QObject *parent = nullptr);
            ~BookmarkUI() override = default;

          public:
            utility::Uuid uuid_;
            caf::actor backend_;

            bookmark::BookmarkDetail detail_;
            QString thumbnailURL_;
        };

        class BookmarkModel : public QAbstractListModel {
            Q_OBJECT

            Q_PROPERTY(int length READ length NOTIFY lengthChanged)

          public:
            enum BookmarkRoles {
                EnabledRole = Qt::UserRole + 1,
                FocusRole,
                FrameRole,
                FrameFromTimecodeRole,
                StartTimecodeRole,
                EndTimecodeRole,
                DurationTimecodeRole,
                StartFrameRole,
                EndFrameRole,
                HasNoteRole,
                SubjectRole,
                NoteRole,
                AuthorRole,
                CategoryRole,
                ColourRole,
                CreatedRole,
                HasAnnotationRole,
                ThumbnailRole,
                OwnerRole,
                UuidRole,
                ObjectRole
            };

            inline static const std::map<int, std::string> role_names = {
                {EnabledRole, "enabledRole"},
                {FocusRole, "focusRole"},
                {FrameRole, "frameRole"},
                {FrameFromTimecodeRole, "frameFromTimecodeRole"},
                {StartTimecodeRole, "startTimecodeRole"},
                {EndTimecodeRole, "endTimecodeRole"},
                {DurationTimecodeRole, "durationTimecodeRole"},
                {StartFrameRole, "startFrameRole"},
                {EndFrameRole, "endFrameRole"},
                {HasNoteRole, "hasNoteRole"},
                {SubjectRole, "subjectRole"},
                {NoteRole, "noteRole"},
                {AuthorRole, "authorRole"},
                {CategoryRole, "categoryRole"},
                {ColourRole, "colourRole"},
                {CreatedRole, "createdRole"},
                {HasAnnotationRole, "hasAnnotationRole"},
                {ThumbnailRole, "thumbnailRole"},
                {OwnerRole, "ownerRole"},
                {UuidRole, "uuidRole"},
                {ObjectRole, "objectRole"}};

            BookmarkModel(BookmarksUI *controller, QObject *parent = nullptr);
            [[nodiscard]] int
            rowCount(const QModelIndex &parent = QModelIndex()) const override;
            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            [[nodiscard]] int length() const { return rowCount(); }

            bool setData(
                const QModelIndex &index,
                const QVariant &value,
                int role = Qt::EditRole) override;

            void populate(QList<BookmarkUI *> &items, const bool changed);

            [[nodiscard]] int startFrame(int row) const;
            [[nodiscard]] int endFrameExclusive(int row) const;

            Q_INVOKABLE [[nodiscard]] QVariantMap get(int i) const {
                QVariantMap data;
                const QModelIndex idx = this->index(i, 0);
                if (!idx.isValid())
                    return data;
                const QHash<int, QByteArray> rn = this->roleNames();
                for (auto it = rn.cbegin(); it != rn.cend(); ++it) {
                    data[it.value()] = idx.data(it.key());
                }
                return data;
            }

          signals:

            void lengthChanged();

          protected:
            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

          private:
            QList<BookmarkUI *> data_;
            BookmarksUI *controller_{nullptr};
        };

        class BookmarksUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(QUuid uuid READ quuid NOTIFY uuidChanged)
            Q_PROPERTY(bool hasBackend READ hasBackend NOTIFY backendChanged)
            Q_PROPERTY(QVariant bookmarkModel READ bookmarkModel NOTIFY bookmarkModelChanged)
            Q_PROPERTY(
                QVariantMap ownedBookmarks READ ownedBookmarks NOTIFY ownedBookmarksChanged)
            Q_PROPERTY(QList<QObject *> categories READ categories NOTIFY categoriesChanged)

          public:
            explicit BookmarksUI(QObject *parent = nullptr);
            ~BookmarksUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);
            [[nodiscard]] caf::actor backend() const { return backend_; }

            [[nodiscard]] QUuid quuid() const { return QUuidFromUuid(uuid_); }
            [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
            [[nodiscard]] bool hasBackend() const {
                return backend_ ? true : false;
                ;
            }

            [[nodiscard]] QList<QObject *> categories() const { return categories_; }

            [[nodiscard]] QVariant bookmarkModel() const {
                return QVariant::fromValue(bookmarks_model_);
            }
            [[nodiscard]] QVariantMap ownedBookmarks() const { return ownedBookmarks_; }

          signals:
            void uuidChanged();
            void backendChanged();
            void bookmarkModelChanged();
            void categoriesChanged();
            void ownedBookmarksChanged();
            void systemInit(QObject *object);
            void newBookmarks();
            void newBookmark(QUuid bookmark_uuid);

          public slots:
            QUuid addBookmark(MediaUI *src);
            void removeBookmarks(const QList<QUuid> &src);
            bool updateBookmark(QObject *detail) {
                if (auto value = dynamic_cast<BookmarkDetailUI *>(detail))
                    return updateBookmark(value->detail_);
                return false;
            }

            void setDuration(const QUuid &uuid, const int frames);

            bool setJSON(BookmarkUI *bm, const QString &json, const QString &path);
            QString getJSON(BookmarkUI *bm, const QString &path);
            QFuture<bool>
            setJSONFuture(BookmarkUI *bm, const QString &json, const QString &path);
            QFuture<QString> getJSONFuture(BookmarkUI *bm, const QString &path);
            QFuture<QString> exportCSVFuture(const QUrl &path);
            QString exportCSV(const QUrl &path) { return exportCSVFuture(path).result(); }

          public:
            bool updateBookmark(const bookmark::BookmarkDetail &detail);

          private:
            bool addCategory(
                const std::string &text, const std::string &value, const std::string &colour);
            void populateBookmarks(const bool changed = false);
            utility::UuidActor addBookmark(const utility::UuidActor &src);

          private:
            BookmarkModel *bookmarks_model_{nullptr};
            caf::actor backend_;
            caf::actor backend_events_;
            utility::Uuid uuid_;
            QList<BookmarkUI *> bookmarks_;
            QVariantMap ownedBookmarks_;
            QList<QObject *> categories_;
            std::set<std::string> cat_map_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio
