// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QUrl>
#include <QFuture>
#include <QtConcurrent>
CAF_POP_WARNINGS

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/studio_qml_export.h"

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qt/offscreen_viewport.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        //  top level utility actor, for stuff that lives out side the session.
        class STUDIO_QML_EXPORT StudioUI : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(QList<QObject *> dataSources READ dataSources NOTIFY dataSourcesChanged)
            Q_PROPERTY(QString sessionActorAddr READ sessionActorAddr WRITE setSessionActorAddr
                           NOTIFY sessionActorAddrChanged)

          public:
            explicit StudioUI(caf::actor_system &system, QObject *parent = nullptr);
            virtual ~StudioUI();

            Q_INVOKABLE bool clearImageCache();

            Q_INVOKABLE [[nodiscard]] QUrl userDocsUrl() const;
            Q_INVOKABLE [[nodiscard]] QUrl apiDocsUrl() const;
            Q_INVOKABLE [[nodiscard]] QUrl releaseDocsUrl() const;
            Q_INVOKABLE [[nodiscard]] QUrl hotKeyDocsUrl() const;

            Q_INVOKABLE void newSession(const QString &name);

            Q_INVOKABLE bool loadSession(const QUrl &path, const QVariant &json = QVariant()) {
                return loadSessionFuture(path, json).result();
            }
            Q_INVOKABLE QFuture<bool>
            loadSessionFuture(const QUrl &path, const QVariant &json = QVariant());

            Q_INVOKABLE bool loadSessionRequest(const QUrl &path) {
                return loadSessionRequestFuture(path).result();
            }
            Q_INVOKABLE QFuture<bool> loadSessionRequestFuture(const QUrl &path);

            QList<QObject *> dataSources() { return data_sources_; }

            [[nodiscard]] QString sessionActorAddr() const { return session_actor_addr_; };

            void setSessionActorAddr(const QString &addr);

            Q_INVOKABLE QString renderScreenShotToDisk(
                const QUrl &path, const int compression, const int width, const int height);

            Q_INVOKABLE QString renderScreenShotToClipboard(const int width, const int height);

            Q_INVOKABLE void setupSnapshotViewport(const QString &playhead_addr);

            Q_INVOKABLE void loadVideoOutputPlugins();

          signals:

            void newSessionCreated(const QString &session_addr);
            void sessionLoaded(const QString &session_addr);
            void dataSourcesChanged();
            void sessionRequest(const QUrl path, const QByteArray jsn);
            void sessionActorAddrChanged();
            void
            openQuickViewers(QStringList mediaActors, QString compareMode, int inpt, int outPt);
            void showMessageBox(
                QString messageTile, QString messageBody, bool closeButton, int timeoutSeconds);

          public slots:

          private:
            void init(caf::actor_system &system) override;
            void updateDataSources();
            xstudio::ui::qt::OffscreenViewport *offscreen_snapshot_viewport();

            QList<QObject *> data_sources_;
            xstudio::ui::qt::OffscreenViewport *snapshot_offscreen_viewport_ = nullptr;
            std::vector<xstudio::ui::qt::OffscreenViewport *> offscreen_viewports_;
            std::vector<caf::actor> video_output_plugins_;
            QString session_actor_addr_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio
