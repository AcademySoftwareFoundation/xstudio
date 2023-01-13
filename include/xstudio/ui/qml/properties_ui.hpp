// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QQmlPropertyMap>
#include <QTimer>
#include <QUrl>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace ui {
    namespace qml {

        class PreferenceSet : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(QObject *properties READ propertyMap NOTIFY propertiesChanged)
            Q_PROPERTY(QString preferencePath READ preferencePath WRITE setPreferencePath NOTIFY
                           preferencePathChanged)

          public:
            explicit PreferenceSet(QObject *parent = nullptr);

            ~PreferenceSet() override = default;

            void init(caf::actor_system &system) override;

          signals:

            void
            propertiesChanged(); // note this gets emitted if a property is changes from QML
            void preferencePathChanged();
            void propertiesChangedFromBackend(); // this does not get emitted if a property
                                                 // is changed from QML

          public slots:

            void push_prefs_to_store();
            QObject *propertyMap() { return static_cast<QObject *>(propertyMap_); }
            QString preferencePath() const { return preference_path_; }
            void build_prefs_from_store();
            void setPreferencePath(const QString &p);

          public:
            caf::actor_system &system();

          private:
            // void fetch_prefs_from_store();
            void update_qml_prefs();

            QTimer *update_timer_;
            caf::actor global_store_;
            QQmlPropertyMap *propertyMap_;
            std::string template_path_;
            utility::JsonStore prefs_store_;
            QString preference_path_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio