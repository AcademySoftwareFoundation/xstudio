// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/mixin/actor_widget.hpp>

// CAF_PUSH_WARNINGS
#include <QString>
#include <QTextEdit>
#include <QWidget>
// CAF_POP_WARNINGS


namespace xstudio {
namespace ui {
    namespace qt {
        class JsonStoreUI : public caf::mixin::actor_widget<QTextEdit> {

            Q_OBJECT
            using super = caf::mixin::actor_widget<QTextEdit>;

          public slots:
            void fchanged(QWidget *, QWidget *);
            void sendUpdate(QString);

          public:
            JsonStoreUI(QWidget *parent = nullptr);
            virtual ~JsonStoreUI();

            virtual void init(caf::actor_system &system);
            void subscribe(caf::actor actor);
            void unsubscribe();

          private:
            caf::actor_system &system() { return self()->home_system(); }

          protected:
            template <typename T> T *get(T *&member, const char *name) {
                if (member == nullptr) {
                    member = findChild<T *>(name);
                    if (member == nullptr) {
                        throw std::runtime_error("unable to find child: " + std::string(name));
                    }
                }
                return member;
            }

          private:
            caf::actor store;
            caf::actor store_events;
            QString before;
        };
        inline QString QStringFromStd(const std::string &str) {
            return QString::fromUtf8(str.c_str());
        }
        inline std::string StdFromQString(const QString &str) {
            return str.toUtf8().constData();
        }
    } // namespace qt
} // namespace ui
} // namespace xstudio