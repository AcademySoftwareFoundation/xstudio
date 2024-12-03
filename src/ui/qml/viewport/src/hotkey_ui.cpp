// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/hotkey_ui.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::ui;

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

        update_hotkeys_model_data(hotkeys);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {
                update_hotkeys_model_data(hotkeys);
            },
            [=](keypress_monitor::hotkey_event_atom, Hotkey & /*hotkey*/) {
                // update_hotkeys_model_data(hotkeys);
            },
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid kotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window) {
                // actual hotkey pressed or release ... we ignore
            }};
    });
}

void HotkeysUI::update_hotkeys_model_data(const std::vector<Hotkey> &new_hotkeys_data) {

    hotkeys_data_ = new_hotkeys_data;

    // sort by the name of the hotkey
    std::sort(
        hotkeys_data_.begin(),
        hotkeys_data_.end(),
        [](const Hotkey &a, const Hotkey &b) -> bool {
            return a.hotkey_name() < b.hotkey_name();
        });

    beginResetModel();
    endResetModel();
    emit rowCountChanged();
    checkCategories();
}

void HotkeysUI::checkCategories() {
    QSet<QString> cats;
    for (const auto &hk : hotkeys_data_) {
        cats.insert(QStringFromStd(hk.hotkey_origin()));
    }
    if (cats.values() != categories_) {
        categories_ = cats.values();
        emit categoriesChanged();
    }
}

QHash<int, QByteArray> HotkeysUI::roleNames() const {
    QHash<int, QByteArray> roles;
    for (auto i = hotkeyRoleNames.keyValueBegin(); i != hotkeyRoleNames.keyValueEnd(); ++i) {
        roles[i->first] = i->second;
    }
    return roles;
}

int HotkeysUI::rowCount(const QModelIndex &) const {
    int ct = 0;
    const std::string curr_cat(StdFromQString(current_category_));
    for (const auto &hk : hotkeys_data_) {
        if (hk.hotkey_origin() == curr_cat) {
            ct++;
        }
    }
    return ct;
}

QVariant HotkeysUI::data(const QModelIndex &index, int role) const {

    QVariant rt;

    try {
        int ct = 0;
        const std::string curr_cat(StdFromQString(current_category_));
        for (const auto &hk : hotkeys_data_) {
            if (hk.hotkey_origin() == curr_cat) {
                if (ct == index.row()) {

                    if (role == keyboardKey)
                        rt = QStringFromStd(hk.key());
                    if (role == keyModifiers)
                        rt = hk.modifiers();
                    if (role == hotkeyName)
                        rt = QStringFromStd(hk.hotkey_name());
                    if (role == hotkeyCategory)
                        rt = QStringFromStd(hk.hotkey_origin());
                    if (role == hotkeyDescription)
                        rt = QStringFromStd(hk.hotkey_description());
                    if (role == hotkeySequence)
                        rt = QStringFromStd(hk.hotkey_sequence());
                    break;
                }
                ct++;
            }
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

QString HotkeysUI::hotkey_sequence(const QVariant &hotkey_uuid) {
    QString result;
    utility::Uuid hk_uuid;
    if (hotkey_uuid.canConvert<QUuid>()) {
        hk_uuid = UuidFromQUuid(hotkey_uuid.value<QUuid>());
    } else {
        hk_uuid.from_string(StdFromQString(hotkey_uuid.toString()));
    }
    for (const auto &hk : hotkeys_data_) {
        if (hk.uuid() == hk_uuid) {
            result = QStringFromStd(hk.hotkey_sequence());
            break;
        }
    }
    return result;
}

QString HotkeysUI::hotkey_sequence_from_hotkey_name(const QString &hotkey_name) {
    QString result;
    const std::string nm(StdFromQString(hotkey_name));
    for (const auto &hk : hotkeys_data_) {
        if (hk.hotkey_name() == nm) {
            result = QStringFromStd(hk.hotkey_sequence());
            break;
        }
    }
    return result;
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
                const std::string &context,
                const std::string &window) {
                // if the hotkey was pressed in a different parent window to
                // the parent of this XsHotkey item, we don't activate
                if (hotkey_pressed && StdFromQString(window_name_) != window)
                    return;

                if (hotkey_uuid_ == QUuidFromUuid(uuid)) {
                    if (context_.isNull() || context_ == QString("any") ||
                        StdFromQString(context_) == context) {
                        if (hotkey_pressed)
                            emit activated();
                        else
                            emit released();
                    }
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
    emit uuidChanged();
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

        window_name_ = item_window_name(parent());

        Hotkey hk(
            key,
            mod,
            StdFromQString(name_),
            StdFromQString(component_name_),
            StdFromQString(description_),
            StdFromQString(window_name_),
            autorepeat_,
            caf::actor_cast<caf::actor_addr>(as_actor()));

        hotkey_uuid_ = QUuidFromUuid(hk.uuid());

        emit uuidChanged();

        anon_send(keypress_event_manager, ui::keypress_monitor::register_hotkey_atom_v, hk);

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
