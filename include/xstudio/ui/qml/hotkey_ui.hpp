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
namespace ui::qml {

    class VIEWPORT_QML_EXPORT HotkeysUI : public caf::mixin::actor_object<JSONTreeModel> {

        Q_OBJECT

      public:
        using super = caf::mixin::actor_object<JSONTreeModel>;

        Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)
        Q_PROPERTY(
            QString searchString READ searchString WRITE setSearchString NOTIFY
                searchStringChanged)
        Q_PROPERTY(
            QString testSequenceStatus READ testSequenceStatus NOTIFY testSequenceStatusChanged)
        Q_PROPERTY(
            QVariant testSequence READ testSequence WRITE setTestSequence NOTIFY
                testSequenceChanged)
        Q_PROPERTY(
            QString testHotkeyID READ testHotkeyID WRITE setTestHotkeyID NOTIFY
                testHotkeyIDChanged)

        HotkeysUI(QObject *parent = nullptr);
        ~HotkeysUI() override = default;

        [[nodiscard]] QStringList categories() { return categories_; }

        [[nodiscard]] QString searchString() { return search_string_; }

        [[nodiscard]] QString testSequenceStatus() { return test_sequence_status_; }

        [[nodiscard]] QVariant testSequence() { return test_sequence_; }

        void setTestSequence(const QVariant &test_sequence);

        [[nodiscard]] QString testHotkeyID() { return test_hotkey_id_; }

        void setTestHotkeyID(const QString &test_hotkey_id);

        void setSearchString(const QString &search_string) {
            if (search_string != search_string_) {
                search_string_ = search_string;
                emit searchStringChanged();
                makeFilteredHotkeysData();
            }
        }

        Q_INVOKABLE QString hotkey_sequence(const QVariant &hotkey_uuid);

        Q_INVOKABLE QString hotkey_sequence_from_hotkey_name(const QString &hotkey_name);

        Q_INVOKABLE QVariant assignTestSequence();

      signals:

        void categoriesChanged();
        void searchStringChanged();
        void testSequenceStatusChanged();
        void testSequenceChanged();
        void testHotkeyIDChanged();

      private:
        void update_hotkeys_model_data();
        void checkCategories();
        void makeFilteredHotkeysData();

        caf::actor_system &system() { return self()->home_system(); }
        virtual void init(caf::actor_system &system) { super::init(system); }

        std::vector<Hotkey> hotkeys_data_;
        std::vector<Hotkey> filtered_hotkeys_data_;
        QStringList categories_;
        QString search_string_;
        QString test_sequence_status_;
        QVariant test_sequence_;
        QString test_hotkey_id_;
        caf::actor keyboard_manager_;
    };

    class VIEWPORT_QML_EXPORT HotkeyUI : public QQuickItemActor {

        Q_OBJECT

        Q_PROPERTY(QString sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
        Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
        Q_PROPERTY(
            QString componentName READ componentName WRITE setComponentName NOTIFY
                componentNameChanged)
        Q_PROPERTY(
            QString description READ description WRITE setDescription NOTIFY descriptionChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(QString context READ context WRITE setContext NOTIFY contextChanged)
        Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat NOTIFY autoRepeatChanged)
        Q_PROPERTY(QUuid uuid READ uuid NOTIFY uuidChanged)

      public:
        explicit HotkeyUI(QQuickItem *parent = nullptr);
        ~HotkeyUI() override = default;

        void componentComplete() override;

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
        void activated(const QString context);
        void autoRepeatChanged();
        void uuidChanged();
        void released(const QString context);

      private:
        QString sequence_;
        QString name_;
        QString component_name_ = QString("General");
        QString description_;
        QString error_message_;
        QString context_ = QString("any");
        QString window_name_;
        bool autorepeat_ = {false};

        QUuid hotkey_uuid_;
    };

    /*XsPressedKeysMonitor item. This simple item has a single attribute that
    provides a string description of the current presswed kweys.*/
    class VIEWPORT_QML_EXPORT PressedKeysMonitor : public QMLActor {

        Q_OBJECT

        Q_PROPERTY(QStringList pressedKeys READ pressedKeys NOTIFY pressedKeysChanged)
        Q_PROPERTY(QStringList modifiers READ modifiers NOTIFY modifiersChanged)

      public:
        explicit PressedKeysMonitor(QObject *parent = nullptr);

        void init(caf::actor_system &system) override;

        [[nodiscard]] const QStringList &pressedKeys() const { return pressed_keys_; }
        [[nodiscard]] const QStringList &modifiers() const { return modifiers_; }

      signals:

        void pressedKeysChanged();
        void modifiersChanged();

      private:
        QStringList pressed_keys_;
        QStringList modifiers_;
    };

    /*XsHotkeyReference item. This lets us 'watch' an already existing hotkey.
    We use the name of the hotkey to identify it.*/
    class VIEWPORT_QML_EXPORT HotkeyReferenceUI : public QMLActor {

        Q_OBJECT

        Q_PROPERTY(QString sequence READ sequence NOTIFY sequenceChanged)
        Q_PROPERTY(
            QString hotkeyName READ hotkeyName WRITE setHotkeyName NOTIFY hotkeyNameChanged)
        Q_PROPERTY(QUuid uuid READ uuid WRITE setHotkeyUUID NOTIFY uuidChanged)
        Q_PROPERTY(QString context READ context NOTIFY contextChanged)
        Q_PROPERTY(bool exclusive READ exclusive WRITE setExclusive NOTIFY exclusiveChanged)
        Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
        Q_PROPERTY(QVariant key READ key NOTIFY keyChanged)
        Q_PROPERTY(QVariant modifiers READ modifiers NOTIFY modifiersChanged)
        Q_PROPERTY(QVariant defaultKey READ defaultKey NOTIFY defaultKeyChanged)
        Q_PROPERTY(
            QVariant defaultModifiers READ defaultModifiers NOTIFY defaultModifiersChanged)

      public:
        explicit HotkeyReferenceUI(QObject *parent = nullptr);
        ~HotkeyReferenceUI() override;

        void init(caf::actor_system &system) override;

        void setHotkeyName(const QString &name);
        void setHotkeyUUID(const QUuid &uuid);

        [[nodiscard]] QString hotkeyName() const {
            return QStringFromStd(hotkey_.hotkey_name());
        }
        [[nodiscard]] QString sequence() const {
            return QStringFromStd(hotkey_.hotkey_sequence());
        }
        [[nodiscard]] QUuid uuid() const { return QUuidFromUuid(hotkey_.uuid()); }
        [[nodiscard]] QString context() const {
            return QStringFromStd(hotkey_.hotkey_origin());
        }
        [[nodiscard]] bool exclusive() const { return exclusive_; }

        [[nodiscard]] QString description() const {
            return QStringFromStd(hotkey_.hotkey_description());
        }
        [[nodiscard]] QVariant key() const;
        [[nodiscard]] QVariant modifiers() const;
        [[nodiscard]] QVariant defaultKey() const;
        [[nodiscard]] QVariant defaultModifiers() const;

        void setExclusive(const bool exclusive);

      signals:

        void activated(const QString context);
        void sequenceChanged();
        void hotkeyNameChanged();
        void uuidChanged();
        void contextChanged();
        void exclusiveChanged();
        void descriptionChanged();
        void keyChanged();
        void modifiersChanged();
        void defaultKeyChanged();
        void defaultModifiersChanged();

      private:
        void notifyExclusiveChanged();

        Hotkey hotkey_;
        bool exclusive_ = {false};
    };

} // namespace ui::qml
} // namespace xstudio
