// SPDX-License-Identifier: Apache-2.0
#pragma once

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/viewport_qml_export.h"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QPoint>
#include <QUrl>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/keyboard.hpp"
#include "xstudio/utility/frame_time.hpp"

namespace xstudio {
namespace utility {
    class MediaReference;
}
namespace ui {
    namespace qml {

        class VIEWPORT_QML_EXPORT HotkeysUI
            : public caf::mixin::actor_object<QAbstractListModel> {

            Q_OBJECT

            enum HotkeyRoles {
                keyboardKey = Qt::UserRole + 1,
                keyModifiers,
                hotkeyName,
                hotkeyCategory,
                hotkeyDescription,
                hotkeySequence,
                hotkeyErrorMessage
            };

            static inline const QMap<HotkeyRoles, QByteArray> hotkeyRoleNames = {
                {keyboardKey, "keyboardKey"},
                {keyModifiers, "keyModifiers"},
                {hotkeyName, "hotkeyName"},
                {hotkeyCategory, "hotkeyCategory"},
                {hotkeyDescription, "hotkeyDescription"},
                {hotkeySequence, "hotkeySequence"},
                {hotkeyErrorMessage, "hotkeyErrorMessage"}};

          public:
            using super = caf::mixin::actor_object<QAbstractListModel>;

            Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
            Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)
            Q_PROPERTY(QString currentCategory READ currentCategory WRITE setCurrentCategory
                           NOTIFY currentCategoryChanged)

            HotkeysUI(QObject *parent = nullptr);
            ~HotkeysUI() override = default;

            [[nodiscard]] int rowCount() { return rowCount(QModelIndex()); }

            [[nodiscard]] QStringList categories() { return categories_; }

            [[nodiscard]] QString currentCategory() { return current_category_; }

            [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

            [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

            [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

            bool setData(const QModelIndex &index, const QVariant &value, int role) override;

            [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &) const override {
                return Qt::ItemIsEnabled | Qt::ItemIsEditable;
            }

            void setCurrentCategory(const QString &category) {
                if (category != current_category_) {
                    current_category_ = category;
                    emit currentCategoryChanged();
                    beginResetModel();
                    endResetModel();
                    emit rowCountChanged();
                }
            }

            Q_INVOKABLE QString hotkey_sequence(const QVariant &hotkey_uuid);

            Q_INVOKABLE QString hotkey_sequence_from_hotkey_name(const QString &hotkey_name);


          signals:

            void rowCountChanged();
            void categoriesChanged();
            void currentCategoryChanged();

          public slots:

          private:
            void update_hotkeys_model_data(const std::vector<Hotkey> &new_hotkeys_data);
            void checkCategories();

            caf::actor_system &system() { return self()->home_system(); }
            virtual void init(caf::actor_system &system) { super::init(system); }

            std::vector<Hotkey> hotkeys_data_;
            QStringList categories_;
            QString current_category_;
        };

        class VIEWPORT_QML_EXPORT HotkeyUI : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(QString sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
            Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
            Q_PROPERTY(QString componentName READ componentName WRITE setComponentName NOTIFY
                           componentNameChanged)
            Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY
                           descriptionChanged)
            Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
            Q_PROPERTY(QString context READ context WRITE setContext NOTIFY contextChanged)
            Q_PROPERTY(
                bool autoRepeat READ autoRepeat WRITE setAutoRepeat NOTIFY autoRepeatChanged)
            Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)

          public:
            explicit HotkeyUI(QObject *parent = nullptr);
            ~HotkeyUI() override = default;

            void init(caf::actor_system &system) override;

            void setSequence(const QString &seq) {
                sequence_ = seq;
                emit sequenceChanged();
            }

            void setName(const QString &name) {
                name_ = name;
                emit nameChanged();
            }

            void setDescription(const QString &seq) {
                description_ = seq;
                emit descriptionChanged();
            }

            void setContext(const QString &context) {
                context_ = context;
                emit contextChanged();
            }

            void setComponentName(const QString &cmp_name) {
                component_name_ = cmp_name;
                emit componentNameChanged();
            }

            void setAutoRepeat(const bool autorepeat) {
                autorepeat_ = autorepeat;
                emit autoRepeatChanged();
            }

            [[nodiscard]] const QString &sequence() const { return sequence_; }
            [[nodiscard]] const QString &name() const { return name_; }
            [[nodiscard]] const QString &componentName() const { return component_name_; }
            [[nodiscard]] const QString &description() const { return description_; }
            [[nodiscard]] const QString &errorMessage() const { return error_message_; }
            [[nodiscard]] const QString &context() const { return context_; }
            [[nodiscard]] bool autoRepeat() const { return autorepeat_; }
            [[nodiscard]] QUuid uuid() const { return hotkey_uuid_; }

          public slots:

            void registerHotkey();

          signals:

            void sequenceChanged();
            void nameChanged();
            void componentNameChanged();
            void descriptionChanged();
            void errorMessageChanged();
            void contextChanged();
            void activated();
            void autoRepeatChanged();
            void uuidChanged();
            void released();

          private:
            QString sequence_;
            QString name_;
            QString component_name_ = QString("xStudio");
            QString description_;
            QString error_message_;
            QString context_ = QString("any");
            QString window_name_;
            bool autorepeat_ = {false};

            QUuid hotkey_uuid_;
        };

        class VIEWPORT_QML_EXPORT HotkeyReferenceUI : public QMLActor {

            Q_OBJECT

            Q_PROPERTY(QString sequence READ sequence NOTIFY sequenceChanged)
            Q_PROPERTY(
                QUuid hotkeyUuid READ hotkeyUuid WRITE setHotkeyUuid NOTIFY hotkeyUuidChanged)

          public:
            explicit HotkeyReferenceUI(QObject *parent = nullptr);
            ~HotkeyReferenceUI() override = default;

            void init(caf::actor_system &system) override;

            void setHotkeyUuid(const QUuid &uuid);

            [[nodiscard]] const QUuid &hotkeyUuid() const { return uuid_; }
            [[nodiscard]] const QString &sequence() const { return sequence_; }

          signals:

            void sequenceChanged();
            void hotkeyUuidChanged();

          private:
            QString sequence_;
            QUuid uuid_;
        };

    } // namespace qml
} // namespace ui
} // namespace xstudio