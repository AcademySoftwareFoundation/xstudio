// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/hotkey_ui.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::ui;

QMap<int, QVariant> hotkey_to_qvariant_map(const Hotkey &hotkey) {

    QMap<int, QVariant> hk_data;
    hk_data[Qt::UserRole + 1] = QVariant(QStringFromStd(hotkey.key()));
    hk_data[Qt::UserRole + 2] = QVariant(hotkey.modifiers());
    hk_data[Qt::UserRole + 3] = QStringFromStd(hotkey.hotkey_name());
    hk_data[Qt::UserRole + 4] = QStringFromStd(hotkey.hotkey_origin());
    hk_data[Qt::UserRole + 5] = QStringFromStd(hotkey.hotkey_description());
    hk_data[Qt::UserRole + 6] = QStringFromStd(hotkey.hotkey_sequence());

    return hk_data;
}

HotkeysUI::HotkeysUI(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    try {

        scoped_actor sys{system()};

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        request_receive<bool>(
            *sys, hotkeys_config_events_group, broadcast::join_broadcast_atom_v, as_actor());

        auto hotkeys = request_receive<std::vector<Hotkey>>(
            *sys, keyboard_manager, keypress_monitor::register_hotkey_atom_v);

        int row = 0;
        for (const auto &hk : hotkeys) {
            hotkeys_data_.push_back(hotkey_to_qvariant_map(hk));
            const QModelIndex index = createIndex(row, 0);
            for (int r = 1; r <= 6; ++r) {
                emit dataChanged(index, index, {Qt::UserRole + r});
            }
            row++;
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {
                update_hotkeys_model_data(hotkeys);
            }

        };
    });
}

void HotkeysUI::update_hotkeys_model_data(const std::vector<Hotkey> &new_hotkeys_data) {

    size_t row = 0;
    for (const auto &hk : new_hotkeys_data) {
        auto var_data = hotkey_to_qvariant_map(hk);
        if (hotkeys_data_.size() > row) {
            if (var_data != hotkeys_data_[row]) {
                hotkeys_data_[row]      = var_data;
                const QModelIndex index = createIndex(row, 0);
                for (int r = 1; r <= 6; ++r)
                    emit dataChanged(index, index, {Qt::UserRole + r});
            }
        } else {
            beginInsertRows(
                QModelIndex(),
                hotkeys_data_.size(),
                static_cast<int>(new_hotkeys_data.size()) - 1);
            hotkeys_data_.push_back(var_data);
            endInsertRows();
        }
        row++;
    }
}

QHash<int, QByteArray> HotkeysUI::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[Qt::UserRole + 1] = "keyboard_key";
    roles[Qt::UserRole + 2] = "modifiers";
    roles[Qt::UserRole + 3] = "hotkey_name";
    roles[Qt::UserRole + 4] = "component";
    roles[Qt::UserRole + 5] = "hotkey_description";
    roles[Qt::UserRole + 6] = "sequence";
    roles[Qt::UserRole + 7] = "error_message";
    return roles;
}

int HotkeysUI::rowCount(const QModelIndex &) const {
    return static_cast<int>(hotkeys_data_.size());
}

QVariant HotkeysUI::data(const QModelIndex &index, int role) const {

    QVariant rt;

    try {
        if ((int)hotkeys_data_.size() > index.row() &&
            hotkeys_data_[index.row()].contains(role)) {
            rt = hotkeys_data_[index.row()][role];
        }
    } catch (const std::exception &) {
    }

    return rt;
}

bool HotkeysUI::setData(const QModelIndex &index, const QVariant &value, int role) {

    /*role -= Qt::UserRole+1;
    emit setAttributeFromFrontEnd(
        attributes_data_[index.row()][Attribute::UuidRole].toUuid(),
        role,
        value
        );*/

    return false;
}


HotkeyUI::HotkeyUI(QObject *parent) : QMLActor(parent) {
    init(CafSystemObject::get_actor_system());
}

void HotkeyUI::init(actor_system &system_) {

    QMLActor::init(system_);

    scoped_actor sys{system()};

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid &uuid,
                const bool hotkey_pressed,
                const std::string &context) {
                if (hotkey_uuid_ == uuid && hotkey_pressed) {
                    emit activated();
                }
            }

        };
    });

    QObject::connect(this, &HotkeyUI::sequenceChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::nameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::componentNameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::descriptionChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::contextChanged, this, &HotkeyUI::registerHotkey);

    emit componentNameChanged(); // for default componentName ('xStudio')
    emit autoRepeatChanged();    // for default componentName ('xStudio')
}

void HotkeyUI::registerHotkey() {
    if (sequence_.isNull() || name_.isNull() || component_name_.isNull() || context_.isNull()) {
        // not ready, some properties not set (yet)
        return;
    }

    QKeySequence seq(sequence_);

    if (seq.count() == 1) {
        int mod = 0;
        if ((seq[0] & Qt::ShiftModifier) == Qt::ShiftModifier) {
            mod |= KeyboardModifier::ShiftModifier;
        }
        if ((seq[0] & Qt::MetaModifier) == Qt::MetaModifier) {
            mod |= KeyboardModifier::MetaModifier;
        }
        if ((seq[0] & Qt::ControlModifier) == Qt::ControlModifier) {
            mod |= KeyboardModifier::ControlModifier;
        }
        if ((seq[0] & Qt::AltModifier) == Qt::AltModifier) {
            mod |= KeyboardModifier::AltModifier;
        }

        int key = seq[0] & 0x01ffffff;

        auto keypress_event_manager =
            self()->home_system().registry().template get<caf::actor>(keyboard_events);

        Hotkey hk(
            key,
            mod,
            StdFromQString(name_),
            StdFromQString(component_name_),
            StdFromQString(description_),
            StdFromQString(context_),
            autorepeat_,
            caf::actor_cast<caf::actor_addr>(as_actor()));

        anon_send(keypress_event_manager, ui::keypress_monitor::register_hotkey_atom_v, hk);

        hotkey_uuid_ = hk.uuid();

    } else {
    }
}

HotkeyReferenceUI::HotkeyReferenceUI(QObject *parent) : QMLActor(parent) {
    init(CafSystemObject::get_actor_system());
}

void HotkeyReferenceUI::init(caf::actor_system &system_) {

    QMLActor::init(system_);
    scoped_actor sys{system()};

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

        };
    });
}

void HotkeyReferenceUI::setHotkeyUuid(const QUuid &uuid) {

    uuid_ = uuid;
    Q_EMIT hotkeyUuidChanged();

    if (!uuid_.isNull()) {
        try {

            scoped_actor sys{system()};

            auto keyboard_manager =
                system().registry().template get<caf::actor>(keyboard_events);

            auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
                *sys,
                keyboard_manager,
                utility::get_event_group_atom_v,
                keypress_monitor::hotkey_event_atom_v);

            const auto hk = request_receive<Hotkey>(
                *sys,
                keyboard_manager,
                ui::keypress_monitor::hotkey_atom_v,
                UuidFromQUuid(uuid_));

            QString seq = QStringFromStd(hk.hotkey_sequence());

            if (seq != sequence_) {
                sequence_ = seq;
                Q_EMIT sequenceChanged();
            }

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
}
