// SPDX-License-Identifier: Apache-2.0
#pragma once


// include CMake auto-generated export hpp
#include "xstudio/ui/qml/embedded_python_qml_export.h"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

// CAF_PUSH_WARNINGS
// #include <QTextEdit>
// #include <QWidget>
// #include <QString>
// CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        // class Snippet {

        // };

        class SnippetUI : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QString menuModelName READ menuModelName NOTIFY menuNameChanged)
            Q_PROPERTY(QString script READ script NOTIFY scriptChanged)
            Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)

          public:
            explicit SnippetUI(
                const QString menu_name,
                const QString name,
                const QString script,
                QObject *parent = nullptr)
                : QObject(parent),
                  menu_name_(std::move(menu_name)),
                  name_(std::move(name)),
                  script_(std::move(script)) {}
            explicit SnippetUI(const utility::JsonStore &json, QObject *parent = nullptr);

            ~SnippetUI() override = default;

            [[nodiscard]] QString name() const { return name_; }
            [[nodiscard]] QString menuModelName() const { return menu_name_; }
            [[nodiscard]] QString script() const { return script_; }
            [[nodiscard]] QString description() const { return description_; }

          signals:
            void nameChanged();
            void menuNameChanged();
            void scriptChanged();
            void descriptionChanged();

          private:
            QString menu_name_   = {};
            QString name_        = {};
            QString script_      = {};
            QString description_ = {};
        };

        class SnippetMenuUI : public QObject {
            Q_OBJECT
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(QList<QObject *> snippets READ snippets NOTIFY snippetsChanged)

          public:
            explicit SnippetMenuUI(const QString name, QObject *parent = nullptr)
                : QObject(parent), name_(std::move(name)) {}
            ~SnippetMenuUI() override = default;

            [[nodiscard]] QString name() const { return name_; }
            [[nodiscard]] QList<QObject *> snippets() const { return snippets_; }

            void addSnippet(QObject *snippet) {
                snippets_.push_back(snippet);
                emit snippetsChanged();
            }

          signals:
            void nameChanged();
            void snippetsChanged();

          private:
            QString name_;
            QList<QObject *> snippets_;
        };

        class EMBEDDED_PYTHON_QML_EXPORT EmbeddedPythonUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(bool waiting READ waiting NOTIFY waitingChanged)
            Q_PROPERTY(
                QList<QObject *> snippetMenus READ snippetMenus NOTIFY snippetMenusChanged)
            Q_PROPERTY(QList<QObject *> snippets READ snippets NOTIFY snippetsChanged)

          public:
            explicit EmbeddedPythonUI(QObject *parent = nullptr);
            ~EmbeddedPythonUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);
            caf::actor backend() { return backend_; }

            [[nodiscard]] bool waiting() const { return waiting_; }

            [[nodiscard]] QList<QObject *> snippetMenus() const { return snippet_menus_; }
            [[nodiscard]] QList<QObject *> snippets() const { return snippets_; }


          public slots:
            void pyEvalFile(const QUrl &path);
            void pyExec(const QString &str);
            QVariant pyEval(const QString &str);
            QUuid createSession();
            bool sendInput(const QString &str);
            bool sendInterrupt();

            // QVariant pyEval(const QString &str, const QVariant &locals);
            // QVariant pyEvalLocals(const QString &str);
            // QVariant pyEvalLocals(const QString &str, const QVariant &locals);

            void addSnippet(SnippetUI *snippet);

          signals:
            void snippetMenusChanged();
            void snippetsChanged();
            void waitingChanged();
            void backendChanged();
            void stdoutEvent(const QString &str);
            void stderrEvent(const QString &str);

          private:
            caf::actor backend_;
            caf::actor backend_events_;
            bool waiting_{false};

            QList<QObject *> snippet_menus_;
            QList<QObject *> snippets_;
            utility::Uuid event_uuid_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio
