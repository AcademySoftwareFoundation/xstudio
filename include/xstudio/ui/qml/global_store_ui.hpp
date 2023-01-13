// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

// CAF_PUSH_WARNINGS
// CAF_POP_WARNINGS

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

namespace xstudio {
namespace ui {
    namespace qml {
        class GlobalStoreUI : public QMLActor {

            Q_OBJECT
            Q_DISABLE_COPY(GlobalStoreUI)
            Q_PROPERTY(QString json_string READ json_string NOTIFY json_string_changed)
            Q_PROPERTY(bool autosave READ autosave WRITE set_autosave NOTIFY autosave_changed)

          public:
            explicit GlobalStoreUI(QObject *parent = nullptr);
            ~GlobalStoreUI() override = default;

            void init(caf::actor_system &system) override;

            [[nodiscard]] QString json_string() const;
            [[nodiscard]] bool autosave() const;

          public slots:
            [[nodiscard]] QVariantMap get_preference(const QString &path) const;
            bool set_preference_value(
                const QString &path, const QVariant &value, const bool async = true);
            [[nodiscard]] QVariant get_preference_value(const QString &path) const;
            [[nodiscard]] QVariant
            get_preference_key(const QString &path, const QString &key) const;
            [[nodiscard]] QVariant get_preference_default(const QString &path) const;
            [[nodiscard]] QVariant get_preference_minimum(const QString &path) const;
            [[nodiscard]] QVariant get_preference_maximum(const QString &path) const;
            [[nodiscard]] QString get_preference_description(const QString &path) const;
            [[nodiscard]] QString get_preference_datatype(const QString &path) const;
            bool save_preferences(const QString &context);
            void set_autosave(const bool enabled);

          signals:
            void preferences_changed();
            void preference_changed(const QString &path);
            void json_string_changed();
            void autosave_changed();

          private:
            void set_json_string(const QString &str);
            [[nodiscard]] int get_qmetatype(const std::string &datatype) const;
            [[nodiscard]] QVariant get_preference_as_qvariant(
                const std::string &path, const std::string &property) const;

          private:
            bool autosave_;
            QString json_string_;
            utility::JsonStore preference_store_;
            std::shared_ptr<global_store::GlobalStoreHelper> gsh_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio
Q_DECLARE_METATYPE(xstudio::ui::qml::GlobalStoreUI *)