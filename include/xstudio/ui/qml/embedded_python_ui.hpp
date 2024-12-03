// SPDX-License-Identifier: Apache-2.0
#pragma once

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/embedded_python_qml_export.h"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

// CAF_PUSH_WARNINGS
// #include <QFuture>
// #include <QList>
// #include <QUuid>
// #include <QtConcurrent>
// CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class EMBEDDED_PYTHON_QML_EXPORT SnippetFilterModel : public QSortFilterProxyModel {

            Q_OBJECT
            // Q_PROPERTY(int length READ length NOTIFY lengthChanged)
            // Q_PROPERTY(int count READ length NOTIFY lengthChanged)

            Q_PROPERTY(QString snippetType READ snippetType WRITE setSnippetType NOTIFY
                           snippetTypeChanged)

          public:
            using super = QSortFilterProxyModel;

            SnippetFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
                // setDynamicSortFilter(true);
                // sort(0);
            }


            const QString &snippetType() const { return snippet_type_; }

            void setSnippetType(const QString &value) {
                if (value != snippet_type_) {
                    snippet_type_ = value;
                    emit snippetTypeChanged();
                    invalidateFilter();
                }
            }

            // [[nodiscard]] int length() const { return rowCount(); }

          protected:
            [[nodiscard]] bool
            filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

          signals:
            void snippetTypeChanged();
            // void lengthChanged();

          private:
            QString snippet_type_;
        };


        class EMBEDDED_PYTHON_QML_EXPORT EmbeddedPythonUI
            : public caf::mixin::actor_object<JSONTreeModel> {

            Q_OBJECT
            Q_PROPERTY(bool waiting READ waiting NOTIFY waitingChanged)
            Q_PROPERTY(QUuid sessionId READ sessionId NOTIFY sessionIdChanged)

            Q_PROPERTY(QObject *applicationMenuModel READ applicationMenuModel NOTIFY
                           applicationMenuModelChanged)
            Q_PROPERTY(QObject *playlistMenuModel READ playlistMenuModel NOTIFY
                           playlistMenuModelChanged)
            Q_PROPERTY(QObject *mediaMenuModel READ mediaMenuModel NOTIFY mediaMenuModelChanged)
            Q_PROPERTY(QObject *sequenceMenuModel READ sequenceMenuModel NOTIFY
                           sequenceMenuModelChanged)
            Q_PROPERTY(QObject *trackMenuModel READ trackMenuModel NOTIFY trackMenuModelChanged)
            Q_PROPERTY(QObject *clipMenuModel READ clipMenuModel NOTIFY clipMenuModelChanged)

          public:
            using super = caf::mixin::actor_object<JSONTreeModel>;
            enum Roles {
                nameRole = JSONTreeModel::Roles::LASTROLE,
                menuPathRole,
                scriptPathRole,
                snippetTypeRole,
                typeRole
            };

            explicit EmbeddedPythonUI(QObject *parent = nullptr);
            ~EmbeddedPythonUI() override = default;

            caf::actor_system &system() const {
                return const_cast<caf::actor_companion *>(self())->home_system();
            }

            void init(caf::actor_system &system);
            void set_backend(caf::actor backend);
            caf::actor backend() { return backend_; }

            [[nodiscard]] bool waiting() const { return waiting_; }
            [[nodiscard]] QUuid sessionId() const { return QUuidFromUuid(event_uuid_); }

            [[nodiscard]] QVariant
            data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

            [[nodiscard]] QObject *applicationMenuModel() {
                if (not application_menu_model_)
                    application_menu_model_ = makeFilterModel("Application");
                return application_menu_model_;
            }

            [[nodiscard]] QObject *playlistMenuModel() {
                if (not playlist_menu_model_)
                    playlist_menu_model_ = makeFilterModel("Playlist");
                return playlist_menu_model_;
            }

            [[nodiscard]] QObject *mediaMenuModel() {
                if (not media_menu_model_)
                    media_menu_model_ = makeFilterModel("Media");
                return media_menu_model_;
            }

            [[nodiscard]] QObject *sequenceMenuModel() {
                if (not sequence_menu_model_)
                    sequence_menu_model_ = makeFilterModel("Sequence");
                return sequence_menu_model_;
            }

            [[nodiscard]] QObject *trackMenuModel() {
                if (not track_menu_model_)
                    track_menu_model_ = makeFilterModel("Track");
                return track_menu_model_;
            }

            [[nodiscard]] QObject *clipMenuModel() {
                if (not clip_menu_model_)
                    clip_menu_model_ = makeFilterModel("Clip");
                return clip_menu_model_;
            }

          public slots:
            void pyEvalFile(const QUrl &path);
            void pyExec(const QString &str) const;
            QVariant pyEval(const QString &str);
            QUuid createSession();
            bool sendInput(const QString &str);
            bool sendInterrupt();
            void reloadSnippets() const;
            bool saveSnippet(const QUrl &path, const QString &content) const;

          signals:
            void waitingChanged();
            void backendChanged();
            void sessionIdChanged();
            void stdoutEvent(const QString &str);
            void stderrEvent(const QString &str);

            void applicationMenuModelChanged();
            void playlistMenuModelChanged();
            void mediaMenuModelChanged();
            void sequenceMenuModelChanged();
            void trackMenuModelChanged();
            void clipMenuModelChanged();

          private:
            SnippetFilterModel *makeFilterModel(const QString &filter) {
                SnippetFilterModel *result = new SnippetFilterModel(this);
                result->setSnippetType(filter);
                result->setSourceModel(this);
                return result;
            }

            caf::actor backend_;
            caf::actor backend_events_;
            bool waiting_{false};

            utility::Uuid event_uuid_;

            SnippetFilterModel *application_menu_model_{nullptr};
            SnippetFilterModel *playlist_menu_model_{nullptr};
            SnippetFilterModel *media_menu_model_{nullptr};
            SnippetFilterModel *sequence_menu_model_{nullptr};
            SnippetFilterModel *track_menu_model_{nullptr};
            SnippetFilterModel *clip_menu_model_{nullptr};

            utility::Uuid snippet_uuid_{utility::Uuid::generate()};
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio