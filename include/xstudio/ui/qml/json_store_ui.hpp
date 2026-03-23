// SPDX-License-Identifier: Apache-2.0
#pragma once

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
        class JsonStoreUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(QString json_string READ json_string WRITE set_json_string NOTIFY
                           json_string_changed)

          public:
            explicit JsonStoreUI(QObject *parent = nullptr);
            ~JsonStoreUI() override = default;

            void init(caf::actor_system &system) override;
            void subscribe(caf::actor actor);
            void unsubscribe();

            QString json_string();
            void set_json_string(const QString &str, const bool from_actor = false);

          signals:
            void json_string_changed();

          private:
            void send_update(QString);

          private:
            caf::actor store;
            caf::actor store_events;
            QString json_string_;
            caf::disposable monitor_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio